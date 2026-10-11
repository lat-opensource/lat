#include "module-pack.h"

#include "lat-aot-v2.h"
#include "native-image.h"
#include "lat-eflags-link.h"

#include <errno.h>
#include <glib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert((unsigned int)LAT_NATIVE_PC_MAP_DYNAMIC_STATE ==
               (unsigned int)LAT_AOT_PC_MAP_DYNAMIC_STATE,
               "native and AOT PC map flags differ");
_Static_assert((unsigned int)LAT_NATIVE_PC_MAP_STACK_POINTER_DELTA ==
               (unsigned int)LAT_AOT_PC_MAP_STACK_POINTER_DELTA,
               "native and AOT stack delta flags differ");

typedef struct ModulePack {
    const LatNativeImageHeaderV2 *header;
    LatNativeTbV1 *tbs;
    LatNativeRelocationV1 *relocations;
    LatNativePcMapV2 *pc_maps;
    int *relocation_owners;
    int *relocation_targets;
    int *relocation_patch_targets;
    int *pc_map_owners;
    unsigned char *pc_maps_complete;
    int *tb_primary_index;
    int *tb_hash;
    size_t tb_hash_mask;
    unsigned char *code;
    unsigned char *supported;
    bool profiled_layout;
    bool dense_profile;
    bool compact_layout;
    bool preserve_native_layout;
    bool local_dispatch_sentinel;
    GArray *code_order;
    GArray *guest_rvas;
    GHashTable *guest_rva_indexes;
    int three_level_guest_slots;
    uint64_t runtime_trampolines;
    uint64_t pf_table;
    uint64_t local_base;
    uint64_t local_size;
    uint32_t local_base_words;
    unsigned char *local_dispatch;
    unsigned char *local_dispatch_words;
    unsigned char *return_guard_count;
    int (*return_guard_targets)[5];
} ModulePack;

static gint compare_tb_code(gconstpointer left, gconstpointer right)
{
    const LatNativeTbV1 *a = *(const LatNativeTbV1 *const *)left;
    const LatNativeTbV1 *b = *(const LatNativeTbV1 *const *)right;
    if (a->code_offset != b->code_offset) {
        return a->code_offset < b->code_offset ? -1 : 1;
    }
    return 0;
}

static gint compare_pc_map_code(gconstpointer left, gconstpointer right)
{
    const LatNativePcMapV2 *a = *(const LatNativePcMapV2 *const *)left;
    const LatNativePcMapV2 *b = *(const LatNativePcMapV2 *const *)right;
    if (a->host_offset_begin != b->host_offset_begin) {
        return a->host_offset_begin < b->host_offset_begin ? -1 : 1;
    }
    if (a->host_offset_end != b->host_offset_end) {
        return a->host_offset_end < b->host_offset_end ? -1 : 1;
    }
    if (a->guest_pc != b->guest_pc) {
        return a->guest_pc < b->guest_pc ? -1 : 1;
    }
    return 0;
}

static int find_code_tb(const ModulePack *pack, uint64_t code_offset)
{
    guint left = 0;
    guint right = pack->code_order->len;
    while (left < right) {
        guint middle = left + (right - left) / 2;
        const LatNativeTbV1 *tb = g_array_index(
            pack->code_order, const LatNativeTbV1 *, middle);
        if (tb->code_offset <= code_offset) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    if (!left) {
        return -1;
    }
    const LatNativeTbV1 *tb = g_array_index(
        pack->code_order, const LatNativeTbV1 *, left - 1);
    if (code_offset >= tb->code_offset + tb->code_size) {
        return -1;
    }
    return (int)(tb - pack->tbs);
}

static void assign_code_owners(const ModulePack *pack, const void *entries,
                               int *owners, uint64_t count, size_t stride,
                               size_t offset_member)
{
    guint cursor = 0;
    uint64_t previous = 0;
    int monotonic = 1;
    for (uint64_t i = 0; i < count; i++) {
        const uint8_t *entry = (const uint8_t *)entries + i * stride;
        uint64_t offset;
        memcpy(&offset, entry + offset_member, sizeof(offset));
        if (i && offset < previous) {
            monotonic = 0;
        }
        if (!monotonic || !pack->code_order->len) {
            owners[i] = find_code_tb(pack, offset);
            previous = offset;
            continue;
        }
        while (cursor + 1 < pack->code_order->len) {
            const LatNativeTbV1 *next = g_array_index(
                pack->code_order, const LatNativeTbV1 *, cursor + 1);
            if (next->code_offset > offset) {
                break;
            }
            cursor++;
        }
        const LatNativeTbV1 *tb = g_array_index(
            pack->code_order, const LatNativeTbV1 *, cursor);
        owners[i] = tb->code_offset <= offset &&
                    offset < tb->code_offset + tb->code_size ?
                    (int)(tb - pack->tbs) : -1;
        previous = offset;
    }
}

static int pc_map_in_tb(const LatNativePcMapV2 *map,
                        const LatNativeTbV1 *tb)
{
    return map->host_offset_begin >= tb->code_offset &&
           map->host_offset_end <= tb->code_offset + tb->code_size;
}

static int pc_map_range_valid(const ModulePack *pack,
                              const LatNativePcMapV2 *map)
{
    return map->host_offset_begin < map->host_offset_end &&
           map->host_offset_end <= pack->header->code_size;
}

static size_t selected_pc_map_count(const ModulePack *pack)
{
    size_t count = 0;
    for (uint64_t i = 0; i < pack->header->pc_map_count; i++) {
        int owner = pack->pc_map_owners[i];
        if (pack->preserve_native_layout) {
            count += pc_map_range_valid(pack, &pack->pc_maps[i]);
            continue;
        }
        if (owner >= 0 && pack->supported[owner] &&
            pc_map_in_tb(&pack->pc_maps[i], &pack->tbs[owner])) {
            count++;
        }
    }
    return count;
}

static int selected_tb_ranges_valid(const ModulePack *pack)
{
    uint64_t previous_end = 0;
    int have_previous = 0;
    for (guint i = 0; i < pack->code_order->len; i++) {
        const LatNativeTbV1 *tb = g_array_index(
            pack->code_order, const LatNativeTbV1 *, i);
        size_t index = (size_t)(tb - pack->tbs);
        if (!pack->supported[index] || !tb->code_size) {
            continue;
        }
        if (have_previous && tb->code_offset < previous_end) {
            return 0;
        }
        previous_end = tb->code_offset + tb->code_size;
        have_previous = 1;
    }
    return 1;
}

static int all_tb_ranges_valid(const ModulePack *pack)
{
    uint64_t previous_end = 0;
    int have_previous = 0;
    for (guint i = 0; i < pack->code_order->len; i++) {
        const LatNativeTbV1 *tb = g_array_index(
            pack->code_order, const LatNativeTbV1 *, i);
        if (!tb->code_size) {
            continue;
        }
        if (have_previous && tb->code_offset < previous_end) {
            return 0;
        }
        previous_end = tb->code_offset + tb->code_size;
        have_previous = 1;
    }
    return 1;
}

static int tb_pc_maps_complete(const ModulePack *pack, uint64_t index)
{
    return pack->pc_maps_complete[index];
}

static void compute_pc_map_completeness(ModulePack *pack)
{
    uint64_t *expected = g_new(uint64_t, pack->header->tb_count);
    memset(pack->pc_maps_complete, 1, pack->header->tb_count);
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        expected[i] = pack->tbs[i].code_offset;
    }
    for (uint64_t i = 0; i < pack->header->pc_map_count; i++) {
        const LatNativePcMapV2 *map = &pack->pc_maps[i];
        int owner = pack->pc_map_owners[i];
        if (owner < 0) {
            continue;
        }
        const LatNativeTbV1 *tb = &pack->tbs[owner];
        const uint32_t known_flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE |
            LAT_NATIVE_PC_MAP_STACK_POINTER_DELTA;
        const int has_stack_delta = map->flags &
            LAT_NATIVE_PC_MAP_STACK_POINTER_DELTA;
        if (!pc_map_in_tb(map, tb) ||
            map->guest_pc < pack->header->preferred_guest_base ||
            !(map->flags & LAT_NATIVE_PC_MAP_DYNAMIC_STATE) ||
            (map->flags & ~known_flags) ||
            (!!map->state_record_offset != !!has_stack_delta) ||
            map->host_offset_begin != expected[owner]) {
            pack->pc_maps_complete[owner] = 0;
        }
        expected[owner] = map->host_offset_end;
    }
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        if (expected[i] != pack->tbs[i].code_offset +
                           pack->tbs[i].code_size) {
            pack->pc_maps_complete[i] = 0;
        }
    }
    g_free(expected);
}

static int selected_pc_maps_complete(const ModulePack *pack)
{
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        if (pack->supported[i] && !tb_pc_maps_complete(pack, i)) {
            return 0;
        }
    }
    return 1;
}

static int fail(char *error, size_t error_size, const char *format, ...)
{
    if (error && error_size) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return -1;
}

void lat_aot_v2_codegen_digest(const char *build_id, uint8_t digest[32])
{
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, (const guchar *)build_id, strlen(build_id));
    gsize size = 32;
    g_checksum_get_digest(checksum, digest, &size);
    g_checksum_free(checksum);
}

static int write_all(const char *path, const void *data, size_t size,
                     char *error, size_t error_size)
{
    FILE *file = fopen(path, "wb");
    if (!file) {
        return fail(error, error_size, "cannot create %s: %s", path,
                    strerror(errno));
    }
    int result = 0;
    if (size && fwrite(data, size, 1, file) != 1) {
        result = fail(error, error_size, "cannot write %s", path);
    }
    if (fclose(file) && !result) {
        result = fail(error, error_size, "cannot close %s", path);
    }
    return result;
}

static const char *runtime_entry(uint32_t symbol)
{
    switch (symbol) {
    case LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_1:
        return "lat_aot_runtime_epilogue_ret_id_1";
    case LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0:
        return "lat_aot_runtime_epilogue_ret_id_0";
    case LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_1:
        return "lat_aot_runtime_jirl_epilogue_ret_id_1";
    case LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0:
        return "lat_aot_runtime_jirl_epilogue_ret_id_0";
    case LAT_NATIVE_SYMBOL_EPILOGUE_RET_0:
        return "lat_aot_runtime_epilogue_ret_0";
    case LAT_NATIVE_SYMBOL_UPDATE_MXCSR_STATUS:
        return "lat_aot_runtime_update_mxcsr_status";
    case LAT_NATIVE_SYMBOL_FXSAVE:
        return "lat_aot_runtime_fxsave";
    case LAT_NATIVE_SYMBOL_FXRSTOR:
        return "lat_aot_runtime_fxrstor";
    case LAT_NATIVE_SYMBOL_FPREGS_X80_TO_64:
        return "lat_aot_runtime_fpregs_x80_to_64";
    case LAT_NATIVE_SYMBOL_FPREGS_64_TO_X80:
        return "lat_aot_runtime_fpregs_64_to_x80";
    case LAT_NATIVE_SYMBOL_UPDATE_FP_STATUS:
        return "lat_aot_runtime_update_fp_status";
    case LAT_NATIVE_SYMBOL_CPUID:
        return "lat_aot_runtime_cpuid";
    case LAT_NATIVE_SYMBOL_RAISE_ILLOP:
        return "lat_aot_runtime_raise_illop";
    case LAT_NATIVE_SYMBOL_RAISE_GPF:
        return "lat_aot_runtime_raise_gpf";
    case LAT_NATIVE_SYMBOL_RAISE_SYSCALL:
        return LAT_AOT_V2_RUNTIME_SYSCALL_SYMBOL;
    case LAT_NATIVE_SYMBOL_PCMPISTRI_XMM:
        return "lat_aot_runtime_pcmpistri_xmm";
    case LAT_NATIVE_SYMBOL_PCMPISTRM_XMM:
        return "lat_aot_runtime_pcmpistrm_xmm";
    case LAT_NATIVE_SYMBOL_EFLAGTF:
        return "lat_aot_runtime_eflagtf";
    case LAT_NATIVE_SYMBOL_LOG2:
        return "lat_aot_runtime_log2";
    case LAT_NATIVE_SYMBOL_POW:
        return "lat_aot_runtime_pow";
    case LAT_NATIVE_SYMBOL_SIN:
        return "lat_aot_runtime_sin";
    case LAT_NATIVE_SYMBOL_COS:
        return "lat_aot_runtime_cos";
    case LAT_NATIVE_SYMBOL_ATAN2:
        return "lat_aot_runtime_atan2";
    case LAT_NATIVE_SYMBOL_LOGB:
        return "lat_aot_runtime_logb";
    case LAT_NATIVE_SYMBOL_SINCOS:
        return "lat_aot_runtime_sincos";
    case LAT_NATIVE_SYMBOL_FPATAN: return "lat_aot_runtime_fpatan";
    case LAT_NATIVE_SYMBOL_FPTAN: return "lat_aot_runtime_fptan";
    case LAT_NATIVE_SYMBOL_FPREM: return "lat_aot_runtime_fprem";
    case LAT_NATIVE_SYMBOL_FPREM1: return "lat_aot_runtime_fprem1";
    case LAT_NATIVE_SYMBOL_FRNDINT: return "lat_aot_runtime_frndint";
    case LAT_NATIVE_SYMBOL_F2XM1: return "lat_aot_runtime_f2xm1";
    case LAT_NATIVE_SYMBOL_FXTRACT: return "lat_aot_runtime_fxtract";
    case LAT_NATIVE_SYMBOL_FYL2X: return "lat_aot_runtime_fyl2x";
    case LAT_NATIVE_SYMBOL_FYL2XP1: return "lat_aot_runtime_fyl2xp1";
    case LAT_NATIVE_SYMBOL_FSINCOS: return "lat_aot_runtime_fsincos";
    case LAT_NATIVE_SYMBOL_FSIN: return "lat_aot_runtime_fsin";
    case LAT_NATIVE_SYMBOL_FCOS: return "lat_aot_runtime_fcos";
    case LAT_NATIVE_SYMBOL_FBLD_ST0: return "lat_aot_runtime_fbld_st0";
    case LAT_NATIVE_SYMBOL_FBST_ST0: return "lat_aot_runtime_fbst_st0";
    case LAT_NATIVE_SYMBOL_AESIMC_XMM:
        return "lat_aot_runtime_aesimc_xmm";
    case LAT_NATIVE_SYMBOL_AESKEYGENASSIST_XMM:
        return "lat_aot_runtime_aeskeygenassist_xmm";
    case LAT_NATIVE_SYMBOL_AESDEC_XMM:
        return "lat_aot_runtime_aesdec_xmm";
    case LAT_NATIVE_SYMBOL_AESDECLAST_XMM:
        return "lat_aot_runtime_aesdeclast_xmm";
    case LAT_NATIVE_SYMBOL_AESENC_XMM:
        return "lat_aot_runtime_aesenc_xmm";
    case LAT_NATIVE_SYMBOL_AESENCLAST_XMM:
        return "lat_aot_runtime_aesenclast_xmm";
    case LAT_NATIVE_SYMBOL_SHA1NEXTE: return "lat_aot_runtime_sha1nexte";
    case LAT_NATIVE_SYMBOL_SHA1MSG1: return "lat_aot_runtime_sha1msg1";
    case LAT_NATIVE_SYMBOL_SHA1MSG2: return "lat_aot_runtime_sha1msg2";
    case LAT_NATIVE_SYMBOL_SHA256MSG1: return "lat_aot_runtime_sha256msg1";
    case LAT_NATIVE_SYMBOL_SHA256MSG2: return "lat_aot_runtime_sha256msg2";
    case LAT_NATIVE_SYMBOL_SHA1RNDS4_F0:
        return "lat_aot_runtime_sha1rnds4_f0";
    case LAT_NATIVE_SYMBOL_SHA1RNDS4_F1:
        return "lat_aot_runtime_sha1rnds4_f1";
    case LAT_NATIVE_SYMBOL_SHA1RNDS4_F2:
        return "lat_aot_runtime_sha1rnds4_f2";
    case LAT_NATIVE_SYMBOL_SHA1RNDS4_F3:
        return "lat_aot_runtime_sha1rnds4_f3";
    case LAT_NATIVE_SYMBOL_SHA256RNDS2_XMM0:
        return "lat_aot_runtime_sha256rnds2_xmm0";
    case LAT_NATIVE_SYMBOL_RAISE_INT: return "lat_aot_runtime_raise_int";
    case LAT_NATIVE_SYMBOL_RAISE_TRAPOP:
        return "lat_aot_runtime_raise_trapop";
    case LAT_NATIVE_SYMBOL_RAISE_INTO: return "lat_aot_runtime_raise_into";
    case LAT_NATIVE_SYMBOL_RAISE_BOUND: return "lat_aot_runtime_raise_bound";
    case LAT_NATIVE_SYMBOL_XGETBV: return "lat_aot_runtime_xgetbv";
    case LAT_NATIVE_SYMBOL_XSAVE: return "lat_aot_runtime_xsave";
    case LAT_NATIVE_SYMBOL_XSAVEOPT: return "lat_aot_runtime_xsaveopt";
    case LAT_NATIVE_SYMBOL_XRSTOR: return "lat_aot_runtime_xrstor";
    case LAT_NATIVE_SYMBOL_XSETBV: return "lat_aot_runtime_xsetbv";
    case LAT_NATIVE_SYMBOL_KZT_GET_ALTERNATE:
        return "lat_aot_runtime_kzt_get_alternate";
    default:
        return NULL;
    }
}

static int runtime_symbol_supported(uint32_t symbol)
{
    return symbol == LAT_NATIVE_SYMBOL_PFTABLE || runtime_entry(symbol);
}

static int instruction_falls_through(uint32_t instruction)
{
    uint32_t opcode = instruction & 0xfc000000u;
    if (opcode == 0x50000000u) {
        return 0;
    }
    if (opcode == 0x4c000000u && !(instruction & 0x1f)) {
        return 0;
    }
    return 1;
}

static size_t tb_hash_slot(uint64_t guest_pc, uint32_t flags, size_t mask)
{
    uint64_t value = guest_pc ^ ((uint64_t)flags << 32);
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value & mask;
}

static void build_tb_primary_index(ModulePack *pack)
{
    pack->tb_primary_index = g_new(int, pack->header->tb_count);
    int primary = 0;
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        if (i && (pack->tbs[i - 1].guest_pc != pack->tbs[i].guest_pc ||
                  pack->tbs[i - 1].flags != pack->tbs[i].flags)) {
            primary = (int)i;
        }
        pack->tb_primary_index[i] = primary;
    }
}

static void build_tb_hash(ModulePack *pack)
{
    size_t capacity = 1;
    while (capacity < pack->header->tb_count * 2) {
        capacity <<= 1;
    }
    if (!pack->tb_hash) {
        pack->tb_hash = g_new(int, capacity);
    }
    memset(pack->tb_hash, 0xff, capacity * sizeof(*pack->tb_hash));
    pack->tb_hash_mask = capacity - 1;
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        if (pack->tb_primary_index[i] != (int)i) {
            continue;
        }
        const LatNativeTbV1 *tb = &pack->tbs[i];
        size_t slot = tb_hash_slot(tb->guest_pc, tb->flags,
                                   pack->tb_hash_mask);
        while (pack->tb_hash[slot] >= 0) {
            slot = (slot + 1) & pack->tb_hash_mask;
        }
        pack->tb_hash[slot] = (int)i;
    }
}

static void select_supported_primaries(ModulePack *pack)
{
    uint64_t first = 0;
    while (first < pack->header->tb_count) {
        uint64_t end = first + 1;
        int primary = (int)first;
        while (end < pack->header->tb_count &&
               pack->tbs[end].guest_pc == pack->tbs[first].guest_pc &&
               pack->tbs[end].flags == pack->tbs[first].flags) {
            if (!pack->supported[primary] && pack->supported[end]) {
                primary = (int)end;
            }
            end++;
        }
        for (uint64_t i = first; i < end; i++) {
            pack->tb_primary_index[i] = primary;
        }
        first = end;
    }
    build_tb_hash(pack);
}

static int supported_primary(ModulePack *pack, int index)
{
    int primary = pack->tb_primary_index[index];
    if (pack->supported[primary]) {
        return primary;
    }
    uint64_t first = index;
    while (first && pack->tbs[first - 1].guest_pc == pack->tbs[index].guest_pc &&
           pack->tbs[first - 1].flags == pack->tbs[index].flags) {
        first--;
    }
    uint64_t end = first;
    while (end < pack->header->tb_count &&
           pack->tbs[end].guest_pc == pack->tbs[index].guest_pc &&
           pack->tbs[end].flags == pack->tbs[index].flags) {
        if (!pack->supported[primary] && pack->supported[end]) {
            primary = (int)end;
        }
        end++;
    }
    for (uint64_t i = first; i < end; i++) {
        pack->tb_primary_index[i] = primary;
    }
    return primary;
}

static int find_tb(const ModulePack *pack, uint64_t guest_pc, uint32_t flags)
{
    size_t slot = tb_hash_slot(guest_pc, flags, pack->tb_hash_mask);
    while (pack->tb_hash[slot] >= 0) {
        int index = pack->tb_hash[slot];
        if (pack->tbs[index].guest_pc == guest_pc &&
            pack->tbs[index].flags == flags) {
            return index;
        }
        slot = (slot + 1) & pack->tb_hash_mask;
    }
    uint64_t left = 0;
    uint64_t right = pack->header->tb_count;
    while (left < right) {
        uint64_t middle = left + (right - left) / 2;
        if (pack->tbs[middle].guest_pc < guest_pc) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return left < pack->header->tb_count &&
           pack->tbs[left].guest_pc == guest_pc &&
           (left + 1 == pack->header->tb_count ||
            pack->tbs[left + 1].guest_pc != guest_pc) ? (int)left : -1;
}

typedef struct LayoutComponent {
    GArray *members;
    uint64_t guest_pc;
    uint64_t original_offset;
    int loop_anchor;
    bool observed;
    bool observed_in_loop;
} LayoutComponent;

#define PROFILED_COMPONENT_ALIGNMENT 32u
#define PROFILED_LOOP_ALIGNMENT 32u
#define DENSE_PROFILE_TB_DIVISOR 10u
#define COMPACT_LAYOUT_SPLIT_PER_MILLE 0u

static bool cfg_layout_tb_in_loop(const CfgProgram *program, size_t tb_index);

static int layout_find(int *parents, int index)
{
    int root = index;
    while (parents[root] != root) {
        root = parents[root];
    }
    while (parents[index] != index) {
        int next = parents[index];
        parents[index] = root;
        index = next;
    }
    return root;
}

static void layout_union(int *parents, int left, int right)
{
    left = layout_find(parents, left);
    right = layout_find(parents, right);
    if (left != right) {
        parents[right] = left;
    }
}

static size_t layout_component_count(int *parents, uint64_t tb_count)
{
    size_t count = 0;

    for (uint64_t i = 0; i < tb_count; i++) {
        count += layout_find(parents, (int)i) == (int)i;
    }
    return count;
}

static int instruction_has_short_branch_range(uint32_t instruction)
{
    uint32_t opcode = instruction & 0xfc000000u;
    uint32_t opcode20 = instruction & 0xfc000100u;
    return (opcode >= 0x58000000u && opcode <= 0x6c000000u) ||
           opcode == 0x40000000u || opcode == 0x44000000u ||
           opcode20 == 0x48000000u || opcode20 == 0x48000100u;
}

static gint compare_layout_component(gconstpointer left, gconstpointer right)
{
    const LayoutComponent *a = left;
    const LayoutComponent *b = right;
    if (a->observed_in_loop != b->observed_in_loop) {
        return a->observed_in_loop ? -1 : 1;
    }
    if (a->observed != b->observed) {
        return a->observed ? -1 : 1;
    }
    if (a->guest_pc != b->guest_pc) {
        return a->guest_pc < b->guest_pc ? -1 : 1;
    }
    if (a->original_offset != b->original_offset) {
        return a->original_offset < b->original_offset ? -1 : 1;
    }
    return 0;
}

static void reorder_profiled_code(ModulePack *pack, const CfgProgram *program)
{
    if (!program || !program->tb_count || !pack->header->tb_count) {
        return;
    }
    /* Embedded constants retain their native offsets, including after merges. */
    for (uint64_t i = 0; i < pack->header->relocation_count; i++) {
        if (pack->relocations[i].kind == LAT_NATIVE_RELOC_IMAGE_DATA) {
            return;
        }
    }
    uint64_t tb_count = pack->header->tb_count;
    unsigned char *observed = g_malloc0(tb_count);
    unsigned char *observed_in_loop = g_malloc0(tb_count);
    size_t observed_count = 0;
    const uint32_t semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL;
    for (size_t i = 0; i < program->tb_count; i++) {
        const CfgTb *cfg_tb = &program->tbs[i];
        if (!cfg_tb->observed) {
            continue;
        }
        uint32_t flags = cfg_tb->semantic_flags & semantic_flags;
        int index = find_tb(pack, cfg_tb->start, flags);
        if (index >= 0 && pack->tbs[index].guest_pc == cfg_tb->start &&
            pack->tbs[index].flags == flags) {
            if (!observed[index]) {
                observed[index] = 1;
                observed_count++;
            }
            observed_in_loop[index] |= cfg_layout_tb_in_loop(program, i);
        }
    }
    if (!observed_count) {
        g_free(observed_in_loop);
        g_free(observed);
        return;
    }
    pack->dense_profile = tb_count >= 128 &&
        observed_count >= (tb_count + DENSE_PROFILE_TB_DIVISOR - 1) /
                          DENSE_PROFILE_TB_DIVISOR;
    size_t loop_count = 0;
    for (uint64_t i = 0; i < tb_count; i++) {
        loop_count += observed_in_loop[i] != 0;
    }
    /*
     * If nearly every observed TB is already part of a loop, the original
     * layout already has the useful locality. Global profile reordering can
     * split fall-through regions in this case and regress hot code.
     */
    if (pack->dense_profile && loop_count * 4 > observed_count * 3) {
        g_free(observed_in_loop);
        g_free(observed);
        return;
    }
    int *parents = g_new(int, tb_count);
    int *component_indexes = g_new(int, tb_count);
    uint64_t *old_offsets = g_new(uint64_t, tb_count);
    uint64_t *new_offsets = g_new(uint64_t, tb_count);
    for (uint64_t i = 0; i < tb_count; i++) {
        parents[i] = (int)i;
        component_indexes[i] = -1;
        old_offsets[i] = pack->tbs[i].code_offset;
    }
    for (guint i = 1; i < pack->code_order->len; i++) {
        const LatNativeTbV1 *left = g_array_index(
            pack->code_order, const LatNativeTbV1 *, i - 1);
        const LatNativeTbV1 *right = g_array_index(
            pack->code_order, const LatNativeTbV1 *, i);
        uint32_t last_instruction = 0;
        int falls_through = left->code_size < sizeof(last_instruction) ||
            left->code_offset > pack->header->code_size ||
            left->code_size > pack->header->code_size - left->code_offset;
        if (!falls_through) {
            memcpy(&last_instruction, pack->code + left->code_offset +
                   left->code_size - sizeof(last_instruction),
                   sizeof(last_instruction));
            falls_through = instruction_falls_through(last_instruction);
        }
        if (right->code_offset == left->code_offset + left->code_size &&
            falls_through) {
            layout_union(parents, (int)(left - pack->tbs),
                         (int)(right - pack->tbs));
        }
    }
    for (uint64_t i = 0; i < pack->header->relocation_count; i++) {
        int owner = pack->relocation_owners[i];
        int target = pack->relocation_targets[i];
        const LatNativeRelocationV1 *relocation = &pack->relocations[i];
        if (owner < 0 || target < 0 ||
            (relocation->kind != LAT_NATIVE_RELOC_TB_TARGET &&
             relocation->kind != LAT_NATIVE_RELOC_JRRA_TARGET) ||
            pack->header->code_size < 4 ||
            relocation->code_offset > pack->header->code_size - 4) {
            continue;
        }
        uint32_t instruction;
        memcpy(&instruction, pack->code + relocation->code_offset,
               sizeof(instruction));
        if (instruction_has_short_branch_range(instruction)) {
            layout_union(parents, owner, target);
        }
    }

    if (pack->dense_profile) {
        int *compact_parents = g_new(int, tb_count);
        memcpy(compact_parents, parents,
               tb_count * sizeof(*compact_parents));
        for (guint i = 1; i < pack->code_order->len; i++) {
            const LatNativeTbV1 *left = g_array_index(
                pack->code_order, const LatNativeTbV1 *, i - 1);
            const LatNativeTbV1 *right = g_array_index(
                pack->code_order, const LatNativeTbV1 *, i);
            if (right->code_offset == left->code_offset + left->code_size) {
                layout_union(compact_parents, (int)(left - pack->tbs),
                             (int)(right - pack->tbs));
            }
        }
        size_t sparse_count = layout_component_count(parents, tb_count);
        size_t compact_count = layout_component_count(compact_parents,
                                                       tb_count);
        pack->compact_layout =
            (sparse_count - compact_count) * 1000 <=
            compact_count * COMPACT_LAYOUT_SPLIT_PER_MILLE;
        if (pack->compact_layout) {
            memcpy(parents, compact_parents,
                   tb_count * sizeof(*compact_parents));
        }
        g_free(compact_parents);
    }

    GArray *components = g_array_new(FALSE, FALSE,
                                     sizeof(LayoutComponent));
    for (guint i = 0; i < pack->code_order->len; i++) {
        const LatNativeTbV1 *tb = g_array_index(
            pack->code_order, const LatNativeTbV1 *, i);
        int tb_index = (int)(tb - pack->tbs);
        int root = layout_find(parents, tb_index);
        int component_index = component_indexes[root];
        if (component_index < 0) {
            LayoutComponent component = {
                .members = g_array_new(FALSE, FALSE, sizeof(int)),
                .guest_pc = UINT64_MAX,
                .original_offset = tb->code_offset,
                .loop_anchor = -1,
            };
            g_array_append_val(components, component);
            component_index = (int)components->len - 1;
            component_indexes[root] = component_index;
        }
        LayoutComponent *component = &g_array_index(
            components, LayoutComponent, component_index);
        g_array_append_val(component->members, tb_index);
        component->observed |= observed[tb_index] != 0;
        component->observed_in_loop |= observed_in_loop[tb_index] != 0;
        if (observed_in_loop[tb_index] && component->loop_anchor < 0) {
            component->loop_anchor = tb_index;
        }
        component->guest_pc = MIN(component->guest_pc, tb->guest_pc);
        component->original_offset = MIN(component->original_offset,
                                         tb->code_offset);
    }
    g_array_sort(components, compare_layout_component);

    uint64_t cursor = 0;
    int layout_valid = 1;
    for (guint i = 0; i < components->len && layout_valid; i++) {
        LayoutComponent *component = &g_array_index(
            components, LayoutComponent, i);
        if (component->loop_anchor >= 0) {
            uint64_t relative = 0;
            uint64_t previous_end = 0;
            for (guint j = 0; j < component->members->len; j++) {
                int index = g_array_index(component->members, int, j);
                const LatNativeTbV1 *tb = &pack->tbs[index];
                if (j && tb->code_offset != previous_end) {
                    relative += 4;
                }
                if (index == component->loop_anchor) {
                    break;
                }
                relative += tb->code_size;
                previous_end = tb->code_offset + tb->code_size;
            }
            uint64_t padding = (-(cursor + relative)) &
                (PROFILED_LOOP_ALIGNMENT - 1);
            if (padding > pack->header->code_size - cursor) {
                layout_valid = 0;
                break;
            }
            cursor += padding;
        }
        uint64_t previous_end = 0;
        for (guint j = 0; j < component->members->len; j++) {
            int index = g_array_index(component->members, int, j);
            const LatNativeTbV1 *tb = &pack->tbs[index];
            if (j && tb->code_offset != previous_end) {
                if (cursor > pack->header->code_size ||
                    pack->header->code_size - cursor < 4) {
                    layout_valid = 0;
                    break;
                }
                cursor += 4;
            }
            if (tb->code_size > pack->header->code_size - cursor) {
                layout_valid = 0;
                break;
            }
            new_offsets[index] = cursor;
            cursor += tb->code_size;
            previous_end = tb->code_offset + tb->code_size;
        }
        if (layout_valid) {
            if (cursor > pack->header->code_size ||
                pack->header->code_size - cursor < 4) {
                layout_valid = 0;
            } else {
                cursor += 4;
                uint64_t alignment = pack->compact_layout ? 4 :
                    PROFILED_COMPONENT_ALIGNMENT;
                uint64_t padding = (-cursor) & (alignment - 1);
                if (padding > pack->header->code_size - cursor) {
                    layout_valid = 0;
                } else {
                    cursor += padding;
                }
            }
        }
    }
    if (!layout_valid) {
        for (guint i = 0; i < components->len; i++) {
            LayoutComponent *component = &g_array_index(
                components, LayoutComponent, i);
            g_array_free(component->members, TRUE);
        }
        g_array_free(components, TRUE);
        g_free(new_offsets);
        g_free(old_offsets);
        g_free(component_indexes);
        g_free(parents);
        g_free(observed_in_loop);
        g_free(observed);
        pack->compact_layout = false;
        return;
    }

    unsigned char *old_code = pack->code;
    unsigned char *new_code = g_malloc0(pack->header->code_size);
    for (uint64_t i = 0; i < tb_count; i++) {
        memcpy(new_code + new_offsets[i], old_code + old_offsets[i],
               pack->tbs[i].code_size);
        pack->tbs[i].code_offset = new_offsets[i];
    }
    pack->code = new_code;
    pack->profiled_layout = true;
    g_free(old_code);
    for (uint64_t i = 0; i < pack->header->relocation_count; i++) {
        int owner = pack->relocation_owners[i];
        if (owner >= 0) {
            int64_t delta = (int64_t)new_offsets[owner] -
                            (int64_t)old_offsets[owner];
            pack->relocations[i].code_offset = (uint64_t)(
                (int64_t)pack->relocations[i].code_offset + delta);
        }
    }
    for (uint64_t i = 0; i < pack->header->pc_map_count; i++) {
        int owner = pack->pc_map_owners[i];
        if (owner >= 0) {
            int64_t delta = (int64_t)new_offsets[owner] -
                            (int64_t)old_offsets[owner];
            pack->pc_maps[i].host_offset_begin = (uint64_t)(
                (int64_t)pack->pc_maps[i].host_offset_begin + delta);
            pack->pc_maps[i].host_offset_end = (uint64_t)(
                (int64_t)pack->pc_maps[i].host_offset_end + delta);
        }
    }
    g_array_sort(pack->code_order, compare_tb_code);

    for (guint i = 0; i < components->len; i++) {
        LayoutComponent *component = &g_array_index(
            components, LayoutComponent, i);
        g_array_free(component->members, TRUE);
    }
    g_array_free(components, TRUE);
    g_free(new_offsets);
    g_free(old_offsets);
    g_free(component_indexes);
    g_free(parents);
    g_free(observed_in_loop);
    g_free(observed);
    return;
}

static int guest_slot(ModulePack *pack, uint64_t guest_rva);
static int patch_address(uint32_t *instructions, uint32_t slots,
                         uint64_t patch, uint64_t target);
static int patch_tb_target_pair(uint32_t *instructions,
                                uint64_t patch, uint64_t target,
                                uint32_t exit_symbol);
static int patch_runtime_target(uint32_t *instructions, uint32_t slots,
                                uint64_t patch, uint64_t target);

static void configure_local_dispatch(ModulePack *pack)
{
    uint64_t first = UINT64_MAX, last = 0;
    int have_exit = 0;
    if (pack->header->code_size > (64u << 20) ||
        pack->header->tb_count > (1u << 20) ||
        pack->header->pc_map_count > (1u << 20)) {
        return;
    }
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        const LatNativeTbV1 *tb = &pack->tbs[i];
        if (tb->flags != (LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL) ||
            tb->guest_pc < pack->header->preferred_guest_base) {
            continue;
        }
        first = MIN(first, tb->guest_pc);
        last = MAX(last, tb->guest_pc);
        have_exit |= tb->indirect_exit_offset != 0;
    }
    if (!have_exit || first == UINT64_MAX || last >> 48) {
        return;
    }
    first = MAX(first & ~(uint64_t)4095,
                pack->header->preferred_guest_base);
    uint64_t end = (last + 4096) & ~(uint64_t)4095;
    uint64_t size = (end - first + 4095) & ~(uint64_t)4095;
    if (size <= (4u << 20)) {
        pack->local_base = first;
        pack->local_size = size;
    }
}

static void select_supported_tbs(ModulePack *pack)
{
    memset(pack->supported, 1, pack->header->tb_count);
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        if (pack->tbs[i].guest_pc < pack->header->preferred_guest_base ||
            !tb_pc_maps_complete(pack, i)) {
            pack->supported[i] = 0;
        }
    }
    for (uint64_t i = 0; i < pack->header->relocation_count; i++) {
        const LatNativeRelocationV1 *relocation = &pack->relocations[i];
        int owner = pack->relocation_owners[i];
        if (owner < 0) {
            continue;
        }
        if (relocation->kind == LAT_NATIVE_RELOC_RUNTIME_SYMBOL &&
            !runtime_symbol_supported(relocation->target)) {
            pack->supported[owner] = 0;
            continue;
        }
        if (relocation->kind == LAT_NATIVE_RELOC_RUNTIME_SYMBOL) {
            if (!relocation->slots || relocation->slots > 3) {
                pack->supported[owner] = 0;
                continue;
            }
            uint32_t instructions[3] = {0};
            memcpy(instructions, pack->code + relocation->code_offset,
                   relocation->slots * sizeof(*instructions));
            uint64_t target = relocation->target == LAT_NATIVE_SYMBOL_PFTABLE ?
                pack->pf_table : pack->runtime_trampolines +
                relocation->target * sizeof(uint32_t);
            if (patch_runtime_target(instructions, relocation->slots,
                                     relocation->code_offset, target)) {
                pack->supported[owner] = 0;
            }
        }
        if (relocation->kind == LAT_NATIVE_RELOC_IMAGE_DATA) {
            uint32_t instructions[3];
            memcpy(instructions, pack->code + relocation->code_offset,
                   relocation->slots * sizeof(*instructions));
            if (patch_address(instructions, relocation->slots,
                              relocation->code_offset, relocation->addend)) {
                pack->supported[owner] = 0;
            }
        }
    }
    int changed;
    do {
        changed = 0;
        /* Logical keys can use a complete delta without discarding physical
         * aliases referenced by relocations in the base native image. */
        select_supported_primaries(pack);
        for (uint64_t i = 0; i < pack->header->relocation_count; i++) {
            int target = pack->relocation_targets[i];
            if (target >= 0) {
                pack->relocation_targets[i] = pack->tb_primary_index[target];
            }
        }
        g_array_set_size(pack->guest_rvas, 0);
        g_hash_table_remove_all(pack->guest_rva_indexes);
        pack->three_level_guest_slots = 0;
        if (pack->header->flags & LAT_NATIVE_IMAGE_PIE) {
            for (uint64_t i = 0; i < pack->header->relocation_count; i++) {
                const LatNativeRelocationV1 *relocation =
                    &pack->relocations[i];
                if (relocation->kind != LAT_NATIVE_RELOC_GUEST_ADDRESS) {
                    continue;
                }
                int owner = pack->relocation_owners[i];
                if (owner < 0 || !pack->supported[owner]) {
                    continue;
                }
                if ((uint64_t)relocation->addend <
                        pack->header->preferred_guest_base ||
                    guest_slot(pack, (uint64_t)relocation->addend -
                               pack->header->preferred_guest_base) < 0) {
                    pack->supported[owner] = 0;
                    changed = 1;
                }
            }
            if (pack->local_size) {
                uint64_t local_rva = pack->local_base -
                                     pack->header->preferred_guest_base;
                gpointer existing = g_hash_table_lookup(
                    pack->guest_rva_indexes, &local_rva);
                if (!existing && pack->guest_rvas->len ==
                        LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT) {
                    pack->local_size = 0;
                } else if (guest_slot(pack, local_rva) < 0) {
                    pack->local_size = 0;
                }
            }
            pack->three_level_guest_slots =
                pack->guest_rvas->len >
                LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT;
            if (pack->three_level_guest_slots) {
                for (uint64_t i = 0;
                     i < pack->header->relocation_count; i++) {
                    const LatNativeRelocationV1 *relocation =
                        &pack->relocations[i];
                    if (relocation->kind != LAT_NATIVE_RELOC_GUEST_ADDRESS ||
                        relocation->slots >= 3) {
                        continue;
                    }
                    int owner = pack->relocation_owners[i];
                    if (owner >= 0 && pack->supported[owner]) {
                        pack->supported[owner] = 0;
                        changed = 1;
                    }
                }
            }
        }
        for (uint64_t i = 0; i < pack->header->relocation_count; i++) {
            const LatNativeRelocationV1 *relocation = &pack->relocations[i];
            if (relocation->kind != LAT_NATIVE_RELOC_TB_TARGET) {
                continue;
            }
            int owner = pack->relocation_owners[i];
            if (owner < 0 || !pack->supported[owner]) {
                continue;
            }
            int target = pack->relocation_targets[i];
            if (target >= 0) {
                target = supported_primary(pack, target);
                pack->relocation_targets[i] = target;
            }
            if (target >= 0 && !pack->supported[target]) {
                pack->supported[owner] = 0;
                changed = 1;
                continue;
            }
            if (!relocation->slots || relocation->slots > 3) {
                pack->supported[owner] = 0;
                changed = 1;
                continue;
            }
            int candidates[2];
            size_t candidate_count = 0;
            if (target >= 0 && pack->supported[target]) {
                candidates[candidate_count++] = target;
            }
            uint32_t physical = relocation->physical_target_plus_one;
            if (physical && pack->supported[physical - 1] &&
                (physical - 1) != (uint32_t)target) {
                candidates[candidate_count++] = (int)(physical - 1);
            }
            int patchable = 0;
            for (size_t candidate_index = 0;
                 candidate_index < candidate_count; candidate_index++) {
                int candidate = candidates[candidate_index];
                uint32_t instructions[3] = {0};
                memcpy(instructions, pack->code + relocation->code_offset,
                       relocation->slots * sizeof(*instructions));
                bool candidate_patchable = relocation->slots == 2 ?
                    !patch_tb_target_pair(instructions,
                        relocation->code_offset,
                        pack->tbs[candidate].code_offset,
                        relocation->reserved) :
                    !patch_runtime_target(instructions, relocation->slots,
                        relocation->code_offset,
                        pack->tbs[candidate].code_offset);
                if (candidate_patchable) {
                    pack->relocation_patch_targets[i] = candidate;
                    patchable = 1;
                    break;
                }
            }
            if (!candidate_count && target < 0) {
                uint32_t instructions[3] = {0};
                memcpy(instructions, pack->code + relocation->code_offset,
                       relocation->slots * sizeof(*instructions));
                patchable = runtime_entry(relocation->reserved) &&
                    !patch_runtime_target(
                        instructions, relocation->slots,
                        relocation->code_offset,
                        pack->runtime_trampolines + relocation->reserved * 4);
            }
            if (!patchable) {
                pack->supported[owner] = 0;
                changed = 1;
            }
        }
        for (guint i = 0; i + 1 < pack->code_order->len; i++) {
            const LatNativeTbV1 *owner = g_array_index(
                pack->code_order, const LatNativeTbV1 *, i);
            size_t owner_index = (size_t)(owner - pack->tbs);
            if (!owner->code_size || !pack->supported[owner_index]) {
                continue;
            }
            uint64_t end = owner->code_offset + owner->code_size;
            const LatNativeTbV1 *next = NULL;
            for (guint j = i + 1; j < pack->code_order->len; j++) {
                const LatNativeTbV1 *candidate = g_array_index(
                    pack->code_order, const LatNativeTbV1 *, j);
                if (candidate->code_offset > end) {
                    break;
                }
                if (candidate->code_offset == end && candidate->code_size) {
                    next = candidate;
                    break;
                }
            }
            uint32_t last_instruction;
            memcpy(&last_instruction,
                   pack->code + end - sizeof(last_instruction),
                   sizeof(last_instruction));
            if (next && instruction_falls_through(last_instruction) &&
                !pack->supported[next - pack->tbs]) {
                pack->supported[owner_index] = 0;
                changed = 1;
            }
        }
    } while (changed);
}

static int patch_branch(uint32_t *instruction, uint64_t patch, uint64_t target)
{
    int64_t difference = (int64_t)target - (int64_t)patch;
    if (difference & 3) {
        return -1;
    }
    int64_t offset = difference >> 2;
    uint32_t opcode = *instruction & 0xfc000000u;
    if (opcode == 0x50000000u || opcode == 0x54000000u) {
        if (offset < -(1 << 25) || offset >= (1 << 25)) {
            return -1;
        }
        *instruction = opcode | ((uint32_t)offset & 0xffffu) << 10 |
                       (((uint32_t)offset >> 16) & 0x3ffu);
        return 0;
    }
    uint32_t opcode20 = *instruction & 0xfc000100u;
    if (opcode == 0x40000000u || opcode == 0x44000000u ||
        opcode20 == 0x48000000u || opcode20 == 0x48000100u) {
        if (offset < -(1 << 19) || offset >= (1 << 19)) {
            return -1;
        }
        *instruction = (*instruction & 0xfc0003e0u) |
                       ((uint32_t)offset & 0xffffu) << 10 |
                       (((uint32_t)offset >> 16) & 0x1fu);
        return 0;
    }
    if (opcode >= 0x58000000u && opcode <= 0x6c000000u) {
        if (offset < -(1 << 15) || offset >= (1 << 15)) {
            return -1;
        }
        *instruction = (*instruction & 0xfc0003ffu) |
                       ((uint32_t)offset & 0xffffu) << 10;
        return 0;
    }
    return -1;
}

static int patch_address(uint32_t *instructions, uint32_t slots,
                         uint64_t patch, uint64_t target)
{
    if (slots == 2 &&
        (instructions[0] & 0xfe000000u) == 0x1e000000u &&
        (instructions[1] & 0xfc000000u) == 0x4c000000u) {
        int64_t difference = (int64_t)target - (int64_t)patch;
        if (difference & 3) {
            return -1;
        }
        int64_t offset = difference >> 2;
        uint32_t base = instructions[0] & 0x1f;
        uint32_t destination = instructions[1] & 0x1f;
        uint32_t upper = (uint32_t)((offset + (1 << 15)) >> 16) & 0xfffff;
        uint32_t lower = (uint32_t)offset & 0xffff;
        instructions[0] = 0x1e000000u | upper << 5 | base;
        instructions[1] = 0x4c000000u | lower << 10 |
                          base << 5 | destination;
        return 0;
    }
    if (slots < 2 || slots > 3 ||
        (instructions[0] & 0xfe000000u) != 0x14000000u ||
        (instructions[1] & 0xffc00000u) != 0x03800000u) {
        return -1;
    }
    int64_t pages = ((int64_t)(target & ~(uint64_t)0xfff) -
                     (int64_t)(patch & ~(uint64_t)0xfff)) >> 12;
    if (pages < -(1 << 19) || pages >= (1 << 19)) {
        return -1;
    }
    uint32_t destination = instructions[0] & 0x1f;
    instructions[0] = 0x1a000000u |
        ((uint32_t)pages & 0xfffff) << 5 | destination;
    instructions[1] = 0x03800000u | ((uint32_t)target & 0xfff) << 10 |
        destination << 5 | destination;
    if (slots == 3) {
        instructions[2] = 0x03400000u;
    }
    return 0;
}

static int patch_tb_target_pair(uint32_t *instructions,
                                uint64_t patch, uint64_t target,
                                uint32_t exit_symbol)
{
    if ((instructions[0] & 0xfe00001fu) == 0x1e00000cu &&
        (instructions[1] & 0xfc0003e0u) == 0x4c000180u) {
        uint32_t destination = instructions[1] & 0x1fu;
        /* A linked local exit does not return through the runtime epilogue. */
        if (destination == 4 &&
            (exit_symbol == LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0 ||
             exit_symbol == LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_1)) {
            int64_t difference = (int64_t)target - (int64_t)patch;
            int64_t offset = difference >> 2;
            if (!(difference & 3) && offset >= -(1 << 25) &&
                offset < (1 << 25)) {
                instructions[0] = 0x50000000u |
                    ((uint32_t)offset & 0xffffu) << 10 |
                    (((uint32_t)offset >> 16) & 0x3ffu);
                instructions[1] = 0x03400000u;
                return 0;
            }
        }
        int64_t difference = (int64_t)target - (int64_t)(patch + 4);
        int64_t offset = difference >> 2;
        if (!(difference & 3) && offset >= -(1 << 25) &&
            offset < (1 << 25)) {
            instructions[0] = destination ?
                0x18000040u | destination : 0x03400000u;
            instructions[1] = 0x50000000u |
                ((uint32_t)offset & 0xffffu) << 10 |
                (((uint32_t)offset >> 16) & 0x3ffu);
            return 0;
        }
    }
    int64_t difference = (int64_t)target - (int64_t)patch;
    if (!(difference & 3)) {
        int64_t offset = difference >> 2;
        if (offset >= -(1 << 25) && offset < (1 << 25)) {
            instructions[0] = 0x50000000u |
                ((uint32_t)offset & 0xffffu) << 10 |
                (((uint32_t)offset >> 16) & 0x3ffu);
            instructions[1] = 0x03400000u;
            return 0;
        }
        int64_t upper = ((offset + (1 << 15)) >> 16) & 0xfffff;
        int64_t lower = offset & 0xffff;
        instructions[0] = 0x1e000000u | (uint32_t)upper << 5 | 12u;
        instructions[1] = 0x4c000000u | (uint32_t)lower << 10 | 12u << 5;
        return 0;
    }
    return -1;
}

static int patch_jrra_target(uint32_t *instructions, uint32_t slots,
                             uint64_t patch, uint64_t target, int enabled)
{
    if (slots != 4) {
        return -1;
    }
    if (!enabled) {
        for (uint32_t i = 0; i < slots; i++) {
            instructions[i] = 0x03400000u;
        }
        return 0;
    }
    int64_t pages = ((int64_t)(target & ~(uint64_t)0xfff) -
                     (int64_t)(patch & ~(uint64_t)0xfff)) >> 12;
    if (pages < -(1 << 19) || pages >= (1 << 19)) {
        return -1;
    }
    uint32_t destination = instructions[0] & 0x1f;
    instructions[0] = 0x1a000000u |
        ((uint32_t)pages & 0xfffff) << 5 | destination;
    instructions[1] = 0x03800000u | ((uint32_t)target & 0xfff) << 10 |
        destination << 5 | destination;
    return 0;
}

static int patch_runtime_target(uint32_t *instructions, uint32_t slots,
                                uint64_t patch, uint64_t target)
{
    if ((instructions[0] & 0xfc000000u) == 0x50000000u ||
        (instructions[0] & 0xfc000000u) == 0x54000000u ||
        (instructions[0] & 0xfc000000u) == 0x40000000u ||
        (instructions[0] & 0xfc000000u) == 0x44000000u ||
        ((instructions[0] & 0xfc000000u) == 0x48000000u &&
         ((instructions[0] >> 8) & 3u) <= 1u) ||
        ((instructions[0] & 0xfc000000u) >= 0x58000000u &&
         (instructions[0] & 0xfc000000u) <= 0x6c000000u)) {
        return patch_branch(instructions, patch, target);
    }
    return patch_address(instructions, slots, patch, target);
}

static int guest_slot(ModulePack *pack, uint64_t guest_rva)
{
    gpointer found = g_hash_table_lookup(pack->guest_rva_indexes, &guest_rva);
    if (found) {
        return (int)GPOINTER_TO_UINT(found) - 1;
    }
    if (pack->guest_rvas->len >= LAT_AOT_V2_GUEST_ADDRESS_LIMIT) {
        return -1;
    }
    uint64_t *key = g_new(uint64_t, 1);
    *key = guest_rva;
    g_array_append_val(pack->guest_rvas, guest_rva);
    guint index = pack->guest_rvas->len - 1;
    g_hash_table_insert(pack->guest_rva_indexes, key,
                        GUINT_TO_POINTER(index + 1));
    return (int)index;
}

static int patch_absolute_guest_address(uint32_t *instructions,
                                        uint32_t slots, uint64_t target)
{
    if (slots < 2 || slots > 3 ||
        (instructions[0] & 0xfe000000u) != 0x14000000u ||
        (instructions[1] & 0xffc00000u) != 0x03800000u ||
        (slots == 3 &&
         (instructions[2] & 0xfe000000u) != 0x16000000u) ||
        (slots == 2 && target >> 32)) {
        return -1;
    }
    uint32_t destination = instructions[0] & 0x1f;
    instructions[0] = 0x14000000u |
                      ((uint32_t)(target >> 12) & 0xfffff) << 5 |
                      destination;
    instructions[1] = 0x03800000u | ((uint32_t)target & 0xfff) << 10 |
                      destination << 5 | destination;
    if (slots == 3) {
        instructions[2] = 0x16000000u |
                          ((uint32_t)(target >> 32) & 0xfffff) << 5 |
                          destination;
    }
    return 0;
}

static int patch_guest_address(ModulePack *pack,
                               const LatNativeRelocationV1 *relocation)
{
    if ((uint64_t)relocation->addend < pack->header->preferred_guest_base) {
        return -1;
    }
    uint64_t target = (uint64_t)relocation->addend;
    uint32_t *instructions = (void *)(pack->code + relocation->code_offset);
    if (!(pack->header->flags & LAT_NATIVE_IMAGE_PIE)) {
        return patch_absolute_guest_address(instructions, relocation->slots,
                                            target);
    }
    uint64_t guest_rva = target - pack->header->preferred_guest_base;
    int slot = guest_slot(pack, guest_rva);
    if (slot < 0 || relocation->slots < 2 || relocation->slots > 3 ||
        (pack->three_level_guest_slots && relocation->slots < 3)) {
        return -1;
    }
    uint32_t destination = instructions[0] & 0x1f;
    uint32_t page = (uint32_t)slot / LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    uint32_t entry = (uint32_t)slot % LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    uint32_t root = pack->three_level_guest_slots ?
        page / LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT : page;
    uint32_t middle = page % LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    int32_t offset = -(int32_t)((root + 1) * 8);
    instructions[0] = 0x28c00000u | ((uint32_t)offset & 0xfff) << 10 |
                      22u << 5 | destination;
    instructions[1] = 0x28c00000u |
                      ((pack->three_level_guest_slots ? middle : entry) * 8)
                          << 10 |
                      destination << 5 | destination;
    if (pack->three_level_guest_slots) {
        instructions[2] = 0x28c00000u | (entry * 8) << 10 |
                          destination << 5 | destination;
    } else {
        for (uint32_t i = 2; i < relocation->slots; i++) {
            instructions[i] = 0x03400000u;
        }
    }
    return 0;
}

static void eliminate_edge_flags(ModulePack *pack,
                                 const LatNativeRelocationV1 *relocation,
                                 int owner, int target)
{
    int edge;
    if (relocation->reserved == LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0) {
        edge = 0;
    } else if (relocation->reserved == LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_1) {
        edge = 1;
    } else {
        return;
    }
    const LatNativeTbV1 *tb = &pack->tbs[owner];
    unsigned actions = lat_eflags_link_actions(
        !(pack->tbs[target].optimization_flags & LAT_NATIVE_TB_ENTRY_FLAGS_DEAD),
        0, tb->eflags_offset[edge] ? tb->eflags_offset[edge] - 1 : UINT16_MAX,
        tb->eflags_stub_offset[edge] ?
            tb->eflags_stub_offset[edge] - 1 : UINT16_MAX);
    if (actions & LAT_EFLAGS_LINK_NOP) {
        uint64_t offset = tb->code_offset + tb->eflags_offset[edge] - 1;
        if (offset + 4 <= relocation->code_offset) {
            /* Preserve the original native code and all PC-map offsets. */
            uint32_t nop = 0x03400000u;
            memcpy(pack->code + offset, &nop, sizeof(nop));
        }
    }
    if (actions & LAT_EFLAGS_LINK_BYPASS) {
        uint64_t offset = tb->code_offset + tb->eflags_stub_offset[edge] - 1;
        uint32_t branch = 0x50000000u;
        if (offset + 4 <= relocation->code_offset &&
            !patch_branch(&branch, offset, pack->tbs[target].code_offset)) {
            memcpy(pack->code + offset, &branch, sizeof(branch));
        }
    }
}

static void thread_conditional_exits(ModulePack *pack)
{
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        const LatNativeTbV1 *tb = &pack->tbs[i];
        if (!pack->supported[i] || !tb->conditional_exit_offset) {
            continue;
        }
        uint64_t site = tb->code_offset + tb->conditional_exit_offset - 1;
        uint32_t branch;
        memcpy(&branch, pack->code + site, sizeof(branch));
        if (branch >> 26 < 0x16 || branch >> 26 > 0x1b) {
            continue;
        }
        int64_t stub = (int64_t)site + (int16_t)(branch >> 10) * 4;
        uint64_t end = tb->code_offset + tb->code_size;
        uint32_t instruction = 0;
        /* Remaining flag operations or other side effects stop threading. */
        while (stub >= (int64_t)tb->code_offset && stub <= (int64_t)end - 4) {
            memcpy(&instruction, pack->code + stub, sizeof(instruction));
            if (instruction != 0x03400000u) {
                break;
            }
            stub += 4;
        }
        if (stub < (int64_t)tb->code_offset || stub > (int64_t)end - 4 ||
            instruction >> 26 != 0x14) {
            continue;
        }
        int64_t immediate = ((instruction & 0x3ffu) << 16) |
                             ((instruction >> 10) & 0xffffu);
        if (immediate & (1 << 25)) {
            immediate -= 1 << 26;
        }
        int64_t target = stub + immediate * 4;
        if (target < 0) {
            continue;
        }
        int index = find_code_tb(pack, target);
        if (index < 0 || !pack->supported[index] ||
            pack->tbs[index].code_offset != (uint64_t)target ||
            pack->tbs[index].flags != tb->flags) {
            continue;
        }
        int64_t difference = target - (int64_t)site;
        if (difference % 4 || difference < -(1 << 17) ||
            difference >= (1 << 17)) {
            continue;
        }
        branch = (branch & 0xfc0003ffu) |
                 ((uint32_t)(difference / 4) & 0xffffu) << 10;
        memcpy(pack->code + site, &branch, sizeof(branch));
    }
}

static int patch_relocations(ModulePack *pack, char *error, size_t error_size)
{
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        if (!pack->supported[i]) {
            memset(pack->code + pack->tbs[i].code_offset, 0,
                   pack->tbs[i].code_size);
        }
    }
    for (uint64_t i = 0; i < pack->header->relocation_count; i++) {
        const LatNativeRelocationV1 *relocation = &pack->relocations[i];
        int owner = pack->relocation_owners[i];
        if (owner < 0 || !pack->supported[owner]) {
            continue;
        }
        uint32_t *instructions = (void *)(pack->code +
                                          relocation->code_offset);
        int result = -1;
        if (relocation->kind == LAT_NATIVE_RELOC_GUEST_ADDRESS) {
            result = patch_guest_address(pack, relocation);
        } else if (relocation->kind == LAT_NATIVE_RELOC_IMAGE_DATA) {
            result = patch_address(instructions, relocation->slots,
                                   relocation->code_offset, relocation->addend);
        } else if (relocation->kind == LAT_NATIVE_RELOC_RUNTIME_SYMBOL &&
                   relocation->target == LAT_NATIVE_SYMBOL_PFTABLE) {
            result = patch_runtime_target(instructions, relocation->slots,
                                          relocation->code_offset,
                                          pack->pf_table);
        } else if (relocation->kind == LAT_NATIVE_RELOC_RUNTIME_SYMBOL &&
                   runtime_entry(relocation->target)) {
            result = patch_runtime_target(
                instructions, relocation->slots, relocation->code_offset,
                pack->runtime_trampolines + relocation->target * 4);
        } else if (relocation->kind == LAT_NATIVE_RELOC_TB_TARGET) {
            int target = pack->relocation_patch_targets[i];
            if (target >= 0 && pack->supported[target]) {
                result = relocation->slots == 2 ?
                    patch_tb_target_pair(
                        instructions, relocation->code_offset,
                        pack->tbs[target].code_offset,
                        relocation->reserved) :
                    patch_runtime_target(
                        instructions, relocation->slots,
                        relocation->code_offset,
                        pack->tbs[target].code_offset);
                if (!result) {
                    eliminate_edge_flags(pack, relocation, owner, target);
                }
            } else if (target < 0 && runtime_entry(relocation->reserved)) {
                result = patch_runtime_target(
                    instructions, relocation->slots,
                    relocation->code_offset,
                    pack->runtime_trampolines + relocation->reserved * 4);
            }
        } else if (relocation->kind == LAT_NATIVE_RELOC_JRRA_TARGET) {
            int target = pack->relocation_targets[i];
            int enabled = target >= 0 && pack->supported[target];
            result = patch_jrra_target(
                instructions, relocation->slots, relocation->code_offset,
                enabled ? pack->tbs[target].code_offset : 0, enabled);
        }
        if (result) {
            return fail(error, error_size,
                        "cannot convert relocation %llu at code offset 0x%llx "
                        "(kind=%u target=%u slots=%u insn=%08x,%08x)",
                        (unsigned long long)i,
                        (unsigned long long)relocation->code_offset,
                        relocation->kind, relocation->target,
                        relocation->slots, instructions[0], instructions[1]);
        }
    }
    return 0;
}

static void print_bytes(FILE *file, const uint8_t bytes[32])
{
    for (size_t i = 0; i < 32; i++) {
        fprintf(file, "%s0x%02x", i ? ", " : "", bytes[i]);
    }
}

static int emit_metadata(const char *path, const ModulePack *pack,
                         char *error, size_t error_size)
{
    FILE *file = fopen(path, "w");
    if (!file) {
        return fail(error, error_size, "cannot create %s: %s", path,
                    strerror(errno));
    }
    uint8_t codegen[32];
    lat_aot_v2_codegen_digest(pack->header->lat_build_id, codegen);
    const char *guest_slot_flag = pack->three_level_guest_slots ?
        "LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS" :
        "LAT_AOT_MODULE_TWO_LEVEL_GUEST_SLOTS";
    fprintf(file,
        "#include \"lat-aot-v2.h\"\n#include <elf.h>\n"
        "#define MAGIC { 'L','A','T','A','O','T','2',0 }\n"
        "extern const uint8_t lat_aot_generated_text_begin[];\n"
        "extern const uint8_t lat_aot_generated_text_end[];\n"
        "typedef struct { Elf64_Nhdr h; char name[4]; LatAotNoteV2 d; } Note;\n"
        "__attribute__((section(\".note.lat.aot\"),aligned(4),used))\n"
        "static const Note note = { {4,sizeof(LatAotNoteV2),0x4c415432},"
        "\"LAT\", { MAGIC,2,sizeof(LatAotNoteV2),"
        "LAT_AOT_MODULE_PARTIAL|LAT_AOT_MODULE_READONLY_TEXT|"
        "LAT_AOT_MODULE_PRECISE_PC_MAP|%s,"
        "LAT_AOT_V2_REQUIRED_BASE_FEATURES|LAT_AOT_FEATURE_LASX,{ ",
        guest_slot_flag);
    print_bytes(file, pack->header->guest_sha256);
    fprintf(file, " },{ ");
    print_bytes(file, codegen);
    fprintf(file, " },{0} } };\n");

    fprintf(file,
        "extern const LatAotTbV2 lat_aot_generated_tbs_begin[];\n"
        "extern const LatAotTbV2 lat_aot_generated_tbs_end[];\n"
        "extern const LatAotPcMapV2 lat_aot_generated_maps_begin[];\n"
        "extern const LatAotPcMapV2 lat_aot_generated_maps_end[];\n"
        "extern const LatAotGuestSlotV2 lat_aot_generated_slots_begin[];\n"
        "extern const LatAotGuestSlotV2 lat_aot_generated_slots_end[];\n"
        "__attribute__((visibility(\"default\"),"
        "section(\".data.rel.ro.lat.module\"),used))\n"
        "const LatAotModuleV2 lat_aot_module_v2 = {"
        "MAGIC,2,sizeof(LatAotModuleV2),"
        "LAT_AOT_MODULE_PARTIAL|LAT_AOT_MODULE_READONLY_TEXT|"
        "LAT_AOT_MODULE_PRECISE_PC_MAP|%s,"
        "LAT_AOT_V2_REQUIRED_BASE_FEATURES|LAT_AOT_FEATURE_LASX,{ ",
        guest_slot_flag);
    print_bytes(file, pack->header->guest_sha256);
    fprintf(file, " },{ ");
    print_bytes(file, codegen);
    fprintf(file,
        " },{0},lat_aot_generated_text_begin,lat_aot_generated_text_end,"
        "lat_aot_generated_tbs_begin,lat_aot_generated_tbs_end,"
        "lat_aot_generated_maps_begin,lat_aot_generated_maps_end,"
        "lat_aot_generated_slots_begin,lat_aot_generated_slots_end};\n");
    int result = 0;
    if (fclose(file)) {
        result = fail(error, error_size, "cannot close %s", path);
    }
    return result;
}

static int emit_tables(const char *directory, const ModulePack *pack,
                       char *error, size_t error_size)
{
    char *tb_path = g_build_filename(directory, "tbs.bin", NULL);
    char *map_path = g_build_filename(directory, "pc-maps.bin", NULL);
    char *slot_path = g_build_filename(directory, "guest-slots.bin", NULL);
    FILE *tb_file = fopen(tb_path, "wb");
    FILE *map_file = fopen(map_path, "wb");
    FILE *slot_file = fopen(slot_path, "wb");
    GArray *map_order = g_array_sized_new(
        FALSE, FALSE, sizeof(const LatNativePcMapV2 *),
        selected_pc_map_count(pack));
    int result = 0;
    if (!tb_file || !map_file || !slot_file) {
        result = fail(error, error_size, "cannot create binary module tables");
        goto out;
    }
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        if (!pack->supported[i] ||
            pack->tb_primary_index[i] != (int)i) {
            continue;
        }
        LatAotTbV2 tb = {
            .guest_rva = pack->tbs[i].guest_pc -
                         pack->header->preferred_guest_base,
            .host_offset = pack->tbs[i].code_offset,
            .host_size = pack->tbs[i].code_size,
            .flags = pack->tbs[i].flags,
        };
        if (fwrite(&tb, sizeof(tb), 1, tb_file) != 1) {
            result = fail(error, error_size, "cannot write binary TB table");
            goto out;
        }
    }
    for (uint64_t i = 0; i < pack->header->pc_map_count; i++) {
        int owner = pack->pc_map_owners[i];
        if (!pack->preserve_native_layout &&
            (owner < 0 || !pack->supported[owner] ||
             !pc_map_in_tb(&pack->pc_maps[i], &pack->tbs[owner]))) {
            continue;
        }
        const LatNativePcMapV2 *map = &pack->pc_maps[i];
        if (!pc_map_range_valid(pack, map)) {
            continue;
        }
        g_array_append_val(map_order, map);
    }
    if (pack->profiled_layout) {
        g_array_sort(map_order, compare_pc_map_code);
    }
    for (guint i = 0; i < map_order->len; i++) {
        const LatNativePcMapV2 *native_map = g_array_index(
            map_order, const LatNativePcMapV2 *, i);
        LatAotPcMapV2 map = {
            .guest_rva = native_map->guest_pc -
                         pack->header->preferred_guest_base,
            .host_offset_begin = native_map->host_offset_begin,
            .host_offset_end = native_map->host_offset_end,
            .state_record_offset = native_map->state_record_offset,
            .flags = native_map->flags,
        };
        if (fwrite(&map, sizeof(map), 1, map_file) != 1) {
            result = fail(error, error_size, "cannot write binary PC map");
            goto out;
        }
    }
    for (guint i = 0; i < pack->guest_rvas->len; i++) {
        guint page = i / LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
        guint root = pack->three_level_guest_slots ?
            page / LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT : page;
        guint middle_offset = pack->three_level_guest_slots ?
            (page % LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT) * 8 : 0;
        guint entry_offset =
            (i % LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT) * 8;
        LatAotGuestSlotV2 slot = {
            .guest_rva = g_array_index(pack->guest_rvas, uint64_t, i),
            .fp_offset = -(int32_t)((root + 1) * 8),
            .reserved = pack->three_level_guest_slots ?
                (middle_offset << 16) | entry_offset : entry_offset,
        };
        if (fwrite(&slot, sizeof(slot), 1, slot_file) != 1) {
            result = fail(error, error_size, "cannot write binary guest slots");
            goto out;
        }
    }
out:
    if (tb_file && fclose(tb_file) && !result) result = -1;
    if (map_file && fclose(map_file) && !result) result = -1;
    if (slot_file && fclose(slot_file) && !result) result = -1;
    g_free(tb_path);
    g_free(map_path);
    g_free(slot_path);
    g_array_free(map_order, TRUE);
    return result;
}

static int prepare_local_dispatch(ModulePack *pack)
{
    if (!pack->local_size) {
        return 0;
    }
    if (pack->header->flags & LAT_NATIVE_IMAGE_PIE) {
        pack->local_base_words = pack->three_level_guest_slots ? 3 : 2;
    } else if (pack->local_base < 0x80000000u) {
        pack->local_base_words = (pack->local_base & 0xfff) ? 2 : 1;
    } else {
        pack->local_base_words = 3;
    }
    for (uint64_t i = 0; i < pack->header->pc_map_count; i++) {
        int owner = pack->pc_map_owners[i];
        if (owner < 0 || !pack->supported[owner] ||
            pack->tb_primary_index[owner] != owner) {
            continue;
        }
        const LatNativeTbV1 *tb = &pack->tbs[owner];
        const LatNativePcMapV2 *map = &pack->pc_maps[i];
        if (!tb->indirect_exit_offset ||
            tb->flags != (LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL)) {
            continue;
        }
        uint64_t site = tb->code_offset + tb->indirect_exit_offset - 1;
        unsigned int words = lat_native_indirect_exit_words(
            pack->code + site, tb->code_size - (tb->indirect_exit_offset - 1));
        if (words && (map->flags & LAT_NATIVE_PC_MAP_DYNAMIC_STATE) &&
            map->host_offset_begin <= site && map->host_offset_end >=
                site + words * 4) {
            pack->local_dispatch[owner] = 1;
        }
    }
    for (uint64_t i = 0; i < pack->header->tb_count; i++) {
        if (!pack->local_dispatch[i] ||
            pack->tb_primary_index[i] != (int)i) {
            continue;
        }
        uint64_t site = pack->tbs[i].code_offset +
                        pack->tbs[i].indirect_exit_offset - 1;
        unsigned int words = lat_native_indirect_exit_words(
            pack->code + site,
            pack->tbs[i].code_size - (pack->tbs[i].indirect_exit_offset - 1));
        if (!words) {
            pack->local_dispatch[i] = 0;
            continue;
        }
        pack->local_dispatch_words[i] = words;
        uint32_t base[3] = {0x1400000cu, 0x0380018cu, 0x1600000cu};
        memcpy(pack->code + site, base, sizeof(base));
        LatNativeRelocationV1 relocation = {
            .code_offset = site, .addend = pack->local_base,
            .kind = LAT_NATIVE_RELOC_GUEST_ADDRESS,
            .slots = MAX(pack->local_base_words, 2u),
        };
        if (patch_guest_address(pack, &relocation)) {
            return -1;
        }
    }
    return 0;
}

typedef struct ReturnCandidates {
    uint64_t target[5];
    unsigned int score[5];
    unsigned int count;
} ReturnCandidates;

static void add_return_candidate(ReturnCandidates *candidates,
                                 uint64_t target, unsigned int score)
{
    for (unsigned int i = 0; i < MIN(candidates->count, 5u); i++) {
        if (candidates->target[i] == target) {
            candidates->score[i] = MAX(candidates->score[i], score);
            return;
        }
    }
    if (candidates->count < 5) {
        candidates->target[candidates->count] = target;
        candidates->score[candidates->count] = score;
    }
    candidates->count++;
}

static const CfgProgramFunction *cfg_function_for_tb(
    const CfgProgram *program, size_t tb_index)
{
    for (size_t i = 0; i < program->function_count; i++) {
        const CfgProgramFunction *function = &program->functions[i];
        if (tb_index >= function->first_tb &&
            tb_index - function->first_tb < function->tb_count) {
            return function;
        }
    }
    return NULL;
}

static bool cfg_layout_tb_in_loop(const CfgProgram *program, size_t tb_index)
{
    const CfgProgramFunction *function = cfg_function_for_tb(program,
                                                              tb_index);
    uint64_t pc = program->tbs[tb_index].start;
    if (!function) {
        for (size_t i = 0; i < program->function_count && !function; i++) {
            const CfgProgramFunction *candidate = &program->functions[i];
            for (size_t j = 0; j < candidate->tb_count; j++) {
                if (program->tbs[candidate->first_tb + j].start == pc) {
                    function = candidate;
                    break;
                }
            }
        }
    }
    if (!function) {
        return false;
    }
    for (size_t i = 0; i < function->tb_count; i++) {
        const CfgTb *tb = &program->tbs[function->first_tb + i];
        for (size_t j = 0; j < tb->edge_count; j++) {
            const CfgProgramEdge *edge =
                &program->edges[tb->first_edge + j];
            if (edge->resolution == CFG_EDGE_STATIC &&
                edge->kind != CFG_EDGE_CALL &&
                edge->kind != CFG_EDGE_CALL_RETURN &&
                edge->to >= function->start &&
                edge->to < function->start + function->size &&
                edge->to <= pc && pc <= edge->from) {
                return true;
            }
        }
    }
    return false;
}

static unsigned int return_candidate_score(const CfgProgram *program,
                                           size_t tb_index)
{
    const CfgProgramFunction *function = cfg_function_for_tb(program, tb_index);
    if (!function) {
        return 0;
    }
    uint64_t pc = program->tbs[tb_index].start;
    unsigned int score = 0;
    for (size_t i = 0; i < function->tb_count; i++) {
        const CfgTb *tb = &program->tbs[function->first_tb + i];
        for (size_t j = 0; j < tb->edge_count; j++) {
            const CfgProgramEdge *edge =
                &program->edges[tb->first_edge + j];
            if (edge->resolution == CFG_EDGE_STATIC &&
                edge->kind != CFG_EDGE_CALL &&
                edge->kind != CFG_EDGE_CALL_RETURN &&
                edge->to >= function->start &&
                edge->to < function->start + function->size &&
                edge->to <= pc && pc <= edge->from) {
                score++;
            }
        }
    }
    return score;
}

static void configure_return_guards(ModulePack *pack,
                                    const CfgProgram *program)
{
    if (!program || !program->function_count || !pack->local_size) {
        return;
    }
    ReturnCandidates *candidates = g_new0(
        ReturnCandidates, program->function_count);
    for (size_t i = 0; i < program->tb_count; i++) {
        const CfgTb *tb = &program->tbs[i];
        if (tb->terminator != CFG_TB_CALL &&
            tb->terminator != CFG_TB_INDIRECT_CALL) {
            continue;
        }
        uint64_t return_pc = 0;
        for (size_t j = 0; j < tb->edge_count; j++) {
            const CfgProgramEdge *edge =
                &program->edges[tb->first_edge + j];
            if (edge->kind == CFG_EDGE_CALL_RETURN &&
                edge->resolution == CFG_EDGE_STATIC) {
                return_pc = edge->to;
                break;
            }
        }
        if (!return_pc) {
            continue;
        }
        unsigned int score = return_candidate_score(program, i);
        for (size_t j = 0; j < tb->edge_count; j++) {
            const CfgProgramEdge *edge =
                &program->edges[tb->first_edge + j];
            if (edge->kind != CFG_EDGE_CALL ||
                edge->resolution != CFG_EDGE_STATIC) {
                continue;
            }
            for (size_t k = 0; k < program->function_count; k++) {
                if (program->functions[k].start == edge->to) {
                    add_return_candidate(&candidates[k], return_pc, score);
                }
            }
        }
    }
    const uint32_t flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL;
    for (size_t i = 0; i < program->function_count; i++) {
        const CfgProgramFunction *function = &program->functions[i];
        if (function->status != CFG_FUNCTION_OK || candidates[i].count < 2 ||
            candidates[i].count > 5) {
            continue;
        }
        unsigned int selected[2] = {UINT_MAX, UINT_MAX};
        for (unsigned int j = 0; j < candidates[i].count; j++) {
            unsigned int first = selected[0];
            bool before_first = first == UINT_MAX ||
                candidates[i].score[j] > candidates[i].score[first] ||
                (candidates[i].score[j] == candidates[i].score[first] &&
                 candidates[i].target[j] > candidates[i].target[first]);
            if (before_first) {
                selected[1] = first;
                selected[0] = j;
                continue;
            }
            unsigned int second = selected[1];
            if (second == UINT_MAX ||
                candidates[i].score[j] > candidates[i].score[second] ||
                (candidates[i].score[j] == candidates[i].score[second] &&
                 candidates[i].target[j] > candidates[i].target[second])) {
                selected[1] = j;
            }
        }
        int targets[2] = {-1, -1};
        for (unsigned int j = 0; j < 2; j++) {
            targets[j] = find_tb(pack,
                candidates[i].target[selected[j]], flags);
            if (targets[j] < 0 || !pack->supported[targets[j]] ||
                pack->tbs[targets[j]].guest_pc < pack->local_base ||
                pack->tbs[targets[j]].guest_pc >=
                    pack->local_base + pack->local_size) {
                targets[0] = -1;
                break;
            }
        }
        if (targets[0] < 0) {
            continue;
        }
        for (size_t j = 0; j < function->tb_count; j++) {
            const CfgTb *cfg_tb = &program->tbs[function->first_tb + j];
            if (cfg_tb->terminator != CFG_TB_RETURN) {
                continue;
            }
            for (uint64_t k = 0; k < pack->header->pc_map_count; k++) {
                const LatNativePcMapV2 *map = &pack->pc_maps[k];
                int owner = pack->pc_map_owners[k];
                if (owner < 0 || map->guest_pc < cfg_tb->start ||
                    map->guest_pc >= cfg_tb->end ||
                    pack->tbs[owner].flags != flags ||
                    !pack->local_dispatch[owner]) {
                    continue;
                }
                if (pack->local_dispatch_words[owner] !=
                    LAT_NATIVE_INDIRECT_EXIT_WORDS) {
                    continue;
                }
                for (unsigned int n = 0; n < 2; n++) {
                    pack->return_guard_targets[owner][n] = targets[n];
                }
                pack->return_guard_count[owner] = 2;
            }
        }
    }
    g_free(candidates);
}

static int emit_local_dispatch(FILE *file, const ModulePack *pack,
                               uint64_t index, uint64_t site,
                               unsigned int words)
{
    uint32_t base[3];
    memcpy(base, pack->code + site, sizeof(base));
    for (uint32_t i = 0; i < pack->local_base_words; i++) {
        fprintf(file, ".word 0x%08x\n", base[i]);
    }
    fprintf(file, "sub.d $t0,$r21,$t0\n");
    for (unsigned int i = 0; i < pack->return_guard_count[index]; i++) {
        int target = pack->return_guard_targets[index][i];
        uint64_t rva = pack->tbs[target].guest_pc - pack->local_base;
        uint64_t branch_site = site + (pack->local_base_words + 4 + i * 4) * 4;
        uint32_t branch = 0x50000000u;
        if (rva >= pack->local_size || rva >> 32 ||
            patch_branch(&branch, branch_site,
                         pack->tbs[target].code_offset)) {
            return -1;
        }
        fprintf(file,
            "lu12i.w $t2,%llu\n"
            "ori $t2,$t2,%llu\n"
            "bne $t0,$t2,.Llat_return_next_%llu_%u\n"
            ".word 0x%08x\n"
            ".Llat_return_next_%llu_%u:\n",
            (unsigned long long)(rva >> 12),
            (unsigned long long)(rva & 0xfff),
            (unsigned long long)index, i, branch,
            (unsigned long long)index, i);
    }
    /* la.pcrel expands to two instructions; keep the replacement window
       exact. */
    fprintf(file,
        "lu12i.w $t2,%llu\n"
        "bgeu $t0,$t2,.Llat_local_miss_%llu\n"
        "la.pcrel $t1,.Llat_local_targets\n"
        "alsl.d $t2,$t0,$t1,2\n"
        "ld.w $a7,$t2,0\n",
        (unsigned long long)(pack->local_size >> 12),
        (unsigned long long)index);
    if (!pack->local_dispatch_sentinel) {
        fprintf(file, "beqz $a7,.Llat_local_miss_%llu\n",
                (unsigned long long)index);
    }
    fprintf(file,
        "add.d $a7,$a7,$t1\n"
        "jr $a7\n"
        ".Llat_local_miss_%llu:\n"
        "srli.d $a7,$r21,16\n"
        "xor $a7,$r21,$a7\n"
        "bstrpick.d $a7,$a7,15,0\n"
        "alsl.d $a7,$a7,$fp,4\n"
        "ld.d $t0,$a7,0\n"
        "bne $t0,$r21,.Llat_local_end_%llu\n"
        "ld.d $a7,$a7,8\n"
        "jr $a7\n"
        ".rept %u\nnop\n.endr\n"
        ".Llat_local_end_%llu:\n",
        (unsigned long long)index, (unsigned long long)index,
        words - 18 + pack->local_dispatch_sentinel - pack->local_base_words -
            4 * pack->return_guard_count[index],
        (unsigned long long)index);
    return 0;
}

static void emit_local_dispatch_gap(FILE *file, const ModulePack *pack,
                                    uint64_t entries)
{
    if (pack->local_dispatch_sentinel) {
        fprintf(file, ".rept %llu\n"
                      ".word .Llat_aot_runtime_%u-.Llat_local_targets\n"
                      ".endr\n",
                (unsigned long long)entries,
                LAT_NATIVE_SYMBOL_EPILOGUE_RET_0);
    } else {
        fprintf(file, ".zero %llu\n", (unsigned long long)(entries * 4));
    }
}

static int emit_assembly(const char *path, const ModulePack *pack,
                          char *error, size_t error_size)
{
    FILE *file = fopen(path, "w");
    if (!file) {
        return fail(error, error_size, "cannot create %s: %s", path,
                    strerror(errno));
    }
    fprintf(file,
        ".section .text.lat.tu,\"ax\",@progbits\n.p2align 12\n"
        ".global lat_aot_generated_text_begin\n"
        ".hidden lat_aot_generated_text_begin\n"
        "lat_aot_generated_text_begin:\n");
    uint64_t cursor = 0;
    int have_local_dispatch = 0;
    for (guint i = 0; i < pack->code_order->len; i++) {
        const LatNativeTbV1 *tb = g_array_index(
            pack->code_order, const LatNativeTbV1 *, i);
        uint64_t index = tb - pack->tbs;
        if (!pack->local_dispatch[index]) {
            continue;
        }
        uint64_t site = tb->code_offset + tb->indirect_exit_offset - 1;
        unsigned int words = pack->local_dispatch_words[index];
        unsigned int used = 18 - pack->local_dispatch_sentinel +
            pack->local_base_words +
            4 * pack->return_guard_count[index];
        if (pack->tb_primary_index[index] != (int)index ||
            (words != LAT_NATIVE_INDIRECT_EXIT_WORDS &&
             words != LAT_NATIVE_AOT_V2_INDIRECT_EXIT_WORDS) ||
            words < used ||
            site < cursor || site > pack->header->code_size ||
            (uint64_t)words * 4 > pack->header->code_size - site) {
            fclose(file);
            return fail(error, error_size, "invalid local dispatch window");
        }
        if (site > cursor) {
            fprintf(file, ".incbin \"text.bin\",%llu,%llu\n",
                    (unsigned long long)cursor,
                    (unsigned long long)(site - cursor));
        }
        if (emit_local_dispatch(file, pack, index, site, words)) {
            fclose(file);
            return fail(error, error_size,
                        "cannot encode local return guard");
        }
        cursor = site + words * 4;
        have_local_dispatch = 1;
    }
    fprintf(file, ".incbin \"text.bin\",%llu,%llu\n.align 2\n",
            (unsigned long long)cursor,
            (unsigned long long)(pack->header->code_size - cursor));
    for (uint32_t symbol = 0; symbol < LAT_NATIVE_SYMBOL_COUNT; symbol++) {
        const char *entry = runtime_entry(symbol);
        fprintf(file, ".Llat_aot_runtime_%u:\n", symbol);
        if (entry) {
            fprintf(file, "b %s\n", entry);
        } else {
            fprintf(file, "nop\n");
        }
    }
    fprintf(file, ".Llat_aot_pf_table:\n");
    for (unsigned int value = 0; value < 256; value++) {
        unsigned int bits = value;
        unsigned int parity = 0;
        while (bits) {
            parity ^= bits & 1;
            bits >>= 1;
        }
        fprintf(file, "%s%u%s", value % 16 ? "," : ".byte ",
                parity ? 0 : 4, value % 16 == 15 ? "\n" : "");
    }
    fprintf(file,
        ".hidden lat_aot_generated_abi_anchor\n"
        "lat_aot_generated_abi_anchor:\n"
        "b lat_aot_runtime_abi_version\n"
        ".global lat_aot_generated_text_end\n"
        ".hidden lat_aot_generated_text_end\n"
        "lat_aot_generated_text_end:\n"
        ".section .rodata.lat.tb,\"a\",@progbits\n.p2align 3\n"
        ".global lat_aot_generated_tbs_begin\n.hidden lat_aot_generated_tbs_begin\n"
        "lat_aot_generated_tbs_begin:\n.incbin \"tbs.bin\"\n"
        ".global lat_aot_generated_tbs_end\n.hidden lat_aot_generated_tbs_end\n"
        "lat_aot_generated_tbs_end:\n"
        ".section .rodata.lat.map,\"a\",@progbits\n.p2align 3\n"
        ".global lat_aot_generated_maps_begin\n.hidden lat_aot_generated_maps_begin\n"
        "lat_aot_generated_maps_begin:\n.incbin \"pc-maps.bin\"\n"
        ".global lat_aot_generated_maps_end\n.hidden lat_aot_generated_maps_end\n"
        "lat_aot_generated_maps_end:\n"
        ".section .rodata.lat.guest,\"a\",@progbits\n.p2align 3\n"
        ".global lat_aot_generated_slots_begin\n.hidden lat_aot_generated_slots_begin\n"
        "lat_aot_generated_slots_begin:\n.incbin \"guest-slots.bin\"\n"
        ".global lat_aot_generated_slots_end\n.hidden lat_aot_generated_slots_end\n"
        "lat_aot_generated_slots_end:\n.zero 16\n");
    if (have_local_dispatch) {
        fprintf(file, ".section .rodata.lat.local,\"a\",@progbits\n"
                      ".p2align 12\n.Llat_local_targets:\n");
        uint64_t next = pack->local_base;
        for (uint64_t i = 0; i < pack->header->tb_count; i++) {
            const LatNativeTbV1 *tb = &pack->tbs[i];
            if (!pack->supported[i] ||
                pack->tb_primary_index[i] != (int)i ||
                tb->flags != (LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL)) {
                continue;
            }
            if (tb->guest_pc > next) {
                emit_local_dispatch_gap(file, pack, tb->guest_pc - next);
            }
            fprintf(file, ".word lat_aot_generated_text_begin+%llu"
                          "-.Llat_local_targets\n",
                    (unsigned long long)tb->code_offset);
            next = tb->guest_pc + 1;
        }
        emit_local_dispatch_gap(
            file, pack, pack->local_base + pack->local_size - next);
    }
    int result = 0;
    if (fclose(file)) {
        result = fail(error, error_size, "cannot close %s", path);
    }
    return result;
}

static int emit_module_sources(const char *native_image,
                               const char *output_directory,
                               const CfgProgram *program,
                               const uint8_t guest_sha256[32],
                               char *error, size_t error_size)
{
    gchar *image = NULL;
    gsize image_size = 0;
    if (!native_image || !output_directory ||
        !g_file_get_contents(native_image, &image, &image_size, NULL)) {
        return fail(error, error_size, "cannot read native image %s",
                    native_image ? native_image : "(null)");
    }
    if (lat_native_image_validate(image, image_size, error, error_size)) {
        g_free(image);
        return -1;
    }
    if (g_mkdir_with_parents(output_directory, 0700)) {
        g_free(image);
        return fail(error, error_size, "cannot create output directory %s",
                    output_directory);
    }
    const LatNativeImageHeaderV2 *header = (const void *)image;
    if (program && (!guest_sha256 ||
                    memcmp(header->guest_sha256, guest_sha256, 32))) {
        g_free(image);
        return fail(error, error_size,
                    "CFG guest digest differs from native image");
    }
    ModulePack pack = {
        .header = header,
        .tbs = g_new(LatNativeTbV1, header->tb_count),
        .relocations = g_new(LatNativeRelocationV1,
                             header->relocation_count),
        .pc_maps = g_new(LatNativePcMapV2, header->pc_map_count),
        .relocation_owners = g_new(int, header->relocation_count),
        .relocation_targets = g_new(int, header->relocation_count),
        .relocation_patch_targets = g_new(int, header->relocation_count),
        .pc_map_owners = g_new(int, header->pc_map_count),
        .pc_maps_complete = g_new(unsigned char, header->tb_count),
        .code = g_malloc(header->code_size),
        .supported = g_malloc0(header->tb_count),
        .local_dispatch = g_malloc0(header->tb_count),
        .local_dispatch_words = g_malloc0(header->tb_count),
        .return_guard_count = g_malloc0(header->tb_count),
        .return_guard_targets = g_malloc0_n(
            header->tb_count, sizeof(*pack.return_guard_targets)),
        .code_order = g_array_sized_new(FALSE, FALSE,
                                        sizeof(const LatNativeTbV1 *),
                                        header->tb_count),
        .guest_rvas = g_array_new(FALSE, FALSE, sizeof(uint64_t)),
        .guest_rva_indexes = g_hash_table_new_full(
            g_int64_hash, g_int64_equal, g_free, NULL),
        .runtime_trampolines = (header->code_size + 3) & ~(uint64_t)3,
    };
    memcpy(pack.tbs, image + header->tb_table_offset,
           header->tb_count * sizeof(*pack.tbs));
    memcpy(pack.relocations, image + header->relocation_offset,
           header->relocation_count * sizeof(*pack.relocations));
    memcpy(pack.pc_maps, image + header->pc_map_offset,
           header->pc_map_count * sizeof(*pack.pc_maps));
    pack.pf_table = pack.runtime_trampolines +
                    LAT_NATIVE_SYMBOL_COUNT * sizeof(uint32_t);
    for (uint64_t i = 0; i < header->tb_count; i++) {
        const LatNativeTbV1 *tb = &pack.tbs[i];
        g_array_append_val(pack.code_order, tb);
    }
    g_array_sort(pack.code_order, compare_tb_code);
    build_tb_primary_index(&pack);
    build_tb_hash(&pack);
    assign_code_owners(&pack, pack.relocations, pack.relocation_owners,
                       header->relocation_count,
                       sizeof(*pack.relocations),
                       offsetof(LatNativeRelocationV1, code_offset));
    assign_code_owners(&pack, pack.pc_maps, pack.pc_map_owners,
                       header->pc_map_count,
                       sizeof(*pack.pc_maps),
                       offsetof(LatNativePcMapV2, host_offset_begin));
    for (uint64_t i = 0; i < header->relocation_count; i++) {
        const LatNativeRelocationV1 *relocation = &pack.relocations[i];
        pack.relocation_targets[i] =
            relocation->kind == LAT_NATIVE_RELOC_TB_TARGET ||
            relocation->kind == LAT_NATIVE_RELOC_JRRA_TARGET ?
            find_tb(&pack, (uint64_t)relocation->addend,
                    relocation->target) : -1;
        pack.relocation_patch_targets[i] = pack.relocation_targets[i];
    }
    memcpy(pack.code, image + header->code_offset, header->code_size);
    int all_ranges_valid = all_tb_ranges_valid(&pack);
    pack.preserve_native_layout = header->tu_count != 0;
    if (all_ranges_valid) {
        reorder_profiled_code(&pack, program);
    }
    compute_pc_map_completeness(&pack);
    configure_local_dispatch(&pack);
    pack.local_dispatch_sentinel = pack.local_size && pack.dense_profile;
    select_supported_tbs(&pack);
    size_t supported_count = 0;
    for (uint64_t i = 0; i < header->tb_count; i++) {
        supported_count += pack.supported[i] != 0;
    }
    int result = 0;
    size_t pc_map_count = selected_pc_map_count(&pack);
    if (!all_ranges_valid) {
        result = fail(error, error_size,
                      "native image has overlapping TB host ranges");
    } else if (!supported_count) {
        result = fail(error, error_size,
                      "native image has no TB supported by AOT v2 M1");
    } else if (!selected_tb_ranges_valid(&pack)) {
        result = fail(error, error_size,
                      "supported AOT v2 TB host ranges overlap");
    } else if (!pc_map_count || !selected_pc_maps_complete(&pack)) {
        result = fail(error, error_size,
                      "supported AOT v2 TBs have no complete PC map");
    } else if (patch_relocations(&pack, error, error_size)) {
        result = -1;
    } else if (prepare_local_dispatch(&pack)) {
        result = fail(error, error_size, "cannot prepare local indirect dispatch");
    } else {
        configure_return_guards(&pack, program);
        thread_conditional_exits(&pack);
        char *text_path = g_build_filename(output_directory, "text.bin", NULL);
        char *metadata_path = g_build_filename(output_directory, "module.c", NULL);
        char *assembly_path = g_build_filename(output_directory, "module.S", NULL);
        if (write_all(text_path, pack.code, header->code_size,
                      error, error_size) ||
            emit_tables(output_directory, &pack, error, error_size) ||
            emit_metadata(metadata_path, &pack, error, error_size) ||
            emit_assembly(assembly_path, &pack, error, error_size)) {
            result = -1;
        }
        g_free(text_path);
        g_free(metadata_path);
        g_free(assembly_path);
    }
    g_hash_table_destroy(pack.guest_rva_indexes);
    g_array_free(pack.guest_rvas, TRUE);
    g_array_free(pack.code_order, TRUE);
    g_free(pack.pc_map_owners);
    g_free(pack.pc_maps_complete);
    g_free(pack.tb_primary_index);
    g_free(pack.tb_hash);
    g_free(pack.relocation_targets);
    g_free(pack.relocation_patch_targets);
    g_free(pack.relocation_owners);
    g_free(pack.supported);
    g_free(pack.local_dispatch);
    g_free(pack.local_dispatch_words);
    g_free(pack.return_guard_count);
    g_free(pack.return_guard_targets);
    g_free(pack.code);
    g_free(pack.pc_maps);
    g_free(pack.relocations);
    g_free(pack.tbs);
    g_free(image);
    return result;
}

int lat_aot_v2_emit_module_sources(const char *native_image,
                                   const char *output_directory,
                                   char *error, size_t error_size)
{
    return emit_module_sources(native_image, output_directory, NULL, NULL,
                               error, error_size);
}

int lat_aot_v2_emit_module_sources_with_cfg(
    const char *native_image, const char *output_directory,
    const CfgProgram *program, const uint8_t guest_sha256[32],
    char *error, size_t error_size)
{
    return emit_module_sources(native_image, output_directory, program,
                               guest_sha256, error, error_size);
}
