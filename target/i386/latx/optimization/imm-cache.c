/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file imm-cache.c
 * @author liuxinpeng <ksliuxp@163.com>
 * @brief IMM optimization
 */
#include "imm-cache.h"
#include <limits.h>
#include "error.h"
#include "la-ir2.h"
#include "lsenv.h"
#include "reg-map.h"
#include "translate.h"

int itemp_stat[4] = {1, 1, 1, 1};

/**
 * itemp_stat index map itemp
 * x32: itemp9 to itemp6
 * x64: itemp6 to itemp3
 */
const int itemp_map[4] = {
#ifndef TARGET_X86_64
    [0] = ITEMP7,
    [1] = ITEMP8,
    [2] = ITEMP9,
    [3] = ITEMP10,
#else
    [0] = ITEMP3,
    [1] = ITEMP4,
    [2] = ITEMP5,
    [3] = ITEMP6,
#endif
};

/**
 * itemp_stat index map itemp
 * x32: itemp9 to itemp6
 * x64: itemp6 to itemp3
 */
const int itemp_reverse_map[11] = {
    [0 ... 10] = -1,
#ifndef TARGET_X86_64
    [ITEMP7] = 0,    [ITEMP8] 1, [ITEMP9] = 2, [ITEMP10] = 3,
#else
    [ITEMP3] = 0,    [ITEMP4] = 1, [ITEMP5] = 2, [ITEMP6] = 3,
#endif
};

//=====================
//     stastic
//=====================
long cache_call = 0;
long cache_hit = 0;

// ========================
//      util functions
// ========================

/**
 * when function is called, free itemp must exist
 * allocate from
 * array reflection: 3-0:
 * to avoid conflict
 */
static inline int alloc_itemp(void)
{
    for (int i = 3; i >= 0; i--) {
        if (itemp_stat[i]) {
            itemp_stat[i] = 0;
            return itemp_map[i];
        }
    }
    lsassert(0);
    return -1;
}

static inline void free_itemp(int itemp_num)
{
    /**
     * itemp_num is
     * x32: [7-10] -> [0-3]
     * x64: [3-6] -> [0-3]
     */
    if (itemp_num < 0) {
        return;
    }
    int index = itemp_reverse_map[itemp_num];
    lsassertm(index != -1, "Invalid itemp_num map to imm_cache reg.");
    itemp_stat[index] = 1;
}

// check if itemp_num(itemp3-6) is viewed as a imm reg
bool imm_cache_is_imm_itemp(int itemp_num)
{
    return itemp_reverse_map[itemp_num] != -1;
}

bool imm_cache_itemp_is_used(IR2_OPND opnd)
{
    int itemp_num = reg_itemp_reverse_map[opnd._reg_num];
    int index = itemp_reverse_map[itemp_num];
    if (itemp_stat[index] != -1) {
        return true;
    }
    return false;
}

bool imm_cache_is_cached_at(IMM_CACHE *cache, int cache_index)
{
    return !cache->bucket[cache_index].free;
}

int imm_cache_la_reg_num_at(IMM_CACHE *cache, int cache_index)
{
    // return itemp4
    int itemp_num = cache->bucket[cache_index].itemp_num;

    int reg_num = reg_itemp_map[itemp_num];
    return reg_num;
}
static inline int compare_imm_cache(const void *a, const void *b)
{
    return ((IMM_CACHE_BUCKET *)b)->use_count -
           ((IMM_CACHE_BUCKET *)a)->use_count;
}

// sort by use frequency, range: cache_count
inline void imm_cache_sort(IMM_CACHE *cache)
{
    qsort(cache->bucket, cache->cache_count, sizeof(IMM_CACHE_BUCKET),
          compare_imm_cache);
}

// ============================
//    cache init functions
// ============================
static inline void imm_cache_init_bucket(IMM_CACHE_BUCKET *bucket, int index)
{
    //======normal cache ======
    bucket[index].free = true;
    bucket[index].use_count = 0;
    bucket[index].itemp_num = -1;
    bucket[index].base = -1;
    bucket[index].index = -1;
    bucket[index].scale = -1;
    bucket[index].curr_ir1_index = -1;
    bucket[index].offset = 0;
    //======pre cache======
    bucket[index].lifecycle = 0;
    bucket[index].pre_cache = false;
    bucket[index].avg_offset = 0;
    bucket[index].min_offset = LONG_LONG_MAX;
    bucket[index].max_offset = LONG_LONG_MIN;
}

void imm_cache_init(IMM_CACHE *imm_cache, int capacity)
{
    lsassertm((imm_cache != NULL), "imm_cache memory is NULL\n");
    /*capacity should > CACHE_DEFAULT_CAPACITY*/
    if (capacity < CACHE_DEFAULT_CAPACITY) {
        capacity = CACHE_DEFAULT_CAPACITY;
    }

    imm_cache->cache_count = capacity;
    imm_cache->free_count = imm_cache->cache_count;
    imm_cache->curr_imm_index = 0;
    imm_cache->curr_ir1_index = 0;
    imm_cache->curr_ir2_index = 0;
    imm_cache->optimized_ir2 = false;
    imm_cache->itemp_allocated = false;
    memset(&imm_cache->rip_stats, 0, sizeof(imm_cache->rip_stats));
    memset(&imm_cache->complex_stats, 0, sizeof(imm_cache->complex_stats));
    for (int i = 0; i < 16; i++) {
        imm_cache->ir1_reg_last_updated_index[i] = -1;
    }
    lsassertm((imm_cache->bucket != NULL),
              "imm_cache buckets memory is NOT malloced \n");
    for (int i = 0; i < 4; i++) {
        itemp_stat[i] = 1;
    }
    for (int i = 0; i < imm_cache->cache_count; i++) {
        imm_cache_init_bucket(imm_cache->bucket, i);
    }
}

void imm_cache_replace_bucket(IMM_CACHE *cache, int cache_id, int base,
                              int index, int scale, int64 offset)
{
    // pre cache changed to cached
    IMM_CACHE_BUCKET *bucket = &(cache->bucket[cache_id]);
    imm_cache_fill_bucket(cache, cache_id, base, index, scale, offset);
    bucket->lifecycle = 0;
    bucket->pre_cache = false;
    bucket->curr_ir1_index = cache->curr_ir1_index;
    bucket->use_count = 1;
}

void imm_cache_fill_bucket(IMM_CACHE *cache, int cache_id, int base, int index,
                           int scale, int64 offset)
{
    /* new bucket data */
    lsassertm(
        (cache_id < cache->cache_count),
        "imm_cache_fill_bucket fail: cache_id %d > cache_count %d fail \n",
        cache_id, cache->cache_count);

    cache->bucket[cache_id].base = base;
    cache->bucket[cache_id].index = index;
    cache->bucket[cache_id].scale = scale;
    cache->bucket[cache_id].offset = offset;

    cache->bucket[cache_id].avg_offset = 0;

    cache->bucket[cache_id].free = false;
    cache->bucket[cache_id].use_count = 0;
}

static void imm_cache_mark_gpr_updated(IMM_CACHE *cache, int reg_num,
                                       int curr_ir1_index)
{
    if (reg_num >= 0 && reg_num < 16) {
        cache->ir1_reg_last_updated_index[reg_num] = curr_ir1_index;
    }
}

void imm_cache_update_ir1_usage(IMM_CACHE *cache, IR1_INST *pir1,
                                int curr_ir1_index)
{
    IR1_OPCODE opcode = ir1_opcode(pir1);
    int op_count = pir1->info->x86.op_count;
    bool string_op = false;

    for (int i = 0; i < op_count; i++) {
        IR1_OPND *opnd = ir1_get_opnd(pir1, i);

        if (ir1_opnd_is_gpr(opnd) &&
            (opnd->access & dt_CS_AC_WRITE)) {
            imm_cache_mark_gpr_updated(cache, ir1_opnd_base_reg_num(opnd),
                                       curr_ir1_index);
        }
    }

    /* Preserve the historical operand-zero fallback for incomplete metadata. */
    if (op_count > 0) {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

        if (ir1_opnd_is_gpr(opnd0) &&
            opnd0->access == dt_CS_AC_INVALID) {
            imm_cache_mark_gpr_updated(cache,
                                       ir1_opnd_base_reg_num(opnd0),
                                       curr_ir1_index);
            cache->complex_stats.use_def_fallbacks++;
        }
    }

    /*
     * CAPSTONE_DIET omits implicit register metadata.  Conservatively treat
     * operand-less instructions as clobbering every GPR so an unlisted hidden
     * destination can only reduce cache reuse, never leave a stale address.
     */
    if (option_imm_complex && op_count == 0) {
        for (int i = 0; i < 16; i++) {
            imm_cache_mark_gpr_updated(cache, i, curr_ir1_index);
        }
    }

    switch (opcode) {
    case dt_X86_INS_PUSH:
    case dt_X86_INS_PUSHF:
    case dt_X86_INS_PUSHFD:
    case dt_X86_INS_PUSHFQ:
    case dt_X86_INS_PUSHAL:
    case dt_X86_INS_PUSHAW:
    case dt_X86_INS_POP:
    case dt_X86_INS_POPF:
    case dt_X86_INS_POPFD:
    case dt_X86_INS_POPFQ:
        imm_cache_mark_gpr_updated(cache, esp_index, curr_ir1_index);
        break;
    case dt_X86_INS_ENTER:
    case dt_X86_INS_LEAVE:
        imm_cache_mark_gpr_updated(cache, esp_index, curr_ir1_index);
        imm_cache_mark_gpr_updated(cache, ebp_index, curr_ir1_index);
        break;
    case dt_X86_INS_MUL:
    case dt_X86_INS_DIV:
    case dt_X86_INS_IDIV:
        imm_cache_mark_gpr_updated(cache, eax_index, curr_ir1_index);
        imm_cache_mark_gpr_updated(cache, edx_index, curr_ir1_index);
        break;
    case dt_X86_INS_IMUL:
        if (op_count == 1) {
            imm_cache_mark_gpr_updated(cache, eax_index, curr_ir1_index);
            imm_cache_mark_gpr_updated(cache, edx_index, curr_ir1_index);
        }
        break;
    case dt_X86_INS_CBW:
    case dt_X86_INS_CWDE:
    case dt_X86_INS_CDQE:
    case dt_X86_INS_LAHF:
    case dt_X86_INS_SALC:
    case dt_X86_INS_IN:
    case dt_X86_INS_XLATB:
        imm_cache_mark_gpr_updated(cache, eax_index, curr_ir1_index);
        break;
    case dt_X86_INS_CWD:
    case dt_X86_INS_CDQ:
    case dt_X86_INS_CQO:
        imm_cache_mark_gpr_updated(cache, edx_index, curr_ir1_index);
        break;
    case dt_X86_INS_CMPXCHG:
        imm_cache_mark_gpr_updated(cache, eax_index, curr_ir1_index);
        break;
    case dt_X86_INS_CMPXCHG8B:
    case dt_X86_INS_CMPXCHG16B:
    case dt_X86_INS_RDTSC:
    case dt_X86_INS_XGETBV:
        imm_cache_mark_gpr_updated(cache, eax_index, curr_ir1_index);
        imm_cache_mark_gpr_updated(cache, edx_index, curr_ir1_index);
        break;
    case dt_X86_INS_RDTSCP:
        imm_cache_mark_gpr_updated(cache, eax_index, curr_ir1_index);
        imm_cache_mark_gpr_updated(cache, ecx_index, curr_ir1_index);
        imm_cache_mark_gpr_updated(cache, edx_index, curr_ir1_index);
        break;
    case dt_X86_INS_CPUID:
        imm_cache_mark_gpr_updated(cache, eax_index, curr_ir1_index);
        imm_cache_mark_gpr_updated(cache, ebx_index, curr_ir1_index);
        imm_cache_mark_gpr_updated(cache, ecx_index, curr_ir1_index);
        imm_cache_mark_gpr_updated(cache, edx_index, curr_ir1_index);
        break;
    case dt_X86_INS_LODSB:
    case dt_X86_INS_LODSW:
    case dt_X86_INS_LODSD:
    case dt_X86_INS_LODSQ:
        imm_cache_mark_gpr_updated(cache, eax_index, curr_ir1_index);
        imm_cache_mark_gpr_updated(cache, esi_index, curr_ir1_index);
        string_op = true;
        break;
    case dt_X86_INS_STOSB:
    case dt_X86_INS_STOSW:
    case dt_X86_INS_STOSD:
    case dt_X86_INS_STOSQ:
    case dt_X86_INS_SCASB:
    case dt_X86_INS_SCASW:
    case dt_X86_INS_SCASD:
    case dt_X86_INS_SCASQ:
    case dt_X86_INS_INSB:
    case dt_X86_INS_INSW:
    case dt_X86_INS_INSD:
        imm_cache_mark_gpr_updated(cache, edi_index, curr_ir1_index);
        string_op = true;
        break;
    case dt_X86_INS_MOVSB:
    case dt_X86_INS_MOVSW:
    case dt_X86_INS_MOVSQ:
    case dt_X86_INS_CMPSB:
    case dt_X86_INS_CMPSW:
    case dt_X86_INS_CMPSQ:
        imm_cache_mark_gpr_updated(cache, esi_index, curr_ir1_index);
        imm_cache_mark_gpr_updated(cache, edi_index, curr_ir1_index);
        string_op = true;
        break;
    case dt_X86_INS_MOVSD:
        if (pir1->info->x86.opcode[0] == 0xa5) {
            imm_cache_mark_gpr_updated(cache, esi_index, curr_ir1_index);
            imm_cache_mark_gpr_updated(cache, edi_index, curr_ir1_index);
            string_op = true;
        }
        break;
    case dt_X86_INS_CMPSD:
        if (pir1->info->x86.opcode[0] == 0xa7) {
            imm_cache_mark_gpr_updated(cache, esi_index, curr_ir1_index);
            imm_cache_mark_gpr_updated(cache, edi_index, curr_ir1_index);
            string_op = true;
        }
        break;
    case dt_X86_INS_XCHG:
    case dt_X86_INS_XADD:
    case dt_X86_INS_MULX:
        /* These instructions have two explicit destination operands. */
        for (int i = 0; i < MIN(op_count, 2); i++) {
            IR1_OPND *opnd = ir1_get_opnd(pir1, i);

            if (ir1_opnd_is_gpr(opnd)) {
                imm_cache_mark_gpr_updated(
                    cache, ir1_opnd_base_reg_num(opnd), curr_ir1_index);
            }
        }
        break;
    case dt_X86_INS_POPAL:
    case dt_X86_INS_POPAW:
        for (int i = 0; i < 16; i++) {
            imm_cache_mark_gpr_updated(cache, i, curr_ir1_index);
        }
        break;
    default:
        break;
    }

    if (string_op &&
        (ir1_prefix(pir1) == dt_X86_PREFIX_REP ||
         ir1_prefix(pir1) == dt_X86_PREFIX_REPNE)) {
        imm_cache_mark_gpr_updated(cache, ecx_index, curr_ir1_index);
    }
}

void imm_cache_check_ir1_should_skip(bool *bool_skip_ptr)
{
    /* skip ir1 whose translation uses too much itemp */
    IR1_OPCODE cur_ir1_op = ir1_opcode(lsenv->tr_data->curr_ir1_inst);
    switch (cur_ir1_op) {
    case dt_X86_INS_FNSTENV:
        *bool_skip_ptr = true;
        break;
    default:
        break;
    }
}

// ========================
//    ir1 precache functions
// ========================

/**
 * precache_put
 * new avg_offset may exceed si12 range
 * 1. base(-100) indicates rip
 * put and update avg_offset
 * check whether new avg_offset can be reached by min and max offset
 */
void imm_cache_precache_put(IMM_CACHE *cache, int base, int index, int scale,
                            int64 offset)
{
    imm_log("[precache put] base:%4d\tindex:%2d\tscale:%2d\toffset:0x%lx", base,
            index, scale, offset);

    int cache_id = imm_cache_get(cache, base, index, scale, offset);
    if (cache_id >= 0) {
        // check if new_avg_offset will make cache si12 overflow
        int64 min_offset = MIN(offset, cache->bucket[cache_id].min_offset);
        int64 max_offset = MAX(offset, cache->bucket[cache_id].max_offset);
        long new_avg_offset = (min_offset + max_offset) / 2;
        long diff = max_offset - new_avg_offset;
        if (si12_overflow(diff)) {
            /* new offset will make cache invalid(si12 oaverflow) */
            imm_log("[precache put hit but overflow]:%d", cache_id);
            cache_id = -2;
        }
    }

    if (cache_id == -1 || cache_id == -2) {
        // new pre cache
        cache_id = cache->cache_count - cache->free_count;
        imm_log("\t[new]:%d\n", cache_id);
        imm_cache_fill_bucket(cache, cache_id, base, index, scale, offset);
        cache->free_count--;

        cache->bucket[cache_id].curr_ir1_index = cache->curr_ir1_index;
    } else {
        // exist
        imm_log("[precache put hit]:%d\n", cache_id);
    }
    /*new or old bucket update these fields*/
    int64 min_offset = MIN(offset, cache->bucket[cache_id].min_offset);
    int64 max_offset = MAX(offset, cache->bucket[cache_id].max_offset);
    long new_avg_offset = (min_offset + max_offset) / 2;
    cache->bucket[cache_id].avg_offset = new_avg_offset;
    cache->bucket[cache_id].min_offset = min_offset;
    cache->bucket[cache_id].max_offset = max_offset;
    cache->bucket[cache_id].use_count++;
    cache->bucket[cache_id].lifecycle = cache->curr_imm_index;
    cache->curr_imm_index++;
}

/**
 * use data collected when precache ir1 list
 * strategy:
 * 1.sort cache
 * 2.calculate avg offset and check overflow
 *  2.1 only imm with base/index or rip needs avg_offset
 * 3.fix top3 imm in head of translate
 */
void imm_cache_finish_precache(IMM_CACHE *cache)
{
    // sort by usage frequence
    imm_cache_sort(cache);
    // reset cache_count
    cache->cache_count = CACHE_DEFAULT_CAPACITY;
    cache->curr_imm_index = 0;
    cache->curr_ir1_index = 0;
    cache->curr_ir2_index = 0;
    cache->optimized_ir2 = false;
    cache->itemp_allocated = false;
    cache->free_count = 0;
    // fix top3 cache clear up [3-capacity] bucket
    for (int i = 0; i < cache->cache_count; i++) {
        if (cache->bucket[i].free || i >= 3) {
            imm_cache_init_bucket(cache->bucket, i);
            cache->free_count++;
        } else {
            cache->bucket[i].pre_cache = true;
            cache->bucket[i].itemp_num = alloc_itemp();
        }
    }

    imm_log("[precache finish]finish precache.\n");
    imm_cache_print_cache(cache);
}

// ===============================
//   cache validation functions
// ===============================
bool imm_cache_can_use(IMM_CACHE *cache, int bucket_id, int base, int index)
{
    /*
     * 1. precache cache
     * 2. normal cache
     * */

    return !imm_cache_check_base_index_has_changed(cache, bucket_id, base,
                                                   index);
}

bool imm_cache_check_base_index_has_changed(IMM_CACHE *cache, int bucket_id,
                                            int base_reg_num, int index_reg_num)
{
    /* skip no 'base+index' imm case */
    if (base_reg_num < 0 && index_reg_num < 0) {
        return false;
    }

    // check whether base/index updated
    int last_update_base = -1;
    int last_update_index = -1;
    IMM_CACHE_BUCKET *bucket = &(cache->bucket[bucket_id]);
    if (base_reg_num >= 0) {
        last_update_base = cache->ir1_reg_last_updated_index[base_reg_num];
    }
    if (index_reg_num >= 0) {
        last_update_index = cache->ir1_reg_last_updated_index[index_reg_num];
    }
    if (last_update_base >= bucket->curr_ir1_index ||
        last_update_index >= bucket->curr_ir1_index) {
        imm_log("[reg change] ir1_index_cache:%d\t"
                "last_update_base:%d\t"
                "last_update_index:%d\t\n",
                bucket->curr_ir1_index, last_update_base, last_update_index);
        return true;
    }
    return false;
}

// ===============================
//   cache fetch/store functions
// ===============================

/**
 * return the cache index of the imm
 * return -1 if miss (including overflow)
 */
int imm_cache_get(IMM_CACHE *cache, int base, int index, int scale,
                  int64 offset)
{
    for (int i = 0; i < cache->cache_count; i++) {
        IMM_CACHE_BUCKET *cachei = &(cache->bucket[i]);
        if (cachei->index == index && cachei->base == base &&
            cachei->scale == scale && cachei->free == false) {
            /**
             * use avg_offset during precache, use offset during translation
             */
            int64 criterion_offset =
                cachei->avg_offset == 0 ? cachei->offset : cachei->avg_offset;

            /* hit */
            long diff = (long)(offset - criterion_offset);
            if (si12_overflow(diff)) {
                // overflow
                // maybe overflowed this bucket but hit another.
                continue;
            } else {
                imm_log("\t[cache get]diff:%lx\n", diff);
                return i;
            }
        }
    }
    return -1;
}

/**
 * 1. clean dead cache
 * 2. get cache > cache_id
 * 3. cache_id
 * 4.
 *
 */
IMM_CACHE_RES imm_cache_allocate(IMM_CACHE *cache, int base, int index,
                                 int scale, int64 offset)
{
    cache_call++;
    // skip imm opt if pc == 0x...
    static bool stop_opt = false;

    if (unlikely(imm_skip_pc != 0)) {
        if (!stop_opt && cache->curr_pc == imm_skip_pc) {
            stop_opt = true;
        }
    }
    bool is_rip = base == IMM_CACHE_RIP_BASE;
    int complex_kind = -1;

    if (!is_rip) {
        if (base >= 0 && index >= 0) {
            complex_kind = IMM_CACHE_COMPLEX_BASE_INDEX_DISP;
        } else if (base >= 0) {
            complex_kind = IMM_CACHE_COMPLEX_BASE_DISP;
        } else if (index >= 0) {
            complex_kind = IMM_CACHE_COMPLEX_INDEX_DISP;
        }
    }
    const char *prefix = is_rip ? "rip" : "complex";
    if (is_rip) {
        cache->rip_stats.calls++;
    } else if (complex_kind >= 0) {
        cache->complex_stats.calls[complex_kind]++;
    }
    imm_log("[allocate %s]>>>>>>>>>> start >>>>>>>>>>>\n"
            "[allocate %s] b:%d\ti:%d\ts:%d\toffset:0x%lx\n",
            prefix, prefix, base, index, scale, offset);

    imm_cache_sort(cache);
    imm_cache_print_cache(cache);

    // dead cache auto free
    // imm_cache_free_dead_cache(cache);

    IMM_CACHE_RES res = {
        .diff = 0,
        .offset = offset,
        .itemp_num = -1,
        .cache_id = -1,
        .pre_cache = false,
        .cached = false,
    };
    int cache_id = -1;

    if (likely(!stop_opt)) {
        cache_id = imm_cache_get(cache, base, index, scale, offset);
    }
    if (cache_id == -1) {
        // a new cache
        imm_log("[new]\t\n");
        if (is_rip) {
            cache->rip_stats.misses++;
            if (cache->free_count == 0) {
                cache->rip_stats.replacements++;
            }
        } else if (complex_kind >= 0) {
            cache->complex_stats.misses[complex_kind]++;
            if (cache->free_count == 0) {
                cache->complex_stats.replacements[complex_kind]++;
            }
        }
        cache_id = imm_cache_put(cache, base, index, scale, offset);
        res.cached = false;
    } else {
        // cached, check validation
        if (imm_cache_can_use(cache, cache_id, base, index)) {
            long diff = imm_cache_diff(cache, cache_id, offset);
            if (cache->bucket[cache_id].avg_offset == 0) {
                res.offset = cache->bucket[cache_id].offset;
            } else {
                res.offset = cache->bucket[cache_id].avg_offset;
            }
            if (cache->bucket[cache_id].pre_cache) {
                cache->bucket[cache_id].pre_cache = false;
                res.pre_cache = true;
            }
            res.diff = diff;
            res.cached = true;
            cache->bucket[cache_id].use_count++;
            cache_hit++;
            if (is_rip) {
                cache->rip_stats.hits++;
            } else if (complex_kind >= 0) {
                cache->complex_stats.hits[complex_kind]++;
            }
        } else {
            // repace cache
            imm_log("[cache cannot use] replace cache_id:%d\n", cache_id);
            // imm_cache_init_bucket(cache->bucket, cache_id);
            imm_cache_replace_bucket(cache, cache_id, base, index, scale,
                                     offset);
            res.offset = offset;
            res.cached = false;
            if (is_rip) {
                cache->rip_stats.misses++;
                cache->rip_stats.replacements++;
            } else if (complex_kind >= 0) {
                cache->complex_stats.misses[complex_kind]++;
                cache->complex_stats.replacements[complex_kind]++;
                cache->complex_stats.base_index_invalidations++;
            }

            // imm_cache_sort(cache);
        }
    }
    res.cache_id = cache_id;
    res.itemp_num = cache->bucket[cache_id].itemp_num;
    //=======cache setting after allocate======
    cache->curr_imm_index++;
    cache->optimized_ir2 = true;

    imm_cache_print_cache(cache);
    imm_log(
        "[allocate %s]diff:%lx\toffset:%lx\titemp_num:%d\tcache_id:%d\tpre_cache:%d\tcached:%d\n"
        "curr_ir2_index:%d\tir2_inst_num_current:%d\n"
        "[statistic]hit:%ld\ttotal:%ld\trate:%.2f%%\n"
        "[allocate %s]<<<<<<<<<< end <<<<<<<<<<<<<\n",
        prefix, res.diff, res.offset, res.itemp_num, res.cache_id,
        res.pre_cache, res.cached, cache->curr_ir2_index,
        lsenv->tr_data->real_ir2_inst_num, cache_hit, cache_call,
        (float)cache_hit / cache_call * 100, prefix);

    return res;
}
/**
 * succeed imm will fetch the first cache value
 * avg_offset indicates whether the offset is pre cached in ahead
 * if avg_offset, should not update
 * else update offset, avg_offset should always be 0;
 * return diff;
 */
long imm_cache_diff(IMM_CACHE *cache, int cache_id, int64 new_offset)
{
    qemu_log_mask(LAT_IMM_REG, "[hit fetch diff]\n");
    IMM_CACHE_BUCKET *bucket = &(cache->bucket[cache_id]);
    int64 criterion_offset =
        bucket->avg_offset == 0 ? bucket->offset : bucket->avg_offset;

    /* hit */
    long diff = (long)(new_offset - criterion_offset);
    return diff;
}

void imm_cache_update_by_diff(IMM_CACHE *cache, int cache_id, long diff)
{
    IMM_CACHE_BUCKET *bucket = &cache->bucket[cache_id];
    int64 old_offset =
        bucket->avg_offset == 0 ? bucket->offset : bucket->avg_offset;

    qemu_log_mask(LAT_IMM_REG, "[update by diff]new:%lx+%lx=%lx\n", old_offset,
                  diff, old_offset + diff);
    bucket->offset = old_offset + diff;
    bucket->avg_offset = 0;
}

void imm_cache_update_by_offset(IMM_CACHE *cache, int cache_id,
                                int64 new_offset)
{
    qemu_log_mask(LAT_IMM_REG, "[update by offset]new:%lx\n", new_offset);
    cache->bucket[cache_id].offset = new_offset;
}
/**
 * called during translation period(not ir1 precache)
 * 1. a new cache( include 'new' and 'miss' )
 * 2. never changes avg_offset
 * 3. find a free cache, if none, free the least used one
 */
int imm_cache_put(IMM_CACHE *cache, int base, int index, int scale,
                  int64 offset)
{
    int cache_id = -1;
    if (cache->free_count == 0) {
        /*replace the least used cache */
        cache_id = cache->cache_count - 1;
        imm_log("[replace]%d\n", cache_id);
        imm_cache_free(cache, cache_id);
        cache->free_count++;
    } else {
        cache_id = cache->cache_count - cache->free_count;
        imm_log("[use]%d\t", cache_id);
    }

    imm_cache_fill_bucket(cache, cache_id, base, index, scale, offset);
    cache->free_count--;

    cache->bucket[cache_id].curr_ir1_index = cache->curr_ir1_index;
    cache->bucket[cache_id].use_count++;
    cache->bucket[cache_id].itemp_num = alloc_itemp();
    return cache_id;
}

void imm_cache_free_cache_use_target_reg(IR2_OPND opnd)
{
    IMM_CACHE *cache = lsenv->tr_data->imm_cache;
    int itemp_num = reg_itemp_reverse_map[opnd._reg_num];
    for (int i = 0; i < cache->cache_count; i++) {
        if (cache->bucket[i].itemp_num == itemp_num && !cache->bucket[i].free) {
            imm_cache_free(cache, i);
            cache->free_count++;
        }
    }
}

void imm_cache_free_itemp(IMM_CACHE *cache, int itemp_num)
{
    for (int i = 0; i < cache->cache_count; i++) {
        if (cache->bucket[i].itemp_num == itemp_num && !cache->bucket[i].free) {
            imm_cache_free(cache, i);
            cache->free_count++;
        }
    }
}

/**
 * we need clean the cache during precache put
 * */
void imm_cache_free_dead_cache(IMM_CACHE *cache)
{
    imm_log("[allocate][free dead]");
    for (int i = 0; i < cache->cache_count; i++) {
        int life = cache->bucket[i].lifecycle;
        if (!cache->bucket[i].free && life != 0 &&
            life < cache->curr_imm_index) {
            imm_log("%d\t", i);
            imm_cache_free(cache, i);
            cache->free_count++;
        }
    }

    imm_log("\n");
    imm_cache_sort(cache);
    imm_cache_print_cache(cache);
}

void imm_cache_free(IMM_CACHE *cache, int id)
{
    free_itemp(cache->bucket[id].itemp_num);
    imm_cache_init_bucket(cache->bucket, id);
}

void imm_cache_free_all(IMM_CACHE *cache)
{
    if (!cache) {
        return;
    }
    for (int i = 0; i < cache->cache_count; i++) {
        if (!cache->bucket[i].free) {
            imm_cache_free(cache, i);
        }
    }
    cache->free_count = cache->cache_count;
    cache->itemp_allocated = false;
}

void imm_cache_invalidate_for_helper(void)
{
    IMM_CACHE *cache;
    bool has_rip = false;

    if (!option_imm_reg || !lsenv || !lsenv->tr_data) {
        return;
    }
    cache = lsenv->tr_data->imm_cache;
    if (!cache) {
        return;
    }
    for (int i = 0; i < cache->cache_count; i++) {
        if (!cache->bucket[i].free &&
            cache->bucket[i].base == IMM_CACHE_RIP_BASE) {
            has_rip = true;
            break;
        }
    }
    if (has_rip) {
        cache->rip_stats.helper_invalidations++;
    }
    imm_cache_free_all(cache);
}

void imm_cache_print_rip_stats(IMM_CACHE *cache, uint64_t tb_pc)
{
    IMM_CACHE_RIP_STATS *stats;

    if (!option_imm_rip_stats || !cache) {
        return;
    }
    stats = &cache->rip_stats;
    if (!stats->calls) {
        return;
    }
    fprintf(stderr,
            "[LATX][imm-rip] tb=0x%" PRIx64
            " calls=%" PRIu64 " hits=%" PRIu64 " misses=%" PRIu64
            " replacements=%" PRIu64 " itemp-fallbacks=%" PRIu64
            " direct-hits=%" PRIu64 " host-offset-hits=%" PRIu64
            " addi-hits=%" PRIu64 " full-loads=%" PRIu64
            " helper-invalidations=%" PRIu64 "\n",
            tb_pc, stats->calls, stats->hits, stats->misses,
            stats->replacements, stats->itemp_fallbacks,
            stats->direct_hits, stats->host_offset_hits, stats->addi_hits,
            stats->full_loads, stats->helper_invalidations);
}

void imm_cache_print_complex_stats(IMM_CACHE *cache, uint64_t tb_pc)
{
    static const char * const kind_names[] = {
        "base-disp", "index-disp", "base-index-disp",
    };
    IMM_CACHE_COMPLEX_STATS *stats;
    uint64_t activity = 0;

    if (!option_imm_complex_stats || !cache) {
        return;
    }
    stats = &cache->complex_stats;
    for (int i = 0; i < IMM_CACHE_COMPLEX_KIND_COUNT; i++) {
        activity += stats->calls[i];
    }
    for (int i = 0; i < IMM_CACHE_COMPLEX_SKIP_COUNT; i++) {
        activity += stats->skips[i];
    }
    if (!activity) {
        return;
    }

    for (int i = 0; i < IMM_CACHE_COMPLEX_KIND_COUNT; i++) {
        fprintf(stderr,
                "[LATX][imm-complex] tb=0x%" PRIx64 " mode=%s"
                " calls=%" PRIu64 " hits=%" PRIu64 " misses=%" PRIu64
                " replacements=%" PRIu64 " itemp-conflicts=%" PRIu64
                " direct-hits=%" PRIu64 " host-offset-hits=%" PRIu64
                " addi-hits=%" PRIu64 " full-generations=%" PRIu64 "\n",
                tb_pc, kind_names[i], stats->calls[i], stats->hits[i],
                stats->misses[i], stats->replacements[i],
                stats->itemp_conflicts[i], stats->direct_hits[i],
                stats->host_offset_hits[i], stats->addi_hits[i],
                stats->full_generations[i]);
    }
    fprintf(stderr,
            "[LATX][imm-complex-skip] tb=0x%" PRIx64
            " disabled=%" PRIu64 " not-code64=%" PRIu64
            " addr-size=%" PRIu64 " segment=%" PRIu64
            " unsupported-form=%" PRIu64 " invalid-scale=%" PRIu64
            " no-benefit=%" PRIu64 " instruction=%" PRIu64
            " fixed-dest=%" PRIu64 " base-index-invalidations=%" PRIu64
            " use-def-fallbacks=%" PRIu64 "\n",
            tb_pc,
            stats->skips[IMM_CACHE_COMPLEX_SKIP_DISABLED],
            stats->skips[IMM_CACHE_COMPLEX_SKIP_NOT_CODE64],
            stats->skips[IMM_CACHE_COMPLEX_SKIP_ADDR_SIZE],
            stats->skips[IMM_CACHE_COMPLEX_SKIP_SEGMENT],
            stats->skips[IMM_CACHE_COMPLEX_SKIP_UNSUPPORTED_FORM],
            stats->skips[IMM_CACHE_COMPLEX_SKIP_INVALID_SCALE],
            stats->skips[IMM_CACHE_COMPLEX_SKIP_NO_BENEFIT],
            stats->skips[IMM_CACHE_COMPLEX_SKIP_INSTRUCTION],
            stats->skips[IMM_CACHE_COMPLEX_SKIP_FIXED_DEST],
            stats->base_index_invalidations, stats->use_def_fallbacks);
}

void imm_cache_print_bucket(IMM_CACHE_BUCKET *bucket, int index)
{
    IMM_CACHE_BUCKET b = bucket[index];
    imm_log("[Dis][%d] itemp:%d\t"
            "b:%4d\ti:%2d\ts:%2d\toffset:0x%lx\t"
            "avg_offset:0x%lx\tmin:0x%lx\tmax:0x%lx\t"
            "use:%d\tir1_index:%d\n",
            index, b.itemp_num, b.base, b.index, b.scale, b.offset,
            b.avg_offset, b.min_offset, b.max_offset, b.use_count,
            b.curr_ir1_index);
}

void imm_cache_print_cache(IMM_CACHE *cache)
{
    if (!option_debug_imm_reg) {
        return;
    }
    imm_log("[Dis]start.\n"
            "[Dis]capacity:%d\tfree:%d {",
            cache->cache_count, cache->free_count);
    for (int i = 0; i < 4; i++) {
        imm_log(" %d", itemp_stat[i]);
    }
    imm_log("}\timm_index:%d\tir1_index:%d\n", cache->curr_imm_index,
            cache->curr_ir1_index);
    for (int i = 0; i < cache->cache_count; i++) {
        if (!cache->bucket[i].free) {
            imm_cache_print_bucket(cache->bucket, i);
        }
    }
    imm_log("[Dis]end.\n");
}

void imm_cache_print_ir2(IR2_INST *ir2, int index)
{
    char str[64];
    if (ir2_opcode(ir2) == 0) {
        return;
    }
    ir2_to_string(ir2, str);

    if (str[0] == '-') {
        imm_log("[%d]%s\n", index, str);
    } else {
        imm_log("[%d]%s\n", index, str);
    }
}

void imm_cache_print_ir1(IR1_INST *pir1)
{
#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
    imm_log("IR1: %s\t%s\n", pir1->info->mnemonic, pir1->info->op_str);
#endif
}

void imm_cache_print_tb_ir1(TranslationBlock *tb)
{
#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
    IR1_INST *pir1 = tb_ir1_inst(tb, 0);
    for (int i = 0; i < tb->icount; ++i) {
        imm_log("IR1[%d]\t: %s\t%s\n", i, pir1->info->mnemonic,
                pir1->info->op_str);
        pir1++;
    }
#endif
}

void imm_cache_print_tr_ir2_if_opted(void)
{
#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
    TRANSLATION_DATA *t = lsenv->tr_data;
    if (t->imm_cache->optimized_ir2) {
        IR1_INST *pir1 = t->curr_ir1_inst;
        imm_log("IR1[%d]\t: %s\t%s\n", t->curr_ir1_count, pir1->info->mnemonic,
                pir1->info->op_str);
        for (int i = t->imm_cache->curr_ir2_index; i < t->ir2_inst_num_current;
             i++) {
            IR2_INST *pir2 = &t->ir2_inst_array[i];
            imm_cache_print_ir2(pir2, i);
        }
        t->imm_cache->optimized_ir2 = false;
    }
#endif
}
