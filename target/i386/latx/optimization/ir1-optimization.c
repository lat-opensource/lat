/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "latx-options.h"
#include "ir1.h"
#include "ir1-optimization.h"

#include "flag-reduction.h"
#include "insts-pattern.h"
#include "tu.h"

/**
 * @brief ir1 optimization, which can get global
 * tb-ir1 information and store into IR1_INST
 *
 * @param tb Current tb
 *
 * @note If you want to add some analysis from ir1
 *       TB information, you can add like this: \n
 * - DEF_XXX: Define a global information which want to use cross per-inst;
 * - CHK_XXX: If you want to cross TB to get information, you can define this;
 * - OPT_XXX: Main OP, which anaylsis the information and add to IR1_INST.
 *
 * **Also, this function only analysis the information and set IR1_INST!**
 */

#ifdef CONFIG_LATX_TU
/* static int opt, noopt; */
static void ir1_optimization_over_tb(TranslationBlock *tb,
        bool use_calculated_live_out)
{
    (void)use_calculated_live_out;
    if (!tb->icount) {
        return;
    }
    IR1_INST *ir1 = NULL;
    /* cross scanning var defination */
    DEF_FLAG_RDTN(rdtn);
    DEF_INSTS_PTN(ptn);
#ifdef CONFIG_LATX_FLAG_REDUCTION
    if (use_calculated_live_out) {
        rdtn_pending_use = tb->s_data->eflag_out;
    } else {
        /* check if need cross tb analyze */
        CHK_FLAG_RDTN(rdtn, tb);
        rdtn_pending_use &= tb->s_data->eflag_out;
    }
#endif
    /* scanning instructions in reverse order */
    for (int i = tb_ir1_num(tb) - 1; i >= 0; --i) {
        ir1 = tb_ir1_inst(tb, i);
        /* do core optimize */
        OPT_FLAG_RDTN(rdtn, ir1);
        /* TODO: TU */
        OPT_INSTS_PTN(tb, ir1, i, ptn);
    }
    SAVE_FLAG_TO_TB(rdtn, tb);
}

static void get_eflag_out(TranslationBlock *tb)
{
    switch (tb->s_data->last_ir1_type) {
        case IR1_TYPE_BRANCH:
            if (tb->s_data->next_tb[TU_TB_INDEX_NEXT] &&
                    tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {
                TranslationBlock *tmp_tb =
                    (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_TARGET];
                tb->s_data->eflag_out |= tmp_tb->eflag_use;
                tmp_tb = (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_NEXT];
                tb->s_data->eflag_out |= tmp_tb->eflag_use;
            } else {
                tb->s_data->eflag_out |= __ALL_EFLAGS;
            }
            break;
        case IR1_TYPE_JUMP:
            if (tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {
                TranslationBlock *tmp_tb =
                    (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_TARGET];
                tb->s_data->eflag_out |= tmp_tb->eflag_use;
            } else {
                tb->s_data->eflag_out |= __ALL_EFLAGS;
            }
            break;
        case IR1_TYPE_CALL:
            if (tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {
                TranslationBlock *tmp_tb =
                    (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_TARGET];
                tb->s_data->eflag_out |= tmp_tb->eflag_use;
            } else {
                tb->s_data->eflag_out |= __ALL_EFLAGS;
            }
            break;
        case IR1_TYPE_NORMAL:
            if (tb->s_data->next_tb[TU_TB_INDEX_NEXT]) {
                TranslationBlock *tmp_tb =
                    (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_NEXT];
                tb->s_data->eflag_out |= tmp_tb->eflag_use;
            } else {
                tb->s_data->eflag_out |= __ALL_EFLAGS;
            }
            break;
        case IR1_TYPE_CALLIN:
        case IR1_TYPE_JUMPIN:
        case IR1_TYPE_RET:
            tb->s_data->eflag_out |= __ALL_EFLAGS;
            break;
        case IR1_TYPE_SYSCALL:
            break;
        default:
            lsassert(0);
    }
}

#ifdef CONFIG_LATX_FLAG_REDUCTION
static int tb_index(TranslationBlock **tb_list, int tb_num,
        TranslationBlock *target)
{
    if (!target) {
        return -1;
    }

    for (int i = 0; i < tb_num; i++) {
        if (tb_list[i] == target) {
            return i;
        }
    }
    return -1;
}

static int tb_pc_index(TranslationBlock **tb_list, int tb_num,
        TranslationBlock *from, target_ulong pc)
{
    for (int i = 0; i < tb_num; i++) {
        TranslationBlock *tb = tb_list[i];

        if (tb->pc == pc &&
            (tb->cflags & CF_PARALLEL) == (from->cflags & CF_PARALLEL) &&
            (tb->bool_flags & IS_CODE64) == (from->bool_flags & IS_CODE64)) {
            return i;
        }
    }
    return -1;
}

static bool tb_is_near_call(TranslationBlock *tb)
{
    return ir1_opcode(tb_ir1_inst_last(tb)) == dt_X86_INS_CALL;
}

static bool tb_is_near_ret(TranslationBlock *tb)
{
    return ir1_opcode(tb_ir1_inst_last(tb)) == dt_X86_INS_RET;
}

static void unknown_eflags_summary(uint8 *use, uint8 *preserve)
{
    *use = __ALL_EFLAGS;
    *preserve = __ALL_EFLAGS;
}

/*
 * Get the EFLAGS summary after a TB.  RET is a symbolic function exit here:
 * its continuation will be supplied at each direct CALL site later.  This
 * opt-in analysis assumes the normal ABI rule that a near RET uses the return
 * slot created by its matching direct CALL.
 */
static void get_summary_after_tb(TranslationBlock **tb_list, int tb_num,
        int tb_id, uint8 *summary_use, uint8 *summary_preserve,
        uint8 *use, uint8 *preserve)
{
    TranslationBlock *tb = tb_list[tb_id];
    int next_id = tb_index(tb_list, tb_num,
            tb->s_data->next_tb[TU_TB_INDEX_NEXT]);
    int target_id = tb_index(tb_list, tb_num,
            tb->s_data->next_tb[TU_TB_INDEX_TARGET]);

    switch (tb->s_data->last_ir1_type) {
    case IR1_TYPE_BRANCH:
        if (next_id < 0 || target_id < 0) {
            unknown_eflags_summary(use, preserve);
        } else {
            *use = summary_use[next_id] | summary_use[target_id];
            *preserve = summary_preserve[next_id] |
                        summary_preserve[target_id];
        }
        break;
    case IR1_TYPE_JUMP:
        if (target_id < 0) {
            unknown_eflags_summary(use, preserve);
        } else {
            *use = summary_use[target_id];
            *preserve = summary_preserve[target_id];
        }
        break;
    case IR1_TYPE_CALL: {
        if (!tb_is_near_call(tb)) {
            unknown_eflags_summary(use, preserve);
            break;
        }

        int call_target_id = tb_pc_index(tb_list, tb_num, tb,
                tb->s_data->target_pc);
        if (call_target_id < 0 || next_id < 0) {
            unknown_eflags_summary(use, preserve);
        } else {
            *use = summary_use[call_target_id] |
                   (summary_preserve[call_target_id] & summary_use[next_id]);
            *preserve = summary_preserve[call_target_id] &
                        summary_preserve[next_id];
        }
        break;
    }
    case IR1_TYPE_NORMAL:
        if (next_id < 0) {
            unknown_eflags_summary(use, preserve);
        } else {
            *use = summary_use[next_id];
            *preserve = summary_preserve[next_id];
        }
        break;
    case IR1_TYPE_RET:
        if (tb_is_near_ret(tb)) {
            *use = __NONE;
            *preserve = __ALL_EFLAGS;
        } else {
            unknown_eflags_summary(use, preserve);
        }
        break;
    case IR1_TYPE_CALLIN:
    case IR1_TYPE_JUMPIN:
    case IR1_TYPE_SYSCALL:
        unknown_eflags_summary(use, preserve);
        break;
    default:
        lsassert(0);
    }
}

/*
 * Calculate which incoming flags a direct callee may read or preserve until
 * RET.  This is a second fixed point over the existing TU graph and does not
 * change execution linking.
 */
static void get_tu_eflags_summary(TranslationBlock **tb_list, int tb_num,
        uint8 *block_use, uint8 *block_must_def,
        uint8 *summary_use, uint8 *summary_preserve)
{
    bool unfinished = true;

    memset(summary_use, 0, tb_num * sizeof(*summary_use));
    memset(summary_preserve, 0, tb_num * sizeof(*summary_preserve));

    while (unfinished) {
        unfinished = false;
        for (int i = tb_num - 1; i >= 0; i--) {
            uint8 succ_use = __NONE;
            uint8 succ_preserve = __NONE;
            uint8 block_preserve = __ALL_EFLAGS & ~block_must_def[i];

            get_summary_after_tb(tb_list, tb_num, i,
                    summary_use, summary_preserve,
                    &succ_use, &succ_preserve);

            uint8 new_use = block_use[i] | (block_preserve & succ_use);
            uint8 new_preserve = block_preserve & succ_preserve;
            if ((new_use & ~summary_use[i]) ||
                (new_preserve & ~summary_preserve[i])) {
                summary_use[i] |= new_use;
                summary_preserve[i] |= new_preserve;
                unfinished = true;
            }
        }
    }
}

static void optimize_tu_calls(TranslationBlock **tb_list, int tb_num)
{
    uint8 block_use[tb_num];
    uint8 block_must_def[tb_num];
    uint8 summary_use[tb_num];
    uint8 summary_preserve[tb_num];
    uint8 baseline_live_in[tb_num];

    for (int i = 0; i < tb_num; i++) {
        flag_reduction_get_tb_summary(tb_list[i],
                &block_use[i], &block_must_def[i]);
        baseline_live_in[i] = tb_list[i]->eflag_use;
    }

    get_tu_eflags_summary(tb_list, tb_num,
            block_use, block_must_def, summary_use, summary_preserve);

    for (int i = 0; i < tb_num; i++) {
        TranslationBlock *tb = tb_list[i];
        int next_id;
        int call_target_id;

        if (tb->s_data->last_ir1_type != IR1_TYPE_CALL ||
            !tb_is_near_call(tb)) {
            continue;
        }

        next_id = tb_index(tb_list, tb_num,
                tb->s_data->next_tb[TU_TB_INDEX_NEXT]);
        call_target_id = tb_pc_index(tb_list, tb_num, tb,
                tb->s_data->target_pc);
        if (call_target_id < 0 || next_id < 0) {
            continue;
        }

        tb->s_data->eflag_out = summary_use[call_target_id] |
                (summary_preserve[call_target_id] &
                 baseline_live_in[next_id]);
        ir1_optimization_over_tb(tb, true);
    }
}
#endif

void over_tb_rfd(TranslationBlock **tb_list, int tb_num)
{
    TranslationBlock *tb;
    uint8_t  eflag_def[tb_num];
    IR1_INST *ir1 = NULL;
    for (int i = 0; i < tb_num; i++) {
        tb = tb_list[i];
        if (!tb->icount) {
            continue;
        }
        eflag_def[i] = __NONE;
#ifdef CONFIG_LATX_FLAG_REDUCTION
        uint8 rdtn_pending_use = __ALL_EFLAGS;
#endif
        for (int j = tb_ir1_num(tb) - 1; j >= 0; --j) {
            ir1 = tb_ir1_inst(tb, j);
            OPT_FLAG_RDTN(rdtn, ir1);
            eflag_def[i] |= ir1_get_eflag_def(ir1);
        }
        SAVE_FLAG_TO_TB(rdtn, tb);
    }

    uint8_t old_livein, old_liveout;
    bool unfinished = true;
    while (unfinished) {
        unfinished = false;
        for (int i = tb_num - 1; i >= 0; i--) {
            tb = tb_list[i];
            old_livein = tb->eflag_use;
            old_liveout = tb->s_data->eflag_out;
            get_eflag_out(tb);
            tb->eflag_use |= (tb->s_data->eflag_out & (~eflag_def[i]));
            if (tb->eflag_use != old_livein || tb->s_data->eflag_out != old_liveout) {
                unfinished = true;
            }
        }
    }

    for (int i = 0; i < tb_num; i++) {
        tb = tb_list[i];
        /* fprintf(stderr, "pc %lx %x\n", tb->pc, (tb->s_data->eflag_out)); */
        ir1_optimization_over_tb(tb, false);
    }

#ifdef CONFIG_LATX_FLAG_REDUCTION
    if (option_eflags_cross) {
        optimize_tu_calls(tb_list, tb_num);
    }
#endif
}
#endif

void ir1_optimization(TranslationBlock *tb)
{
    if (!tb->icount) {
        return;
    }
    IR1_INST *ir1 = NULL;
    /* cross scanning var defination */
    DEF_FLAG_RDTN(rdtn);
    DEF_INSTS_PTN(ptn);
    /* check if need cross tb analyze */
    CHK_FLAG_RDTN(rdtn, tb);
    /* scanning instructions in reverse order */
    for (int i = tb_ir1_num(tb) - 1; i >= 0; --i) {
        ir1 = tb_ir1_inst(tb, i);
        /* do core optimize */
        OPT_FLAG_RDTN(rdtn, ir1);
        /* TODO: TU */
        OPT_INSTS_PTN(tb, ir1, i, ptn);
    }
    SAVE_FLAG_TO_TB(rdtn, tb);
}
