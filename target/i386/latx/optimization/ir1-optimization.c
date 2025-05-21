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
static void ir1_optimization_over_tb(TranslationBlock *tb)
{
    if (!tb->icount) {
        return;
    }
    IR1_INST *ir1 = dt_X86_INS_INVALID;
    /* cross scanning var defination */
    DEF_FLAG_RDTN(rdtn);
    DEF_INSTS_PTN(ptn);
    /* check if need cross tb analyze */
    CHK_FLAG_RDTN(rdtn, tb);
    rdtn_pending_use &= tb->s_data->eflag_out;
    /* scanning instructions in reverse order */
    for (int i = tb_ir1_num(tb) - 1; i >= 0; --i) {
        ir1 = tb_ir1_inst(tb, i);
        /* do core optimize */
        OPT_FLAG_RDTN(rdtn, ir1);
        /* TODO: TU */
        OPT_INSTS_PTN(ptn, ir1);
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

void over_tb_rfd(TranslationBlock **tb_list, int tb_num)
{
    TranslationBlock *tb;
    uint8_t  eflag_def[tb_num];
    IR1_INST *ir1 = dt_X86_INS_INVALID;
    for (int i = 0; i < tb_num; i++) {
        tb = tb_list[i];
        if (!tb->icount) {
            continue;
        }
        eflag_def[i] = __NONE;
        uint8 rdtn_pending_use = __ALL_EFLAGS;
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
        ir1_optimization_over_tb(tb);
    }
}
#endif

void ir1_optimization(TranslationBlock *tb)
{
    if (!tb->icount) {
        return;
    }
    IR1_INST *ir1 = dt_X86_INS_INVALID;
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
        OPT_INSTS_PTN(ptn, ir1);
    }
    SAVE_FLAG_TO_TB(rdtn, tb);
}
