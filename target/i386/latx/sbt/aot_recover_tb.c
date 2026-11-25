/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file aot_recover_tb.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "qemu/osdep.h"

#include "aot_recover_tb.h"
#include "latx-options.h"
#include "hardware-ras.h"
#include "lsenv.h"
#include "accel/tcg/internal.h"
#include "exec/cpu-all.h"
#include "exec/translate-all.h"
#include "aot_page.h"
#include "aot_smc.h"
#include "smc_reload.h"
#ifdef CONFIG_LATX_TU
#include "tu.h"
#endif

#ifdef CONFIG_LATX_AOT

inline static void copy_code_to_buff(void *tb_buff, aot_tb *p_aot_tb) {
    assert(p_aot_tb->tu_size >= p_aot_tb->tb_cache_size);
    memcpy(tb_buff, aot_buffer + p_aot_tb->tb_cache_offset,
        p_aot_tb->tu_size);
    uintptr_t new_ptr = (uintptr_t)tcg_ctx->code_gen_ptr + p_aot_tb->tu_size;
    qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
                ROUND_UP(new_ptr, CODE_GEN_ALIGN));
}

inline static TranslationBlock *creat_tb(aot_tb *p_aot_tb, abi_ulong start, 
        target_ulong base, void *tc_ptr) {
    TranslationBlock *tb = tcg_tb_alloc(tcg_ctx);
    if (unlikely(!tb)) {
	return NULL;
    }
#ifdef CONFIG_LATX_PROFILER
    /* tcg_tb_alloc() does not initialize profile fields. Both TB and TU
     * recovery start new dynamic counters and restore only static IR2 data.
     */
    CLN_TB_PROFILE(tb);
    SET_TB_PROFILE(tb, nr_code, p_aot_tb->profile_nr_code);
#endif
    /* Initialize Tb */
    qemu_spin_init(&tb->jmp_lock);
    tb->lazypc[0] = p_aot_tb->lazypc[0];
    tb->lazypc[1] = p_aot_tb->lazypc[1];
    tb->canlink[0] = 1;
    tb->canlink[1] = 1;
    tb->lazylink[0] = 1;
    tb->lazylink[1] = 1;
    tb->cflags = p_aot_tb->cflags;
    tb->jmp_dest[0] = (uintptr_t)NULL;
    tb->jmp_dest[1] = (uintptr_t)NULL;
    tb->jmp_list_next[0] = (uintptr_t)NULL;
    tb->jmp_list_next[1] = (uintptr_t)NULL;
    tb->jmp_list_head = 0;
    tb->icount = p_aot_tb->icount;
    tb->tc.size = p_aot_tb->tb_cache_size;
    tb->tc.ptr = tc_ptr; 
    tb->pc = start + p_aot_tb->offset_in_segment;
    /* tb->cs_base = base; */
    tb->flags = p_aot_tb->flags;
    tb->size = p_aot_tb->size;
    tb->first_jmp_align = p_aot_tb->first_jmp_align;
    tb->bool_flags = p_aot_tb->bool_flags;
    tb->jmp_stub_target_arg[0] = p_aot_tb->jmp_stub_target_arg[0];
    tb->jmp_stub_target_arg[1] = p_aot_tb->jmp_stub_target_arg[1];
#ifdef CONFIG_LATX_TU
    if (is_tu_tb(tb)) {
        tb->tu_search_addr_offset = p_aot_tb->tu_search_addr_offset;
        tb->tu_cold_offset = p_aot_tb->tu_cold_offset;
        tb->tu_cold_size = p_aot_tb->tu_cold_size;
    } else {
        tb->tu_cold_offset = 0;
        tb->tu_cold_size = 0;
    }
#endif
    if (use_tu_jmp(tb)) {
        tb->tu_jmp[0] = p_aot_tb->tu_jmp[0];
        tb->tu_jmp[1] = p_aot_tb->tu_jmp[1];
        tb->tu_unlink.stub_offset = p_aot_tb->tu_unlink.stub_offset;
        tb->tu_unlink.ins = p_aot_tb->tu_unlink.ins;
    } else {
        tb->jmp_indirect = p_aot_tb->jmp_indirect;
        tb->jmp_reset_offset[0] = p_aot_tb->jmp_reset_offset[0];
        tb->jmp_reset_offset[1] = p_aot_tb->jmp_reset_offset[1];
        tb->jmp_target_arg[0] = p_aot_tb->jmp_target_arg[0];
        tb->jmp_target_arg[1] = p_aot_tb->jmp_target_arg[1];
        tb->jmp_stub_reset_offset[0] = p_aot_tb->jmp_stub_reset_offset[0];
        tb->jmp_stub_reset_offset[1] = p_aot_tb->jmp_stub_reset_offset[1];
    }
#if defined(CONFIG_LATX_JRRA) || defined(CONFIG_LATX_JRRA_STACK)
    tb->return_target_ptr = NULL;
    if (p_aot_tb->return_target_ptr_offset) {
        tb->next_86_pc = start + p_aot_tb->next_86_pc_offset;
    } else {
        tb->next_86_pc = 0;
    }
#endif
#ifdef CONFIG_LATX_INSTS_PATTERN
    tb->eflags_target_arg[0] = p_aot_tb->eflags_target_arg[0];
    tb->eflags_target_arg[1] = p_aot_tb->eflags_target_arg[1];
    tb->eflags_target_arg[2] = p_aot_tb->eflags_target_arg[2];
#endif
    tb->eflag_use = p_aot_tb->eflag_use;
#ifdef CONFIG_LATX_SMC_OPT
    if (tb->bool_flags & TBSMC_OPTED_MASK) {
        tb->smc_data = TBSMC_OPTED_MASK;
    } else {
        tb->smc_data = 0;
    }
#endif
    return tb;
}

#include "latx-signal.h"
#include "qemu/atomic.h"
#ifdef CONFIG_LATX_TBMINI_ENABLE
static inline void aot_tbmini_set_pointer(uintptr_t tbm, uint64_t tb_addr,
        uint64_t tc_size)
{
    *(uint64_t *)tbm = (tb_addr & MAKE_64BIT_MASK(0, HOST_VIRT_ADDR_SPACE_BITS)) |
                (tc_size << HOST_VIRT_ADDR_SPACE_BITS);
}
#endif

#ifdef CONFIG_LATX_TU
/* Dry-run the allocator without advancing pointers or changing regions. */
static bool aot_space_take(uintptr_t *ptr, uintptr_t limit,
                           size_t size, size_t align)
{
    size_t padding = -*ptr & (align - 1);

    if (*ptr > limit || padding > limit - *ptr ||
        size > limit - *ptr - padding) {
        return false;
    }
    *ptr += padding + size;
    return true;
}

static bool aot_tu_has_space(int members, uint32_t code_size)
{
    uintptr_t code = (uintptr_t)tcg_ctx->code_gen_ptr;
    uintptr_t tbs = (uintptr_t)tcg_ctx->tb_gen_ptr;
    uintptr_t code_limit = (uintptr_t)tcg_ctx->code_gen_highwater;
    uintptr_t tb_limit = (uintptr_t)tcg_ctx->tb_gen_highwater;
    size_t align = qemu_icache_linesize;
    size_t table_size = 0;

    for (int i = 0; i < members; i++) {
        uintptr_t *ptr = option_split_tb ? &tbs : &code;
        uintptr_t limit = option_split_tb ? tb_limit : code_limit;

        if (!aot_space_take(ptr, limit, sizeof(TranslationBlock), align) ||
            !aot_space_take(ptr, limit, 0, align) ||
            !aot_space_take(&code, code_limit, 0, align)) {
            return false;
        }
    }
#ifdef CONFIG_LATX_TBMINI_ENABLE
    table_size = sizeof(struct TBMini) * (members + 1);
#endif
    return aot_space_take(&code, code_limit, table_size, CODE_GEN_ALIGN) &&
           aot_space_take(&code, code_limit, 0, qemu_icache_linesize) &&
           aot_space_take(&code, code_limit, code_size, 1) &&
           aot_space_take(&code, code_limit, 0, CODE_GEN_ALIGN);
}

static int aot_tb_lower_bound(aot_tb *tbs, int num, uint32_t parallel,
        uint32_t offset)
{
    int left = 0;
    int right = num;

    while (left < right) {
        int middle = left + (right - left) / 2;
        uint32_t middle_parallel = tbs[middle].cflags & CF_PARALLEL;

        if (middle_parallel < parallel ||
                (middle_parallel == parallel &&
                 tbs[middle].offset_in_segment < offset)) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return left;
}

static bool aot_tu_add_dependency_page(target_ulong *pages, int *page_num,
        target_ulong page)
{
    for (int i = 0; i < *page_num; i++) {
        if (pages[i] == page) {
            return true;
        }
    }
    if (*page_num == MAX_PAGES_IN_TU) {
        return false;
    }
    pages[(*page_num)++] = page;
    return true;
}

#endif

#ifdef CONFIG_LATX_TU
static void aot_restore_direct_links(TranslationBlock *tb,
                                    const aot_tb *aot)
{
    int first_exit = 0;
    int exit_end;

    if (!aot_cross_page_tu_enabled() || use_tu_jmp(tb)) {
        return;
    }
    switch (aot->last_ir1_type) {
    case IR1_TYPE_BRANCH:
        exit_end = 2;
        break;
    case IR1_TYPE_NORMAL:
    case IR1_TYPE_CALL:
        exit_end = 1;
        break;
    case IR1_TYPE_JUMP:
        first_exit = 1;
        exit_end = 2;
        break;
    default:
        return;
    }

    for (int i = first_exit; i < exit_end; i++) {
        target_ulong target;

        if (tb->jmp_reset_offset[i] == TB_JMP_RESET_OFFSET_INVALID ||
                aot->lazypc[i] == 0) {
            continue;
        }
        target = tb->pc + aot->lazypc[i];
        aot_link_direct_target(thread_cpu, tb, target, tb->flags,
                               tb->cflags, i);
    }
}
#endif

static bool recover_tb_range_page(target_ulong page, struct aot_tb *p_aot_tbs,
        int num, abi_long start, abi_long end)
{
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    target_ulong base = env->segs[R_CS].base;
    bool recovered = false;

/* AOT + TB */
#ifndef CONFIG_LATX_TU
    for (int i = 0; i < num; i++) {
        if ((p_aot_tbs[i].offset_in_segment > end - start)
				|| (p_aot_tbs[i].cflags != tcg_ctx->tb_cflags)) {
            continue;
        }
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
				ROUND_UP((uintptr_t)(tcg_ctx->code_gen_ptr), qemu_icache_linesize));
        void *tb_buff = (void *)tcg_splitwx_to_rx(tcg_ctx->code_gen_ptr);
#if defined(CONFIG_LATX_TBMINI_ENABLE)
        /* set TBMini */
        uintptr_t tbmini_ptr = (uintptr_t)tb_buff;
        tb_buff = (void *)ROUND_UP((uintptr_t)tb_buff + sizeof(struct TBMini), qemu_icache_linesize);

        TranslationBlock *tb = creat_tb(&p_aot_tbs[i], start, base, tb_buff);
        qatomic_set(&tcg_ctx->code_gen_ptr, tb_buff);
        aot_tbmini_set_pointer(tbmini_ptr, (uint64_t)tb, TB_MAGIC);
#else
        TranslationBlock *tb = creat_tb(&p_aot_tbs[i], start, base, tb_buff);
#endif
        copy_code_to_buff(tb_buff, &p_aot_tbs[i]);
        /* Relocate this tb by traverse aot_rel_table. */
        aot_do_tb_reloc(tb, &p_aot_tbs[i], start, end);
        if (p_aot_tbs[i].last_ir1_type == IR1_TYPE_BRANCH) {
            lsassert(tb->jmp_target_arg[1] && tb->jmp_target_arg[1]
                < tb->tc.size);
            tb_reset_jump(tb, 1);
        }
#ifdef CONFIG_LATX_PROFILER
        /* creat_tb() already initialized counters and restored nr_code. */
#endif
        aot_tb_register(tb, AOT_TB_RECOVERED);
        recovered = true;

#ifdef CONFIG_LATX_DEBUG
        assert((tb->pc & TARGET_PAGE_MASK) == page);
	if (i) {
            if (p_aot_tbs[i].offset_in_segment <= p_aot_tbs[i - 1].offset_in_segment) {
                assert((p_aot_tbs[i].offset_in_segment
                    == p_aot_tbs[i - 1].offset_in_segment)
                    && (p_aot_tbs[i].cflags != p_aot_tbs[i - 1].cflags));
	    }
	}
#endif

#if defined(CONFIG_LATX_JRRA) || defined(CONFIG_LATX_JRRA_STACK)
        if (p_aot_tbs[i].return_target_ptr_offset) {
            tb->return_target_ptr = p_aot_tbs[i].return_target_ptr_offset
                + tb->tc.ptr;
            aot_link_tree_insert(tb, 0, 0);
        }
#endif
    }

/* AOT + TU */
#else
    TranslationBlock *tb_in_order[MAX_TB_IN_CACHE];
    for (int i = 0; i < num;) {
#ifdef CONFIG_LATX_DEBUG
        /* Dump this tb. */
        if (option_debug_aot) {
            qemu_log_mask(LAT_LOG_AOT, "Loading tb: offset 0x%x, x86_size 0x%x\n",
                    p_aot_tbs[i].offset_in_segment, p_aot_tbs[i].size);
        }
#endif
        /*
         * Page tables separate parallel and non-parallel TBs, but a group
         * must still match the exact cflags requested for this load.
         */
        while (i < num &&
               (!p_aot_tbs[i].is_first_tb ||
                p_aot_tbs[i].cflags != tcg_ctx->tb_cflags)) {
            i++;
        }
        if (i == num) {
            break;
        }
        bool has_hras_cold = false;
        int j;
        for (j = i; j < num; j++) {
            if (p_aot_tbs[j].cflags != tcg_ctx->tb_cflags ||
                    (p_aot_tbs[i].tu_id != p_aot_tbs[j].tu_id) ||
                    ((j != i) && p_aot_tbs[j].is_first_tb)) {
                break;
            }
            tb_in_order[j - i] = creat_tb(&p_aot_tbs[j], start, base, NULL);
            if (unlikely(!tb_in_order[j - i])) {
                return recovered;
            }
            has_hras_cold |= hras_callin_has_cold_exit(tb_in_order[j - i]);
        }
#if defined(CONFIG_LATX_TBMINI_ENABLE)
        int tb_num_in_tu = j - i;
        /* Reserve space for tu tbmin table. */
        TBMini *tbmini_ptr = (TBMini *)
            ROUND_UP((uintptr_t)tcg_ctx->code_gen_ptr, CODE_GEN_ALIGN);
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
                    ROUND_UP((uintptr_t)tbmini_ptr +
                    sizeof(struct TBMini) * (tb_num_in_tu + 1), qemu_icache_linesize));
        /* First TBMini records the member count for a complete TU table. */
        aot_tbmini_set_pointer((uintptr_t)tbmini_ptr, tb_num_in_tu,
                               TU_TBM_TABLE_MAGIC);
        for (int k = i; k < j; k++) {
            TranslationBlock *tb = tb_in_order[k - i];
            aot_tbmini_set_pointer((uintptr_t)++tbmini_ptr, (uint64_t)tb, (uint64_t)tb->tc.size);
        }
#else
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
            ROUND_UP((uintptr_t)tcg_ctx->code_gen_ptr, CODE_GEN_ALIGN));
#endif

        void *tu_begin_ptr = tcg_ctx->code_gen_ptr;
        memcpy(tcg_ctx->code_gen_ptr, aot_buffer + p_aot_tbs[i].tb_cache_offset,
                    p_aot_tbs[i].tu_size);
        uintptr_t new_buf_ptr =
            (uintptr_t)(tcg_ctx->code_gen_ptr + p_aot_tbs[i].tu_size);
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
                ROUND_UP(new_buf_ptr, CODE_GEN_ALIGN));
        for (int k = i; k < j; k++) {
            TranslationBlock *tb = tb_in_order[k - i];
            aot_tb *curr_aot_tb = &p_aot_tbs[k];
            tb->tc.ptr = tu_begin_ptr + curr_aot_tb->offset_in_tu;
            assert((tb->pc & TARGET_PAGE_MASK) == page);
            assert((tb->cflags & CF_PARALLEL) == (tcg_ctx->tb_cflags & CF_PARALLEL));
            aot_do_tb_reloc(tb, curr_aot_tb, start, end);
            target_ulong next_pc = 0, target_pc = 0;
            if (curr_aot_tb->next_tb_pc_offset != -1) {
               next_pc = curr_aot_tb->next_tb_pc_offset + start;
            }
            if (curr_aot_tb->target_tb_pc_offset != -1) {
               target_pc = curr_aot_tb->target_tb_pc_offset + start;
            }
            if ((next_pc || target_pc) && !use_tu_jmp(tb) &&
                    !use_indirect_jmp(tb)) {
                if (curr_aot_tb->last_ir1_type == IR1_TYPE_CALL) {
                    aot_link_tree_insert(tb, target_pc, 0);
                } else {
                    aot_link_tree_insert(tb, next_pc, target_pc);
                }
            }
#if defined(CONFIG_LATX_JRRA) || defined(CONFIG_LATX_JRRA_STACK)
            else if (curr_aot_tb->return_target_ptr_offset) {
                tb->return_target_ptr =
                (unsigned long *)(curr_aot_tb->return_target_ptr_offset + tb->tc.ptr);
                aot_link_tree_insert(tb, 0, 0);
            }
#endif
        }
        /* Publish all host PC ranges before any member can execute. */
        if (has_hras_cold) {
            tcg_tb_insert_tu(tb_in_order, j - i);
        }
        for (int k = i; k < j; k++) {
            aot_tb_register(tb_in_order[k - i], AOT_TB_RECOVERED);
        }
        recovered = true;
        i = j;
    }
#endif
    return recovered;
}

#ifdef CONFIG_LATX_TU
static int aot_tu_member_offset_cmp(const void *ap, const void *bp)
{
    const aot_tb *a = *(aot_tb * const *)ap;
    const aot_tb *b = *(aot_tb * const *)bp;

    return (a->offset_in_tu > b->offset_in_tu) -
           (a->offset_in_tu < b->offset_in_tu);
}
#endif

static bool recover_tb_range(target_ulong page, struct aot_tb *p_aot_tbs,
        int num, struct aot_tb *segment_tbs, int segment_tb_num,
        abi_long start, abi_long end)
{
#ifndef CONFIG_LATX_TU
    return recover_tb_range_page(page, p_aot_tbs, num, start, end);
#else
    if (!aot_cross_page_tu_enabled()) {
        return recover_tb_range_page(page, p_aot_tbs, num, start, end);
    }
    bool recovered = false;
    CPUArchState *env = lsenv->cpu_state;
    target_ulong base = env->segs[R_CS].base;

    (void)page;
    for (int i = 0; i < num; i++) {
        aot_tb *key = &p_aot_tbs[i];
        aot_tb *members[MAX_TB_IN_CACHE];
        TranslationBlock *tb_in_order[MAX_TB_IN_CACHE];
        aot_tb *first = NULL;
        int member_num = 0;

        if (key->cflags != tcg_ctx->tb_cflags) {
            continue;
        }

        /* A page can contain several members of the same TU. */
        bool already_seen = false;
        for (int k = 0; k < i; k++) {
            if (p_aot_tbs[k].tu_id == key->tu_id &&
                    p_aot_tbs[k].cflags == key->cflags &&
                    p_aot_tbs[k].flags == key->flags) {
                already_seen = true;
                break;
            }
        }
        if (already_seen) {
            continue;
        }

        /*
         * Members can interleave in the PC-ordered AOT table.  The TU boundary
         * has a separate address-span limit, so search only that window instead
         * of the whole segment.  Empty pages do not consume the dependency-page
         * limit.
         */
        target_ulong tu_pc = start + key->tu_id;
        target_ulong window_first = tu_pc & TARGET_PAGE_MASK;
        target_ulong window_end = window_first +
            MAX_TU_PAGE_SPAN * TARGET_PAGE_SIZE;
        uint32_t first_offset = window_first > start ?
            window_first - start : 0;
        uint32_t parallel = key->cflags & CF_PARALLEL;
        int first_index = aot_tb_lower_bound(segment_tbs, segment_tb_num,
                parallel, first_offset);

        if (window_end < window_first) {
            continue;
        }
        for (int k = first_index; k < segment_tb_num; k++) {
            aot_tb *member = &segment_tbs[k];
            target_ulong member_pc = start + member->offset_in_segment;

            if ((member->cflags & CF_PARALLEL) != parallel ||
                    member_pc >= window_end) {
                break;
            }
            if (member->tu_id != key->tu_id ||
                    member->cflags != key->cflags ||
                    member->flags != key->flags) {
                continue;
            }
            if (member_num == MAX_TB_IN_CACHE) {
                qemu_log_mask(LAT_LOG_AOT,
                        "AOT TU 0x%x exceeds member limit, skip\n", key->tu_id);
                member_num = 0;
                break;
            }
            members[member_num++] = member;
            if (member->is_first_tb) {
                first = member;
            }
        }
        if (!member_num || !first) {
            continue;
        }

        /*
         * Sort by code offset so the runtime membership list follows the TU
         * code layout, independent of the page-table ordering.
         */
        qsort(members, member_num, sizeof(*members), aot_tu_member_offset_cmp);
        if (members[0] != first || first->offset_in_tu != 0 ||
                !first->tu_size) {
            qemu_log_mask(LAT_LOG_AOT,
                    "AOT TU 0x%x has invalid first-member metadata, skip\n",
                    key->tu_id);
            continue;
        }

        target_ulong first_page = -1;
        target_ulong last_page = 0;
        target_ulong dependency_pages[MAX_PAGES_IN_TU];
        int dependency_page_num = 0;
        bool smc = false;
        bool invalid_metadata = false;
        uint32_t previous_end = 0;

        if (first->tu_size % sizeof(uint32_t)) {
            continue;
        }
        for (int k = 0; k < member_num; k++) {
            aot_tb *member = members[k];
            target_ulong member_start = start + member->offset_in_segment;
            target_ulong member_end = member_start + member->size - 1;
            target_ulong member_first_page = member_start & TARGET_PAGE_MASK;
            target_ulong member_last_page = member_end & TARGET_PAGE_MASK;

            if (!member->size || member_end < member_start ||
                    (member->offset_in_tu % sizeof(uint32_t)) ||
                    member->offset_in_tu < previous_end ||
                    !member->tb_cache_size ||
                    (member->tb_cache_size % sizeof(uint32_t)) ||
                    member->offset_in_tu > first->tu_size ||
                    member->tb_cache_size >
                        first->tu_size - member->offset_in_tu) {
                invalid_metadata = true;
                break;
            }
            previous_end = member->offset_in_tu + member->tb_cache_size;
            if (aot_tu_loading_enabled() &&
                (member_start < start || member_end >= end)) {
                invalid_metadata = true;
                break;
            }
            first_page = MIN(first_page, member_first_page);
            last_page = MAX(last_page, member_last_page);
            for (target_ulong p = member_first_page; p <= member_last_page;
                    p += TARGET_PAGE_SIZE) {
                if (!aot_tu_add_dependency_page(dependency_pages,
                            &dependency_page_num, p)) {
                    invalid_metadata = true;
                    break;
                }
                if (page_get_page_state(p) == PAGE_SMC) {
                    smc = true;
                    break;
                }
                if (aot_tu_loading_enabled() &&
                        (page_get_flags(p) & (PAGE_VALID | PAGE_EXEC)) !=
                        (PAGE_VALID | PAGE_EXEC)) {
                    invalid_metadata = true;
                    break;
                }
                if (p == member_last_page) {
                    break;
                }
            }
            if (invalid_metadata || smc) {
                break;
            }
        }
        if (invalid_metadata || smc ||
                ((last_page - first_page) >> TARGET_PAGE_BITS) + 1 >
                    MAX_TU_PAGE_SPAN) {
            continue;
        }

        seg_info *segment = segment_tree_lookup(start);
        if (segment && (segment->aot_file_type &
                        (PE_AOT_FILE | CACHE_AOT_FILE))) {
            bool changed = false;
            for (int k = 0; k < member_num; k++) {
                aot_tb *member = members[k];
                target_ulong guest_pc = start + member->offset_in_segment;
                uint8_t bytes[512];
                size_t checked = 0;

                while (checked < member->size) {
                    size_t size = MIN(sizeof(bytes), member->size - checked);
                    if (cpu_memory_rw_debug(thread_cpu, guest_pc + checked,
                                            bytes, size, false) ||
                        memcmp(bytes, aot_buffer + member->ir1_code_offset +
                                      checked, size)) {
                        changed = true;
                        page_set_page_state(guest_pc + checked, PAGE_SMC);
                        break;
                    }
                    checked += size;
                }
                if (changed) {
                    break;
                }
            }
            if (changed) {
                continue;
            }
        }

        /*
         * Page loading may only publish complete, page-local TUs. Do not
         * truncate a cross-page TU: its code can branch to other members.
         */
        if (!aot_cross_page_tu_enabled() &&
                (first_page != page || last_page != page)) {
            continue;
        }

        /*
         * Loading one page may rediscover a TU loaded through its other page.
         */
        if (tb_htable_lookup(thread_cpu, start + first->offset_in_segment, 0,
                first->flags, first->cflags)) {
            continue;
        }

        if (aot_tu_loading_enabled()) {
            bool overlap = false;
            size_t links = 0;

            /* Never publish a partial TU over an existing member instance. */
            for (int k = 0; k < member_num; k++) {
                aot_tb *member = members[k];
                size_t rels = 0;

                /* -1 means no relocations; otherwise the end is inclusive. */
                if (member->rel_start_index != -1) {
                    if (member->rel_start_index < 0 ||
                        member->rel_end_index < member->rel_start_index) {
                        overlap = true;
                        break;
                    }
                    rels = (size_t)member->rel_end_index -
                           member->rel_start_index + 1;
                }
                if (links >= INT_MAX || rels > (INT_MAX - links - 1) / 2) {
                    overlap = true;
                    break;
                }
                /* Each relocation can enqueue two exits, plus one JRRA. */
                links += 2 * rels + 1;
                if (tb_htable_lookup(thread_cpu,
                        start + member->offset_in_segment, base,
                        member->flags, member->cflags)) {
                    overlap = true;
                    break;
                }
            }
            if (overlap || !aot_tu_has_space(member_num, first->tu_size) ||
                !aot_link_tree_reserve(links)) {
                continue;
            }
        }

        void *saved_code = tcg_ctx->code_gen_ptr;
        void *saved_tbs = tcg_ctx->tb_gen_ptr;
        void *saved_data = tcg_ctx->data_gen_ptr;

        for (int k = 0; k < member_num; k++) {
            tb_in_order[k] = creat_tb(members[k], start, base, NULL);
            if (unlikely(!tb_in_order[k])) {
                if (aot_tu_loading_enabled()) {
                    /* Preflight prevents region changes; none is published. */
                    while (k) {
                        tb_destroy(tb_in_order[--k]);
                    }
                    qatomic_set(&tcg_ctx->code_gen_ptr, saved_code);
                    qatomic_set(&tcg_ctx->tb_gen_ptr, saved_tbs);
                    tcg_ctx->data_gen_ptr = saved_data;
                }
                return recovered;
            }
        }
        if (aot_tu_loading_enabled()) {
            /* No fallible allocation remains before publishing all members. */
            for (int k = 0; k < dependency_page_num; k++) {
                page_protect(dependency_pages[k]);
            }
        }
#if defined(CONFIG_LATX_TBMINI_ENABLE)
        /* Reserve space for tu tbmin table. */
        TBMini *tbmini_ptr = (TBMini *)
            ROUND_UP((uintptr_t)tcg_ctx->code_gen_ptr, CODE_GEN_ALIGN);
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
            ROUND_UP((uintptr_t)(tbmini_ptr + member_num + 1),
                     qemu_icache_linesize));
        aot_tbmini_set_pointer((uintptr_t)tbmini_ptr, member_num,
                               TU_TBM_TABLE_MAGIC);
#else
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
            ROUND_UP((uintptr_t)tcg_ctx->code_gen_ptr, CODE_GEN_ALIGN));
#endif

        void *tu_begin_ptr = tcg_ctx->code_gen_ptr;
        memcpy(tcg_ctx->code_gen_ptr, aot_buffer + first->tb_cache_offset,
                    first->tu_size);
        uintptr_t new_buf_ptr =
            (uintptr_t)(tcg_ctx->code_gen_ptr + first->tu_size);
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
                ROUND_UP(new_buf_ptr, CODE_GEN_ALIGN));
        for (int k = 0; k < member_num; k++) {
            TranslationBlock *tb = tb_in_order[k];
            aot_tb *record = members[k];

            tb->tc.ptr = tu_begin_ptr + record->offset_in_tu;
#ifdef CONFIG_LATX_TBMINI_ENABLE
            aot_tbmini_set_pointer((uintptr_t)(tbmini_ptr + k + 1),
                                   (uint64_t)tb, tb->tc.size);
#endif
            assert(tb->cflags == tcg_ctx->tb_cflags);
            aot_do_tb_reloc(tb, record, start, end);
            if (hras_call_is_single(tb)) {
                tb_reset_jump(tb, 0);
            }
            if (!aot_cross_page_tu_enabled()) {
                target_ulong next_pc = record->next_tb_pc_offset == -1 ? 0 :
                    start + record->next_tb_pc_offset;
                target_ulong target_pc = record->target_tb_pc_offset == -1 ?
                    0 : start + record->target_tb_pc_offset;

                if (record->last_ir1_type == IR1_TYPE_CALL) {
                    aot_link_tree_insert(tb, target_pc, 0);
                } else if (next_pc || target_pc) {
                    aot_link_tree_insert(tb, next_pc, target_pc);
                }
            }
#if defined(CONFIG_LATX_JRRA) || defined(CONFIG_LATX_JRRA_STACK)
            if (record->return_target_ptr_offset) {
                tb->return_target_ptr = (unsigned long *)(tb->tc.ptr +
                                        record->return_target_ptr_offset);
                aot_link_tree_insert(tb, 0, 0);
            }
#endif
        }

        tu_link_members(tb_in_order, member_num);
        tcg_tb_insert_tu(tb_in_order, member_num);
        for (int k = 0; k < member_num; k++) {
            aot_tb_register(tb_in_order[k], AOT_TB_RECOVERED);
        }
        recovered = true;
        for (int k = 0; k < member_num; k++) {
            aot_restore_direct_links(tb_in_order[k], members[k]);
            uintptr_t callee = qatomic_read(&tb_in_order[k]->jmp_dest[0]);
            if (callee && !(callee & 1)) {
                hras_link_call(tb_in_order[k], (TranslationBlock *)callee);
            }
        }
    }
    return recovered;
#endif
}

void do_recover_segment(aot_segment *p_segment,
        abi_long start, abi_long end) 
{
    struct aot_tb *p_aot_tbs =
        (struct aot_tb *)(aot_buffer + p_segment->segment_tbs_offset);
    int num = p_segment->segment_tbs_num;

    recover_tb_range(0, p_aot_tbs, num, p_aot_tbs, num, start, end);
}

static bool check_ir1(target_ulong seg_begin, struct aot_tb *p_aot_tbs, int num,
        void *curr_aot_buffer)
{
    uintptr_t pc;
    aot_tb *tb;
    uintptr_t p_ir1;
    assert(curr_aot_buffer);
    for (int i = 0; i < num; i++) {
        tb = &p_aot_tbs[i];
        pc = seg_begin + tb->offset_in_segment;
        p_ir1 = (uintptr_t)curr_aot_buffer + tb->ir1_code_offset;
        if (memcmp((void *)pc, (void *)p_ir1, tb->size)) {
            return false;
        }
    }
    return true;
}

#include<sys/syscall.h>

#ifdef CONFIG_LATX_TU
int load_aot_tu(target_ulong pc, target_ulong cs_base,
                uint32_t flags, uint32_t cflags)
{
    seg_info *info;
    aot_segment *segment;
    aot_tb *records;
    int index;

    tcg_debug_assert(have_mmap_lock());
    /* This cache format records flat code, not a nonzero CS base. */
    if (!aot_tu_loading_enabled() || in_pre_translate || cs_base ||
        page_get_page_state(pc) == PAGE_SMC) {
        return 0;
    }
    info = segment_tree_lookup(pc);
    if (!info) {
        return 0;
    }
    if (!info->buffer || pc < info->seg_begin || pc >= info->seg_end) {
        return 0;
    }
    aot_buffer = info->buffer;
    segment = info->p_segment;
    records = (aot_tb *)(aot_buffer + segment->segment_tbs_offset);
    index = aot_tb_lower_bound(records, segment->segment_tbs_num,
                              cflags & CF_PARALLEL, pc - info->seg_begin);
    for (; index < segment->segment_tbs_num; index++) {
        aot_tb *key = &records[index];

        if ((key->cflags & CF_PARALLEL) != (cflags & CF_PARALLEL) ||
            key->offset_in_segment != pc - info->seg_begin) {
            break;
        }
        if (key->flags != flags || key->cflags != cflags) {
            continue;
        }
        tcg_ctx->tb_cflags = cflags;
        /*
         * Exactly one candidate TU, even when other TUs share this page.
         * PAGE_LOADED is deliberately neither checked nor set: runtime TB
         * lookup owns TU residency, so invalidation needs no extra TU map.
         */
        recover_tb_range(pc & TARGET_PAGE_MASK, key, 1, records,
                         segment->segment_tbs_num,
                         info->seg_begin, info->seg_end);
        try_aot_link();
        return tb_htable_lookup(thread_cpu, pc, cs_base, flags, cflags) != NULL;
    }
    return 0;
}
#endif

inline int load_page_4(target_ulong pc, uint32_t cflags, seg_info *info)
{
    int ret = load_page(pc, cflags, info);
    pc += TARGET_PAGE_SIZE;
    load_page(pc, cflags, info);
    pc += TARGET_PAGE_SIZE;
    load_page(pc, cflags, info);
    pc += TARGET_PAGE_SIZE;
    load_page(pc, cflags, info);
    try_aot_link();
    return ret;
}


static int load_all_seg(uint32_t cflags, seg_info *info)
{
    info->seg_flag |= SEG_RUNNING;
    for (target_ulong addr = info->seg_begin; addr < info->seg_end;
            addr += TARGET_PAGE_SIZE) {
        load_page(addr, cflags, info);
    }
    try_aot_link();
    return 1;
}

inline int load_page(target_ulong pc, uint32_t cflags, seg_info *info)
{
    if (pc < info->seg_begin || pc >= info->seg_end) {
        return 0;
    }
    int page_state = page_get_page_state(pc);
    uint8_t aot_file_type =
        ((aot_header *)info->buffer)->aot_file_type & ~CROSS_PAGE_TU_AOT;
    if (page_state >= PAGE_LOADED || ((page_state == PAGE_SMC)
                && aot_file_type == ELF_AOT_FILE)) {
        return 0;
    }
    aot_buffer = info->buffer;
    aot_segment *p_segment = (aot_segment *)info->p_segment;
    page_table_info *pt = (page_table_info *)
        (p_segment->page_table_offset + (uintptr_t)aot_buffer);

    int pt_id = (pc - info->seg_begin) / TARGET_PAGE_SIZE;
    assert(pt_id < ((p_segment->details.seg_end
                    - p_segment->details.seg_begin) / TARGET_PAGE_SIZE));

    int tb_num_in_page;
    struct aot_tb *p_aot_tbs;
    if (cflags & CF_PARALLEL) {
        tb_num_in_page = pt[pt_id].tb_num_in_page_parallel;
        p_aot_tbs = 
            (struct aot_tb *)(aot_buffer + pt[pt_id].page_tbs_offset_parallel);
    } else {
        tb_num_in_page = pt[pt_id].tb_num_in_page;
        p_aot_tbs = (struct aot_tb *)(aot_buffer + pt[pt_id].page_tbs_offset);
    }

    if (tb_num_in_page == 0) {
        return 0;
    }

    target_ulong p1 = pc & TARGET_PAGE_MASK;

    if (p_segment->aot_file_type & (PE_AOT_FILE | CACHE_AOT_FILE)) {
        if (!check_ir1(info->seg_begin, p_aot_tbs, tb_num_in_page, aot_buffer)) {
            page_set_page_state(pc, PAGE_SMC);
            return 0;
        }
    } else {
        target_ulong p2 =
            (p_aot_tbs[tb_num_in_page - 1].offset_in_segment + info->seg_begin
             + p_aot_tbs[tb_num_in_page - 1].size) & TARGET_PAGE_MASK;
        while ((p1 != p2) && (page_get_page_state(p2) == PAGE_SMC)) {
#ifdef CONFIG_LATX_TU
            while (tb_num_in_page && !p_aot_tbs[tb_num_in_page - 1].is_first_tb) {
                tb_num_in_page--;
            }
#endif
            tb_num_in_page--;
            if (tb_num_in_page <= 0) {
                return 0;
            }
            p2 = (p_aot_tbs[tb_num_in_page - 1].offset_in_segment + info->seg_begin
                    + p_aot_tbs[tb_num_in_page - 1].size) & TARGET_PAGE_MASK;
        }
    }

    tcg_ctx->tb_cflags = cflags;

    struct aot_tb *segment_tbs =
        aot_buffer + p_segment->segment_tbs_offset;
    if (!recover_tb_range(p1, p_aot_tbs, tb_num_in_page, segment_tbs,
                          p_segment->segment_tbs_num,
                          info->seg_begin, info->seg_end)) {
        return 0;
    }
    /* Keep an SMC mark discovered during recovery. */
    if (page_state == PAGE_SMC || page_get_page_state(pc) != PAGE_SMC) {
        page_set_page_state(pc, PAGE_LOADED);
    }
    return 1;
}

int load_aot(target_ulong pc, uint32_t cflags)
{
    if (in_pre_translate) {
        return 0;
    }

    seg_info *info = segment_tree_lookup(pc);
    if (info == NULL || info->buffer == NULL) {
        if (smc_page_reload(pc & TARGET_PAGE_MASK, cflags)) {
            return 1;
        }
        page_set_page_state(pc, PAGE_NOINFO);
        return 0;
    }

    if (option_aot == 2 && (!(info->seg_flag & SEG_RUNNING)
                || (page_get_page_state(pc) == PAGE_FLUSH))) {
        return load_all_seg(cflags, info);
    } else {
        return load_page_4(pc, cflags, info);
    }

    return 0;
}

#endif
