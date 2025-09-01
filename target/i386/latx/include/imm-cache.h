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

#ifdef CONFIG_LATX_DECODE_DEBUG
void imm_cache_update_ir1_usage(IMM_CACHE *cache, IR1_INST *pir1,
                                int curr_ir1_index);
#endif
void imm_cache_update_ir1_usage_bd(IMM_CACHE *cache, IR1_INST *pir1,
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
bool imm_cache_is_imm_itemp(int itemp_num);
bool imm_cache_itemp_is_used(IR2_OPND opnd);
void imm_cache_free_cache_use_target_reg(IR2_OPND opnd);

//=========debug========
void imm_cache_print_cache(IMM_CACHE *cache);
void imm_cache_print_bucket(IMM_CACHE_BUCKET *bucket, int index);
void imm_cache_print_ir2(IR2_INST *ir2, int index);
void imm_cache_print_tb_ir1(TranslationBlock *tb);
void imm_cache_print_tb_ir1_bd(TranslationBlock *tb);
void imm_cache_print_ir1(IR1_INST *pir1);
void imm_cache_print_ir1_bd(IR1_INST *pir1);
void imm_cache_print_tr_ir2_if_opted(void);
#endif
