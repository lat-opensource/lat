#include "native-image.h"

#include <elf.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int invalid(char *error, size_t error_size, const char *format, ...)
{
    va_list args;

    if (error && error_size) {
        va_start(args, format);
        vsnprintf(error, error_size, format, args);
        va_end(args);
    }
    return -1;
}

static int range_valid(uint64_t offset, uint64_t length, size_t size)
{
    return offset <= size && length <= size - offset;
}

static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static int read_image(const char *path, unsigned char **data, size_t *size,
                      char *error, size_t error_size)
{
    FILE *input = fopen(path, "rb");
    if (!input) {
        return invalid(error, error_size, "cannot open native image %s", path);
    }
    int result = -1;
    if (fseek(input, 0, SEEK_END)) goto out;
    long end = ftell(input);
    if (end < 0 || fseek(input, 0, SEEK_SET)) goto out;
    *size = (size_t)end;
    if ((long)*size != end || !(*data = malloc(*size ? *size : 1)) ||
        (*size && fread(*data, *size, 1, input) != 1)) {
        goto out;
    }
    result = 0;
out:
    if (fclose(input) && !result) result = -1;
    if (result) {
        free(*data);
        *data = NULL;
        if (error && error_size && !error[0]) {
            snprintf(error, error_size, "cannot read native image %s", path);
        }
    }
    return result;
}

static int write_image(const char *path, const void *data, size_t size,
                       char *error, size_t error_size)
{
    FILE *output = fopen(path, "wb");
    int failed = !output || (size && fwrite(data, size, 1, output) != 1);
    if (output && fclose(output)) failed = 1;
    if (!failed) return 0;
    invalid(error, error_size, "cannot write merged native image %s", path);
    remove(path);
    return -1;
}

static int tb_target_valid(const LatNativeTbV1 *tbs, uint64_t count,
                           uint64_t guest_pc, uint32_t flags)
{
    uint64_t left = 0;
    uint64_t right = count;
    while (left < right) {
        uint64_t middle = left + (right - left) / 2;
        const LatNativeTbV1 *tb = &tbs[middle];
        if (tb->guest_pc < guest_pc ||
            (tb->guest_pc == guest_pc && tb->flags < flags)) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    if (left < count && tbs[left].guest_pc == guest_pc &&
        tbs[left].flags == flags) {
        return 1;
    }
    left = 0;
    right = count;
    while (left < right) {
        uint64_t middle = left + (right - left) / 2;
        if (tbs[middle].guest_pc < guest_pc) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return left < count && tbs[left].guest_pc == guest_pc &&
        (left + 1 == count || tbs[left + 1].guest_pc != guest_pc);
}

static int tb_fallback_symbol_valid(uint32_t symbol)
{
    return symbol == LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_1 ||
           symbol == LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0 ||
           symbol == LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_1 ||
           symbol == LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0 ||
           symbol == LAT_NATIVE_SYMBOL_EPILOGUE_RET_0;
}

int lat_native_image_validate(const void *data, size_t size,
                              char *error, size_t error_size)
{
    const LatNativeImageHeaderV2 *header = data;
    const LatNativeTbV1 *tbs;
    const LatNativeRelocationV1 *relocations;
    const LatNativePcMapV2 *pc_maps;
    const LatNativeTuV1 *tus;
    uint64_t table_size;

    if (!data || size < sizeof(*header)) {
        return invalid(error, error_size, "native image is truncated");
    }
    if (memcmp(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8) != 0 ||
        header->version != LAT_NATIVE_IMAGE_VERSION ||
        header->header_size != sizeof(*header)) {
        return invalid(error, error_size, "invalid native image header");
    }
    if (!header->lat_build_id[0] ||
        header->lat_build_id[LAT_NATIVE_BUILD_ID_SIZE - 1] != '\0') {
        return invalid(error, error_size, "invalid LAT build ID");
    }
    if (header->tb_count > UINT32_MAX) {
        return invalid(error, error_size, "native image has too many TBs");
    }
    if (!header->guest_image_size || !header->code_size || !header->tb_count ||
        header->guest_image_offset < sizeof(*header)) {
        return invalid(error, error_size, "native image has an empty section");
    }
    if (!range_valid(header->guest_image_offset, header->guest_image_size,
                     size) ||
        !range_valid(header->code_offset, header->code_size, size)) {
        return invalid(error, error_size, "native image payload is truncated");
    }
    if (header->code_offset <
        header->guest_image_offset + header->guest_image_size) {
        return invalid(error, error_size, "native image sections overlap");
    }
    if (header->tb_count > UINT64_MAX / sizeof(*tbs)) {
        return invalid(error, error_size, "native TB table is too large");
    }
    table_size = header->tb_count * sizeof(*tbs);
    if (!range_valid(header->tb_table_offset, table_size, size)) {
        return invalid(error, error_size, "native TB table is truncated");
    }
    if (header->tb_table_offset < header->code_offset + header->code_size) {
        return invalid(error, error_size, "native image sections overlap");
    }
    if (header->relocation_count > UINT64_MAX / sizeof(*relocations)) {
        return invalid(error, error_size,
                       "native relocation table is too large");
    }
    table_size = header->relocation_count * sizeof(*relocations);
    if (!range_valid(header->relocation_offset, table_size, size)) {
        return invalid(error, error_size,
                       "native relocation table is truncated");
    }
    if (header->relocation_offset <
            header->tb_table_offset +
            header->tb_count * sizeof(*tbs) ||
        header->relocation_offset + table_size != header->pc_map_offset) {
        return invalid(error, error_size,
                       "native image has invalid section boundaries");
    }
    if (header->pc_map_count > UINT64_MAX / sizeof(*pc_maps)) {
        return invalid(error, error_size, "native PC map is too large");
    }
    table_size = header->pc_map_count * sizeof(*pc_maps);
    if (!range_valid(header->pc_map_offset, table_size, size)) {
        return invalid(error, error_size, "native PC map is truncated");
    }
    if (header->tu_count > UINT64_MAX / sizeof(*tus)) {
        return invalid(error, error_size, "native TU table is too large");
    }
    table_size = header->tu_count * sizeof(*tus);
    if (!header->tu_count && !header->tu_table_offset &&
        header->pc_map_offset + header->pc_map_count * sizeof(*pc_maps) == size) {
        table_size = 0;
    } else if (!range_valid(header->tu_table_offset, table_size, size) ||
        header->tu_table_offset !=
            header->pc_map_offset + header->pc_map_count * sizeof(*pc_maps) ||
        header->tu_table_offset + table_size != size) {
        return invalid(error, error_size, "native TU table is truncated");
    }
    if (!header->pc_map_count &&
        !(header->flags & LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP)) {
        return invalid(error, error_size, "native image has no precise PC map");
    }

    tus = (const void *)((const unsigned char *)data +
                         header->tu_table_offset);
    for (uint64_t i = 0; i < header->tu_count; i++) {
        if (!tus[i].code_size || tus[i].code_offset >= header->code_size ||
            tus[i].code_size > header->code_size - tus[i].code_offset ||
            tus[i].search_offset < tus[i].code_offset ||
            tus[i].search_offset > tus[i].code_offset + tus[i].code_size ||
            (tus[i].flags & ~UINT32_C(1)) || tus[i].reserved ||
            (i && tus[i - 1].code_offset + tus[i - 1].code_size >
                tus[i].code_offset)) {
            return invalid(error, error_size,
                           "native TU %llu has an invalid range",
                           (unsigned long long)i);
        }
    }

    tbs = (const void *)((const unsigned char *)data +
                         header->tb_table_offset);
    for (uint64_t i = 0; i < header->tb_count; i++) {
        if (tbs[i].code_offset >= header->code_size ||
            tbs[i].code_size > header->code_size - tbs[i].code_offset) {
            return invalid(error, error_size,
                           "native TB %llu has an invalid code range",
                           (unsigned long long)i);
        }
        if (header->tu_count &&
            (tbs[i].tu_index >= header->tu_count ||
            tbs[i].code_offset < tus[tbs[i].tu_index].code_offset ||
            tbs[i].code_offset + tbs[i].code_size >
                tus[tbs[i].tu_index].code_offset +
                    tus[tbs[i].tu_index].code_size)) {
            return invalid(error, error_size,
                           "native TB %llu has an invalid TU",
                           (unsigned long long)i);
        }
        if (tbs[i].optimization_flags & ~LAT_NATIVE_TB_ENTRY_FLAGS_DEAD) {
            return invalid(error, error_size, "invalid native TB optimization flags");
        }
        if (tbs[i].indirect_exit_offset) {
            uint32_t offset = tbs[i].indirect_exit_offset - 1;
            if (offset % 4 || offset >= tbs[i].code_size) {
                return invalid(error, error_size, "invalid native indirect exit");
            }
            uint32_t remaining = tbs[i].code_size - offset;
            unsigned int words = lat_native_indirect_exit_words(
                (const uint8_t *)data + header->code_offset +
                    tbs[i].code_offset + offset, remaining);
            if (!words || remaining <= words * 4) {
                return invalid(error, error_size, "invalid native indirect exit");
            }
        }
        if (tbs[i].conditional_exit_offset) {
            uint32_t offset = tbs[i].conditional_exit_offset - 1;
            uint32_t instruction;
            if (offset % 4 || tbs[i].code_size < 4 ||
                offset > tbs[i].code_size - 4) {
                return invalid(error, error_size,
                               "invalid native conditional exit offset");
            }
            memcpy(&instruction, (const unsigned char *)data +
                   header->code_offset + tbs[i].code_offset + offset, 4);
            int64_t target = (int64_t)offset +
                (int16_t)(instruction >> 10) * 4;
            if (instruction >> 26 < 0x16 || instruction >> 26 > 0x1b ||
                target < 0 || target > tbs[i].code_size - 4) {
                return invalid(error, error_size,
                               "invalid native conditional exit instruction");
            }
        }
        for (int edge = 0; edge < 2; edge++) {
            uint32_t encoded = tbs[i].eflags_offset[edge];
            if (encoded && ((encoded - 1) % 4 ||
                            encoded - 1 + 4 > tbs[i].code_size)) {
                return invalid(error, error_size, "invalid native TB eflags offset");
            }
            if (encoded) {
                uint32_t instruction;
                memcpy(&instruction, (const unsigned char *)data +
                       header->code_offset + tbs[i].code_offset + encoded - 1,
                       sizeof(instruction));
                if (instruction != tbs[i].eflags_instruction) {
                    return invalid(error, error_size,
                                   "invalid native TB eflags instruction");
                }
            }
            encoded = tbs[i].eflags_stub_offset[edge];
            if (encoded) {
                uint32_t instruction;
                if ((encoded - 1) % 4 ||
                    encoded - 1 + 4 > tbs[i].code_size) {
                    return invalid(error, error_size,
                                   "invalid native TB eflags stub offset");
                }
                memcpy(&instruction, (const unsigned char *)data +
                       header->code_offset + tbs[i].code_offset + encoded - 1,
                       sizeof(instruction));
                if (instruction != 0x03400000u ||
                    encoded == tbs[i].eflags_offset[0] ||
                    encoded == tbs[i].eflags_offset[1]) {
                    return invalid(error, error_size,
                                   "invalid native TB eflags stub instruction");
                }
            }
        }
        if (i &&
            ((tbs[i - 1].guest_pc > tbs[i].guest_pc) ||
             (tbs[i - 1].guest_pc == tbs[i].guest_pc &&
              tbs[i - 1].flags > tbs[i].flags) ||
             (tbs[i - 1].guest_pc == tbs[i].guest_pc &&
              tbs[i - 1].flags == tbs[i].flags &&
              tbs[i - 1].code_offset == tbs[i].code_offset))) {
            return invalid(error, error_size,
                           "native TB table is not sorted by guest PC and flags");
        }
    }

    relocations = (const void *)((const unsigned char *)data +
                                 header->relocation_offset);
    for (uint64_t i = 0; i < header->relocation_count; i++) {
        if (relocations[i].physical_target_plus_one > header->tb_count) {
            return invalid(error, error_size,
                           "native relocation %llu has an invalid physical target",
                           (unsigned long long)i);
        }
        if (relocations[i].code_offset >= header->code_size ||
            !relocations[i].slots || relocations[i].slots > 4 ||
            relocations[i].slots * 4 >
                header->code_size - relocations[i].code_offset ||
            relocations[i].kind < LAT_NATIVE_RELOC_RUNTIME_SYMBOL ||
            relocations[i].kind > LAT_NATIVE_RELOC_IMAGE_DATA) {
            return invalid(error, error_size,
                           "native relocation %llu is invalid "
                           "(kind=%u slots=%u offset=0x%llx)",
                           (unsigned long long)i, relocations[i].kind,
                           relocations[i].slots,
                           (unsigned long long)relocations[i].code_offset);
        }
        if (relocations[i].kind == LAT_NATIVE_RELOC_RUNTIME_SYMBOL &&
            (relocations[i].target <= LAT_NATIVE_SYMBOL_INVALID ||
             relocations[i].target >= LAT_NATIVE_SYMBOL_COUNT)) {
            return invalid(error, error_size,
                           "native relocation %llu has an invalid runtime symbol",
                           (unsigned long long)i);
        }
        if (relocations[i].kind == LAT_NATIVE_RELOC_GUEST_ADDRESS &&
            relocations[i].target != 0) {
            return invalid(error, error_size,
                           "native relocation %llu has an invalid guest target",
                           (unsigned long long)i);
        }
        if (relocations[i].kind == LAT_NATIVE_RELOC_IMAGE_DATA) {
            const LatNativeRelocationV1 *relocation = &relocations[i];
            uint64_t offset = (uint64_t)relocation->addend;
            uint32_t words[3] = {0};
            if (relocation->slots < 2 || relocation->slots > 3 ||
                relocation->target || relocation->physical_target_plus_one ||
                relocation->addend < 0 || !relocation->reserved ||
                offset >= header->code_size ||
                relocation->reserved > header->code_size - offset) {
                return invalid(error, error_size,
                               "native data relocation %llu has an invalid range",
                               (unsigned long long)i);
            }
            uint64_t end = offset + relocation->reserved;
            for (uint64_t j = 0; j < header->relocation_count; j++) {
                const LatNativeRelocationV1 *patch = &relocations[j];
                if (patch->code_offset < end &&
                    (patch->code_offset >= offset ||
                     (uint64_t)patch->slots * 4 > offset - patch->code_offset)) {
                    return invalid(error, error_size,
                                   "native data relocation overlaps a patch window");
                }
            }
            for (uint64_t j = 0; j < header->tb_count; j++) {
                if (offset < tbs[j].code_offset + tbs[j].code_size &&
                    tbs[j].code_offset < end) {
                    return invalid(error, error_size,
                                   "native data relocation overlaps TB code");
                }
            }
            for (uint64_t j = 0; j < header->tu_count; j++) {
                if (offset < tus[j].code_offset + tus[j].code_size &&
                    tus[j].code_offset < end) {
                    return invalid(error, error_size,
                                   "native data relocation overlaps TU code");
                }
            }
            memcpy(words, (const unsigned char *)data + header->code_offset +
                   relocation->code_offset, relocation->slots * 4);
            uint32_t reg = words[0] & 0x1f;
            if ((words[0] & 0xfe000000u) != 0x14000000u ||
                (words[1] & 0xffc00000u) != 0x03800000u ||
                (words[1] & 0x3ffu) != (reg << 5 | reg) ||
                (relocation->slots == 3 &&
                 (words[2] & 0xfe00001fu) != (0x16000000u | reg))) {
                return invalid(error, error_size,
                               "native data relocation %llu has invalid instructions",
                               (unsigned long long)i);
            }
        }
        if (relocations[i].kind == LAT_NATIVE_RELOC_JRRA_TARGET &&
            relocations[i].slots != 4) {
            return invalid(error, error_size,
                           "native JRRA relocation %llu must use 4 slots",
                           (unsigned long long)i);
        }
        if ((header->flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC) &&
            (relocations[i].kind == LAT_NATIVE_RELOC_TB_TARGET ||
             relocations[i].kind == LAT_NATIVE_RELOC_JRRA_TARGET) &&
            !tb_target_valid(tbs, header->tb_count,
                             (uint64_t)relocations[i].addend,
                             relocations[i].target) &&
            (!(header->flags & LAT_NATIVE_IMAGE_CROSS_MODULE_TARGETS) ||
             relocations[i].kind != LAT_NATIVE_RELOC_TB_TARGET ||
             !tb_fallback_symbol_valid(relocations[i].reserved))) {
            return invalid(error, error_size,
                           "native relocation %llu targets a missing TB",
                           (unsigned long long)i);
        }
    }

    pc_maps = (const void *)((const unsigned char *)data +
                             header->pc_map_offset);
    for (uint64_t i = 0; i < header->pc_map_count; i++) {
        const uint32_t known_flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE |
            LAT_NATIVE_PC_MAP_STACK_POINTER_DELTA;
        const int has_stack_delta = pc_maps[i].flags &
            LAT_NATIVE_PC_MAP_STACK_POINTER_DELTA;
        if (!pc_maps[i].guest_pc ||
            pc_maps[i].host_offset_begin >= pc_maps[i].host_offset_end ||
            pc_maps[i].host_offset_end > header->code_size ||
            !(pc_maps[i].flags & LAT_NATIVE_PC_MAP_DYNAMIC_STATE) ||
            (pc_maps[i].flags & ~known_flags) ||
            (!!pc_maps[i].state_record_offset != !!has_stack_delta) ||
            (i && pc_maps[i - 1].host_offset_end >
                  pc_maps[i].host_offset_begin)) {
            return invalid(error, error_size,
                           "native PC map entry %llu is invalid",
                           (unsigned long long)i);
        }
    }
    return 0;
}

int lat_native_image_merge_files(const char *base_path,
                                 const char *delta_path,
                                 const char *output_path,
                                 char *error, size_t error_size)
{
    unsigned char *base_data = NULL;
    unsigned char *delta_data = NULL;
    unsigned char *output_data = NULL;
    size_t base_size = 0, delta_size = 0;
    int result = -1;

    if (!base_path || !delta_path || !output_path ||
        read_image(base_path, &base_data, &base_size, error, error_size) ||
        read_image(delta_path, &delta_data, &delta_size, error, error_size) ||
        lat_native_image_validate(base_data, base_size, error, error_size) ||
        lat_native_image_validate(delta_data, delta_size, error, error_size)) {
        goto out;
    }
    const LatNativeImageHeaderV2 *base = (const void *)base_data;
    const LatNativeImageHeaderV2 *delta = (const void *)delta_data;
    if (base->flags != delta->flags ||
        base->guest_entry != delta->guest_entry ||
        base->preferred_guest_base != delta->preferred_guest_base ||
        base->guest_image_size != delta->guest_image_size ||
        memcmp(base->guest_sha256, delta->guest_sha256,
               sizeof(base->guest_sha256)) ||
        strcmp(base->lat_build_id, delta->lat_build_id) ||
        memcmp(base_data + base->guest_image_offset,
               delta_data + delta->guest_image_offset,
               base->guest_image_size)) {
        invalid(error, error_size,
                "native images do not describe the same guest and codegen");
        goto out;
    }
    if (base->tb_count > UINT64_MAX - delta->tb_count ||
        base->relocation_count > UINT64_MAX - delta->relocation_count ||
        base->pc_map_count > UINT64_MAX - delta->pc_map_count ||
        base->tu_count > UINT64_MAX - delta->tu_count ||
        base->code_size > UINT64_MAX - 7) {
        invalid(error, error_size, "merged native image is too large");
        goto out;
    }

    uint64_t delta_code_offset = align_up(base->code_size, 32);
    if (delta->code_size > UINT64_MAX - delta_code_offset) {
        invalid(error, error_size, "merged native code is too large");
        goto out;
    }
    const LatNativeTbV1 *base_tbs =
        (const void *)(base_data + base->tb_table_offset);
    const LatNativeTbV1 *delta_tbs =
        (const void *)(delta_data + delta->tb_table_offset);
    LatNativeImageHeaderV2 merged = *base;
    merged.guest_image_offset = sizeof(merged);
    merged.code_offset = align_up(merged.guest_image_offset +
                                  merged.guest_image_size, 8);
    merged.code_size = delta_code_offset + delta->code_size;
    if (merged.code_offset > UINT64_MAX - merged.code_size - 7) {
        invalid(error, error_size, "merged native image offsets overflow");
        goto out;
    }
    merged.tb_table_offset = align_up(merged.code_offset + merged.code_size, 8);
    if (delta->tb_count > UINT64_MAX - base->tb_count) {
        invalid(error, error_size, "merged native TB count overflows");
        goto out;
    }
    merged.tb_count = base->tb_count + delta->tb_count;
    if (merged.tb_count > UINT32_MAX) {
        invalid(error, error_size, "merged native TB count is too large");
        goto out;
    }
    if (merged.tb_count > (UINT64_MAX - merged.tb_table_offset) /
                          sizeof(LatNativeTbV1)) {
        invalid(error, error_size, "merged native TB table is too large");
        goto out;
    }
    merged.relocation_offset = merged.tb_table_offset +
        merged.tb_count * sizeof(LatNativeTbV1);
    merged.relocation_count = base->relocation_count +
                              delta->relocation_count;
    if (merged.relocation_count >
        (UINT64_MAX - merged.relocation_offset) /
            sizeof(LatNativeRelocationV1)) {
        invalid(error, error_size,
                "merged native relocation table is too large");
        goto out;
    }
    merged.pc_map_offset = merged.relocation_offset +
        merged.relocation_count * sizeof(LatNativeRelocationV1);
    merged.pc_map_count = base->pc_map_count + delta->pc_map_count;
    merged.tu_table_offset = merged.pc_map_offset +
        merged.pc_map_count * sizeof(LatNativePcMapV2);
    merged.tu_count = base->tu_count + delta->tu_count;
    if (merged.pc_map_count >
        (UINT64_MAX - merged.pc_map_offset) / sizeof(LatNativePcMapV2)) {
        invalid(error, error_size, "merged native PC map is too large");
        goto out;
    }
    if (merged.tu_count >
        (UINT64_MAX - merged.tu_table_offset) / sizeof(LatNativeTuV1)) {
        invalid(error, error_size, "merged native TU table is too large");
        goto out;
    }
    uint64_t output_size64 = merged.tu_table_offset +
        merged.tu_count * sizeof(LatNativeTuV1);
    size_t output_size = (size_t)output_size64;
    if ((uint64_t)output_size != output_size64 ||
        !(output_data = calloc(output_size ? output_size : 1, 1))) {
        invalid(error, error_size, "cannot allocate merged native image");
        goto out;
    }

    memcpy(output_data, &merged, sizeof(merged));
    memcpy(output_data + merged.guest_image_offset,
           base_data + base->guest_image_offset, base->guest_image_size);
    memcpy(output_data + merged.code_offset,
           base_data + base->code_offset, base->code_size);
    memcpy(output_data + merged.code_offset + delta_code_offset,
           delta_data + delta->code_offset, delta->code_size);

    LatNativeTbV1 *merged_tbs =
        (void *)(output_data + merged.tb_table_offset);
    uint32_t *base_tb_map = calloc(base->tb_count, sizeof(*base_tb_map));
    uint32_t *delta_tb_map = calloc(delta->tb_count, sizeof(*delta_tb_map));
    if (!base_tb_map || !delta_tb_map) {
        free(base_tb_map);
        free(delta_tb_map);
        invalid(error, error_size, "cannot allocate merged TB map");
        goto out;
    }
    memset(base_tb_map, 0xff, base->tb_count * sizeof(*base_tb_map));
    memset(delta_tb_map, 0xff, delta->tb_count * sizeof(*delta_tb_map));
    uint64_t bi = 0, di = 0, oi = 0;
    while (bi < base->tb_count || di < delta->tb_count) {
        bool take_base = di == delta->tb_count;
        if (bi < base->tb_count && di < delta->tb_count) {
            const LatNativeTbV1 *left = &base_tbs[bi];
            const LatNativeTbV1 *right = &delta_tbs[di];
            take_base = left->guest_pc < right->guest_pc ||
                (left->guest_pc == right->guest_pc &&
                 left->flags <= right->flags);
        }
        if (take_base) {
            base_tb_map[bi] = (uint32_t)oi;
            merged_tbs[oi] = base_tbs[bi++];
        } else {
            delta_tb_map[di] = (uint32_t)oi;
            merged_tbs[oi] = delta_tbs[di++];
            merged_tbs[oi].code_offset += delta_code_offset;
            merged_tbs[oi].tu_index += base->tu_count;
        }
        oi++;
    }
    if (oi != merged.tb_count) {
        invalid(error, error_size, "merged native TB count is inconsistent");
        goto out;
    }

    const LatNativeRelocationV1 *base_relocations =
        (const void *)(base_data + base->relocation_offset);
    const LatNativeRelocationV1 *delta_relocations =
        (const void *)(delta_data + delta->relocation_offset);
    LatNativeRelocationV1 *merged_relocations =
        (void *)(output_data + merged.relocation_offset);
    memcpy(merged_relocations, base_relocations,
           base->relocation_count * sizeof(*merged_relocations));
    for (uint64_t i = 0; i < base->relocation_count; i++) {
        uint32_t physical = merged_relocations[i].physical_target_plus_one;
        if (physical) {
            uint32_t mapped = base_tb_map[physical - 1];
            merged_relocations[i].physical_target_plus_one =
                mapped == UINT32_MAX ? 0 : mapped + 1;
        }
    }
    for (uint64_t i = 0; i < delta->relocation_count; i++) {
        merged_relocations[base->relocation_count + i] = delta_relocations[i];
        merged_relocations[base->relocation_count + i].code_offset +=
            delta_code_offset;
        if (delta_relocations[i].kind == LAT_NATIVE_RELOC_IMAGE_DATA) {
            merged_relocations[base->relocation_count + i].addend +=
                delta_code_offset;
        }
        uint32_t physical =
            merged_relocations[base->relocation_count + i].physical_target_plus_one;
        if (physical) {
            uint32_t mapped = delta_tb_map[physical - 1];
            merged_relocations[base->relocation_count + i].physical_target_plus_one =
                mapped == UINT32_MAX ? 0 : mapped + 1;
        }
    }
    free(base_tb_map);
    free(delta_tb_map);

    const LatNativePcMapV2 *base_maps =
        (const void *)(base_data + base->pc_map_offset);
    const LatNativePcMapV2 *delta_maps =
        (const void *)(delta_data + delta->pc_map_offset);
    LatNativePcMapV2 *merged_maps =
        (void *)(output_data + merged.pc_map_offset);
    memcpy(merged_maps, base_maps, base->pc_map_count * sizeof(*merged_maps));
    for (uint64_t i = 0; i < delta->pc_map_count; i++) {
        merged_maps[base->pc_map_count + i] = delta_maps[i];
        merged_maps[base->pc_map_count + i].host_offset_begin +=
            delta_code_offset;
        merged_maps[base->pc_map_count + i].host_offset_end +=
            delta_code_offset;
    }

    const LatNativeTuV1 *base_tus =
        (const void *)(base_data + base->tu_table_offset);
    const LatNativeTuV1 *delta_tus =
        (const void *)(delta_data + delta->tu_table_offset);
    LatNativeTuV1 *merged_tus =
        (void *)(output_data + merged.tu_table_offset);
    memcpy(merged_tus, base_tus, base->tu_count * sizeof(*merged_tus));
    for (uint64_t i = 0; i < delta->tu_count; i++) {
        merged_tus[base->tu_count + i] = delta_tus[i];
        merged_tus[base->tu_count + i].code_offset += delta_code_offset;
        merged_tus[base->tu_count + i].search_offset += delta_code_offset;
    }

    if (lat_native_image_validate(output_data, output_size,
                                  error, error_size)) {
        goto out;
    }
    result = write_image(output_path, output_data, output_size,
                         error, error_size);
out:
    free(output_data);
    free(delta_data);
    free(base_data);
    return result;
}

int lat_native_image_inspect_file(const char *path,
                                  LatNativeImageHeaderV2 *header,
                                  char *error, size_t error_size)
{
    FILE *input = fopen(path, "rb");
    void *data = NULL;
    int result = -1;

    if (!input) {
        return invalid(error, error_size, "cannot open native image");
    }
    if (fseek(input, 0, SEEK_END) != 0) {
        goto out;
    }
    long end = ftell(input);
    if (end < 0 || fseek(input, 0, SEEK_SET) != 0) {
        goto out;
    }
    size_t size = (size_t)end;
    if ((long)size != end || !(data = malloc(size ? size : 1)) ||
        (size && fread(data, size, 1, input) != 1)) {
        goto out;
    }
    if (lat_native_image_validate(data, size, error, error_size) != 0) {
        goto out;
    }
    if (header) {
        memcpy(header, data, sizeof(*header));
    }
    result = 0;
out:
    if (result && error && error_size && !error[0]) {
        snprintf(error, error_size, "cannot read native image");
    }
    free(data);
    fclose(input);
    return result;
}

static int validate_static_x86_guest(const LatNativeImageHeaderV2 *header,
                                     const unsigned char *data, size_t size,
                                     char *error, size_t error_size)
{
    if (!range_valid(header->guest_image_offset, header->guest_image_size,
                     size) || header->guest_image_size < sizeof(Elf64_Ehdr)) {
        return invalid(error, error_size, "embedded x86 ELF is truncated");
    }
    const unsigned char *guest = data + header->guest_image_offset;
    const Elf64_Ehdr *elf = (const void *)guest;
    if (memcmp(elf->e_ident, ELFMAG, SELFMAG) ||
        elf->e_ident[EI_CLASS] != ELFCLASS64 ||
        elf->e_ident[EI_DATA] != ELFDATA2LSB ||
        elf->e_machine != EM_X86_64 ||
        (elf->e_type != ET_EXEC && elf->e_type != ET_DYN) ||
        elf->e_entry != header->guest_entry ||
        elf->e_phentsize != sizeof(Elf64_Phdr) || !elf->e_phnum ||
        elf->e_phoff > header->guest_image_size ||
        elf->e_phnum > (header->guest_image_size - elf->e_phoff) /
                       sizeof(Elf64_Phdr)) {
        return invalid(error, error_size,
                       "embedded guest is not a supported x86-64 ELF");
    }
    const Elf64_Phdr *phdrs = (const void *)(guest + elf->e_phoff);
    int entry_is_executable = 0;
    for (uint16_t i = 0; i < elf->e_phnum; i++) {
        if (phdrs[i].p_type == PT_INTERP) {
            return invalid(error, error_size,
                           "embedded x86 ELF is dynamically linked");
        }
        if (phdrs[i].p_type == PT_LOAD && (phdrs[i].p_flags & PF_X) &&
            elf->e_entry >= phdrs[i].p_vaddr &&
            elf->e_entry - phdrs[i].p_vaddr < phdrs[i].p_memsz) {
            entry_is_executable = 1;
        }
    }
    if (!entry_is_executable) {
        return invalid(error, error_size,
                       "embedded x86 ELF entry is not executable");
    }
    return 0;
}

int lat_native_image_mark_x86_static_file(const char *path,
                                          char *error, size_t error_size)
{
    FILE *image = fopen(path, "r+b");
    unsigned char *data = NULL;
    int result = -1;
    if (!image) {
        return invalid(error, error_size, "cannot open native image");
    }
    if (fseek(image, 0, SEEK_END)) goto out;
    long end = ftell(image);
    if (end < 0 || fseek(image, 0, SEEK_SET)) {
        goto out;
    }
    size_t size = (size_t)end;
    if ((long)size != end || !(data = malloc(size ? size : 1)) ||
        (size && fread(data, size, 1, image) != 1)) {
        goto out;
    }
    if (lat_native_image_validate(data, size, error, error_size) ||
        validate_static_x86_guest((const void *)data, data, size,
                                  error, error_size)) {
        goto out;
    }
    LatNativeImageHeaderV2 *header = (void *)data;
    header->flags |= LAT_NATIVE_IMAGE_X86_STATIC_EXEC;
    if (lat_native_image_validate(data, size, error, error_size)) {
        goto out;
    }
    if (fseek(image, 0, SEEK_SET) ||
        fwrite(header, sizeof(*header), 1, image) != 1 || fflush(image)) {
        goto out;
    }
    result = 0;
out:
    if (result && error && error_size && !error[0]) {
        snprintf(error, error_size, "cannot update native image");
    }
    free(data);
    if (image) fclose(image);
    return result;
}
