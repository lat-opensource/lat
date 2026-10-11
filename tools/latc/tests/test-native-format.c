#include "lat-fallback.h"
#include "lat-native-image.h"
#include "native-image.h"

#include <elf.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(LatX86StateV1) == 560,
               "LatX86StateV1 ABI size changed");
_Static_assert(offsetof(LatX86StateV1, xmm) == 300,
               "LatX86StateV1 XMM offset changed");
_Static_assert(sizeof(LatNativeImageHeaderV2) == 240,
               "native image header size changed");
_Static_assert(sizeof(LatNativeTbV1) == 56,
               "native TB record size changed");
_Static_assert(sizeof(LatNativeRelocationV1) == 40,
               "native relocation record size changed");
_Static_assert(sizeof(LatNativePcMapV2) == 32,
               "native PC map record size changed");
_Static_assert(sizeof(LatNativeTuV1) == 32,
               "native TU record size changed");

static int write_image(const char *path, const void *image, size_t size)
{
    FILE *output = fopen(path, "wb");
    return !output || fwrite(image, size, 1, output) != 1 || fclose(output);
}

static int test_branch_target_decode(void)
{
    uint32_t words[2] = {0};
    uint64_t target = 0;

    words[0] = 0x58000400u;
    if (!lat_native_decode_local_branch_target(
            words, 1, 8, 32, &target) || target != 12) {
        return 1;
    }
    words[0] = 0x48000000u | (0xffffu << 10) | (0x1fu << 16);
    if (!lat_native_decode_local_branch_target(
            words, 1, 8, 32, &target) || target != 4) {
        return 1;
    }
    words[0] = 0x50000000u | (0xfffeu << 10) | (0x3ffu << 16);
    if (!lat_native_decode_local_branch_target(
            words, 1, 8, 32, &target) || target != 0) {
        return 1;
    }
    words[0] = 0x58000000u | (0xffffu << 10);
    if (!lat_native_decode_local_branch_target(
            words, 1, 8, 32, &target) || target != 4) {
        return 1;
    }
    words[0] = 0x1e000000u;
    words[1] = 0x4c000000u;
    if (!lat_native_decode_local_branch_target(
            words, 2, 8, 32, &target) || target != 8) {
        return 1;
    }
    return lat_native_decode_local_branch_target(
               words, 1, 8, 4, &target);
}

static int test_image_data(const unsigned char *source, size_t size,
                           int argc, char **argv)
{
    unsigned char image[1024];
    memcpy(image, source, sizeof(image));
    LatNativeImageHeaderV2 *header = (void *)image;
    LatNativeRelocationV1 *relocation =
        (void *)(image + header->relocation_offset);
    LatNativeTuV1 *tu = (void *)(image + header->tu_table_offset);
    uint32_t *code = (void *)(image + header->code_offset);
    tu->code_size = 16;
    tu->search_offset = 16;
    code[2] = 0x1400000c;
    code[3] = 0x0380018c;
    memset(image + header->code_offset + 24, 0xa5, 8);
    *relocation = (LatNativeRelocationV1){
        .code_offset = 8, .kind = LAT_NATIVE_RELOC_IMAGE_DATA,
        .slots = 2, .addend = 24, .reserved = 8,
    };
    char error[256] = {0};
    if (lat_native_image_validate(image, size, error, sizeof(error))) {
        fprintf(stderr, "valid data relocation rejected: %s\n", error);
        return 1;
    }
    LatNativeRelocationV1 valid = *relocation;
    for (int bad = 0; bad < 10; bad++) {
        *relocation = valid;
        switch (bad) {
        case 0: relocation->addend = -1; break;
        case 1: relocation->addend = 32; break;
        case 2: relocation->reserved = 9; break;
        case 3: relocation->reserved = 0; break;
        case 4: relocation->addend = 12; break;
        case 5: relocation->slots = 1; break;
        case 6: relocation->target = 1; break;
        case 7: relocation->physical_target_plus_one = 1; break;
        case 8: code[3] ^= 1; break;
        case 9:
            code[3] ^= 1;
            relocation->code_offset = 24;
            code[6] = 0x1400000c;
            code[7] = 0x0380018c;
            break;
        }
        if (!lat_native_image_validate(image, size, error, sizeof(error))) {
            fprintf(stderr, "invalid data relocation %d accepted\n", bad);
            return 1;
        }
    }
    memset(image + header->code_offset + 24, 0xa5, 8);
    *relocation = valid;
    if (argc == 4) {
        if (write_image(argv[1], image, size) ||
            lat_native_image_merge_files(argv[1], argv[1], argv[3],
                                         error, sizeof(error))) {
            fprintf(stderr, "cannot merge data relocations: %s\n", error);
            return 1;
        }
        unsigned char merged[1024];
        FILE *file = fopen(argv[3], "rb");
        size_t count = file ? fread(merged, 1, sizeof(merged), file) : 0;
        if (!file || fclose(file) ||
            lat_native_image_validate(merged, count, error, sizeof(error))) {
            fprintf(stderr, "invalid merged data image: %s\n", error);
            return 1;
        }
        const LatNativeImageHeaderV2 *mh = (const void *)merged;
        const LatNativeRelocationV1 *mr =
            (const void *)(merged + mh->relocation_offset);
        if (mh->code_size != 64 || mr[0].addend != 24 ||
            mr[1].addend != 56 || mr[1].code_offset != 40 ||
            memcmp(merged + mh->code_offset + 24,
                   image + header->code_offset + 24, 8) ||
            memcmp(merged + mh->code_offset + 56,
                   image + header->code_offset + 24, 8)) {
            fprintf(stderr, "merged data offset or contents changed\n");
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    unsigned char image[1024] = {0};
    LatNativeImageHeaderV2 *header = (void *)image;
    LatNativeTbV1 *tb;
    LatNativeRelocationV1 *relocation;
    char error[128] = {0};
    size_t image_size;
    if (test_branch_target_decode()) return 1;

    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    header->code_offset = header->guest_image_offset + header->guest_image_size;
    header->code_size = 32;
    header->tb_table_offset = header->code_offset + header->code_size;
    header->tb_count = 1;
    header->relocation_offset = header->tb_table_offset + sizeof(*tb);
    header->relocation_count = 1;
    header->pc_map_offset = header->relocation_offset +
                            sizeof(LatNativeRelocationV1);
    header->tu_table_offset = header->pc_map_offset;
    header->tu_count = 1;
    strcpy(header->lat_build_id, "test-build");
    Elf64_Ehdr *elf = (void *)(image + header->guest_image_offset);
    memcpy(elf->e_ident, ELFMAG, SELFMAG);
    elf->e_ident[EI_CLASS] = ELFCLASS64;
    elf->e_ident[EI_DATA] = ELFDATA2LSB;
    elf->e_type = ET_EXEC;
    elf->e_machine = EM_X86_64;
    elf->e_entry = 0x401000;
    elf->e_phoff = sizeof(*elf);
    elf->e_ehsize = sizeof(*elf);
    elf->e_phentsize = sizeof(Elf64_Phdr);
    elf->e_phnum = 1;
    Elf64_Phdr *phdr = (void *)((unsigned char *)elf + elf->e_phoff);
    phdr->p_type = PT_LOAD;
    phdr->p_flags = PF_R | PF_X;
    phdr->p_vaddr = elf->e_entry;
    phdr->p_memsz = 16;
    header->guest_entry = elf->e_entry;
    tb = (void *)(image + header->tb_table_offset);
    tb->guest_pc = 0x401000;
    tb->code_offset = 0;
    tb->code_size = 16;
    tb->tu_index = 0;
    relocation = (void *)(image + header->relocation_offset);
    relocation->code_offset = 8;
    relocation->kind = LAT_NATIVE_RELOC_RUNTIME_SYMBOL;
    relocation->target = LAT_NATIVE_SYMBOL_RAISE_SYSCALL;
    relocation->slots = 3;
    relocation->physical_target_plus_one = 1;
    LatNativeTuV1 *tu = (void *)(image + header->tu_table_offset);
    tu->code_offset = 0;
    tu->code_size = header->code_size;
    tu->search_offset = header->code_size;
    image_size = header->tu_table_offset + sizeof(*tu);

    if (lat_native_image_validate(image, image_size, error,
                                  sizeof(error)) != 0) {
        fprintf(stderr, "valid image rejected: %s\n", error);
        return 1;
    }
    uint32_t *code = (void *)(image + header->code_offset);
    code[0] = 0x58000400u; /* beq zero, zero, +4 */
    tb->conditional_exit_offset = 1;
    if (lat_native_image_validate(image, image_size, error, sizeof(error))) {
        fprintf(stderr, "valid conditional exit rejected: %s\n", error);
        return 1;
    }
    const uint32_t bad_offsets[] = {2, 17, UINT32_MAX};
    for (size_t i = 0; i < sizeof(bad_offsets) / sizeof(bad_offsets[0]); i++) {
        tb->conditional_exit_offset = bad_offsets[i];
        if (!lat_native_image_validate(image, image_size, error, sizeof(error))) {
            fprintf(stderr, "bad conditional offset accepted\n");
            return 1;
        }
    }
    tb->conditional_exit_offset = 1;
    const uint32_t bad_instructions[] = {
        0x03400000u, 0x58001000u, 0x5bfffc00u,
    }; /* NOP, target at TB end, negative target */
    for (size_t i = 0;
         i < sizeof(bad_instructions) / sizeof(bad_instructions[0]); i++) {
        code[0] = bad_instructions[i];
        if (!lat_native_image_validate(image, image_size, error, sizeof(error))) {
            fprintf(stderr, "bad conditional instruction accepted\n");
            return 1;
        }
    }
    code[0] = 0x58000400u;
    tb->indirect_exit_offset = 1;
    if (!lat_native_image_validate(image, image_size, error, sizeof(error))) {
        fprintf(stderr, "invalid indirect exit accepted\n");
        return 1;
    }
    tb->indirect_exit_offset = 0;
    header->version = 5;
    if (!lat_native_image_validate(image, image_size, error, sizeof(error))) {
        fprintf(stderr, "old native version accepted\n");
        return 1;
    }
    header->version = LAT_NATIVE_IMAGE_VERSION;
    if (test_image_data(image, image_size, argc, argv)) return 1;
    if (argc == 4) {
        if (write_image(argv[1], image, image_size)) return 1;
        tb->guest_pc = 0x401010;
        if (write_image(argv[2], image, image_size) ||
            lat_native_image_merge_files(argv[1], argv[2], argv[3],
                                         error, sizeof(error)) ||
            lat_native_image_inspect_file(argv[3], header, error,
                                          sizeof(error)) ||
            header->tb_count != 2 || header->code_size != 64 ||
            header->relocation_count != 2 || header->tu_count != 2) {
            fprintf(stderr, "cannot merge native images: %s\n", error);
            return 1;
        }
        LatNativeTbV1 merged_tbs[2];
        LatNativeTuV1 merged_tus[2];
        LatNativeRelocationV1 merged_relocations[2];
        FILE *merged_file = fopen(argv[3], "rb");
        if (!merged_file || fseek(merged_file, header->tb_table_offset, SEEK_SET) ||
            fread(merged_tbs, sizeof(merged_tbs), 1, merged_file) != 1 ||
            fseek(merged_file, header->tu_table_offset, SEEK_SET) ||
            fread(merged_tus, sizeof(merged_tus), 1, merged_file) != 1 ||
            fseek(merged_file, header->relocation_offset, SEEK_SET) ||
            fread(merged_relocations, sizeof(merged_relocations), 1,
                  merged_file) != 1 || fclose(merged_file) ||
            merged_tbs[0].conditional_exit_offset != 1 ||
            merged_tbs[1].conditional_exit_offset != 1 ||
            merged_tbs[0].tu_index != 0 || merged_tbs[1].tu_index != 1 ||
            merged_tbs[1].code_offset != 32 ||
            merged_tus[0].code_offset != 0 ||
            merged_tus[1].code_offset != 32 ||
            merged_relocations[0].physical_target_plus_one != 1 ||
            merged_relocations[1].physical_target_plus_one != 2) {
            fprintf(stderr, "merged conditional exit metadata changed\n");
            return 1;
        }
        error[0] = '\0';
        if (lat_native_image_merge_files(argv[1], argv[1], argv[3],
                                         error, sizeof(error)) ||
            lat_native_image_inspect_file(argv[3], header, error,
                                          sizeof(error)) ||
            header->tb_count != 2 || header->code_size != 64 ||
            header->relocation_count != 2 || header->tu_count != 2) {
            fprintf(stderr, "cannot merge overlapping native TBs: %s\n",
                    error);
            return 1;
        }
        FILE *aliased_file = fopen(argv[3], "rb");
        LatNativeTbV1 aliased_tbs[2];
        if (!aliased_file ||
            fseek(aliased_file, header->tb_table_offset, SEEK_SET) ||
            fread(aliased_tbs, sizeof(aliased_tbs), 1, aliased_file) != 1 ||
            fclose(aliased_file) ||
            aliased_tbs[0].guest_pc != aliased_tbs[1].guest_pc ||
            aliased_tbs[0].flags != aliased_tbs[1].flags ||
            aliased_tbs[0].code_offset != 0 ||
            aliased_tbs[1].code_offset != 32 ||
            aliased_tbs[0].tu_index != 0 ||
            aliased_tbs[1].tu_index != 1) {
            fprintf(stderr, "merged physical TB alias was not preserved\n");
            return 1;
        }
        remove(argv[1]);
        remove(argv[2]);
        remove(argv[3]);
        puts("test-native-format: PASS merge=2 overlap=physical-alias");
        return 0;
    }
    tb->conditional_exit_offset = 0;
    code[0] = 0;
    if (argc == 2) {
        if (write_image(argv[1], image, image_size)) {
            fprintf(stderr, "cannot write native image fixture\n");
            return 1;
        }
        if (lat_native_image_mark_x86_static_file(argv[1], error,
                                                  sizeof(error)) != 0 ||
            lat_native_image_inspect_file(argv[1], header, error,
                                          sizeof(error)) != 0 ||
            !(header->flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
            fprintf(stderr, "cannot mark static x86 image: %s\n", error);
            return 1;
        }

        header->flags = LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP |
                        LAT_NATIVE_IMAGE_PIE;
        elf->e_type = ET_DYN;
        elf->e_entry = 0x1000;
        phdr->p_vaddr = elf->e_entry;
        header->guest_entry = elf->e_entry;
        if (write_image(argv[1], image, image_size) ||
            lat_native_image_mark_x86_static_file(argv[1], error,
                                                  sizeof(error)) != 0 ||
            lat_native_image_inspect_file(argv[1], header, error,
                                          sizeof(error)) != 0 ||
            !(header->flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
            fprintf(stderr, "cannot mark static PIE x86 image: %s\n", error);
            return 1;
        }

        header->flags = LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP;
        elf->e_type = ET_EXEC;
        elf->e_entry = 0x401000;
        phdr->p_vaddr = elf->e_entry;
        header->guest_entry = elf->e_entry;
        phdr->p_type = PT_INTERP;
        if (write_image(argv[1], image, image_size)) return 1;
        error[0] = '\0';
        if (lat_native_image_mark_x86_static_file(argv[1], error,
                                                  sizeof(error)) == 0 ||
            !strstr(error, "dynamically linked")) {
            fprintf(stderr, "dynamic x86 image accepted: %s\n", error);
            return 1;
        }
        phdr->p_type = PT_LOAD;
        if (write_image(argv[1], image, image_size) ||
            lat_native_image_mark_x86_static_file(argv[1], error,
                                                  sizeof(error))) {
            fprintf(stderr, "cannot restore static x86 image: %s\n", error);
            return 1;
        }
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [OUTPUT | BASE DELTA MERGED]\n", argv[0]);
        return 2;
    }
    tb->code_size = 33;
    if (lat_native_image_validate(image, image_size, error,
                                  sizeof(error)) == 0) {
        fprintf(stderr, "bad TB range accepted: %s\n", error);
        return 1;
    }
    tb->code_size = 16;
    relocation->code_offset = header->code_size - 4;
    if (lat_native_image_validate(image, image_size, error,
                                  sizeof(error)) == 0 ||
        !strstr(error, "relocation")) {
        fprintf(stderr, "bad relocation range accepted: %s\n", error);
        return 1;
    }
    relocation->code_offset = 8;
    LatNativePcMapV2 *pc_map = (void *)(image + header->pc_map_offset);
    LatNativeTuV1 saved_tu = *(LatNativeTuV1 *)(
        image + header->tu_table_offset);
    header->tu_table_offset += sizeof(*pc_map);
    *(LatNativeTuV1 *)(image + header->tu_table_offset) = saved_tu;
    *pc_map = (LatNativePcMapV2) {
        .guest_pc = 0x401000,
        .host_offset_begin = 0,
        .host_offset_end = 16,
        .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
    };
    header->pc_map_count = 1;
    header->flags &= ~LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP;
    image_size += sizeof(*pc_map);
    if (lat_native_image_validate(image, image_size, error,
                                  sizeof(error)) != 0) {
        fprintf(stderr, "precise PC map rejected: %s\n", error);
        return 1;
    }
    pc_map->state_record_offset = (uint32_t)(int32_t)-8;
    pc_map->flags |= LAT_NATIVE_PC_MAP_STACK_POINTER_DELTA;
    if (lat_native_image_validate(image, image_size, error,
                                  sizeof(error)) != 0) {
        fprintf(stderr, "stack delta PC map rejected: %s\n", error);
        return 1;
    }
    pc_map->flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE;
    if (lat_native_image_validate(image, image_size, error,
                                  sizeof(error)) == 0 ||
        !strstr(error, "PC map")) {
        fprintf(stderr, "unflagged stack delta accepted: %s\n", error);
        return 1;
    }
    pc_map->state_record_offset = 0;
    pc_map->flags |= LAT_NATIVE_PC_MAP_STACK_POINTER_DELTA;
    if (lat_native_image_validate(image, image_size, error,
                                  sizeof(error)) == 0 ||
        !strstr(error, "PC map")) {
        fprintf(stderr, "empty stack delta flag accepted: %s\n", error);
        return 1;
    }
    pc_map->flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE | (1u << 31);
    if (lat_native_image_validate(image, image_size, error,
                                  sizeof(error)) == 0 ||
        !strstr(error, "PC map")) {
        fprintf(stderr, "unknown PC map flag accepted: %s\n", error);
        return 1;
    }
    pc_map->flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE;
    pc_map->host_offset_end = header->code_size + 1;
    if (lat_native_image_validate(image, image_size, error,
                                  sizeof(error)) == 0 ||
        !strstr(error, "PC map")) {
        fprintf(stderr, "bad PC map accepted: %s\n", error);
        return 1;
    }
    puts("test-native-format: PASS");
    return 0;
}
