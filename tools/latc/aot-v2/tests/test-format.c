#define _GNU_SOURCE

#include "elf-validate.h"

#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
    FILE_SIZE = 2048,
    NOTE_OFFSET = 0x200,
    DYNAMIC_OFFSET = 0x300,
    STRING_OFFSET = 0x380,
    SYMBOL_OFFSET = 0x400,
    TEXT_OFFSET = 0x480,
    SECTION_OFFSET = 0x500,
    RELOCATION_OFFSET = 0x700,
};

typedef struct TestNote {
    Elf64_Nhdr header;
    char name[4];
    LatAotNoteV2 description;
} TestNote;

static size_t build_elf(unsigned char file[FILE_SIZE])
{
    memset(file, 0, FILE_SIZE);
    Elf64_Ehdr *header = (void *)file;
    memcpy(header->e_ident, ELFMAG, SELFMAG);
    header->e_ident[EI_CLASS] = ELFCLASS64;
    header->e_ident[EI_DATA] = ELFDATA2LSB;
    header->e_ident[EI_VERSION] = EV_CURRENT;
    header->e_type = ET_DYN;
    header->e_machine = EM_LOONGARCH;
    header->e_version = EV_CURRENT;
    header->e_ehsize = sizeof(*header);
    header->e_phoff = sizeof(*header);
    header->e_phentsize = sizeof(Elf64_Phdr);
    header->e_phnum = 4;
    header->e_shoff = SECTION_OFFSET;
    header->e_shentsize = sizeof(Elf64_Shdr);
    header->e_shnum = 5;

    Elf64_Phdr *phdrs = (void *)(file + header->e_phoff);
    phdrs[0] = (Elf64_Phdr) {
        .p_type = PT_LOAD,
        .p_flags = PF_R | PF_X,
        .p_offset = 0,
        .p_vaddr = 0,
        .p_filesz = FILE_SIZE,
        .p_memsz = FILE_SIZE,
        .p_align = 0x1000,
    };
    phdrs[1] = (Elf64_Phdr) {
        .p_type = PT_NOTE,
        .p_flags = PF_R,
        .p_offset = NOTE_OFFSET,
        .p_vaddr = NOTE_OFFSET,
        .p_filesz = sizeof(TestNote),
        .p_memsz = sizeof(TestNote),
        .p_align = 4,
    };
    phdrs[2] = (Elf64_Phdr) {
        .p_type = PT_DYNAMIC,
        .p_flags = PF_R,
        .p_offset = DYNAMIC_OFFSET,
        .p_vaddr = DYNAMIC_OFFSET,
        .p_filesz = 5 * sizeof(Elf64_Dyn),
        .p_memsz = 5 * sizeof(Elf64_Dyn),
        .p_align = 8,
    };
    phdrs[3] = (Elf64_Phdr) {
        .p_type = PT_LOAD,
        .p_flags = PF_R | PF_W,
        .p_offset = RELOCATION_OFFSET,
        .p_vaddr = 0x1700,
        .p_filesz = 0x100,
        .p_memsz = 0x100,
        .p_align = 0x1000,
    };

    TestNote *note = (void *)(file + NOTE_OFFSET);
    note->header.n_namesz = sizeof(LAT_AOT_V2_NOTE_NAME);
    note->header.n_descsz = sizeof(LatAotNoteV2);
    note->header.n_type = LAT_AOT_V2_NOTE_TYPE;
    memcpy(note->name, LAT_AOT_V2_NOTE_NAME,
           sizeof(LAT_AOT_V2_NOTE_NAME));
    memcpy(note->description.magic, "LATAOT2", 8);
    note->description.abi_version = LAT_AOT_V2_ABI_VERSION;
    note->description.struct_size = sizeof(LatAotNoteV2);
    note->description.module_flags = LAT_AOT_MODULE_READONLY_TEXT |
                                     LAT_AOT_MODULE_SYNTHETIC_FIXTURE;
    note->description.required_features = LAT_AOT_V2_REQUIRED_BASE_FEATURES;
    memset(note->description.source_sha256, 0x11, 32);
    memset(note->description.codegen_id, 0x22, 32);
    memset(note->description.tbset_digest, 0x33, 32);

    char *strings = (void *)(file + STRING_OFFSET);
    size_t needed = 1;
    strcpy(strings + needed, LAT_AOT_V2_RUNTIME_SONAME);
    size_t runtime = needed + strlen(strings + needed) + 1;
    strcpy(strings + runtime, LAT_AOT_V2_RUNTIME_ABI_SYMBOL);
    size_t descriptor = runtime + strlen(strings + runtime) + 1;
    strcpy(strings + descriptor, LAT_AOT_V2_DESCRIPTOR_SYMBOL);
    size_t string_size = descriptor + strlen(strings + descriptor) + 1;

    Elf64_Dyn *dynamic = (void *)(file + DYNAMIC_OFFSET);
    dynamic[0] = (Elf64_Dyn) { .d_tag = DT_STRTAB,
                               .d_un.d_ptr = STRING_OFFSET };
    dynamic[1] = (Elf64_Dyn) { .d_tag = DT_STRSZ,
                               .d_un.d_val = string_size };
    dynamic[2] = (Elf64_Dyn) { .d_tag = DT_NEEDED,
                               .d_un.d_val = needed };
    dynamic[3] = (Elf64_Dyn) { .d_tag = DT_NULL };

    Elf64_Sym *symbols = (void *)(file + SYMBOL_OFFSET);
    symbols[1] = (Elf64_Sym) {
        .st_name = runtime,
        .st_info = (STB_GLOBAL << 4) | STT_FUNC,
        .st_shndx = SHN_UNDEF,
    };
    symbols[2] = (Elf64_Sym) {
        .st_name = descriptor,
        .st_info = (STB_GLOBAL << 4) | STT_OBJECT,
        .st_shndx = 3,
    };

    Elf64_Shdr *sections = (void *)(file + SECTION_OFFSET);
    sections[1] = (Elf64_Shdr) {
        .sh_type = SHT_STRTAB,
        .sh_flags = SHF_ALLOC,
        .sh_addr = STRING_OFFSET,
        .sh_offset = STRING_OFFSET,
        .sh_size = string_size,
        .sh_addralign = 1,
    };
    sections[2] = (Elf64_Shdr) {
        .sh_type = SHT_DYNSYM,
        .sh_flags = SHF_ALLOC,
        .sh_addr = SYMBOL_OFFSET,
        .sh_offset = SYMBOL_OFFSET,
        .sh_size = 3 * sizeof(Elf64_Sym),
        .sh_link = 1,
        .sh_info = 1,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf64_Sym),
    };
    sections[3] = (Elf64_Shdr) {
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
        .sh_addr = TEXT_OFFSET,
        .sh_offset = TEXT_OFFSET,
        .sh_size = 4,
        .sh_addralign = 4,
    };
    sections[4] = (Elf64_Shdr) {
        .sh_type = SHT_RELA,
        .sh_flags = SHF_ALLOC,
        .sh_addr = 0x1700,
        .sh_offset = RELOCATION_OFFSET,
        .sh_size = sizeof(Elf64_Rela),
        .sh_link = 2,
        .sh_info = 3,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf64_Rela),
    };
    Elf64_Rela *relocation = (void *)(file + RELOCATION_OFFSET);
    relocation->r_offset = 0x1700;
    relocation->r_info = 3;
    return string_size;
}

static int rewrite(int fd, const unsigned char file[FILE_SIZE])
{
    return ftruncate(fd, 0) || lseek(fd, 0, SEEK_SET) != 0 ||
           write(fd, file, FILE_SIZE) != FILE_SIZE || fsync(fd);
}

static int expect_result(int fd, const LatAotExpectedV2 *expected,
                         int success, const char *message)
{
    char error[256] = {0};
    LatAotNoteV2 note;
    int result = lat_aot_v2_elf_validate_fd(fd, expected, &note,
                                            error, sizeof(error));
    if ((success && result) || (!success && !result)) {
        fprintf(stderr, "%s: result=%d error=%s\n", message, result, error);
        return -1;
    }
    return 0;
}

static int expect_memory_result(const unsigned char file[FILE_SIZE],
                                const LatAotExpectedV2 *expected,
                                int success, const char *message)
{
    char error[256] = {0};
    LatAotNoteV2 note;
    int result = lat_aot_v2_elf_validate_memory(file, FILE_SIZE, expected,
                                                &note, error, sizeof(error));
    if ((success && result) || (!success && !result)) {
        fprintf(stderr, "%s: result=%d error=%s\n", message, result, error);
        return -1;
    }
    return 0;
}

int main(void)
{
    _Alignas(Elf64_Ehdr) unsigned char file[FILE_SIZE];
    build_elf(file);
    char path[] = "/tmp/lat-aot-v2-format.XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        perror("mkstemp");
        return 1;
    }
    unlink(path);
    LatAotExpectedV2 expected = {
        .available_features = LAT_AOT_V2_REQUIRED_BASE_FEATURES,
    };
    memset(expected.source_sha256, 0x11, 32);
    memset(expected.codegen_id, 0x22, 32);
    char memory_error[256] = {0};
    LatAotNoteV2 memory_note;
    if (lat_aot_v2_elf_validate_memory(file, sizeof(file), &expected,
                                       &memory_note, memory_error,
                                       sizeof(memory_error))) {
        fprintf(stderr, "valid memory ELF: %s\n", memory_error);
        return 1;
    }
    {
        Elf64_Ehdr *test_header = (void *)file;
        test_header->e_phoff++;
        if (expect_memory_result(file, &expected, 0,
                                 "unaligned program headers")) {
            return 1;
        }
        test_header->e_phoff--;
        test_header->e_shoff++;
        if (expect_memory_result(file, &expected, 0,
                                 "unaligned section headers")) {
            return 1;
        }
        test_header->e_shoff--;
    }
    {
        Elf64_Ehdr *test_header = (void *)file;
        Elf64_Phdr *test_phdrs = (void *)(file + test_header->e_phoff);
        test_phdrs[1].p_offset++;
        if (expect_memory_result(file, &expected, 0, "unaligned note")) {
            return 1;
        }
        test_phdrs[1].p_offset--;
        test_phdrs[2].p_offset++;
        if (expect_memory_result(file, &expected, 0, "unaligned dynamic")) {
            return 1;
        }
        test_phdrs[2].p_offset--;
        uint64_t saved_offset = test_phdrs[0].p_offset;
        test_phdrs[0].p_offset = UINT64_MAX;
        if (expect_memory_result(file, &expected, 0,
                                 "overflowing load offset")) {
            return 1;
        }
        test_phdrs[0].p_offset = saved_offset;
    }
    {
        TestNote *test_note = (void *)(file + NOTE_OFFSET);
        uint32_t saved_namesz = test_note->header.n_namesz;
        test_note->header.n_namesz = UINT32_MAX;
        if (expect_memory_result(file, &expected, 0,
                                 "overflowing note name")) {
            return 1;
        }
        test_note->header.n_namesz = saved_namesz;
    }
    {
        Elf64_Shdr *test_sections = (void *)(file + SECTION_OFFSET);
        test_sections[2].sh_offset++;
        if (expect_memory_result(file, &expected, 0,
                                 "unaligned symbol table")) {
            return 1;
        }
        test_sections[2].sh_offset--;
        test_sections[4].sh_offset++;
        if (expect_memory_result(file, &expected, 0,
                                 "unaligned relocation table")) {
            return 1;
        }
        test_sections[4].sh_offset--;
    }
    if (rewrite(fd, file) || expect_result(fd, &expected, 1, "valid ELF")) {
        return 1;
    }

    TestNote *note = (void *)(file + NOTE_OFFSET);
    note->description.abi_version++;
    if (rewrite(fd, file) || expect_result(fd, &expected, 0, "ABI mismatch")) {
        return 1;
    }
    note->description.abi_version--;

    LatAotExpectedV2 mismatch = expected;
    mismatch.source_sha256[0] ^= 1;
    if (expect_result(fd, &mismatch, 0, "source mismatch")) {
        return 1;
    }
    mismatch = expected;
    mismatch.available_features = LAT_AOT_FEATURE_LBT;
    if (expect_result(fd, &mismatch, 0, "feature mismatch")) {
        return 1;
    }

    Elf64_Ehdr *header = (void *)file;
    Elf64_Phdr *phdrs = (void *)(file + header->e_phoff);
    phdrs[0].p_flags |= PF_W;
    if (rewrite(fd, file) || expect_result(fd, &expected, 0, "W+X ELF")) {
        return 1;
    }
    phdrs[0].p_flags &= ~PF_W;

    Elf64_Dyn *dynamic = (void *)(file + DYNAMIC_OFFSET);
    dynamic[3] = (Elf64_Dyn) { .d_tag = DT_INIT, .d_un.d_ptr = TEXT_OFFSET };
    dynamic[4] = (Elf64_Dyn) { .d_tag = DT_NULL };
    if (rewrite(fd, file) || expect_result(fd, &expected, 0, "constructor ELF")) {
        return 1;
    }
    dynamic[3] = (Elf64_Dyn) { .d_tag = DT_NULL };

    Elf64_Rela *relocation = (void *)(file + RELOCATION_OFFSET);
    relocation->r_info = 99;
    if (rewrite(fd, file) ||
        expect_result(fd, &expected, 0, "relocation type ELF")) {
        return 1;
    }
    relocation->r_info = 3;

    file[STRING_OFFSET + 1] = 'X';
    if (rewrite(fd, file) || expect_result(fd, &expected, 0, "dependency ELF")) {
        return 1;
    }
    close(fd);
    puts("test-aot-v2-format: PASS");
    return 0;
}
