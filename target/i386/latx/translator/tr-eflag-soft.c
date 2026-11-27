/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "flag-lbt.h"
#include "translate.h"
#include "eflag-internal.h"

static IR2_OPND soft_flag_operand(IR2_OPND value, IR1_INST *pir1, int index,
                                  int size, bool *allocated)
{
    IR2_OPND result = value;

    *allocated = false;
    if (ir2_opnd_is_imm(&value)) {
        result = ra_alloc_itemp();
        load_ireg_from_ir1_2(result, ir1_get_opnd(pir1, index),
                             ZERO_EXTENSION, false);
        *allocated = true;
    } else if (size < 64) {
        result = ra_alloc_itemp();
        la_bstrpick_d(result, value, size - 1, 0);
        *allocated = true;
    }
    return result;
}

static IR2_OPND soft_flag_materialize(IR2_OPND value, IR1_INST *pir1,
                                      int index, bool *allocated)
{
    IR2_OPND result = value;

    *allocated = false;
    if (ir2_opnd_is_imm(&value)) {
        result = ra_alloc_itemp();
        load_ireg_from_ir1_2(result, ir1_get_opnd(pir1, index),
                             ZERO_EXTENSION, false);
        *allocated = true;
    }
    return result;
}

static IR2_OPND generate_common_result(IR2_OPND src0, IR2_OPND src1,
                                       IR1_INST *pir1, bool *handled)
{
    IR2_OPND lhs;
    IR2_OPND rhs;
    IR2_OPND result;
    bool lhs_allocated;
    bool rhs_allocated;
    int size = ir1_opnd_size(ir1_get_opnd(pir1, 0));

    *handled = true;
    lhs = soft_flag_operand(src0, pir1, 0, size, &lhs_allocated);
    rhs = soft_flag_operand(src1, pir1, 1, size, &rhs_allocated);
    if (ir1_opcode(pir1) == dt_X86_INS_INC ||
        ir1_opcode(pir1) == dt_X86_INS_DEC) {
        if (rhs_allocated) {
            ra_free_temp(rhs);
        }
        rhs = ra_alloc_itemp();
        li_d(rhs, 1);
        rhs_allocated = true;
    }
    result = ra_alloc_itemp();

    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_XADD:
    case dt_X86_INS_ADD:
    case dt_X86_INS_INC:
        la_add_d(result, lhs, rhs);
        break;
    case dt_X86_INS_ADC: {
        IR2_OPND carry = ra_alloc_itemp();
        latx_read_eflags(carry, CF_USEDEF_BIT);
        la_andi(carry, carry, CF_BIT);
        la_add_d(result, lhs, rhs);
        la_add_d(result, result, carry);
        ra_free_temp(carry);
        break;
    }
    case dt_X86_INS_CMPSB:
    case dt_X86_INS_CMPSW:
    case dt_X86_INS_CMPSD:
    case dt_X86_INS_CMPSQ:
    case dt_X86_INS_SCASB:
    case dt_X86_INS_SCASW:
    case dt_X86_INS_SCASD:
    case dt_X86_INS_SCASQ:
    case dt_X86_INS_CMPXCHG:
    case dt_X86_INS_NEG:
    case dt_X86_INS_CMP:
    case dt_X86_INS_SUB:
    case dt_X86_INS_DEC:
        la_sub_d(result, lhs, rhs);
        break;
    case dt_X86_INS_SBB: {
        IR2_OPND carry = ra_alloc_itemp();
        latx_read_eflags(carry, CF_USEDEF_BIT);
        la_andi(carry, carry, CF_BIT);
        la_sub_d(result, lhs, rhs);
        la_sub_d(result, result, carry);
        ra_free_temp(carry);
        break;
    }
    case dt_X86_INS_TEST:
    case dt_X86_INS_AND:
    case dt_X86_INS_ANDN:
        la_and(result, lhs, rhs);
        break;
    case dt_X86_INS_XOR:
        la_xor(result, lhs, rhs);
        break;
    case dt_X86_INS_OR:
        la_or(result, lhs, rhs);
        break;
    case dt_X86_INS_SAL:
    case dt_X86_INS_SHL:
    case dt_X86_INS_SHR:
    case dt_X86_INS_SAR: {
        IR2_OPND count = ra_alloc_itemp();
        la_andi(count, rhs, size == 64 ? 0x3f : 0x1f);
        if (ir1_opcode(pir1) == dt_X86_INS_SAL ||
            ir1_opcode(pir1) == dt_X86_INS_SHL) {
            la_sll_d(result, lhs, count);
        } else if (ir1_opcode(pir1) == dt_X86_INS_SHR) {
            la_srl_d(result, lhs, count);
        } else {
            if (size < 64) {
                la_slli_d(result, lhs, 64 - size);
                la_srai_d(result, result, 64 - size);
                la_sra_d(result, result, count);
            } else {
                la_sra_d(result, lhs, count);
            }
        }
        ra_free_temp(count);
        break;
    }
    default:
        *handled = false;
        la_or(result, zero_ir2_opnd, zero_ir2_opnd);
        break;
    }
    if (size < 64) {
        la_bstrpick_d(result, result, size - 1, 0);
    }
    if (rhs_allocated) {
        ra_free_temp(rhs);
    }
    if (lhs_allocated) {
        ra_free_temp(lhs);
    }
    return result;
}

static void generate_multiply_overflow(IR2_OPND overflow, IR2_OPND src0,
                                       IR2_OPND src1, IR1_INST *pir1)
{
    IR2_OPND lhs = ra_alloc_itemp();
    IR2_OPND rhs = ra_alloc_itemp();
    int size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    bool is_signed = ir1_opcode(pir1) == dt_X86_INS_IMUL;

    la_or(lhs, src0, zero_ir2_opnd);
    if (ir2_opnd_is_imm(&src1)) {
        int src1_index = ir1_opnd_num(pir1) == 3 ? 2 : 1;
        load_ireg_from_ir1_2(rhs, ir1_get_opnd(pir1, src1_index),
                             is_signed ? SIGN_EXTENSION : ZERO_EXTENSION,
                             false);
    } else {
        la_or(rhs, src1, zero_ir2_opnd);
    }

    if (size < 64) {
        la_bstrpick_d(lhs, lhs, size - 1, 0);
        la_bstrpick_d(rhs, rhs, size - 1, 0);
        if (is_signed) {
            la_slli_d(lhs, lhs, 64 - size);
            la_srai_d(lhs, lhs, 64 - size);
            la_slli_d(rhs, rhs, 64 - size);
            la_srai_d(rhs, rhs, 64 - size);
        }
        la_mul_d(overflow, lhs, rhs);
        if (is_signed) {
            la_bstrpick_d(lhs, overflow, size - 1, 0);
            la_slli_d(lhs, lhs, 64 - size);
            la_srai_d(lhs, lhs, 64 - size);
            la_xor(lhs, lhs, overflow);
        } else {
            la_srli_d(lhs, overflow, size);
        }
    } else if (is_signed) {
        la_mulh_d(overflow, lhs, rhs);
        la_mul_d(lhs, lhs, rhs);
        la_srai_d(lhs, lhs, 63);
        la_xor(lhs, overflow, lhs);
    } else {
        la_mulh_du(overflow, lhs, rhs);
        la_or(lhs, overflow, zero_ir2_opnd);
    }
    la_sltu(overflow, zero_ir2_opnd, lhs);

    ra_free_temp(rhs);
    ra_free_temp(lhs);
}

static bool generate_common_cf(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1,
                               IR1_INST *pir1)
{
    IR2_OPND result = dest;
    IR2_OPND lhs = src0;
    IR2_OPND rhs = src1;
    IR2_OPND cf;
    bool lhs_allocated = false;
    bool rhs_allocated = false;
    int size = ir1_opnd_size(ir1_get_opnd(pir1, 0));

    if (ir1_opcode(pir1) == dt_X86_INS_MUL ||
        ir1_opcode(pir1) == dt_X86_INS_IMUL) {
        cf = ra_alloc_itemp();
        generate_multiply_overflow(cf, src0, src1, pir1);
        latx_write_eflags(cf, CF_USEDEF_BIT);
        ra_free_temp(cf);
        return true;
    }

    if (ir1_opcode(pir1) == dt_X86_INS_INC ||
        ir1_opcode(pir1) == dt_X86_INS_DEC) {
        return true; /* INC/DEC preserve CF. */
    }

    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_XADD:
    case dt_X86_INS_ADD: {
        lhs = soft_flag_materialize(lhs, pir1, 0, &lhs_allocated);
        cf = ra_alloc_itemp();
        if (size < 64) {
            la_bstrpick_d(cf, lhs, size - 1, 0);
            la_sltu(cf, result, cf);
        } else {
            la_sltu(cf, result, lhs);
        }
        break;
    }
    case dt_X86_INS_ADC: {
        IR2_OPND partial;

        lhs = soft_flag_materialize(lhs, pir1, 0, &lhs_allocated);
        rhs = soft_flag_materialize(rhs, pir1, 1, &rhs_allocated);
        cf = ra_alloc_itemp();
        partial = ra_alloc_itemp();
        if (size < 64) {
            la_bstrpick_d(cf, lhs, size - 1, 0);
            la_bstrpick_d(partial, rhs, size - 1, 0);
        } else {
            la_or(cf, lhs, zero_ir2_opnd);
            la_or(partial, rhs, zero_ir2_opnd);
        }
        la_add_d(partial, cf, partial);
        if (size < 64) {
            la_bstrpick_d(partial, partial, size - 1, 0);
        }
        la_sltu(cf, partial, cf);
        la_sltu(partial, result, partial);
        la_or(cf, cf, partial);
        ra_free_temp(partial);
        break;
    }
    case dt_X86_INS_CMPSB:
    case dt_X86_INS_CMPSW:
    case dt_X86_INS_CMPSD:
    case dt_X86_INS_CMPSQ:
    case dt_X86_INS_SCASB:
    case dt_X86_INS_SCASW:
    case dt_X86_INS_SCASD:
    case dt_X86_INS_SCASQ:
    case dt_X86_INS_CMPXCHG:
    case dt_X86_INS_NEG:
    case dt_X86_INS_CMP:
    case dt_X86_INS_SUB: {
        IR2_OPND rhs_value;

        lhs = soft_flag_materialize(lhs, pir1, 0, &lhs_allocated);
        rhs = soft_flag_materialize(rhs, pir1, 1, &rhs_allocated);
        cf = ra_alloc_itemp();
        rhs_value = ra_alloc_itemp();
        if (size < 64) {
            la_bstrpick_d(cf, lhs, size - 1, 0);
            la_bstrpick_d(rhs_value, rhs, size - 1, 0);
        } else {
            la_or(cf, lhs, zero_ir2_opnd);
            la_or(rhs_value, rhs, zero_ir2_opnd);
        }
        la_sltu(cf, cf, rhs_value);
        ra_free_temp(rhs_value);
        break;
    }
    case dt_X86_INS_SBB: {
        IR2_OPND rhs_value;
        IR2_OPND borrow;

        lhs = soft_flag_materialize(lhs, pir1, 0, &lhs_allocated);
        rhs = soft_flag_materialize(rhs, pir1, 1, &rhs_allocated);
        cf = ra_alloc_itemp();
        rhs_value = ra_alloc_itemp();
        borrow = ra_alloc_itemp();
        if (size < 64) {
            la_bstrpick_d(cf, lhs, size - 1, 0);
            la_bstrpick_d(rhs_value, rhs, size - 1, 0);
        } else {
            la_or(cf, lhs, zero_ir2_opnd);
            la_or(rhs_value, rhs, zero_ir2_opnd);
        }
        la_sltu(borrow, cf, rhs_value);
        la_sub_d(cf, cf, rhs_value);
        if (size < 64) {
            la_bstrpick_d(cf, cf, size - 1, 0);
        }
        latx_read_eflags(rhs_value, CF_USEDEF_BIT);
        la_andi(rhs_value, rhs_value, CF_BIT);
        la_sltu(cf, cf, rhs_value);
        la_or(cf, borrow, cf);
        ra_free_temp(borrow);
        ra_free_temp(rhs_value);
        break;
    }
    case dt_X86_INS_TEST:
    case dt_X86_INS_XOR:
    case dt_X86_INS_AND:
    case dt_X86_INS_ANDN:
    case dt_X86_INS_OR:
        cf = ra_alloc_itemp();
        la_or(cf, zero_ir2_opnd, zero_ir2_opnd);
        break;
    case dt_X86_INS_SAL:
    case dt_X86_INS_SHL:
    case dt_X86_INS_SHR:
    case dt_X86_INS_SAR: {
        IR2_OPND count = ra_alloc_itemp();
        IR2_OPND shift = ra_alloc_itemp();

        lhs = soft_flag_materialize(lhs, pir1, 0, &lhs_allocated);
        rhs = soft_flag_materialize(rhs, pir1, 1, &rhs_allocated);
        cf = ra_alloc_itemp();
        la_andi(count, rhs, size == 64 ? 0x3f : 0x1f);
        if (ir1_opcode(pir1) == dt_X86_INS_SAL ||
            ir1_opcode(pir1) == dt_X86_INS_SHL) {
            li_d(shift, size);
            la_sub_d(shift, shift, count);
        } else {
            la_addi_d(shift, count, -1);
        }
        la_srl_d(cf, lhs, shift);
        la_andi(cf, cf, 1);
        ra_free_temp(shift);
        ra_free_temp(count);
        break;
    }
    default:
        return false;
    }

    if (rhs_allocated) {
        ra_free_temp(rhs);
    }
    if (lhs_allocated) {
        ra_free_temp(lhs);
    }
    latx_write_eflags(cf, CF_USEDEF_BIT);
    ra_free_temp(cf);
    return true;
}

static bool generate_common_of(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1,
                               IR1_INST *pir1)
{
    IR2_OPND result = dest;
    IR2_OPND lhs = src0;
    IR2_OPND rhs = src1;
    IR2_OPND first = { 0 };
    IR2_OPND of = { 0 };
    bool first_allocated = false;
    bool of_allocated = false;
    bool lhs_allocated = false;
    bool rhs_allocated = false;
    int size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    bool handled = true;

    if (ir1_opcode(pir1) == dt_X86_INS_MUL ||
        ir1_opcode(pir1) == dt_X86_INS_IMUL) {
        of = ra_alloc_itemp();
        generate_multiply_overflow(of, src0, src1, pir1);
        la_slli_d(of, of, OF_BIT_INDEX);
        latx_write_eflags(of, OF_USEDEF_BIT);
        ra_free_temp(of);
        return true;
    }

    lsassert(!ir2_opnd_is_imm(&result));
    if (ir2_opnd_is_imm(&lhs)) {
        lhs = ra_alloc_itemp();
        load_ireg_from_ir1_2(lhs, ir1_get_opnd(pir1, 0),
                             ZERO_EXTENSION, false);
        lhs_allocated = true;
    }
    if (ir1_opcode(pir1) != dt_X86_INS_INC &&
        ir1_opcode(pir1) != dt_X86_INS_DEC &&
        ir2_opnd_is_imm(&rhs)) {
        rhs = ra_alloc_itemp();
        load_ireg_from_ir1_2(rhs, ir1_get_opnd(pir1, 1),
                             ZERO_EXTENSION, false);
        rhs_allocated = true;
    }

    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_XADD:
    case dt_X86_INS_ADD:
    case dt_X86_INS_ADC:
        first = ra_alloc_itemp();
        of = ra_alloc_itemp();
        first_allocated = true;
        of_allocated = true;
        la_xor(first, lhs, rhs);
        la_nor(first, first, zero_ir2_opnd);
        la_xor(of, lhs, result);
        la_and(of, first, of);
        break;
    case dt_X86_INS_INC:
        of = ra_alloc_itemp();
        of_allocated = true;
        la_nor(of, lhs, zero_ir2_opnd);
        la_and(of, of, result);
        break;
    case dt_X86_INS_CMPSB:
    case dt_X86_INS_CMPSW:
    case dt_X86_INS_CMPSD:
    case dt_X86_INS_CMPSQ:
    case dt_X86_INS_SCASB:
    case dt_X86_INS_SCASW:
    case dt_X86_INS_SCASD:
    case dt_X86_INS_SCASQ:
    case dt_X86_INS_CMPXCHG:
    case dt_X86_INS_NEG:
    case dt_X86_INS_CMP:
    case dt_X86_INS_SUB:
    case dt_X86_INS_SBB:
        first = ra_alloc_itemp();
        of = ra_alloc_itemp();
        first_allocated = true;
        of_allocated = true;
        la_xor(first, lhs, rhs);
        la_xor(of, lhs, result);
        la_and(of, first, of);
        break;
    case dt_X86_INS_DEC:
        of = ra_alloc_itemp();
        of_allocated = true;
        la_nor(of, result, zero_ir2_opnd);
        la_and(of, lhs, of);
        break;
    case dt_X86_INS_TEST:
    case dt_X86_INS_XOR:
    case dt_X86_INS_AND:
    case dt_X86_INS_ANDN:
    case dt_X86_INS_OR:
        of = ra_alloc_itemp();
        of_allocated = true;
        la_or(of, zero_ir2_opnd, zero_ir2_opnd);
        break;
    case dt_X86_INS_SAL:
    case dt_X86_INS_SHL:
    case dt_X86_INS_SHR:
    case dt_X86_INS_SAR: {
        IR2_OPND count = ra_alloc_itemp();
        IR2_OPND one = ra_alloc_itemp();
        IR2_OPND done = ra_alloc_label();
        of = ra_alloc_itemp();
        of_allocated = true;
        la_andi(count, rhs, size == 64 ? 0x3f : 0x1f);
        li_d(one, 1);
        la_bne(count, one, done);
        if (ir1_opcode(pir1) == dt_X86_INS_SAL ||
            ir1_opcode(pir1) == dt_X86_INS_SHL) {
            latx_read_eflags(one, CF_USEDEF_BIT);
            la_andi(one, one, CF_BIT);
            la_bstrpick_d(count, result, size - 1, size - 1);
            la_xor(of, count, one);
        } else if (ir1_opcode(pir1) == dt_X86_INS_SHR) {
            la_bstrpick_d(of, lhs, size - 1, size - 1);
        } else {
            la_or(of, zero_ir2_opnd, zero_ir2_opnd);
        }
        la_slli_d(of, of, OF_BIT_INDEX);
        ra_free_temp(one);
        ra_free_temp(count);
        if (rhs_allocated) {
            ra_free_temp(rhs);
            rhs_allocated = false;
        }
        if (lhs_allocated) {
            ra_free_temp(lhs);
            lhs_allocated = false;
        }
        latx_write_eflags(of, OF_USEDEF_BIT);
        la_label(done);
        goto out;
    }
    default:
        handled = false;
        goto out;
    }
    la_bstrpick_d(of, of, size - 1, size - 1);
    la_slli_d(of, of, OF_BIT_INDEX);
    if (first_allocated) {
        ra_free_temp(first);
        first_allocated = false;
    }
    if (rhs_allocated) {
        ra_free_temp(rhs);
        rhs_allocated = false;
    }
    if (lhs_allocated) {
        ra_free_temp(lhs);
        lhs_allocated = false;
    }
    latx_write_eflags(of, OF_USEDEF_BIT);

out:
    if (of_allocated) {
        ra_free_temp(of);
    }
    if (first_allocated) {
        ra_free_temp(first);
    }
    if (rhs_allocated) {
        ra_free_temp(rhs);
    }
    if (lhs_allocated) {
        ra_free_temp(lhs);
    }
    return handled;
}

void generate_eflags_soft(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1,
                          IR1_INST *pir1)
{
    bool soft_result_handled;
    IR2_OPND soft_result =
        generate_common_result(src0, src1, pir1, &soft_result_handled);
    IR2_OPND flag_result;

    if (soft_result_handled) {
        flag_result = soft_result;
    } else {
        ra_free_temp(soft_result);
        flag_result = dest;
    }

    if (ir1_need_calculate_pf(pir1)) {
        generate_pf(flag_result, src0, src1);
    }
    if (ir1_need_calculate_af(pir1)) {
        generate_af(flag_result, src0, src1, pir1);
    }
    if (ir1_need_calculate_zf(pir1)) {
        generate_zf(flag_result, src0, src1, pir1);
    }
    if (ir1_need_calculate_sf(pir1)) {
        generate_sf(flag_result, src0, src1);
    }
    if (ir1_need_calculate_cf(pir1) &&
        !generate_common_cf(flag_result, src0, src1, pir1)) {
        generate_cf(flag_result, src0, src1, pir1);
    }
    if (ir1_need_calculate_of(pir1) &&
        !generate_common_of(flag_result, src0, src1, pir1)) {
        generate_of(flag_result, src0, src1, pir1);
    }
    if (soft_result_handled) {
        ra_free_temp(soft_result);
    }
}
