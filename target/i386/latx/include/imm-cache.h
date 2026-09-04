/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file imm-cache.h
 * @author liuxinpeng <ksliuxp@163.com>
 * @brief IMM optimization header file
 */
#ifndef _IMM_CACHE_H_
#define _IMM_CACHE_H_

#include "common.h"
#include "ir1.h"
#include "ir2.h"
#include "latx-options.h"

#define CACHE_DEFAULT_CAPACITY 4
#define CACHE_MAX_CAPACITY 300
#define IMM_CACHE_RIP_BASE (-100)

#define imm_log(FMT, ...)                                   \
    do {                                                    \
        if (option_debug_imm_reg) {                         \
            qemu_log_mask(LAT_IMM_REG, FMT, ##__VA_ARGS__); \
        }                                                   \
    } while (0)

typedef struct {
    /* addition data */
    bool free;
    int use_count;
    // last used imm index in tb
    int lifecycle;
    // 3-6
    int itemp_num;
    bool pre_cache;

    /* bucket data */
    /*base(-100)indicates rip*/
    int base;
    int index;
    int scale;

    /**
     * record the ir1 index when cached
     * later compare with last updated ir1 index to check whether base and
     * index reg changed
     */
    int curr_ir1_index;

    /* unscaned offset use 'offset' */
    int64 offset;
    /**
     * offset scaned use 'avg_offset'
     * use avg_offset to maxmize the offset range which si12 can reach
     */
    int64 avg_offset;
    int64 min_offset;
    int64 max_offset;
} IMM_CACHE_BUCKET;

typedef struct {
    uint64_t calls;
    uint64_t hits;
    uint64_t misses;
    uint64_t replacements;
    uint64_t itemp_fallbacks;
    uint64_t direct_hits;
    uint64_t host_offset_hits;
    uint64_t addi_hits;
    uint64_t full_loads;
    uint64_t helper_invalidations;
} IMM_CACHE_RIP_STATS;

typedef enum {
    IMM_CACHE_COMPLEX_BASE_DISP,
    IMM_CACHE_COMPLEX_INDEX_DISP,
    IMM_CACHE_COMPLEX_BASE_INDEX_DISP,
    IMM_CACHE_COMPLEX_KIND_COUNT,
} IMM_CACHE_COMPLEX_KIND;

typedef enum {
    IMM_CACHE_COMPLEX_SKIP_DISABLED,
    IMM_CACHE_COMPLEX_SKIP_NOT_CODE64,
    IMM_CACHE_COMPLEX_SKIP_ADDR_SIZE,
    IMM_CACHE_COMPLEX_SKIP_SEGMENT,
    IMM_CACHE_COMPLEX_SKIP_UNSUPPORTED_FORM,
    IMM_CACHE_COMPLEX_SKIP_INVALID_SCALE,
    IMM_CACHE_COMPLEX_SKIP_NO_BENEFIT,
    IMM_CACHE_COMPLEX_SKIP_INSTRUCTION,
    IMM_CACHE_COMPLEX_SKIP_FIXED_DEST,
    IMM_CACHE_COMPLEX_SKIP_COUNT,
} IMM_CACHE_COMPLEX_SKIP_REASON;

typedef struct {
    uint64_t calls[IMM_CACHE_COMPLEX_KIND_COUNT];
    uint64_t hits[IMM_CACHE_COMPLEX_KIND_COUNT];
    uint64_t misses[IMM_CACHE_COMPLEX_KIND_COUNT];
    uint64_t replacements[IMM_CACHE_COMPLEX_KIND_COUNT];
    uint64_t itemp_conflicts[IMM_CACHE_COMPLEX_KIND_COUNT];
    uint64_t direct_hits[IMM_CACHE_COMPLEX_KIND_COUNT];
    uint64_t host_offset_hits[IMM_CACHE_COMPLEX_KIND_COUNT];
    uint64_t addi_hits[IMM_CACHE_COMPLEX_KIND_COUNT];
    uint64_t full_generations[IMM_CACHE_COMPLEX_KIND_COUNT];
    uint64_t skips[IMM_CACHE_COMPLEX_SKIP_COUNT];
    uint64_t base_index_invalidations;
    uint64_t use_def_fallbacks;
} IMM_CACHE_COMPLEX_STATS;

/**
 * imm format:
 *   (offset can be 0)
 *   regular imm = offset
 *   rip imm = rip + offset
 *   complex mem = base + index * scale + offset
 *     compare base
 *     compare index and scale
 *     compare offset
 *     compare final mem
 * imm diff cache:
 * successor inst can use the diff to optimize the imm
 */
typedef struct {
    IMM_CACHE_BUCKET *bucket;

    int cache_count;
    int free_count;
    /* record curr imm index, lifecycle < curr_imm_index means cache die */
    int curr_imm_index;
    /*record ir1_index to be translated*/
    int curr_ir1_index;
    int curr_ir2_index;
    // if optimized_ir2, print if log
    bool optimized_ir2;
    long curr_pc;
    /**
     * true alloc fail and use ra itemp
     * 0 use cached itemp
     * if we use cached itemp, ra_free_temp should skip
     */
    bool itemp_allocated;
    IMM_CACHE_RIP_STATS rip_stats;
    IMM_CACHE_COMPLEX_STATS complex_stats;

    /**
     * for complex imm, we need to compare whether base/index reg has been
     * changed since last cached. record the latest ir1_reg(rax,etc.) updated
     * index compare the last updated index with the index of base/index reg
     * when caching
     * 1-16 eax_index - r15_index defined in reg_map.h
     */
    int ir1_reg_last_updated_index[16];
} IMM_CACHE;

/* imm_cache response */
typedef struct {
    long diff;
    int64 offset;
    int itemp_num;
    int cache_id;
    bool pre_cache;
    bool cached;
} IMM_CACHE_RES;

void imm_cache_init(IMM_CACHE *cache, int capacity);
void imm_cache_fill_bucket(IMM_CACHE *cache, int cache_id, int base, int index,
                           int scale, int64 offset);
void imm_cache_sort(IMM_CACHE *cache);
void imm_cache_replace_bucket(IMM_CACHE *cache, int cache_id, int base,
                              int index, int scale, int64 offset);
void imm_cache_precache_put(IMM_CACHE *cache, int base, int index, int scale,
                            int64 offset);

void imm_cache_finish_precache(IMM_CACHE *cache);
int imm_cache_get(IMM_CACHE *cache, int base, int index, int scale,
                  int64 offset);
bool imm_cache_can_use(IMM_CACHE *cache, int bucket_id, int base, int index);
bool imm_cache_check_base_index_has_changed(IMM_CACHE *cache, int bucket_id,
                                            int base_reg_num,
                                            int index_reg_num);

IMM_CACHE_RES imm_cache_allocate(IMM_CACHE *cache, int base, int index,
                                 int scale, int64 offset);

long imm_cache_diff(IMM_CACHE *cache, int cache_id, int64 new_offset);
void imm_cache_update_by_offset(IMM_CACHE *cache, int cache_id,
                                int64 new_offset);
void imm_cache_update_by_diff(IMM_CACHE *cache, int cache_id, long diff);

int imm_cache_put(IMM_CACHE *cache, int base, int index, int scale,
                  int64 offset);

void imm_cache_update_ir1_usage(IMM_CACHE *cache, IR1_INST *pir1,
                                int curr_ir1_index);

bool imm_cache_is_cached_at(IMM_CACHE *cache, int cache_index);
int imm_cache_la_reg_num_at(IMM_CACHE *cache, int cache_index);

//=======validation function=======
void imm_cache_check_ir1_should_skip(bool *bool_skip_ptr);

//======cache_free======
void imm_cache_free_itemp(IMM_CACHE *cache, int itemp_num);
void imm_cache_free_dead_cache(IMM_CACHE *cache);
void imm_cache_free(IMM_CACHE *cache, int id);
void imm_cache_free_all(IMM_CACHE *cache);
void imm_cache_invalidate_for_helper(void);
void imm_cache_print_rip_stats(IMM_CACHE *cache, uint64_t tb_pc);
void imm_cache_print_complex_stats(IMM_CACHE *cache, uint64_t tb_pc);
bool imm_cache_is_imm_itemp(int itemp_num);
bool imm_cache_itemp_is_used(IR2_OPND opnd);
void imm_cache_free_cache_use_target_reg(IR2_OPND opnd);

//=========debug========
void imm_cache_print_cache(IMM_CACHE *cache);
void imm_cache_print_bucket(IMM_CACHE_BUCKET *bucket, int index);
void imm_cache_print_ir2(IR2_INST *ir2, int index);
void imm_cache_print_tb_ir1(TranslationBlock *tb);
void imm_cache_print_ir1(IR1_INST *pir1);
void imm_cache_print_tr_ir2_if_opted(void);
#endif
