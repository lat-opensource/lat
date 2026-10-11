#include "lat-native-image.h"
#include "lat-eflags-link.h"
#include "lat-aot-v2.h"
#include "module-pack.h"
#include "native-image.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint32_t indirect_dispatch[] = {
    0x004542ab, 0x0015aeab, 0x00cf016b, 0x002dd96b,
    0x28c0016c, 0x5c000d95, 0x28c0216b, 0x4c000160,
    0x28c543ed, 0x580075a0, 0x004542ae, 0x0015baae,
    0x00cf01ce, 0x004119ce, 0x0010b5ce, 0x28c001cc,
    0x5c005995, 0x28c061cc, 0x58005180, 0x28c0018b,
    0x28c081cc, 0x5c00458b, 0x28c041cc, 0x28c563eb,
    0x5800318b, 0x28c0a1cd, 0x28c0c1cb, 0x58002160,
    0x001502cf, 0x02ffe1ad, 0x02ffe1ef, 0x28c001b0,
    0x29c001f0, 0x02fffd6b, 0x5fffed60, 0x29c563ec,
    0x28c021cb, 0x4c000160,
};

static const uint32_t aot_v2_indirect_dispatch[2][27] = {
    {
        0x004542ab, 0x0015aeab, 0x00cf016b, 0x002dd96b,
        0x28c0016c, 0x5c000d95, 0x28c0216b, 0x4c000160,
        0x28c543ed, 0x580049a0, 0x004542ae, 0x0015baae,
        0x00cf01ce, 0x004119ce, 0x0010b5ce, 0x28c001cc,
        0x5c002d95, 0x28c061cc, 0x58002580, 0x28c0018b,
        0x28c081cc, 0x5c00198b, 0x28c041cc, 0x28c563eb,
        0x5c000d8b, 0x28c021cb, 0x4c000160,
    },
    {
        0x004542ab, 0x0015aeab, 0x00cf016b, 0x002dd96b,
        0x28c0016d, 0x5c000db5, 0x28c0216b, 0x4c000160,
        0x28c543ee, 0x580049c0, 0x004542af, 0x0015beaf,
        0x00cf01ef, 0x004119ef, 0x0010b9ef, 0x28c001ed,
        0x5c002db5, 0x28c061ed, 0x580025a0, 0x28c001ab,
        0x28c081ed, 0x5c0019ab, 0x28c041ed, 0x28c563eb,
        0x5c000dab, 0x28c021eb, 0x4c000160,
    },
};

static int test_indirect_register_renaming(void)
{
    for (unsigned int variant = 0; variant < 2; variant++) {
        uint32_t window[28] = {0};
        memcpy(window, aot_v2_indirect_dispatch[variant],
               sizeof(aot_v2_indirect_dispatch[variant]));
        window[27] = 0x29c001f0u;
        if (lat_native_indirect_exit_words(window, sizeof(window)) != 27 ||
            lat_native_indirect_exit_words(window, 107) != 0) {
            fprintf(stderr, "current indirect window rejected\n");
            return -1;
        }
        window[24] ^= 1u;
        if (lat_native_indirect_exit_words(window, sizeof(window))) {
            fprintf(stderr, "modified current indirect window accepted\n");
            return -1;
        }
    }
    if (lat_native_indirect_exit_words(indirect_dispatch,
                                      sizeof(indirect_dispatch)) != 38) {
        fprintf(stderr, "legacy indirect window rejected\n");
        return -1;
    }
    static const uint8_t register_fields[LAT_NATIVE_INDIRECT_EXIT_WORDS] = {
        2, 3, 2, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 3, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    };
    static const uint8_t renamed_scratch[] = {13, 14, 15, 16, 1};
    uint32_t renamed[LAT_NATIVE_INDIRECT_EXIT_WORDS];
    memcpy(renamed, indirect_dispatch, sizeof(renamed));

    for (unsigned int i = 0; i < LAT_NATIVE_INDIRECT_EXIT_WORDS; i++) {
        for (unsigned int field = 0; field < register_fields[i]; field++) {
            unsigned int shift = field * 5;
            unsigned int reg = (renamed[i] >> shift) & 31;
            if (reg >= 12 && reg <= 16) {
                renamed[i] &= ~(31u << shift);
                renamed[i] |= (uint32_t)renamed_scratch[reg - 12] << shift;
            }
        }
    }
    if (!lat_native_indirect_exit_valid(renamed)) {
        fprintf(stderr, "renamed indirect dispatch rejected\n");
        return -1;
    }
    renamed[17] ^= 1u;
    if (lat_native_indirect_exit_valid(renamed)) {
        fprintf(stderr, "inconsistent indirect register accepted\n");
        return -1;
    }
    return 0;
}

static int write_fixture(const char *path, int overlap, int incomplete_map,
                         int missing_target, int exit_variant)
{
    unsigned char image[1024] = {0};
    LatNativeImageHeaderV2 *header = (void *)image;
    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_X86_STATIC_EXEC |
        (missing_target ? LAT_NATIVE_IMAGE_CROSS_MODULE_TARGETS : 0);
    header->guest_entry = 0x401000;
    header->preferred_guest_base = 0x400000;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = 1;
    header->code_offset = header->guest_image_offset + 8;
    header->code_size = missing_target ? 24 : 28;
    header->tb_table_offset = header->code_offset + header->code_size;
    header->tb_count = missing_target ? 1 : 2;
    header->relocation_offset = header->tb_table_offset +
                                header->tb_count * sizeof(LatNativeTbV1);
    header->relocation_count = 3;
    header->pc_map_offset = header->relocation_offset +
                            3 * sizeof(LatNativeRelocationV1);
    header->pc_map_count = missing_target ? 1 : 2;
    strcpy(header->lat_build_id, "aot-v2-module-pack-test-v1");

    uint32_t *code = (void *)(image + header->code_offset);
    code[0] = 0x1e00000c; /* pcaddu18i $t0, 0 */
    code[1] = 0x4c000184; /* jirl $a0, $t0, 0 */
    if (exit_variant == 1) {
        code[1] = 0x4c000181; /* jirl $ra, $t0, 0 */
    }
    code[2] = 0x1e00000c; /* pcaddu18i $t0, 0 */
    code[3] = 0x4c000180; /* jirl $zero, $t0, 0 */
    code[4] = 0x50000000; /* b 0 */
    code[5] = 0x03400000; /* reserved nop */
    code[6] = 0x03400000; /* target TB */

    LatNativeTbV1 *tbs = (void *)(image + header->tb_table_offset);
    tbs[0] = (LatNativeTbV1){
        .guest_pc = 0x401000, .code_offset = 0, .code_size = 24,
    };
    if (!missing_target) {
        tbs[1] = (LatNativeTbV1){
            .guest_pc = 0x402000,
            .code_offset = overlap ? 20 : 24,
            .code_size = overlap ? 8 : 4,
        };
    }

    LatNativeRelocationV1 *relocations =
        (void *)(image + header->relocation_offset);
    relocations[0] = (LatNativeRelocationV1){
        .code_offset = 0,
        .addend = 0x402000,
        .kind = LAT_NATIVE_RELOC_TB_TARGET,
        .slots = 2,
        .reserved = LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0,
    };
    if (exit_variant == 2) {
        relocations[0].reserved = LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0;
    } else if (exit_variant == 3) {
        relocations[0].reserved = LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_1;
    }
    relocations[1] = (LatNativeRelocationV1){
        .code_offset = 8,
        .addend = 0x402000,
        .kind = LAT_NATIVE_RELOC_TB_TARGET,
        .slots = 2,
        .reserved = LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0,
    };
    relocations[2] = (LatNativeRelocationV1){
        .code_offset = 16,
        .addend = 0x402000,
        .kind = LAT_NATIVE_RELOC_TB_TARGET,
        .slots = 2,
        .reserved = LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0,
    };

    LatNativePcMapV2 *maps = (void *)(image + header->pc_map_offset);
    maps[0] = (LatNativePcMapV2){
        .guest_pc = 0x401000,
        .host_offset_begin = 0,
        .host_offset_end = incomplete_map ? 20 : 24,
        .state_record_offset = (uint32_t)(int32_t)-8,
        .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE |
                 LAT_NATIVE_PC_MAP_STACK_POINTER_DELTA,
    };
    if (!missing_target) {
        maps[1] = (LatNativePcMapV2){
            .guest_pc = 0x402000,
            .host_offset_begin = 24,
            .host_offset_end = 28,
            .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
        };
    }

    size_t size = header->pc_map_offset +
                  header->pc_map_count * sizeof(LatNativePcMapV2);
    FILE *file = fopen(path, "wb");
    if (!file) {
        return -1;
    }
    int result = fwrite(image, size, 1, file) == 1 ? 0 : -1;
    if (fclose(file)) {
        result = -1;
    }
    if (result) {
        return -1;
    }
    return 0;
}

static int test_primary_tb_aliases(const char *directory)
{
    unsigned char image[1024] = {0};
    LatNativeImageHeaderV2 *header = (void *)image;
    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP;
    header->guest_entry = 0x401000;
    header->preferred_guest_base = 0x400000;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = 1;
    header->code_offset = header->guest_image_offset + 8;
    header->code_size = 24;
    header->tb_table_offset = header->code_offset + header->code_size;
    header->tb_count = 3;
    header->relocation_offset = header->tb_table_offset +
                                3 * sizeof(LatNativeTbV1);
    header->relocation_count = 1;
    header->pc_map_offset = header->relocation_offset +
                            sizeof(LatNativeRelocationV1);
    header->pc_map_count = 3;
    header->tu_table_offset = header->pc_map_offset +
                              3 * sizeof(LatNativePcMapV2);
    header->tu_count = 1;
    strcpy(header->lat_build_id, "aot-v2-primary-alias-test");

    uint32_t *code = (void *)(image + header->code_offset);
    code[0] = 0x03400000;
    code[1] = 0x03400000;
    code[2] = 0x1e00000c;
    code[3] = 0x4c000184;
    code[4] = 0x03400000;
    code[5] = 0x03400000;

    LatNativeTbV1 *tbs = (void *)(image + header->tb_table_offset);
    tbs[0] = (LatNativeTbV1){
        .guest_pc = 0x401000, .code_offset = 0, .code_size = 8,
        .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
    };
    tbs[1] = (LatNativeTbV1){
        .guest_pc = 0x401000, .code_offset = 8, .code_size = 8,
        .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
    };
    tbs[2] = (LatNativeTbV1){
        .guest_pc = 0x402000, .code_offset = 16, .code_size = 8,
        .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
    };

    LatNativeRelocationV1 *relocation =
        (void *)(image + header->relocation_offset);
    *relocation = (LatNativeRelocationV1){
        .code_offset = 8,
        .addend = 0x402000,
        .kind = LAT_NATIVE_RELOC_TB_TARGET,
        .slots = 2,
        .target = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
        .reserved = LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0,
    };

    LatNativePcMapV2 *maps = (void *)(image + header->pc_map_offset);
    maps[0] = (LatNativePcMapV2){
        .guest_pc = 0x401000, .host_offset_begin = 0,
        .host_offset_end = 8, .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
    };
    maps[1] = (LatNativePcMapV2){
        .guest_pc = 0x401000, .host_offset_begin = 8,
        .host_offset_end = 16, .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
    };
    maps[2] = (LatNativePcMapV2){
        .guest_pc = 0x402000, .host_offset_begin = 16,
        .host_offset_end = 24, .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
    };
    LatNativeTuV1 *tu = (void *)(image + header->tu_table_offset);
    *tu = (LatNativeTuV1){
        .code_offset = 0, .code_size = header->code_size,
        .search_offset = header->code_size,
    };

    size_t size = header->tu_table_offset + sizeof(*tu);
    char *image_path = g_build_filename(directory, "primary-alias.native", NULL);
    char *text_path = g_build_filename(directory, "text.bin", NULL);
    char *tbs_path = g_build_filename(directory, "tbs.bin", NULL);
    char error[256] = {0};
    gchar *text = NULL;
    gchar *tb_data = NULL;
    gsize text_size = 0;
    gsize tb_size = 0;
    int failed = !g_file_set_contents(image_path, (const char *)image,
                                      size, NULL) ||
        lat_aot_v2_emit_module_sources(image_path, directory,
                                       error, sizeof(error)) ||
        !g_file_get_contents(text_path, &text, &text_size, NULL) ||
        !g_file_get_contents(tbs_path, &tb_data, &tb_size, NULL);
    const LatAotTbV2 *output_tbs = (const void *)tb_data;
    const uint32_t *words = (const void *)text;
    if (!failed) {
        failed = text_size != header->code_size ||
            tb_size != 2 * sizeof(*output_tbs) ||
            output_tbs[0].guest_rva != 0x1000 ||
            output_tbs[0].host_offset != 0 ||
            output_tbs[1].guest_rva != 0x2000 ||
            output_tbs[1].host_offset != 16 ||
            words[2] != 0x50000800 ||
            words[3] != 0x03400000;
    }
    if (failed) {
        fprintf(stderr, "physical TB alias was not packed: %s\n",
                error[0] ? error : "invalid output");
    }
    if (!failed) {
        char *base_path = g_build_filename(directory, "incomplete.native", NULL);
        char *merged_path = g_build_filename(directory, "repaired.native", NULL);
        maps[0].host_offset_end = 4;
        maps[1].host_offset_end = 12;
        g_free(tb_data);
        tb_data = NULL;
        g_free(text);
        text = NULL;
        failed = !g_file_set_contents(base_path, (const char *)image,
                                      size, NULL) ||
            lat_native_image_merge_files(base_path, image_path, merged_path,
                                          error, sizeof(error)) ||
            lat_aot_v2_emit_module_sources(merged_path, directory,
                                           error, sizeof(error)) ||
            !g_file_get_contents(tbs_path, &tb_data, &tb_size, NULL) ||
            !g_file_get_contents(text_path, &text, &text_size, NULL);
        output_tbs = (const void *)tb_data;
        words = (const void *)text;
        if (!failed) {
            failed = tb_size != 2 * sizeof(*output_tbs) ||
                output_tbs[0].guest_rva != 0x1000 ||
                output_tbs[0].host_offset != 32 ||
                output_tbs[0].flags !=
                    (LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL) ||
                output_tbs[1].guest_rva != 0x2000 ||
                text_size != 56 || words[10] != 0x53ffebff ||
                words[11] != 0x03400000;
        }
        if (failed) {
            fprintf(stderr, "complete delta alias did not repair key: %s\n",
                    error[0] ? error : "invalid output");
        }
        g_free(merged_path);
        g_free(base_path);
    }
    g_free(tb_data);
    g_free(text);
    g_free(tbs_path);
    g_free(text_path);
    g_free(image_path);
    return failed ? -1 : 0;
}

static int write_alias_dependency(const char *path, bool delta, bool complete)
{
    unsigned char image[1024] = {0};
    LatNativeImageHeaderV2 *h = (void *)image;
    memcpy(h->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    h->version = LAT_NATIVE_IMAGE_VERSION;
    h->header_size = sizeof(*h);
    h->guest_entry = 0x401000;
    h->preferred_guest_base = 0x400000;
    h->guest_image_offset = sizeof(*h);
    h->guest_image_size = 1;
    h->code_offset = sizeof(*h) + 8;
    h->tb_count = delta ? 1 : 3;
    h->code_size = h->tb_count * 8;
    h->tb_table_offset = h->code_offset + h->code_size;
    h->relocation_offset = h->tb_table_offset +
                           h->tb_count * sizeof(LatNativeTbV1);
    h->relocation_count = delta ? 0 : 2;
    h->pc_map_offset = h->relocation_offset +
                      h->relocation_count * sizeof(LatNativeRelocationV1);
    h->pc_map_count = h->tb_count;
    strcpy(h->lat_build_id, "alias-dependency-test");
    uint32_t *code = (void *)(image + h->code_offset);
    LatNativeTbV1 *tbs = (void *)(image + h->tb_table_offset);
    LatNativePcMapV2 *maps = (void *)(image + h->pc_map_offset);
    for (uint64_t i = 0; i < h->tb_count; i++) {
        uint64_t pc = 0x401000 + i * 0x1000;
        code[i * 2] = 0x50000000;
        code[i * 2 + 1] = 0x03400000;
        tbs[i] = (LatNativeTbV1) {
            .guest_pc = pc, .code_offset = i * 8, .code_size = 8,
            .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
        };
        maps[i] = (LatNativePcMapV2) {
            .guest_pc = pc, .host_offset_begin = i * 8,
            .host_offset_end = i * 8 + ((!delta && i == 1) ||
                                       (delta && !complete) ? 4 : 8),
            .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
        };
    }
    if (!delta) {
        LatNativeRelocationV1 *rel = (void *)(image + h->relocation_offset);
        for (unsigned int i = 0; i < 2; i++) {
            unsigned int offset = i ? 16 : 0;
            code[offset / 4] = 0x1e00000c;
            code[offset / 4 + 1] = 0x4c000184;
            rel[i] = (LatNativeRelocationV1) {
                .code_offset = offset, .addend = i ? 0x401000 : 0x402000,
                .kind = LAT_NATIVE_RELOC_TB_TARGET, .slots = 2,
                .target = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
                .reserved = LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0,
            };
        }
    }
    return g_file_set_contents(path, (const char *)image,
        h->pc_map_offset + h->pc_map_count * sizeof(*maps), NULL) ? 0 : -1;
}

static int test_alias_dependency(const char *directory)
{
    g_autofree char *base = g_build_filename(directory, "dependency-base", NULL);
    g_autofree char *delta = g_build_filename(directory, "dependency-delta", NULL);
    g_autofree char *merged = g_build_filename(directory, "dependency-merge", NULL);
    g_autofree char *table = g_build_filename(directory, "tbs.bin", NULL);
    g_autofree char *text_path = g_build_filename(directory, "text.bin", NULL);
    char error[256] = {0};
    g_autofree gchar *data = NULL;
    g_autofree gchar *text = NULL;
    gsize size = 0, text_size = 0;
    if (write_alias_dependency(base, false, false) ||
        write_alias_dependency(delta, true, true) ||
        lat_native_image_merge_files(base, delta, merged, error, sizeof(error)) ||
        lat_aot_v2_emit_module_sources(merged, directory, error, sizeof(error)) ||
        !g_file_get_contents(table, &data, &size, NULL) ||
        !g_file_get_contents(text_path, &text, &text_size, NULL)) {
        fprintf(stderr, "cannot repair dependent alias: %s\n", error);
        return -1;
    }
    const LatAotTbV2 *tbs = (const void *)data;
    if (size != 2 * sizeof(*tbs) || tbs[0].guest_rva != 0x1000 ||
        tbs[1].guest_rva != 0x3000 || text_size < tbs[1].host_offset + 8 ||
        ((const uint32_t *)(text + tbs[1].host_offset))[0] != 0x50001000) {
        fprintf(stderr, "caller lost a usable delta primary\n");
        return -1;
    }
    if (write_alias_dependency(delta, true, false) ||
        lat_native_image_merge_files(base, delta, merged, error, sizeof(error)) ||
        !lat_aot_v2_emit_module_sources(merged, directory, error, sizeof(error))) {
        fprintf(stderr, "incomplete delta alias was accepted\n");
        return -1;
    }
    return 0;
}

static int test_conditional_exits(const char *directory)
{
    const struct {
        int displacement;
        unsigned effects, target_flags;
        int missing, no_hint, interior, threaded;
    } cases[] = {
        {32, 0, 3, 0, 0, 0, 1}, {-32, 0, 3, 0, 0, 0, 1},
        {131068, 0, 3, 0, 0, 0, 1}, {-131072, 0, 3, 0, 0, 0, 1},
        {131072, 0, 3, 0, 0, 0, 0}, {-131076, 0, 3, 0, 0, 0, 0},
        {32, 1, 3, 0, 0, 0, 0}, {32, 2, 3, 0, 0, 0, 1},
        {32, 0, 1, 0, 0, 0, 0}, {32, 0, 3, 1, 0, 0, 0},
        {32, 0, 3, 0, 1, 0, 0}, {32, 0, 3, 0, 0, 1, 0},
    };
    char *path = g_build_filename(directory, "conditional.native", NULL);
    char *text_path = g_build_filename(directory, "text.bin", NULL);
    for (unsigned test = 0; test < G_N_ELEMENTS(cases); test++) {
        uint64_t source = cases[test].displacement < 0 ?
                          -cases[test].displacement : 0;
        uint64_t target = cases[test].displacement > 0 ?
                          cases[test].displacement : 0;
        uint64_t code_size = MAX(source + 24, target + 8);
        size_t image_size = sizeof(LatNativeImageHeaderV2) + 8 + code_size +
            2 * sizeof(LatNativeTbV1) + sizeof(LatNativeRelocationV1) +
            2 * sizeof(LatNativePcMapV2);
        uint8_t *image = g_malloc0(image_size);
        LatNativeImageHeaderV2 *h = (void *)image;
        memcpy(h->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
        h->version = LAT_NATIVE_IMAGE_VERSION;
        h->header_size = sizeof(*h);
        h->flags = LAT_NATIVE_IMAGE_X86_STATIC_EXEC |
                   LAT_NATIVE_IMAGE_CROSS_MODULE_TARGETS;
        h->guest_image_offset = sizeof(*h);
        h->guest_image_size = 8;
        h->code_offset = sizeof(*h) + 8;
        h->code_size = code_size;
        h->tb_table_offset = h->code_offset + code_size;
        h->tb_count = cases[test].missing ? 1 : 2;
        h->relocation_offset = h->tb_table_offset +
                              h->tb_count * sizeof(LatNativeTbV1);
        h->relocation_count = cases[test].interior ? 0 : 1;
        h->pc_map_offset = h->relocation_offset +
                          h->relocation_count * sizeof(LatNativeRelocationV1);
        h->pc_map_count = h->tb_count;
        strcpy(h->lat_build_id, "conditional-exit-test");
        uint32_t *code = (void *)(image + h->code_offset + source);
        uint32_t original = ((0x16u + test % 6) << 26) | (2u << 10) | 0x18du;
        code[0] = original;
        code[1] = 0x50000000u;
        code[2] = cases[test].effects ? 0x003f358au : 0x03400000u;
        code[3] = 0x03400000u;
        code[4] = 0x1e00000cu;
        code[5] = 0x4c000184u;
        if (cases[test].interior) {
            code[4] = 0x50000000u | ((target + 4 - source - 16) / 4) << 10;
            code[5] = 0x03400000u;
        }
        LatNativeTbV1 *tbs = (void *)(image + h->tb_table_offset);
        tbs[0] = (LatNativeTbV1){
            .guest_pc = 0x1000, .code_offset = source, .code_size = 24,
            .flags = 3, .conditional_exit_offset = cases[test].no_hint ? 0 : 1,
        };
        if (!cases[test].missing) {
            tbs[1] = (LatNativeTbV1){
                .guest_pc = 0x2000, .code_offset = target, .code_size = 8,
                .flags = cases[test].target_flags,
                .optimization_flags = LAT_NATIVE_TB_ENTRY_FLAGS_DEAD,
            };
        }
        if (cases[test].effects == 2) {
            tbs[0].eflags_offset[1] = 9;
            tbs[0].eflags_instruction = code[2];
        }
        LatNativeRelocationV1 *rel = (void *)(image + h->relocation_offset);
        if (h->relocation_count) {
            *rel = (LatNativeRelocationV1){
                .code_offset = source + 16, .addend = 0x2000,
                .kind = LAT_NATIVE_RELOC_TB_TARGET, .slots = 2,
                .target = cases[test].target_flags,
                .reserved = LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_1,
            };
        }
        LatNativePcMapV2 *maps = (void *)(image + h->pc_map_offset);
        unsigned source_map = source && !cases[test].missing ? 1 : 0;
        maps[source_map] = (LatNativePcMapV2){
            .guest_pc = 0x1000, .host_offset_begin = source,
            .host_offset_end = source + 24, .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
        };
        if (!cases[test].missing) {
            maps[!source_map] = (LatNativePcMapV2){
                .guest_pc = 0x2000, .host_offset_begin = target,
                .host_offset_end = target + 8, .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
            };
        }
        image_size = h->pc_map_offset + h->pc_map_count * sizeof(*maps);
        char error[256] = {0};
        gchar *text = NULL;
        gsize size = 0;
        int failed = !g_file_set_contents(path, (const char *)image, image_size, NULL) ||
            lat_aot_v2_emit_module_sources(path, directory, error, sizeof(error)) ||
            !g_file_get_contents(text_path, &text, &size, NULL);
        uint32_t expected = cases[test].threaded ?
            (original & 0xfc0003ffu) |
            (((uint32_t)(cases[test].displacement / 4) & 0xffffu) << 10) : original;
        uint32_t actual = 0;
        if (!failed && size >= source + 4) {
            memcpy(&actual, text + source, 4);
        }
        g_free(text);
        g_free(image);
        if (failed || actual != expected) {
            fprintf(stderr, "conditional exit case %u failed: %s (%08x != %08x)\n",
                    test, error, actual, expected);
            g_free(path);
            g_free(text_path);
            return -1;
        }
    }
    g_free(path);
    g_free(text_path);
    return 0;
}

static int test_indirect_exits(const char *directory)
{
    if (test_indirect_register_renaming()) {
        return -1;
    }
    char *path = g_build_filename(directory, "fixture.native", NULL);
    char *assembly_path = g_build_filename(directory, "module.S", NULL);
    char *text_path = g_build_filename(directory, "text.bin", NULL);
    char *slots_path = g_build_filename(directory, "guest-slots.bin", NULL);
    for (unsigned int test = 0; test < 12; test++) {
        unsigned char image[1024] = {0};
        LatNativeImageHeaderV2 *h = (void *)image;
        memcpy(h->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
        h->version = LAT_NATIVE_IMAGE_VERSION;
        h->header_size = sizeof(*h);
        h->flags = LAT_NATIVE_IMAGE_X86_STATIC_EXEC |
                   (test == 1 ? LAT_NATIVE_IMAGE_PIE : 0);
        uint64_t source_pc = test == 7 ? 0x400800 :
                             test == 8 ? 0x80001000 : 0x401000;
        uint64_t target_pc = test == 8 ? 0x80002000 : 0x402000;
        h->guest_entry = source_pc;
        h->preferred_guest_base = test == 7 ? 0x400123 :
                                  test == 8 ? 0x80000000 : 0x400000;
        h->guest_image_offset = sizeof(*h);
        h->guest_image_size = 1;
        h->code_offset = sizeof(*h) + 8;
        h->code_size = test == 11 ? 320 : 164;
        h->tb_table_offset = h->code_offset + h->code_size;
        h->tb_count = 2;
        h->relocation_offset = h->tb_table_offset + 2 * sizeof(LatNativeTbV1);
        h->pc_map_offset = h->relocation_offset;
        h->pc_map_count = test == 5 ? 3 : test == 6 ? 1 : 2;
        strcpy(h->lat_build_id, "indirect-dispatch-test");
        memcpy(image + h->code_offset + 4, indirect_dispatch,
               sizeof(indirect_dispatch));
        if (test >= 9) {
            memcpy(image + h->code_offset + 4,
                   aot_v2_indirect_dispatch[test == 11 ? 0 : test - 9],
                   sizeof(aot_v2_indirect_dispatch[0]));
        }
        if (test == 11) {
            memcpy(image + h->code_offset + 164,
                   aot_v2_indirect_dispatch[0],
                   sizeof(aot_v2_indirect_dispatch[0]));
        }
        uint32_t runtime_exit = 0x50000000u;
        memcpy(image + h->code_offset + 156, &runtime_exit, 4);
        LatNativeTbV1 *tbs = (void *)(image + h->tb_table_offset);
        tbs[0] = (LatNativeTbV1){
            .guest_pc = source_pc, .code_size = 160,
            .flags = test == 4 ? 1 : 3,
            .indirect_exit_offset = test == 2 ? 0 : 5,
        };
        tbs[1] = (LatNativeTbV1){
            .guest_pc = test == 11 ? source_pc :
                        test == 3 ? 0x1000000 : target_pc,
            .code_offset = 160, .code_size = test == 11 ? 160 : 4,
            .flags = 3, .indirect_exit_offset = test == 11 ? 5 : 0,
        };
        LatNativePcMapV2 *maps = (void *)(image + h->pc_map_offset);
        maps[0] = (LatNativePcMapV2){
            .guest_pc = source_pc, .host_offset_end = test == 5 ? 80 : 160,
            .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
        };
        if (test == 5) {
            maps[1] = (LatNativePcMapV2){
                .guest_pc = 0x401000, .host_offset_begin = 80,
                .host_offset_end = 160, .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
            };
        }
        if (test != 6) {
            maps[h->pc_map_count - 1] = (LatNativePcMapV2){
                .guest_pc = tbs[1].guest_pc, .host_offset_begin = 160,
                .host_offset_end = h->code_size,
                .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
            };
        }
        size_t size = h->pc_map_offset + h->pc_map_count * sizeof(*maps);
        char error[256] = {0};
        if (test == 0) {
            const uint32_t invalid[] = {2, 9, UINT32_MAX};
            for (unsigned int i = 0; i < G_N_ELEMENTS(invalid); i++) {
                tbs[0].indirect_exit_offset = invalid[i];
                if (!lat_native_image_validate(image, size, error, sizeof(error))) {
                    fprintf(stderr, "bad indirect offset accepted\n");
                    return -1;
                }
            }
            tbs[0].indirect_exit_offset = 5;
            image[h->code_offset + 4] ^= 1;
            if (!lat_native_image_validate(image, size, error, sizeof(error))) {
                fprintf(stderr, "bad indirect template accepted\n");
                return -1;
            }
            image[h->code_offset + 4] ^= 1;
        }
        if (!g_file_set_contents(path, (const char *)image, size, NULL) ||
            lat_aot_v2_emit_module_sources(path, directory, error, sizeof(error))) {
            fprintf(stderr, "indirect case %u failed: %s\n", test, error);
            return -1;
        }
        gchar *assembly = NULL, *text = NULL, *slots = NULL;
        gsize text_size, slots_size;
        int enabled = test == 0 || test == 1 || test == 6 ||
                      test == 7 || test == 8 || test >= 9;
        unsigned base_words = test == 1 || test == 7 ? 2 :
                              test == 8 ? 3 : 1;
        if (!g_file_get_contents(assembly_path, &assembly, NULL, NULL) ||
            !g_file_get_contents(text_path, &text, &text_size, NULL) ||
            !g_file_get_contents(slots_path, &slots, &slots_size, NULL) ||
            !!strstr(assembly, ".Llat_local_targets:\n") != enabled ||
            text_size != h->code_size ||
            (enabled && (!strstr(assembly, "beqz $a7,.Llat_local_miss_") ||
                         !strstr(assembly, "add.d $a7,$a7,$t1\n"
                                           "jr $a7\n") ||
                         !strstr(assembly, "bne $t0,$r21,.Llat_local_end_") ||
                         !strstr(assembly, "la.pcrel $t1,.Llat_local_targets\n") ||
                         !strstr(assembly, test >= 9 ? ".rept 8\n" :
                             base_words == 1 ? ".rept 19\n" :
                             base_words == 2 ? ".rept 18\n" :
                                               ".rept 17\n"))) ||
            (test >= 9 && test < 11 && (!strstr(assembly,
                ".incbin \"text.bin\",112,52\n") ||
                memcmp(text + 112, image + h->code_offset + 112, 52))) ||
            (test == 6 && strstr(assembly, "lat_aot_generated_text_begin+160-"))) {
            fprintf(stderr, "indirect output case %u failed\n", test);
            return -1;
        }
        if (test == 11 &&
            (strstr(assembly, ".Llat_local_miss_1:\n") ||
             strstr(assembly, ".rept 4294967") ||
             !strstr(assembly, ".incbin \"text.bin\",112,208\n") ||
             memcmp(text + 160, image + h->code_offset + 160, 160))) {
            fprintf(stderr, "non-primary indirect exit was dispatched\n");
            return -1;
        }
        if (test == 1) {
            const LatAotGuestSlotV2 *slot = (const void *)slots;
            uint32_t load;
            memcpy(&load, text + 4, 4);
            if (slots_size != sizeof(*slot) || slot->guest_rva != 0x1000 ||
                slot->fp_offset != -8 || load != 0x28ffe2ccu) {
                fprintf(stderr, "PIE indirect base did not use guest slots\n");
                return -1;
            }
        }
        if (enabled) {
            uint64_t local_base = test == 7 ? 0x400123 :
                                  test == 8 ? 0x80001000 : 0x401000;
            uint32_t expected[3] = {
                0x1400000cu |
                    ((uint32_t)(local_base >> 12) & 0xfffff) << 5,
                0x03800000u | ((uint32_t)local_base & 0xfff) << 10 |
                    12u << 5 | 12u,
                0x1600000cu |
                    ((uint32_t)(local_base >> 32) & 0xfffff) << 5,
            };
            if (test == 1) {
                expected[0] = 0x28ffe2ccu;
                expected[1] = 0x28c0018cu;
            }
            gchar *block = base_words == 1 ? g_strdup_printf(
                ".incbin \"text.bin\",0,4\n.word 0x%08x\nsub.d", expected[0]) :
                base_words == 2 ? g_strdup_printf(
                ".incbin \"text.bin\",0,4\n.word 0x%08x\n"
                ".word 0x%08x\nsub.d", expected[0], expected[1]) :
                g_strdup_printf(
                ".incbin \"text.bin\",0,4\n.word 0x%08x\n"
                ".word 0x%08x\n.word 0x%08x\nsub.d",
                expected[0], expected[1], expected[2]);
            if (!strstr(assembly, block)) {
                fprintf(stderr, "indirect base case %u was encoded incorrectly\n",
                        test);
                return -1;
            }
            g_free(block);
        }
        if (!enabled && memcmp(text + 4, indirect_dispatch,
                               sizeof(indirect_dispatch))) {
            fprintf(stderr, "disabled indirect exit was changed\n");
            return -1;
        }
        g_free(assembly);
        g_free(text);
        g_free(slots);
    }
    g_free(path);
    g_free(assembly_path);
    g_free(text_path);
    g_free(slots_path);
    return 0;
}

static int write_return_guard_fixture(const char *path,
                                      unsigned int caller_count,
                                      uint8_t guest_sha256[32])
{
    const size_t code_size = 160 + caller_count * sizeof(uint32_t);
    const size_t tb_count = 1 + caller_count;
    const size_t header_bytes = sizeof(LatNativeImageHeaderV2) + 8;
    const size_t tb_offset = header_bytes + code_size;
    const size_t pc_map_offset = tb_offset +
        tb_count * sizeof(LatNativeTbV1);
    const size_t image_size = pc_map_offset +
        tb_count * sizeof(LatNativePcMapV2);
    unsigned char *image = g_malloc0(image_size);
    LatNativeImageHeaderV2 *header = (void *)image;
    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_X86_STATIC_EXEC;
    header->guest_entry = 0x401000;
    header->preferred_guest_base = 0x400000;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = 1;
    header->code_offset = header_bytes;
    header->code_size = code_size;
    header->tb_table_offset = tb_offset;
    header->tb_count = tb_count;
    header->relocation_offset = pc_map_offset;
    header->pc_map_offset = pc_map_offset;
    header->pc_map_count = tb_count;
    strcpy(header->lat_build_id, "return-guard-test");
    for (unsigned int i = 0; i < 32; i++) {
        guest_sha256[i] = i;
    }
    memcpy(header->guest_sha256, guest_sha256, 32);

    memcpy(image + header->code_offset + 4, indirect_dispatch,
           sizeof(indirect_dispatch));
    uint32_t runtime_exit = 0x50000000u;
    memcpy(image + header->code_offset + 156, &runtime_exit, 4);
    LatNativeTbV1 *tbs = (void *)(image + tb_offset);
    tbs[0] = (LatNativeTbV1) {
        .guest_pc = 0x401000, .code_size = 160,
        .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
        .indirect_exit_offset = 5,
    };
    LatNativePcMapV2 *maps = (void *)(image + pc_map_offset);
    maps[0] = (LatNativePcMapV2) {
        .guest_pc = 0x401010, .host_offset_end = 160,
        .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
    };
    uint32_t nop = 0x03400000u;
    for (unsigned int i = 0; i < caller_count; i++) {
        uint64_t target = 0x402005 + i * 0x100;
        memcpy(image + header->code_offset + 160 + i * 4, &nop, 4);
        tbs[i + 1] = (LatNativeTbV1) {
            .guest_pc = target, .code_offset = 160 + i * 4,
            .code_size = 4,
            .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
        };
        maps[i + 1] = (LatNativePcMapV2) {
            .guest_pc = target, .host_offset_begin = 160 + i * 4,
            .host_offset_end = 164 + i * 4,
            .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
        };
    }
    int result = g_file_set_contents(path, (const char *)image,
                                     image_size, NULL) ? 0 : -1;
    g_free(image);
    return result;
}

static void make_return_guard_cfg(CfgProgram *program,
                                  unsigned int caller_count)
{
    *program = (CfgProgram) {0};
    program->function_count = 2;
    program->functions = g_new0(CfgProgramFunction, 2);
    program->functions[0] = (CfgProgramFunction) {
        .name = "callee", .start = 0x401000, .size = 0x20,
        .status = CFG_FUNCTION_OK, .first_tb = 0, .tb_count = 1,
    };
    program->functions[1] = (CfgProgramFunction) {
        .name = "caller", .start = 0x402000,
        .size = caller_count * 0x100 + 0x100,
        .status = CFG_FUNCTION_OK, .first_tb = 1,
        .tb_count = caller_count + 2,
    };
    program->tb_count = 1 + caller_count + 2;
    program->tbs = g_new0(CfgTb, program->tb_count);
    program->tbs[0] = (CfgTb) {
        .start = 0x401010, .end = 0x401012,
        .terminator_pc = 0x401011, .terminator = CFG_TB_RETURN,
    };
    program->edge_count = caller_count * 2 + 2;
    program->edges = g_new0(CfgProgramEdge, program->edge_count);
    for (unsigned int i = 0; i < caller_count; i++) {
        uint64_t caller = 0x402000 + i * 0x100;
        program->tbs[i + 1] = (CfgTb) {
            .start = caller, .end = caller + 5,
            .terminator_pc = caller, .terminator = CFG_TB_CALL,
            .first_edge = i * 2, .edge_count = 2,
        };
        program->edges[i * 2] = (CfgProgramEdge) {
            .from = caller, .to = 0x401000,
            .kind = CFG_EDGE_CALL, .resolution = CFG_EDGE_STATIC,
        };
        program->edges[i * 2 + 1] = (CfgProgramEdge) {
            .from = caller, .to = caller + 5,
            .kind = CFG_EDGE_CALL_RETURN, .resolution = CFG_EDGE_STATIC,
        };
    }
    uint64_t outer_end = 0x402000 + (caller_count - 1) * 0x100 + 0x80;
    uint64_t inner_end = 0x402000 + (caller_count - 2) * 0x100 + 0x80;
    program->tbs[caller_count + 1] = (CfgTb) {
        .start = outer_end, .end = outer_end + 2,
        .terminator_pc = outer_end, .terminator = CFG_TB_JUMP,
        .first_edge = caller_count * 2, .edge_count = 1,
    };
    program->tbs[caller_count + 2] = (CfgTb) {
        .start = inner_end, .end = inner_end + 2,
        .terminator_pc = inner_end, .terminator = CFG_TB_JUMP,
        .first_edge = caller_count * 2 + 1, .edge_count = 1,
    };
    program->edges[caller_count * 2] = (CfgProgramEdge) {
        .from = outer_end, .to = 0x402100,
        .kind = CFG_EDGE_JUMP, .resolution = CFG_EDGE_STATIC,
    };
    program->edges[caller_count * 2 + 1] = (CfgProgramEdge) {
        .from = inner_end, .to = 0x402200,
        .kind = CFG_EDGE_JUMP, .resolution = CFG_EDGE_STATIC,
    };
}

static void free_return_guard_cfg(CfgProgram *program)
{
    g_free(program->edges);
    g_free(program->tbs);
    g_free(program->functions);
}

static int test_return_guards(const char *directory)
{
    char *path = g_build_filename(directory, "return-guard.native", NULL);
    char *assembly_path = g_build_filename(directory, "module.S", NULL);
    uint8_t digest[32];
    CfgProgram program;
    char error[256] = {0};
    gchar *assembly = NULL;
    int result = -1;
    if (write_return_guard_fixture(path, 5, digest)) {
        goto cleanup_files;
    }
    make_return_guard_cfg(&program, 5);
    result = lat_aot_v2_emit_module_sources_with_cfg(
        path, directory, &program, digest, error, sizeof(error));
    if (result || !g_file_get_contents(assembly_path, &assembly, NULL, NULL) ||
        !strstr(assembly, "ori $t2,$t2,773\n"
                          "bne $t0,$t2,.Llat_return_next_0_0\n") ||
        !strstr(assembly, "ori $t2,$t2,517\n"
                          "bne $t0,$t2,.Llat_return_next_0_1\n") ||
        strstr(assembly, "ori $t2,$t2,1029\n") ||
        !strstr(assembly, ".Llat_return_next_0_1:\n") ||
        !strstr(assembly, ".rept 11\n")) {
        fprintf(stderr, "ranked return guards were not emitted: %s\n", error);
        result = -1;
        goto out;
    }
    g_free(assembly);
    assembly = NULL;
    uint8_t wrong_digest[32];
    memcpy(wrong_digest, digest, sizeof(wrong_digest));
    wrong_digest[0] ^= 1;
    memset(error, 0, sizeof(error));
    if (!lat_aot_v2_emit_module_sources_with_cfg(
            path, directory, &program, wrong_digest,
            error, sizeof(error)) || !strstr(error, "digest differs")) {
        fprintf(stderr, "mismatched CFG digest was accepted: %s\n", error);
        result = -1;
        goto out;
    }
    memset(error, 0, sizeof(error));
    if (lat_aot_v2_emit_module_sources(path, directory,
                                       error, sizeof(error)) ||
        !g_file_get_contents(assembly_path, &assembly, NULL, NULL) ||
        strstr(assembly, ".Llat_return_next_") ||
        !strstr(assembly, ".rept 19\n")) {
        fprintf(stderr, "CFG-free return fallback changed: %s\n", error);
        result = -1;
        goto out;
    }
    free_return_guard_cfg(&program);
    g_free(assembly);
    assembly = NULL;
    if (write_return_guard_fixture(path, 6, digest)) {
        result = -1;
        goto cleanup_files;
    }
    make_return_guard_cfg(&program, 6);
    memset(error, 0, sizeof(error));
    if (lat_aot_v2_emit_module_sources_with_cfg(
            path, directory, &program, digest, error, sizeof(error)) ||
        !g_file_get_contents(assembly_path, &assembly, NULL, NULL) ||
        strstr(assembly, ".Llat_return_next_") ||
        !strstr(assembly, ".rept 19\n")) {
        fprintf(stderr, "large return set did not fall back: %s\n", error);
        result = -1;
    }
out:
    free_return_guard_cfg(&program);
cleanup_files:
    g_free(assembly);
    g_remove(path);
    g_free(assembly_path);
    g_free(path);
    return result;
}

static int test_profiled_layout(const char *directory)
{
    const uint64_t code_size = 160;
    size_t image_size = sizeof(LatNativeImageHeaderV2) + 8 + code_size +
        4 * sizeof(LatNativeTbV1) + sizeof(LatNativeRelocationV1) +
        4 * sizeof(LatNativePcMapV2);
    uint8_t *image = g_malloc0(image_size);
    LatNativeImageHeaderV2 *header = (void *)image;
    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_X86_STATIC_EXEC;
    header->preferred_guest_base = 0x400000;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = 1;
    header->code_offset = header->guest_image_offset + 8;
    header->code_size = code_size;
    header->tb_table_offset = header->code_offset + code_size;
    header->tb_count = 4;
    header->relocation_offset = header->tb_table_offset +
                                4 * sizeof(LatNativeTbV1);
    header->relocation_count = 1;
    header->pc_map_offset = header->relocation_offset +
                            sizeof(LatNativeRelocationV1);
    header->pc_map_count = 4;
    memset(header->guest_sha256, 0x5a, sizeof(header->guest_sha256));
    strcpy(header->lat_build_id, "profile-layout-test");

    uint32_t *code = (void *)(image + header->code_offset);
    code[0] = 0x58000000u;
    code[1] = 0x03400000u;
    code[4] = 0x03400001u;
    code[5] = 0x50000000u;
    code[6] = 0x03400002u;
    code[7] = 0x03400000u;
    code[10] = 0x03400003u;
    code[11] = 0x03400000u;

    LatNativeTbV1 *tbs = (void *)(image + header->tb_table_offset);
    tbs[0] = (LatNativeTbV1) {
        .guest_pc = 0x401000, .code_offset = 0, .code_size = 8, .flags = 3,
    };
    tbs[1] = (LatNativeTbV1) {
        .guest_pc = 0x402000, .code_offset = 16, .code_size = 8, .flags = 3,
    };
    tbs[2] = (LatNativeTbV1) {
        .guest_pc = 0x403000, .code_offset = 40, .code_size = 8, .flags = 3,
    };
    tbs[3] = (LatNativeTbV1) {
        .guest_pc = 0x404000, .code_offset = 24, .code_size = 8, .flags = 3,
    };
    LatNativeRelocationV1 *relocation =
        (void *)(image + header->relocation_offset);
    *relocation = (LatNativeRelocationV1) {
        .code_offset = 0, .addend = 0x404000,
        .kind = LAT_NATIVE_RELOC_TB_TARGET, .target = 3, .slots = 1,
    };
    LatNativePcMapV2 *maps = (void *)(image + header->pc_map_offset);
    const unsigned int code_order[] = {0, 1, 3, 2};
    for (unsigned int i = 0; i < G_N_ELEMENTS(code_order); i++) {
        unsigned int index = code_order[i];
        maps[i] = (LatNativePcMapV2) {
            .guest_pc = tbs[index].guest_pc,
            .host_offset_begin = tbs[index].code_offset,
            .host_offset_end = tbs[index].code_offset + tbs[index].code_size,
            .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
        };
    }

    char *image_path = g_build_filename(
        directory, "profile-layout.native", NULL);
    char *text_path = g_build_filename(directory, "text.bin", NULL);
    char *tbs_path = g_build_filename(directory, "tbs.bin", NULL);
    char *maps_path = g_build_filename(directory, "pc-maps.bin", NULL);
    CfgProgram program = {0};
    program.function_count = 1;
    program.functions = g_new0(CfgProgramFunction, 1);
    program.functions[0] = (CfgProgramFunction) {
        .start = 0x402000, .size = 0x3000,
        .first_tb = 0, .tb_count = 2,
    };
    program.tb_count = 4;
    program.tbs = g_new0(CfgTb, 4);
    program.tbs[0] = (CfgTb) {
        .start = 0x402000, .end = 0x402001,
        .semantic_flags = CFG_TB_CODE64,
    };
    program.tbs[1] = (CfgTb) {
        .start = 0x404000, .end = 0x404001,
        .semantic_flags = CFG_TB_CODE64,
        .first_edge = 0, .edge_count = 1,
    };
    program.tbs[2] = program.tbs[0];
    program.tbs[2].observed = true;
    program.tbs[2].semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL;
    program.tbs[3] = program.tbs[1];
    program.tbs[3].observed = true;
    program.tbs[3].semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL;
    program.edge_count = 1;
    program.edges = g_new0(CfgProgramEdge, 1);
    program.edges[0] = (CfgProgramEdge) {
        .from = 0x404000, .to = 0x404000,
        .kind = CFG_EDGE_JUMP, .resolution = CFG_EDGE_STATIC,
    };
    char error[256] = {0};
    gchar *text = NULL, *tb_data = NULL, *map_data = NULL;
    gsize text_size = 0, tb_size = 0, map_size = 0;
    int failed = !g_file_set_contents(
        image_path, (const char *)image, image_size, NULL) ||
        lat_aot_v2_emit_module_sources_with_cfg(
            image_path, directory, &program, header->guest_sha256,
            error, sizeof(error)) ||
        !g_file_get_contents(text_path, &text, &text_size, NULL) ||
        !g_file_get_contents(tbs_path, &tb_data, &tb_size, NULL) ||
        !g_file_get_contents(maps_path, &map_data, &map_size, NULL);
    const LatAotTbV2 *output_tbs = (const void *)tb_data;
    const LatAotPcMapV2 *output_maps = (const void *)map_data;
    const uint64_t expected_tb_offsets[] = {20, 64, 96, 32};
    const uint64_t expected_map_rvas[] = {0x1000, 0x4000, 0x2000, 0x3000};
    const uint64_t expected_map_offsets[] = {20, 32, 64, 96};
    for (unsigned int i = 0; !failed && i < 4; i++) {
        failed |= output_tbs[i].host_offset != expected_tb_offsets[i];
        failed |= output_maps[i].guest_rva != expected_map_rvas[i];
        failed |= output_maps[i].host_offset_begin != expected_map_offsets[i];
    }
    if (!failed) {
        const uint32_t *words = (const void *)text;
        failed = text_size != code_size ||
                 tb_size != 4 * sizeof(*output_tbs) ||
                 map_size != 4 * sizeof(*output_maps) ||
                 (words[5] & 0xfc000000u) != 0x58000000u ||
                 words[8] != 0x03400002u ||
                 words[16] != 0x03400001u ||
                 words[24] != 0x03400003u;
    }
    if (failed) {
        fprintf(stderr, "profiled TB layout was not preserved: %s\n",
                error[0] ? error : "invalid output");
    }
    g_free(map_data);
    g_free(tb_data);
    g_free(text);
    g_free(program.edges);
    g_free(program.tbs);
    g_free(program.functions);
    g_remove(image_path);
    g_free(maps_path);
    g_free(tbs_path);
    g_free(text_path);
    g_free(image_path);
    g_free(image);
    return failed ? -1 : 0;
}

static int test_profiled_layout_fallback(const char *directory)
{
    const uint64_t code_size = 8;
    size_t image_size = sizeof(LatNativeImageHeaderV2) + 8 + code_size +
        2 * sizeof(LatNativeTbV1) + 2 * sizeof(LatNativePcMapV2);
    uint8_t *image = g_malloc0(image_size);
    LatNativeImageHeaderV2 *header = (void *)image;
    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_X86_STATIC_EXEC;
    header->preferred_guest_base = 0x400000;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = 1;
    header->code_offset = header->guest_image_offset + 8;
    header->code_size = code_size;
    header->tb_table_offset = header->code_offset + code_size;
    header->tb_count = 2;
    header->relocation_offset = header->tb_table_offset +
                                2 * sizeof(LatNativeTbV1);
    header->pc_map_offset = header->relocation_offset;
    header->pc_map_count = 2;
    memset(header->guest_sha256, 0x6b, sizeof(header->guest_sha256));
    strcpy(header->lat_build_id, "profile-layout-fallback-test");

    uint32_t *code = (void *)(image + header->code_offset);
    code[0] = 0x50000000u;
    code[1] = 0x03400000u;
    LatNativeTbV1 *tbs = (void *)(image + header->tb_table_offset);
    tbs[0] = (LatNativeTbV1) {
        .guest_pc = 0x401000, .code_offset = 0, .code_size = 4, .flags = 3,
    };
    tbs[1] = (LatNativeTbV1) {
        .guest_pc = 0x402000, .code_offset = 4, .code_size = 4, .flags = 3,
    };
    LatNativePcMapV2 *maps = (void *)(image + header->pc_map_offset);
    for (unsigned int i = 0; i < 2; i++) {
        maps[i] = (LatNativePcMapV2) {
            .guest_pc = tbs[i].guest_pc,
            .host_offset_begin = tbs[i].code_offset,
            .host_offset_end = tbs[i].code_offset + tbs[i].code_size,
            .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
        };
    }

    CfgProgram program = {0};
    program.tb_count = 2;
    program.tbs = g_new0(CfgTb, 2);
    for (unsigned int i = 0; i < 2; i++) {
        program.tbs[i] = (CfgTb) {
            .start = tbs[i].guest_pc,
            .end = tbs[i].guest_pc + 1,
            .semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL,
            .observed = true,
        };
    }

    char *image_path = g_build_filename(
        directory, "profile-layout-fallback.native", NULL);
    char *text_path = g_build_filename(directory, "text.bin", NULL);
    char *tbs_path = g_build_filename(directory, "tbs.bin", NULL);
    char error[256] = {0};
    gchar *text = NULL, *tb_data = NULL;
    gsize text_size = 0, tb_size = 0;
    int failed = !g_file_set_contents(
        image_path, (const char *)image, image_size, NULL) ||
        lat_aot_v2_emit_module_sources_with_cfg(
            image_path, directory, &program, header->guest_sha256,
            error, sizeof(error)) ||
        !g_file_get_contents(text_path, &text, &text_size, NULL) ||
        !g_file_get_contents(tbs_path, &tb_data, &tb_size, NULL);
    const LatAotTbV2 *output_tbs = (const void *)tb_data;
    if (!failed) {
        failed = text_size != code_size ||
                 memcmp(text, code, code_size) ||
                 tb_size != 2 * sizeof(*output_tbs) ||
                 output_tbs[0].host_offset != 0 ||
                 output_tbs[1].host_offset != 4;
    }
    if (failed) {
        fprintf(stderr, "tight profiled layout did not fall back: %s\n",
                error[0] ? error : "invalid output");
    }
    g_free(tb_data);
    g_free(text);
    g_free(program.tbs);
    g_remove(image_path);
    g_free(tbs_path);
    g_free(text_path);
    g_free(image_path);
    g_free(image);
    return failed ? -1 : 0;
}

static int write_large_guest_table_fixture(const char *path,
                                           size_t address_count,
                                           int local_dispatch)
{
    const size_t relocation_code_size =
        address_count * 3 * sizeof(uint32_t);
    const size_t code_size = relocation_code_size + (local_dispatch ?
        sizeof(indirect_dispatch) + sizeof(uint32_t) : 0);
    const size_t tb_offset = sizeof(LatNativeImageHeaderV2) + 8 + code_size;
    const size_t relocation_offset = tb_offset + sizeof(LatNativeTbV1);
    const size_t pc_map_offset = relocation_offset +
        address_count * sizeof(LatNativeRelocationV1);
    const size_t image_size = pc_map_offset + sizeof(LatNativePcMapV2);
    unsigned char *image = g_malloc0(image_size);
    if (!image) {
        return -1;
    }
    LatNativeImageHeaderV2 *header = (void *)image;
    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_PIE;
    header->guest_entry = 0x401000;
    header->preferred_guest_base = 0x400000;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = 1;
    header->code_offset = sizeof(*header) + 8;
    header->code_size = code_size;
    header->tb_table_offset = tb_offset;
    header->tb_count = 1;
    header->relocation_offset = relocation_offset;
    header->relocation_count = address_count;
    header->pc_map_offset = pc_map_offset;
    header->pc_map_count = 1;
    strcpy(header->lat_build_id, "aot-v2-two-level-guest-table-test-v1");

    uint32_t *code = (void *)(image + header->code_offset);
    LatNativeRelocationV1 *relocations =
        (void *)(image + relocation_offset);
    for (size_t i = 0; i < address_count; i++) {
        code[i * 3] = 12; /* destination register */
        code[i * 3 + 1] = 0x03400000u;
        code[i * 3 + 2] = 0x03400000u;
        relocations[i] = (LatNativeRelocationV1) {
            .code_offset = i * 3 * sizeof(uint32_t),
            .addend = 0x401000 + i * 8,
            .kind = LAT_NATIVE_RELOC_GUEST_ADDRESS,
            .slots = 3,
        };
    }
    if (local_dispatch) {
        memcpy((unsigned char *)code + relocation_code_size,
               indirect_dispatch, sizeof(indirect_dispatch));
        code[(code_size - sizeof(uint32_t)) / sizeof(uint32_t)] = 0x50000000u;
    }
    LatNativeTbV1 *tb = (void *)(image + tb_offset);
    *tb = (LatNativeTbV1) {
        .guest_pc = 0x401000, .code_offset = 0, .code_size = code_size,
        .flags = local_dispatch ?
            LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL : 0,
        .indirect_exit_offset = local_dispatch ? relocation_code_size + 1 : 0,
    };
    LatNativePcMapV2 *map = (void *)(image + pc_map_offset);
    *map = (LatNativePcMapV2) {
        .guest_pc = 0x401000,
        .host_offset_begin = 0,
        .host_offset_end = code_size,
        .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
    };
    FILE *file = fopen(path, "wb");
    int result = file && fwrite(image, image_size, 1, file) == 1 ? 0 : -1;
    if (file && fclose(file)) {
        result = -1;
    }
    g_free(image);
    return result;
}

static int write_guest_slot_boundary_fixture(const char *path)
{
    const size_t address_count = LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT;
    const size_t relocation_code_size = address_count * 2 * sizeof(uint32_t);
    const size_t code_size = relocation_code_size +
                             sizeof(indirect_dispatch) + sizeof(uint32_t);
    const size_t tb_offset = sizeof(LatNativeImageHeaderV2) + 8 + code_size;
    const size_t relocation_offset = tb_offset + sizeof(LatNativeTbV1);
    const size_t pc_map_offset = relocation_offset +
        address_count * sizeof(LatNativeRelocationV1);
    const size_t image_size = pc_map_offset + sizeof(LatNativePcMapV2);
    unsigned char *image = g_malloc0(image_size);
    if (!image) {
        return -1;
    }
    LatNativeImageHeaderV2 *header = (void *)image;
    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_PIE;
    header->guest_entry = 0x401000;
    header->preferred_guest_base = 0x400000;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = 1;
    header->code_offset = sizeof(*header) + 8;
    header->code_size = code_size;
    header->tb_table_offset = tb_offset;
    header->tb_count = 1;
    header->relocation_offset = relocation_offset;
    header->relocation_count = address_count;
    header->pc_map_offset = pc_map_offset;
    header->pc_map_count = 1;
    strcpy(header->lat_build_id, "aot-v2-local-slot-boundary-test-v1");

    uint32_t *code = (void *)(image + header->code_offset);
    LatNativeRelocationV1 *relocations =
        (void *)(image + relocation_offset);
    for (size_t i = 0; i < address_count; i++) {
        code[i * 2] = 12; /* destination register */
        code[i * 2 + 1] = 0x03400000u;
        relocations[i] = (LatNativeRelocationV1) {
            .code_offset = i * 2 * sizeof(uint32_t),
            .addend = 0x402000 + i * 8,
            .kind = LAT_NATIVE_RELOC_GUEST_ADDRESS,
            .slots = 2,
        };
    }
    memcpy((unsigned char *)code + relocation_code_size,
           indirect_dispatch, sizeof(indirect_dispatch));
    code[(code_size - sizeof(uint32_t)) / sizeof(uint32_t)] = 0x50000000u;
    LatNativeTbV1 *tb = (void *)(image + tb_offset);
    *tb = (LatNativeTbV1) {
        .guest_pc = 0x401000,
        .code_offset = 0,
        .code_size = code_size,
        .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
        .indirect_exit_offset = relocation_code_size + 1,
    };
    LatNativePcMapV2 *map = (void *)(image + pc_map_offset);
    *map = (LatNativePcMapV2) {
        .guest_pc = 0x401000,
        .host_offset_begin = 0,
        .host_offset_end = code_size,
        .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
    };
    FILE *file = fopen(path, "wb");
    int result = file && fwrite(image, image_size, 1, file) == 1 ? 0 : -1;
    if (file && fclose(file)) {
        result = -1;
    }
    g_free(image);
    return result;
}

static int test_edge_flags(const char *directory)
{
    /* All four widths, logical, arithmetic and shift flag writes. */
    const uint32_t flag_instructions[] = {
        0x003f3588, 0x003f3589, 0x003f358a, 0x003f358b,
        0x003fb190, 0x003fb191, 0x003fb192, 0x003fb193,
        0x003fb194, 0x003fb195, 0x003fb196, 0x003fb197,
        0x003fb198, 0x003fb199, 0x003fb19a, 0x003fb19b,
        0x003f3194, 0x003f3195, 0x003f3196, 0x003f3197,
        0x003f3198, 0x003f3199, 0x003f319a, 0x003f319b,
        0x003f319c, 0x003f319d, 0x003f319e, 0x003f319f,
        0x00542180, 0x00544181, 0x00548182, 0x00550183,
        0x00542184, 0x00544185, 0x00548186, 0x00550187,
        0x00542188, 0x00544189, 0x0054818a, 0x0055018b,
        0x00008180, 0x00008181, 0x00008182, 0x00008183,
        0x00008184, 0x00008185, 0x00008186, 0x00008187,
    };
    for (unsigned use = 0; use < 256; use++) {
        g_assert(lat_eflags_link_actions(use, 0, 0, 4) ==
                 (use ? 0 : LAT_EFLAGS_LINK_NOP | LAT_EFLAGS_LINK_BYPASS));
        g_assert(lat_eflags_link_actions(use, 1, 0, 4) ==
                 (use ? 0 : LAT_EFLAGS_LINK_BYPASS));
        g_assert(lat_eflags_link_actions(use, 0, UINT16_MAX, UINT16_MAX) == 0);
    }
    char *path = g_build_filename(directory, "flags.native", NULL);
    char *base_path = g_build_filename(directory, "flags-base.native", NULL);
    char *merged_path = g_build_filename(directory, "flags-merged.native",
                                         NULL);
    char *text_path = g_build_filename(directory, "text.bin", NULL);
    const unsigned mode_count = 18 + G_N_ELEMENTS(flag_instructions);
    for (unsigned mode = 0; mode < mode_count; mode++) {
        unsigned char image[512] = {0};
        LatNativeImageHeaderV2 *h = (void *)image;
        memcpy(h->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
        h->version = LAT_NATIVE_IMAGE_VERSION;
        h->header_size = sizeof(*h);
        h->flags = LAT_NATIVE_IMAGE_X86_STATIC_EXEC |
                   LAT_NATIVE_IMAGE_CROSS_MODULE_TARGETS;
        h->guest_image_offset = sizeof(*h);
        h->guest_image_size = 8;
        h->code_offset = h->guest_image_offset + 8;
        h->code_size = 16;
        h->tb_table_offset = h->code_offset + h->code_size;
        h->tb_count = mode == 2 ? 1 : 2;
        h->relocation_offset = h->tb_table_offset +
                               h->tb_count * sizeof(LatNativeTbV1);
        h->relocation_count = 1;
        h->pc_map_offset = h->relocation_offset + sizeof(LatNativeRelocationV1);
        h->pc_map_count = h->tb_count;
        strcpy(h->lat_build_id, "flags-test");
        uint32_t *code = (void *)(image + h->code_offset);
        code[0] = 0x003f358a; /* x86sub.w t0, t1 */
        code[1] = 0x1e00000c;
        code[2] = 0x4c000184;
        code[3] = 0x03400000;
        LatNativeTbV1 *tbs = (void *)(image + h->tb_table_offset);
        tbs[0] = (LatNativeTbV1){ .guest_pc = 0x1000, .code_size = 12,
                                .eflags_instruction = code[0],
                                .eflags_offset = {1, 0} };
        if (h->tb_count == 2) {
            tbs[1] = (LatNativeTbV1){ .guest_pc = 0x2000, .code_offset = 12,
                .code_size = 4,
                .optimization_flags = LAT_NATIVE_TB_ENTRY_FLAGS_DEAD };
        }
        LatNativeRelocationV1 *rel = (void *)(image + h->relocation_offset);
        *rel = (LatNativeRelocationV1){ .code_offset = 4, .addend = 0x2000,
            .kind = LAT_NATIVE_RELOC_TB_TARGET, .slots = 2,
            .reserved = LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0 };
        LatNativePcMapV2 *maps = (void *)(image + h->pc_map_offset);
        maps[0] = (LatNativePcMapV2){ .guest_pc = 0x1000, .host_offset_end = 12,
                                    .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE };
        if (h->tb_count == 2) {
            maps[1] = (LatNativePcMapV2){ .guest_pc = 0x2000,
                .host_offset_begin = 12, .host_offset_end = 16,
                .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE };
        }
        switch (mode) {
        case 0:
            tbs[1].optimization_flags = 0;
            break;
        case 3:
            tbs[0].eflags_offset[0] = 2;
            break;
        case 4:
            tbs[0].eflags_offset[0] = 13;
            break;
        case 5:
            code[0] = 0x0010b58c; /* add.d must not be erased */
            break;
        case 6:
            tbs[1].optimization_flags = 2;
            break;
        case 7:
            tbs[0].eflags_offset[0] = 0;
            break;
        case 8:
            tbs[0].eflags_offset[0] = 0;
            tbs[0].eflags_offset[1] = 1;
            rel->reserved = LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_1;
            break;
        case 9:
            rel->reserved = LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0;
            break;
        case 10:
            h->version = 2;
            break;
        case 12:
        case 13:
        case 14:
        case 15:
            code[0] = 0x03400000;
            tbs[0].eflags_offset[0] = 0;
            tbs[0].eflags_stub_offset[0] = mode == 14 ? 2 : 1;
            if (mode == 13) {
                tbs[1].optimization_flags = 0;
            } else if (mode == 15) {
                code[0] = 0x003f358a;
            }
            break;
        case 16:
            code[0] = tbs[0].eflags_instruction = 0x003f9490; /* x86and.b */
            break;
        case 17:
            h->version = 3;
            break;
        }
        if (mode >= 18) {
            code[0] = tbs[0].eflags_instruction = flag_instructions[mode - 18];
        }
        size_t size = h->pc_map_offset + h->pc_map_count * sizeof(*maps);
        if (mode == 11) {
            size--;
        }
        g_assert(g_file_set_contents(path, (const char *)image, size, NULL));
        char error[256] = {0};
        int rc = lat_aot_v2_emit_module_sources(path, directory, error,
                                               sizeof(error));
        if ((mode >= 3 && mode <= 6) || mode == 10 || mode == 11 ||
            mode == 14 || mode == 15 || mode == 17) {
            g_assert(rc != 0);
            continue;
        }
        g_assert(rc == 0);
        gchar *text = NULL;
        g_assert(g_file_get_contents(text_path, &text, NULL, NULL));
        uint32_t expected = (mode == 1 || mode == 8 || mode == 13 ||
                             mode == 16 || mode >= 18) ?
                             0x03400000u : 0x003f358au;
        if (mode == 12) {
            expected = 0x50000c00u; /* bypass the entire stub to TB+12 */
        }
        g_assert(*(uint32_t *)text == expected);
        g_free(text);
        if (mode == 2) {
            g_assert(g_file_set_contents(base_path, (const char *)image,
                                         size, NULL));
        }
        if (mode == 7) {
            /* The base edge gains a target supplied by the delta. */
            g_assert(lat_native_image_merge_files(base_path, path, merged_path,
                                                   error, sizeof(error)) == 0);
            g_assert(lat_aot_v2_emit_module_sources(merged_path, directory,
                                                    error, sizeof(error)) == 0);
            g_assert(g_file_get_contents(text_path, &text, NULL, NULL));
            g_assert(*(uint32_t *)text == 0x03400000u);
            g_free(text);
        }
    }
    g_remove(path);
    g_remove(base_path);
    g_remove(merged_path);
    g_free(path);
    g_free(base_path);
    g_free(merged_path);
    g_free(text_path);
    return 0;
}

static int test_image_data(const char *directory)
{
    unsigned char image[1024] = {0};
    LatNativeImageHeaderV2 *h = (void *)image;
    memcpy(h->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    h->version = LAT_NATIVE_IMAGE_VERSION;
    h->header_size = sizeof(*h);
    h->flags = LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP;
    h->guest_image_offset = sizeof(*h);
    h->guest_image_size = 1;
    h->code_offset = sizeof(*h) + 8;
    h->code_size = 96;
    h->tb_table_offset = h->code_offset + h->code_size;
    h->tb_count = 2;
    h->relocation_offset = h->tb_table_offset + 2 * sizeof(LatNativeTbV1);
    h->relocation_count = 2;
    h->pc_map_offset = h->relocation_offset + 2 * sizeof(LatNativeRelocationV1);
    h->pc_map_count = 2;
    h->tu_table_offset = h->pc_map_offset + 2 * sizeof(LatNativePcMapV2);
    strcpy(h->lat_build_id, "image-data-test");
    uint32_t *code = (void *)(image + h->code_offset);
    LatNativeTbV1 *tbs = (void *)(image + h->tb_table_offset);
    LatNativeRelocationV1 *rels = (void *)(image + h->relocation_offset);
    LatNativePcMapV2 *maps = (void *)(image + h->pc_map_offset);
    CfgTb cfg_tbs[2] = {0};
    for (int i = 0; i < 2; i++) {
        code[i * 8] = 0x1400000c;
        code[i * 8 + 1] = 0x0380018c;
        code[i * 8 + 2] = 0x1600000c;
        tbs[i] = (LatNativeTbV1){
            .guest_pc = 0x401000 + i * 16, .code_offset = i * 32,
            .code_size = 12, .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
        };
        rels[i] = (LatNativeRelocationV1){
            .code_offset = i * 32, .kind = LAT_NATIVE_RELOC_IMAGE_DATA,
            .slots = i ? 3 : 2, .addend = 64, .reserved = 32,
        };
        cfg_tbs[i] = (CfgTb){
            .start = tbs[i].guest_pc, .end = tbs[i].guest_pc + 16,
            .semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL,
            .observed = i == 1,
        };
        maps[i] = (LatNativePcMapV2){
            .guest_pc = tbs[i].guest_pc,
            .host_offset_begin = i * 32,
            .host_offset_end = i * 32 + 12,
            .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE,
        };
    }
    memset(image + h->code_offset + 64, 0xa5, 32);
    g_autofree char *path = g_build_filename(directory, "data.native", NULL);
    g_autofree char *text_path = g_build_filename(directory, "text.bin", NULL);
    if (!g_file_set_contents(path, (const char *)image, h->tu_table_offset, NULL)) {
        return 1;
    }
    CfgProgram program = {.tbs = cfg_tbs, .tb_count = 2};
    char error[256] = {0};
    for (int profile = 0; profile < 2; profile++) {
        g_autofree gchar *text = NULL;
        gsize size = 0;
        if (lat_aot_v2_emit_module_sources_with_cfg(path, directory,
                profile ? &program : NULL, h->guest_sha256,
                error, sizeof(error)) ||
            !g_file_get_contents(text_path, &text, &size, NULL) || size != 96 ||
            memcmp(text + 64, image + h->code_offset + 64, 32)) {
            fprintf(stderr, "data module layout failed: %s\n", error);
            return 1;
        }
        const uint32_t *words = (const void *)text;
        for (int i = 0; i < 2; i++) {
            if (words[i * 8] != 0x1a00000cu ||
                words[i * 8 + 1] != (0x0380018cu | 64u << 10) ||
                (i && words[i * 8 + 2] != 0x03400000u)) {
                fprintf(stderr, "data module PIC address changed\n");
                return 1;
            }
        }
    }
    return 0;
}

static int test_xsave_runtime_symbols(const char *directory)
{
    static const uint32_t symbols[] = {
        LAT_NATIVE_SYMBOL_XSAVE, LAT_NATIVE_SYMBOL_XSAVEOPT,
        LAT_NATIVE_SYMBOL_XRSTOR, LAT_NATIVE_SYMBOL_XSETBV,
    };
    static const char *const names[] = {
        "lat_aot_runtime_xsave", "lat_aot_runtime_xsaveopt",
        "lat_aot_runtime_xrstor", "lat_aot_runtime_xsetbv",
    };
    g_autofree char *path = g_build_filename(directory, "helper.native", NULL);
    g_autofree char *assembly_path = g_build_filename(directory, "module.S", NULL);
    for (size_t i = 0; i < G_N_ELEMENTS(symbols); i++) {
        g_autofree gchar *image = NULL;
        g_autofree gchar *assembly = NULL;
        gsize size = 0;
        char error[256] = {0};
        if (write_fixture(path, 0, 0, 0, 0) ||
            !g_file_get_contents(path, &image, &size, NULL)) {
            return 1;
        }
        LatNativeImageHeaderV2 *header = (void *)image;
        LatNativeRelocationV1 *relocations =
            (void *)(image + header->relocation_offset);
        relocations[0].kind = LAT_NATIVE_RELOC_RUNTIME_SYMBOL;
        relocations[0].target = symbols[i];
        relocations[0].addend = 0;
        relocations[0].reserved = 0;
        if (!g_file_set_contents(path, image, size, NULL) ||
            lat_aot_v2_emit_module_sources(path, directory, error, sizeof(error)) ||
            !g_file_get_contents(assembly_path, &assembly, NULL, NULL) ||
            !strstr(assembly, names[i])) {
            fprintf(stderr, "XSAVE runtime symbol mapping failed: %s\n", error);
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    g_autofree char *directory = g_build_filename(g_get_tmp_dir(),
        "latc-aot-v2-module-pack-XXXXXX", NULL);
    if (!g_mkdtemp(directory)) {
        perror("create test directory");
        return 1;
    }
    if (test_image_data(directory)) return 1;
    if (test_xsave_runtime_symbols(directory)) return 1;
    if (test_alias_dependency(directory)) return 1;
    char *image_path = g_build_filename(directory, "fixture.latnative", NULL);
    if (test_primary_tb_aliases(directory)) {
        g_free(image_path);
        return 1;
    }
    test_edge_flags(directory);
    char error[256] = {0};
    if (write_fixture(image_path, 0, 0, 0, 0) ||
        lat_aot_v2_emit_module_sources(image_path, directory,
                                       error, sizeof(error))) {
        fprintf(stderr, "cannot emit test module: %s\n", error);
        g_free(image_path);
        return 1;
    }
    char *metadata_path = g_build_filename(directory, "module.c", NULL);
    char *text_path = g_build_filename(directory, "text.bin", NULL);
    char *tbs_path = g_build_filename(directory, "tbs.bin", NULL);
    char *maps_path = g_build_filename(directory, "pc-maps.bin", NULL);
    char *slots_path = g_build_filename(directory, "guest-slots.bin", NULL);
    gchar *text = NULL;
    gsize text_size = 0;
    if (!g_file_get_contents(text_path, &text, &text_size, NULL) ||
        text_size != 28) {
        fprintf(stderr, "cannot read emitted module text\n");
        g_free(text_path);
        g_free(image_path);
        return 1;
    }
    gchar *maps = NULL;
    gsize maps_size = 0;
    if (!g_file_get_contents(maps_path, &maps, &maps_size, NULL) ||
        maps_size != 2 * sizeof(LatAotPcMapV2) ||
        (int32_t)((LatAotPcMapV2 *)maps)[0].state_record_offset != -8 ||
        ((LatAotPcMapV2 *)maps)[0].flags !=
            (LAT_AOT_PC_MAP_DYNAMIC_STATE |
             LAT_AOT_PC_MAP_STACK_POINTER_DELTA)) {
        fprintf(stderr, "stack delta PC map was not preserved\n");
        g_free(maps);
        g_free(text);
        g_free(text_path);
        g_free(metadata_path);
        g_free(image_path);
        return 1;
    }
    g_free(maps);
    char *assembly_path = g_build_filename(directory, "module.S", NULL);
    for (int variant = 1; variant <= 3; variant++) {
        gchar *variant_text = NULL;
        gsize variant_size = 0;
        if (write_fixture(image_path, 0, 0, 0, variant) ||
            lat_aot_v2_emit_module_sources(image_path, directory,
                                           error, sizeof(error)) ||
            !g_file_get_contents(text_path, &variant_text,
                                 &variant_size, NULL) || variant_size != 28) {
            fprintf(stderr, "cannot emit exit variant %d: %s\n",
                    variant, error);
            return 1;
        }
        const uint32_t *words = (const void *)variant_text;
        uint32_t first = variant == 3 ? 0x50001800u :
                         variant == 1 ? 0x18000041u : 0x18000044u;
        uint32_t second = variant == 3 ? 0x03400000u : 0x50001400u;
        if (words[0] != first || words[1] != second) {
            fprintf(stderr, "exit variant %d changed link semantics\n",
                    variant);
            g_free(variant_text);
            return 1;
        }
        g_free(variant_text);
    }
    gchar *assembly = NULL;
    if (!g_file_get_contents(assembly_path, &assembly, NULL, NULL) ||
        !strstr(assembly, "b lat_aot_runtime_log2") ||
        !strstr(assembly, "b lat_aot_runtime_pow") ||
        !strstr(assembly, "b lat_aot_runtime_sincos") ||
        !strstr(assembly, "b lat_aot_runtime_fprem") ||
        !strstr(assembly, "b lat_aot_runtime_fbst_st0") ||
        !strstr(assembly, "b lat_aot_runtime_aesenc_xmm") ||
        !strstr(assembly, "b lat_aot_runtime_aesenclast_xmm") ||
        !strstr(assembly, "b lat_aot_runtime_sha1nexte") ||
        !strstr(assembly, "b lat_aot_runtime_sha256rnds2_xmm0") ||
        !strstr(assembly, "b lat_aot_runtime_raise_int") ||
        !strstr(assembly, "b lat_aot_runtime_xgetbv")) {
        fprintf(stderr, "host math AOT runtime trampolines were not emitted\n");
        g_free(assembly);
        g_free(assembly_path);
        g_free(text);
        g_free(text_path);
        g_free(metadata_path);
        g_free(image_path);
        return 1;
    }
    g_free(assembly);
    gchar *missing_text = NULL;
    gsize missing_text_size = 0;
    if (write_fixture(image_path, 0, 0, 1, 0) ||
        lat_aot_v2_emit_module_sources(image_path, directory,
                                       error, sizeof(error)) ||
        !g_file_get_contents(text_path, &missing_text,
                             &missing_text_size, NULL) ||
        missing_text_size != 24 ||
        (((const uint32_t *)missing_text)[0] & 0xfe00001fu) !=
            0x1e00000cu ||
        (((const uint32_t *)missing_text)[1] & 0xfc0003ffu) !=
            0x4c000184u) {
        fprintf(stderr, "cross-shard fallback TB was not emitted: %s\n",
                error);
        g_free(missing_text);
        g_free(text);
        g_free(assembly_path);
        g_free(text_path);
        g_free(metadata_path);
        g_free(image_path);
        return 1;
    }
    g_free(missing_text);
    if (write_large_guest_table_fixture(
            image_path, LAT_AOT_V2_CONTEXT_GUEST_SLOT_LIMIT + 1, 0) ||
        lat_aot_v2_emit_module_sources(image_path, directory,
                                       error, sizeof(error))) {
        fprintf(stderr, "cannot emit two-level guest table: %s\n", error);
        g_free(text);
        g_free(text_path);
        g_free(metadata_path);
        g_free(image_path);
        return 1;
    }
    gchar *metadata = NULL;
    gchar *slots = NULL;
    gsize slots_size = 0;
    if (!g_file_get_contents(metadata_path, &metadata, NULL, NULL) ||
        !strstr(metadata, "LAT_AOT_MODULE_TWO_LEVEL_GUEST_SLOTS") ||
        strstr(metadata, "LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS") ||
        !g_file_get_contents(slots_path, &slots, &slots_size, NULL) ||
        slots_size != 257 * sizeof(LatAotGuestSlotV2) ||
        ((LatAotGuestSlotV2 *)slots)[256].guest_rva != 0x1800 ||
        ((LatAotGuestSlotV2 *)slots)[256].fp_offset != -16 ||
        ((LatAotGuestSlotV2 *)slots)[256].reserved != 0) {
        fprintf(stderr, "257-entry two-level guest table was not emitted\n");
        g_free(slots);
        g_free(metadata);
        g_free(text);
        g_free(text_path);
        g_free(metadata_path);
        g_free(image_path);
        return 1;
    }
    g_free(slots);
    slots = NULL;
    g_free(metadata);
    metadata = NULL;
    if (write_large_guest_table_fixture(
            image_path, LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT + 1, 0) ||
        lat_aot_v2_emit_module_sources(image_path, directory,
                                       error, sizeof(error)) ||
        !g_file_get_contents(metadata_path, &metadata, NULL, NULL) ||
        !strstr(metadata, "LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS") ||
        strstr(metadata, "LAT_AOT_MODULE_PRECISE_PC_MAP|"
                         "LAT_AOT_MODULE_TWO_LEVEL_GUEST_SLOTS") ||
        !g_file_get_contents(slots_path, &slots, &slots_size, NULL) ||
        slots_size != 65537 * sizeof(LatAotGuestSlotV2) ||
        ((LatAotGuestSlotV2 *)slots)[65536].guest_rva != 0x81000 ||
        ((LatAotGuestSlotV2 *)slots)[65536].fp_offset != -16 ||
        ((LatAotGuestSlotV2 *)slots)[65536].reserved != 0) {
        fprintf(stderr, "65537-entry three-level guest table failed: %s\n",
                error[0] ? error : "invalid metadata");
        g_free(metadata);
        g_free(slots);
        g_free(text);
        g_free(text_path);
        g_free(metadata_path);
        g_free(image_path);
        return 1;
    }
    g_free(slots);
    g_free(metadata);
    gchar *three_assembly = NULL;
    gchar *three_metadata = NULL;
    gchar *three_slots = NULL;
    gchar *three_text = NULL;
    gsize three_slots_size = 0;
    gsize three_text_size = 0;
    const size_t three_dispatch_offset =
        (LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT + 1) *
        3 * sizeof(uint32_t);
    const uint32_t three_base[] = {
        0x28ffe2ccu, 0x28c0018cu, 0x28c0018cu,
    };
    if (write_large_guest_table_fixture(
            image_path, LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT + 1, 1) ||
        lat_aot_v2_emit_module_sources(image_path, directory,
                                       error, sizeof(error)) ||
        !g_file_get_contents(assembly_path, &three_assembly, NULL, NULL) ||
        !g_file_get_contents(metadata_path, &three_metadata, NULL, NULL) ||
        !g_file_get_contents(slots_path, &three_slots,
                             &three_slots_size, NULL) ||
        !g_file_get_contents(text_path, &three_text,
                             &three_text_size, NULL) ||
        !strstr(three_metadata, "LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS") ||
        !strstr(three_assembly, ".Llat_local_targets:\n") ||
        !strstr(three_assembly,
                 ".word 0x28ffe2cc\n"
                 ".word 0x28c0018c\n"
                 ".word 0x28c0018c\n"
                 "sub.d $t0,$r21,$t0\n") ||
        !strstr(three_assembly, "la.pcrel $t1,.Llat_local_targets\n") ||
        !strstr(three_assembly, "alsl.d $t2,$t0,$t1,2\n") ||
        !strstr(three_assembly, "add.d $a7,$a7,$t1\n"
                                "jr $a7\n") ||
        !strstr(three_assembly, "ld.d $t0,$a7,0\n") ||
        !strstr(three_assembly, "ld.d $a7,$a7,8\n") ||
        !strstr(three_assembly, ".rept 17\n") ||
        three_slots_size !=
            (LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT + 1) *
            sizeof(LatAotGuestSlotV2) ||
        three_text_size != three_dispatch_offset +
            sizeof(indirect_dispatch) + sizeof(uint32_t) ||
        memcmp(three_text + three_dispatch_offset,
               three_base, sizeof(three_base))) {
        fprintf(stderr, "three-level local dispatch base failed: %s\n",
                error[0] ? error : "invalid output");
        g_free(three_assembly);
        g_free(three_metadata);
        g_free(three_slots);
        g_free(three_text);
        g_free(text);
        g_free(assembly_path);
        g_free(text_path);
        g_free(metadata_path);
        g_free(image_path);
        return 1;
    }
    g_free(three_assembly);
    g_free(three_metadata);
    g_free(three_slots);
    g_free(three_text);
    gchar *boundary_assembly = NULL;
    gchar *boundary_metadata = NULL;
    gchar *boundary_slots = NULL;
    gchar *boundary_text = NULL;
    gsize boundary_slots_size = 0;
    gsize boundary_text_size = 0;
    const size_t boundary_dispatch_offset =
        LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT * 2 * sizeof(uint32_t);
    const size_t boundary_code_size = boundary_dispatch_offset +
        sizeof(indirect_dispatch) + sizeof(uint32_t);
    if (write_guest_slot_boundary_fixture(image_path) ||
        lat_aot_v2_emit_module_sources(image_path, directory,
                                       error, sizeof(error)) ||
        !g_file_get_contents(assembly_path, &boundary_assembly, NULL, NULL) ||
        !g_file_get_contents(metadata_path, &boundary_metadata, NULL, NULL) ||
        !g_file_get_contents(slots_path, &boundary_slots,
                             &boundary_slots_size, NULL) ||
        !g_file_get_contents(text_path, &boundary_text,
                             &boundary_text_size, NULL) ||
        !strstr(boundary_metadata, "LAT_AOT_MODULE_TWO_LEVEL_GUEST_SLOTS") ||
        strstr(boundary_metadata, "LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS") ||
        strstr(boundary_assembly, ".Llat_local_targets:\n") ||
        boundary_slots_size !=
            LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT *
            sizeof(LatAotGuestSlotV2) ||
        boundary_text_size != boundary_code_size ||
        memcmp(boundary_text + boundary_dispatch_offset,
               indirect_dispatch, sizeof(indirect_dispatch))) {
        fprintf(stderr, "local dispatch changed two-level slot boundary: %s\n",
                error[0] ? error : "invalid output");
        g_free(boundary_assembly);
        g_free(boundary_metadata);
        g_free(boundary_slots);
        g_free(boundary_text);
        g_free(text);
        g_free(assembly_path);
        g_free(text_path);
        g_free(metadata_path);
        g_free(image_path);
        return 1;
    }
    const LatAotGuestSlotV2 *boundary_entries =
        (const void *)boundary_slots;
    if (boundary_entries[0].guest_rva != 0x2000 ||
        boundary_entries[LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT - 1]
            .guest_rva != 0x81ff8 ||
        boundary_entries[LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT - 1]
            .fp_offset != -2048) {
        fprintf(stderr, "two-level slot boundary contents are invalid\n");
        return 1;
    }
    g_free(boundary_assembly);
    g_free(boundary_metadata);
    g_free(boundary_slots);
    g_free(boundary_text);
    const uint32_t *code = (const void *)text;
    if (code[0] != 0x50001800u || code[1] != 0x03400000u ||
        code[2] != 0x03400000u ||
        (code[3] & 0xfc000000u) != 0x50000000u ||
        code[4] == 0x50000000u || code[5] != 0x03400000u) {
        fprintf(stderr, "AOT v2 TB target pair was not direct-linked\n");
        g_free(text);
        g_free(text_path);
        g_free(image_path);
        return 1;
    }
    memset(error, 0, sizeof(error));
    gchar *incomplete_metadata = NULL;
    gchar *incomplete_tbs = NULL;
    gsize incomplete_tbs_size = 0;
    if (write_fixture(image_path, 0, 1, 0, 0) ||
        lat_aot_v2_emit_module_sources(image_path, directory,
                                       error, sizeof(error)) ||
        !g_file_get_contents(metadata_path, &incomplete_metadata,
                             NULL, NULL) ||
        !g_file_get_contents(tbs_path, &incomplete_tbs,
                             &incomplete_tbs_size, NULL) ||
        incomplete_tbs_size != sizeof(LatAotTbV2)) {
        fprintf(stderr, "incomplete PC-map TB was not isolated: %s\n",
                error);
        g_free(incomplete_metadata);
        g_free(incomplete_tbs);
        g_free(text);
        g_free(text_path);
        g_free(metadata_path);
        g_free(image_path);
        return 1;
    }
    g_free(incomplete_tbs);
    g_free(incomplete_metadata);
    memset(error, 0, sizeof(error));
    if (write_fixture(image_path, 1, 0, 0, 0) ||
        !lat_aot_v2_emit_module_sources(image_path, directory,
                                        error, sizeof(error)) ||
        !strstr(error, "overlap")) {
        fprintf(stderr, "overlapping AOT v2 TBs were accepted: %s\n", error);
        g_free(text);
        g_free(text_path);
        g_free(image_path);
        return 1;
    }
    g_free(text);
    if (test_conditional_exits(directory) || test_indirect_exits(directory) ||
        test_return_guards(directory) || test_profiled_layout(directory) ||
        test_profiled_layout_fallback(directory)) {
        return 1;
    }
    g_remove(text_path);
    g_remove(tbs_path);
    g_remove(maps_path);
    g_remove(slots_path);
    g_remove(metadata_path);
    g_remove(assembly_path);
    g_remove(image_path);
    g_rmdir(directory);
    g_free(assembly_path);
    g_free(metadata_path);
    g_free(text_path);
    g_free(tbs_path);
    g_free(maps_path);
    g_free(slots_path);
    g_free(image_path);
    puts("test-aot-v2-module-pack: PASS");
    return 0;
}
