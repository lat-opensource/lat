/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"
#include "env.h"

#ifdef CONFIG_LATX_AVX_OPT

static inline void xcomisx(IR1_INST *pir1, bool is_double, bool qnan_exp)
{
    /**
     * (bit 6)ZF = 1 if EQ || UOR
     * (bit 2)PF = 1 if UOR (= ZF & CF)
     * (bit 0)CF = 1 if LT || UOR
     */
    lsassert(ir1_opnd_num(pir1) == 2);
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    /* 0. set flag = 0 */
    IR2_OPND flag_zf = ra_alloc_itemp();
    IR2_OPND flag_pf = ra_alloc_itemp();
    IR2_OPND flag = ra_alloc_itemp();
    la_mov64(flag, zero_ir2_opnd);

    /* 1. check ZF, are they equal & unordered? */
    if (is_double) {
        la_fcmp_cond_d(fcc0_ir2_opnd, dest, src, FCMP_COND_CUEQ + qnan_exp);
    } else {
        la_fcmp_cond_s(fcc0_ir2_opnd, dest, src, FCMP_COND_CUEQ + qnan_exp);
    }
    la_movcf2gr(flag_zf, fcc0_ir2_opnd);

    /* 2. check CF, are they less & unordered? */
    if (is_double) {
        la_fcmp_cond_d(fcc2_ir2_opnd, dest, src, FCMP_COND_CULT + qnan_exp);
    } else {
        la_fcmp_cond_s(fcc2_ir2_opnd, dest, src, FCMP_COND_CULT + qnan_exp);
    }
    la_movcf2gr(flag, fcc2_ir2_opnd);

    /* 3. check PF, are they unordered? (= ZF & CF) */
    la_and(flag_pf, flag, flag_zf);

    la_bstrins_w(flag, flag_zf, ZF_BIT_INDEX, ZF_BIT_INDEX);
    la_bstrins_w(flag, flag_pf, PF_BIT_INDEX, PF_BIT_INDEX);

    /* 4. mov flag to EFLAGS */
    latx_write_eflags(flag, 0x3f);

    ra_free_temp(flag_pf);
    ra_free_temp(flag_zf);
    ra_free_temp(flag);

}

bool translate_vucomisd_lsx(IR1_INST *pir1)
{
    xcomisx(pir1, true, false);
    return true;
}

bool translate_vcomisd_lsx(IR1_INST *pir1)
{
    xcomisx(pir1, true, true);
    return true;
}

bool translate_vcomiss_lsx(IR1_INST *pir1)
{
    xcomisx(pir1, false, true);
    return true;
}

bool translate_vucomiss_lsx(IR1_INST *pir1)
{
    xcomisx(pir1, false, false);
    return true;
}

typedef IR2_INST *(*latx_avx_integer_cmp_lsx_fn)(IR2_OPND, IR2_OPND,
                                                 IR2_OPND);

static void translate_avx_integer_cmp_lsx_apply(
    latx_avx_integer_cmp_lsx_fn cmp, bool reverse, IR2_OPND dest,
    IR2_OPND lhs, IR2_OPND rhs)
{
    if (reverse) {
        cmp(dest, rhs, lhs);
    } else {
        cmp(dest, lhs, rhs);
    }
}

static bool translate_avx_integer_cmp_lsx(
    IR1_INST *pir1, latx_avx_integer_cmp_lsx_fn cmp, bool reverse)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int dest_index = ir1_opnd_base_reg_num(opnd0);
    int src1_index = ir1_opnd_base_reg_num(opnd1);
    IR2_OPND src1_low = ra_alloc_ftemp();
    IR2_OPND src2_low;

    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
             (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd1));
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));

    la_vori_b(src1_low, ra_alloc_xmm(src1_index), 0);
    if (ir1_opnd_is_mem(opnd2)) {
        if (ir1_opnd_is_ymm(opnd0)) {
            IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
            IR2_OPND src2_high;

            load_v256_from_ir1_mem_exact(opnd2, &src2_low, &src2_high);
            translate_avx_integer_cmp_lsx_apply(
                cmp, reverse, src1_low, src1_low, src2_low);
            translate_avx_integer_cmp_lsx_apply(
                cmp, reverse, src1_high, src1_high, src2_high);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            store_ymm_high128_shadow(src1_high, dest_index);
            ra_free_temp(src1_high);
            ra_free_temp(src2_high);
        } else {
            src2_low = load_v128_from_ir1_mem_exact(opnd2);
            translate_avx_integer_cmp_lsx_apply(
                cmp, reverse, src1_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            clear_ymm_high128_shadow(dest_index);
        }
    } else {
        int src2_index = ir1_opnd_base_reg_num(opnd2);

        lsassert((ir1_opnd_is_xmm(opnd2) && ir1_opnd_is_xmm(opnd0)) ||
                 (ir1_opnd_is_ymm(opnd2) && ir1_opnd_is_ymm(opnd0)));
        src2_low = ra_alloc_ftemp();
        la_vori_b(src2_low, ra_alloc_xmm(src2_index), 0);
        if (ir1_opnd_is_ymm(opnd0)) {
            IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
            IR2_OPND src2_high = load_ymm_high128_shadow(src2_index);

            translate_avx_integer_cmp_lsx_apply(
                cmp, reverse, src1_low, src1_low, src2_low);
            translate_avx_integer_cmp_lsx_apply(
                cmp, reverse, src1_high, src1_high, src2_high);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            store_ymm_high128_shadow(src1_high, dest_index);
            ra_free_temp(src1_high);
            ra_free_temp(src2_high);
        } else {
            translate_avx_integer_cmp_lsx_apply(
                cmp, reverse, src1_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            clear_ymm_high128_shadow(dest_index);
        }
    }
    ra_free_temp(src2_low);
    ra_free_temp(src1_low);
    return true;
}

#define LATX_AVX_INTEGER_CMP_LSX_DEFINE(opcode, name, cmp, reverse) \
bool translate_v##name##_lsx(IR1_INST *pir1) \
{ \
    return translate_avx_integer_cmp_lsx(pir1, cmp, reverse); \
}
LATX_AVX_INTEGER_CMP_LSX_TABLE(LATX_AVX_INTEGER_CMP_LSX_DEFINE)
#undef LATX_AVX_INTEGER_CMP_LSX_DEFINE

static bool translate_avx_integer_cmp_opcode_lsx(IR1_INST *pir1)
{
    switch (ir1_opcode(pir1)) {
#define LATX_AVX_INTEGER_CMP_LSX_CASE(opcode, name, cmp, reverse) \
    case dt_X86_INS_##opcode: \
        return translate_v##name##_lsx(pir1);
        LATX_AVX_INTEGER_CMP_LSX_TABLE(LATX_AVX_INTEGER_CMP_LSX_CASE)
#undef LATX_AVX_INTEGER_CMP_LSX_CASE
    default:
        return false;
    }
}

bool translate_vpcmpeqx_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_cmp_opcode_lsx(pir1);
}

bool translate_vpcmpgtx_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_cmp_opcode_lsx(pir1);
}

static uint8_t avx_float_cmp_predicate_lsx(IR1_INST *pir1, int base_opcode)
{
    int opcode = ir1_opcode(pir1);

    if (opcode == base_opcode) {
        lsassert(ir1_opnd_num(pir1) == 4 &&
                 ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
        return ir1_opnd_uimm(ir1_get_opnd(pir1, 3)) & 0x1f;
    }

    lsassert(opcode > base_opcode && opcode <= base_opcode + 32);
    return opcode - base_opcode - 1;
}

static void avx_float_cmp_lsx_params(uint8_t predicate, int *condition,
                                     bool *reverse, bool *set_all_ones)
{
    *reverse = false;
    *set_all_ones = false;
    switch (predicate) {
    case 0:
        *condition = X86_FCMP_COND_EQ;
        break;
    case 1:
        *condition = X86_FCMP_COND_LT;
        break;
    case 2:
        *condition = X86_FCMP_COND_LE;
        break;
    case 3:
        *condition = X86_FCMP_COND_UNORD;
        break;
    case 4:
        *condition = X86_FCMP_COND_NEQ;
        break;
    case 5:
        *condition = X86_FCMP_COND_NLT;
        *reverse = true;
        break;
    case 6:
        *condition = X86_FCMP_COND_NLE;
        *reverse = true;
        break;
    case 7:
        *condition = X86_FCMP_COND_ORD;
        break;
    case 8:
        *condition = X86_FCMP_COND_EQ_UQ;
        break;
    case 9:
        *condition = X86_FCMP_COND_NGE;
        break;
    case 10:
        *condition = X86_FCMP_COND_NGT;
        break;
    case 11:
        *condition = X86_FCMP_COND_FALSE;
        break;
    case 12:
        *condition = X86_FCMP_COND_NEQ_OQ;
        break;
    case 13:
        *condition = X86_FCMP_COND_GE;
        *reverse = true;
        break;
    case 14:
        *condition = X86_FCMP_COND_GT;
        *reverse = true;
        break;
    case 15:
        *condition = X86_FCMP_COND_TRUE;
        *set_all_ones = true;
        break;
    case 16:
        *condition = X86_FCMP_COND_EQ_OS;
        break;
    case 17:
        *condition = X86_FCMP_COND_LT_OQ;
        break;
    case 18:
        *condition = X86_FCMP_COND_LE_OQ;
        break;
    case 19:
        *condition = X86_FCMP_COND_UNORD_S;
        break;
    case 20:
        *condition = X86_FCMP_COND_NEQ_US;
        break;
    case 21:
        *condition = X86_FCMP_COND_NLT_UQ;
        *reverse = true;
        break;
    case 22:
        *condition = X86_FCMP_COND_NLE_UQ;
        *reverse = true;
        break;
    case 23:
        *condition = X86_FCMP_COND_ORD_S;
        break;
    case 24:
        *condition = X86_FCMP_COND_EQ_US;
        break;
    case 25:
        *condition = X86_FCMP_COND_NGE_UQ;
        break;
    case 26:
        *condition = X86_FCMP_COND_NGT_UQ;
        break;
    case 27:
        *condition = X86_FCMP_COND_FALSE_OS;
        break;
    case 28:
        *condition = X86_FCMP_COND_NEQ_OS;
        break;
    case 29:
        *condition = X86_FCMP_COND_GE_OQ;
        *reverse = true;
        break;
    case 30:
        *condition = X86_FCMP_COND_GT_OQ;
        *reverse = true;
        break;
    case 31:
        *condition = X86_FCMP_COND_TRUE_US;
        *set_all_ones = true;
        break;
    default:
        lsassert(0);
    }
}

static void translate_avx_float_cmp_lsx_apply(bool double_precision,
                                               IR2_OPND dest, IR2_OPND lhs,
                                               IR2_OPND rhs, int condition,
                                               bool reverse, bool set_all_ones)
{
    if (double_precision) {
        if (reverse) {
            la_vfcmp_cond_d(dest, rhs, lhs, condition);
        } else {
            la_vfcmp_cond_d(dest, lhs, rhs, condition);
        }
    } else if (reverse) {
        la_vfcmp_cond_s(dest, rhs, lhs, condition);
    } else {
        la_vfcmp_cond_s(dest, lhs, rhs, condition);
    }
    if (set_all_ones) {
        la_vori_b(dest, dest, 0xff);
    }
}

static IR2_OPND load_avx_float_cmp_packed_lsx_source(IR1_OPND *opnd)
{
    IR2_OPND value;

    if (ir1_opnd_is_mem(opnd)) {
        return load_v128_from_ir1_mem_exact(opnd);
    }
    lsassert(ir1_opnd_is_xmm(opnd) || ir1_opnd_is_ymm(opnd));
    value = ra_alloc_ftemp();
    la_vori_b(value, ra_alloc_xmm(ir1_opnd_base_reg_num(opnd)), 0);
    return value;
}

static IR2_OPND load_avx_float_cmp_scalar_lsx_source(IR1_OPND *opnd,
                                                       bool double_precision)
{
    IR2_OPND value = ra_alloc_ftemp();

    la_vxor_v(value, value, value);
    if (ir1_opnd_is_mem(opnd)) {
        IR2_OPND bits = double_precision ?
            load_u64_from_ir1_mem_exact(opnd) :
            load_u32_from_ir1_mem_exact(opnd);

        if (double_precision) {
            la_vinsgr2vr_d(value, bits, 0);
        } else {
            la_vinsgr2vr_w(value, bits, 0);
        }
        ra_free_temp(bits);
    } else {
        lsassert(ir1_opnd_is_xmm(opnd));
        la_vori_b(value, ra_alloc_xmm(ir1_opnd_base_reg_num(opnd)), 0);
    }
    return value;
}

static bool translate_avx_float_cmp_packed_lsx(IR1_INST *pir1,
                                                int base_opcode,
                                                bool double_precision)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    bool is_ymm = ir1_opnd_is_ymm(opnd0);
    int dest_index = ir1_opnd_base_reg_num(opnd0);
    int src1_index = ir1_opnd_base_reg_num(opnd1);
    uint8_t predicate = avx_float_cmp_predicate_lsx(pir1, base_opcode);
    int condition;
    bool reverse;
    bool set_all_ones;
    IR2_OPND src1_low;
    IR2_OPND src2_low;

    lsassert((is_ymm && ir1_opnd_is_ymm(opnd1)) ||
             (!is_ymm && ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)));
    avx_float_cmp_lsx_params(predicate, &condition, &reverse,
                             &set_all_ones);
    lsassert(ir1_opnd_is_mem(opnd2) ||
             (is_ymm ? ir1_opnd_is_ymm(opnd2) : ir1_opnd_is_xmm(opnd2)));
    src1_low = load_avx_float_cmp_packed_lsx_source(opnd1);
    if (ir1_opnd_is_mem(opnd2) && is_ymm) {
        IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
        IR2_OPND src2_high;

        load_v256_from_ir1_mem_exact(opnd2, &src2_low, &src2_high);
        translate_avx_float_cmp_lsx_apply(double_precision, src1_low,
                                          src1_low, src2_low, condition,
                                          reverse, set_all_ones);
        translate_avx_float_cmp_lsx_apply(double_precision, src1_high,
                                          src1_high, src2_high, condition,
                                          reverse, set_all_ones);
        la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
        store_ymm_high128_shadow(src1_high, dest_index);
        ra_free_temp(src2_high);
        ra_free_temp(src1_high);
    } else {
        src2_low = load_avx_float_cmp_packed_lsx_source(opnd2);
        if (is_ymm) {
            int src2_index = ir1_opnd_base_reg_num(opnd2);
            IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
            IR2_OPND src2_high = load_ymm_high128_shadow(src2_index);

            translate_avx_float_cmp_lsx_apply(double_precision, src1_low,
                                              src1_low, src2_low, condition,
                                              reverse, set_all_ones);
            translate_avx_float_cmp_lsx_apply(double_precision, src1_high,
                                              src1_high, src2_high, condition,
                                              reverse, set_all_ones);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            store_ymm_high128_shadow(src1_high, dest_index);
            ra_free_temp(src2_high);
            ra_free_temp(src1_high);
        } else {
            translate_avx_float_cmp_lsx_apply(double_precision, src1_low,
                                              src1_low, src2_low, condition,
                                              reverse, set_all_ones);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            clear_ymm_high128_shadow(dest_index);
        }
    }
    ra_free_temp(src2_low);
    ra_free_temp(src1_low);
    return true;
}

static bool translate_avx_float_cmp_scalar_lsx(IR1_INST *pir1,
                                                int base_opcode,
                                                bool double_precision)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int dest_index = ir1_opnd_base_reg_num(opnd0);
    int src1_index = ir1_opnd_base_reg_num(opnd1);
    uint8_t predicate = avx_float_cmp_predicate_lsx(pir1, base_opcode);
    int condition;
    bool reverse;
    bool set_all_ones;
    IR2_OPND src1_scalar;
    IR2_OPND src2_scalar;
    IR2_OPND result = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    avx_float_cmp_lsx_params(predicate, &condition, &reverse,
                             &set_all_ones);
    src1_scalar = load_avx_float_cmp_scalar_lsx_source(opnd1,
                                                        double_precision);
    src2_scalar = load_avx_float_cmp_scalar_lsx_source(opnd2,
                                                        double_precision);
    if (double_precision) {
        la_vreplve_d(src1_scalar, src1_scalar, zero_ir2_opnd);
        la_vreplve_d(src2_scalar, src2_scalar, zero_ir2_opnd);
    } else {
        la_vreplve_w(src1_scalar, src1_scalar, zero_ir2_opnd);
        la_vreplve_w(src2_scalar, src2_scalar, zero_ir2_opnd);
    }
    translate_avx_float_cmp_lsx_apply(double_precision, result,
                                      src1_scalar, src2_scalar, condition,
                                      reverse, set_all_ones);
    if (dest_index != src1_index) {
        la_vori_b(ra_alloc_xmm(dest_index),
                  ra_alloc_xmm(src1_index), 0);
    }
    if (double_precision) {
        la_vextrins_d(ra_alloc_xmm(dest_index), result, 0);
    } else {
        la_vextrins_w(ra_alloc_xmm(dest_index), result, 0);
    }
    clear_ymm_high128_shadow(dest_index);

    ra_free_temp(result);
    ra_free_temp(src2_scalar);
    ra_free_temp(src1_scalar);
    return true;
}

bool translate_vcmppd_lsx(IR1_INST *pir1)
{
    return translate_avx_float_cmp_packed_lsx(pir1, dt_X86_INS_VCMPPD, true);
}

bool translate_vcmpps_lsx(IR1_INST *pir1)
{
    return translate_avx_float_cmp_packed_lsx(pir1, dt_X86_INS_VCMPPS, false);
}

bool translate_vcmpsd_lsx(IR1_INST *pir1)
{
    return translate_avx_float_cmp_scalar_lsx(pir1, dt_X86_INS_VCMPSD, true);
}

bool translate_vcmpss_lsx(IR1_INST *pir1)
{
    return translate_avx_float_cmp_scalar_lsx(pir1, dt_X86_INS_VCMPSS, false);
}
#endif
