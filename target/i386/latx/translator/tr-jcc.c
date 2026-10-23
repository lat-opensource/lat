/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

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
        tb->tu_unlink.rel_num = 2;
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
        tb->tu_unlink.rel_num = 2;
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

/*
 * JECXZ tests only the low 32 bits of RCX, so the generic load has to clear
 * the upper half before the 64-bit compare.  A full 32-bit write to ECX in
 * the immediately preceding instruction already zero-extends the host
 * register, and straight-line code inside a TB guarantees that write ran
 * before this branch.  Test the register directly in that case.
 */
static bool jecxz_prev_defines_zero_extended_ecx(IR1_INST *jecxz)
{
    TranslationBlock *tb = lsenv->tr_data->curr_tb;
    IR1_INST *first = tb_ir1_inst(tb, 0);
    ptrdiff_t index = jecxz - first;

    if (index <= 0 || index >= tb_ir1_num(tb)) {
        return false;
    }

    IR1_INST *prev = jecxz - 1;
#ifdef CONFIG_LATX_INSTS_PATTERN
    if (prev->instptn.opc != INSTPTN_OPC_NONE) {
        return false;
    }
#endif
    if (ir1_get_opnd_num(prev) < 1) {
        return false;
    }

    IR1_OPND *dst = ir1_get_opnd(prev, 0);
    if (!ir1_opnd_is_gpr(dst) ||
        ir1_opnd_base_reg_num(dst) != ecx_index ||
        ir1_opnd_size(dst) != 32) {
        return false;
    }

    switch (ir1_opcode(prev)) {
    /* These either store through store_ireg_to_ir1() or use an explicit
     * zero-extending operation for a 32-bit destination in every mode. */
    case dt_X86_INS_MOV:
    case dt_X86_INS_MOVZX:
    case dt_X86_INS_MOVSX:
    case dt_X86_INS_LEA:
    case dt_X86_INS_ADC:
    case dt_X86_INS_SBB:
        return true;
    default:
        break;
    }

#ifdef TARGET_X86_64
    /* In 64-bit mode these clear the upper half unless HBR proved the
     * register dead.  The following JECXZ keeps ECX live, so HBR cannot
     * have elided the clear here. */
    if (CODEIS64) {
        switch (ir1_opcode(prev)) {
        case dt_X86_INS_ADD:
        case dt_X86_INS_SUB:
        case dt_X86_INS_AND:
        case dt_X86_INS_OR:
        case dt_X86_INS_XOR:
        case dt_X86_INS_INC:
        case dt_X86_INS_DEC:
        case dt_X86_INS_NEG:
        case dt_X86_INS_NOT:
            return true;
        default:
            break;
        }
    }
#endif

    return false;
}

bool translate_jecxz(IR1_INST *pir1)
{
    IR2_OPND ecx_opnd;

    if (jecxz_prev_defines_zero_extended_ecx(pir1)) {
        ecx_opnd = ra_alloc_gpr(ecx_index);
    } else {
        ecx_opnd = load_ireg_from_ir1(&ecx_ir1_opnd, ZERO_EXTENSION, false);
    }
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
        tb->tu_unlink.rel_num = 2;
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
        tb->tu_unlink.rel_num = 2;
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
