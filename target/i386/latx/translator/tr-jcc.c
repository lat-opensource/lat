#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "flag-lbt.h"
#include "translate.h"
#include "tu.h"

bool translate_jcc(IR1_INST *pir1)
{
    IR2_OPND target_label_opnd = ra_alloc_label();

    IR2_OPND cond_opnd = ra_alloc_itemp();
    get_eflag_condition(&cond_opnd, pir1);

#ifdef CONFIG_LATX_TU
    IR2_OPND translated_label_opnd = ra_alloc_label();
    TranslationBlock *tb = lsenv->tr_data->curr_tb;

    if (tb->s_data->next_tb[TU_TB_INDEX_NEXT] && tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {
        /* calculate eflag */
        IR2_OPND tu_target_label_opnd = ra_alloc_label();
        la_label(tu_target_label_opnd);
        la_bne(cond_opnd, zero_ir2_opnd, tu_target_label_opnd);
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_target_label_opnd._label_id;
        tu_jcc_nop_gen(tb);
        /* tb->jmp_target_arg[0] = tu_target_label_opnd._label_id; */
        /* ra_free_temp(cond_opnd); */

        /* next_tb[TU_TB_INDEX_NEXT] already be translated */
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }
        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);
    } else {
        tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    }
#endif

    la_bne(cond_opnd, zero_ir2_opnd, target_label_opnd);
    ra_free_temp(cond_opnd);

    tr_generate_exit_tb(pir1, 0);
    la_label(target_label_opnd);
    tr_generate_exit_tb(pir1, 1);
    return true;
}

bool translate_jcxz(IR1_INST *pir1)
{
    /* 1. load cx */
    IR2_OPND cx_opnd = load_ireg_from_ir1(&cx_ir1_opnd, ZERO_EXTENSION, false);
    IR2_OPND target_label_opnd = ra_alloc_label();

#ifdef CONFIG_LATX_TU
    TranslationBlock *tb = lsenv->tr_data->curr_tb;
    if (tb->s_data->next_tb[TU_TB_INDEX_NEXT] && tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {
			/* && tb->tu_jmp[TU_TB_INDEX_TARGET] != TB_JMP_RESET_OFFSET_INVALID) { */
        /* IR2_OPND cx_opnd = load_ireg_from_ir1(&cx_ir1_opnd, ZERO_EXTENSION, false); */
        IR2_OPND tu_target_label_opnd = ra_alloc_label();
        la_label(tu_target_label_opnd);
        la_beq(cx_opnd, zero_ir2_opnd, tu_target_label_opnd);
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_target_label_opnd._label_id;
        tu_jcc_nop_gen(tb);
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND next_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(next_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = next_label_opnd._label_id;
        }
        /* tb->jmp_target_arg[0] = tu_target_label_opnd._label_id; */
        /* ra_free_temp(cx_opnd); */
        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);
        /* return true; */
    } else {
        tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    }

#endif

    la_beq(cx_opnd, zero_ir2_opnd, target_label_opnd);

    tr_generate_exit_tb(pir1, 0);
    la_label(target_label_opnd);
    tr_generate_exit_tb(pir1, 1);

    return true;
}

bool translate_jecxz(IR1_INST *pir1)
{
    /* TODO opt */
    IR2_OPND ecx_opnd =
        load_ireg_from_ir1(&ecx_ir1_opnd, ZERO_EXTENSION, false);
    IR2_OPND target_label_opnd = ra_alloc_label();

#ifdef CONFIG_LATX_TU
    TranslationBlock *tb = lsenv->tr_data->curr_tb;
    if (tb->s_data->next_tb[TU_TB_INDEX_NEXT] && tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {
			/* && tb->tu_jmp[TU_TB_INDEX_TARGET] != TB_JMP_RESET_OFFSET_INVALID) { */
        IR2_OPND tu_target_label_opnd = ra_alloc_label();
        la_label(tu_target_label_opnd);
        la_beq(ecx_opnd, zero_ir2_opnd, tu_target_label_opnd);
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_target_label_opnd._label_id;
        tu_jcc_nop_gen(tb);
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND next_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(next_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = next_label_opnd._label_id;
        }
        /* tb->jmp_target_arg[0] = tu_target_label_opnd._label_id; */
        /* ra_free_temp(cx_opnd); */
        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);
        /* return true; */
    } else {
        tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    }
#endif


    la_beq(ecx_opnd, zero_ir2_opnd, target_label_opnd);

    tr_generate_exit_tb(pir1, 0);
    la_label(target_label_opnd);
    tr_generate_exit_tb(pir1, 1);

    return true;
}

bool translate_jrcxz(IR1_INST *pir1)
{
    /* 1. load rcx */
    IR2_OPND rcx_opnd = ra_alloc_gpr(ecx_index);
    IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
    TranslationBlock *tb = lsenv->tr_data->curr_tb;
    if (tb->s_data->next_tb[TU_TB_INDEX_NEXT] && tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {
			/* && tb->tu_jmp[TU_TB_INDEX_TARGET] != TB_JMP_RESET_OFFSET_INVALID) { */
        IR2_OPND tu_target_label_opnd = ra_alloc_label();
        la_label(tu_target_label_opnd);
        la_beq(rcx_opnd, zero_ir2_opnd, tu_target_label_opnd);
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_target_label_opnd._label_id;
        tu_jcc_nop_gen(tb);
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND next_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(next_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = next_label_opnd._label_id;
        }
        /* tb->jmp_target_arg[0] = tu_target_label_opnd._label_id; */
        /* ra_free_temp(cx_opnd); */
        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);
        /* return true; */
    } else {
        tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    }
#endif


    la_beq(rcx_opnd, zero_ir2_opnd, target_label_opnd);

    tr_generate_exit_tb(pir1, 0);
    la_label(target_label_opnd);
    tr_generate_exit_tb(pir1, 1);

    return true;
}
