/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "translate.h"

#ifdef CONFIG_LATX_AVX_OPT

static bool avx_lsx_vex_256(IR1_INST *pir1)
{
    const uint8_t *bytes = pir1->info->bytes;

    if (bytes[0] == 0xc5) {
        return bytes[1] & 0x04;
    }
    if (bytes[0] == 0xc4) {
        return bytes[2] & 0x04;
    }
    return false;
}

/* Keep a 256-bit guest value as two LSX registers.  The high half is kept in
 * ymmh[] while LASX is disabled, so never use load_freg256_from_ir1() here. */
static void load_avx_cvt_value_lsx(IR1_OPND *opnd, bool ymm,
                                   IR2_OPND *low, IR2_OPND *high)
{
    *high = (IR2_OPND){ 0 };
    if (ir1_opnd_is_mem(opnd)) {
        if (ymm) {
            /* VEX.L carries the width for these destination-narrowing
             * conversions, while IR1 may retain a 128-bit memory size. */
            IR1_OPND wide = *opnd;

            wide.size = 32;
            load_v256_from_ir1_mem_exact(&wide, low, high);
        } else if (ir1_opnd_size(opnd) == 64) {
            /* XMM VCVTPS2PD/VCVTDQ2PD consume only two 32-bit elements. */
            IR2_OPND value = load_u64_from_ir1_mem_exact(opnd);

            *low = ra_alloc_ftemp();
            la_vreplgr2vr_d(*low, value);
            ra_free_temp(value);
        } else {
            *low = load_v128_from_ir1_mem_exact(opnd);
        }
        return;
    }

    lsassert(ir1_opnd_is_xmm(opnd) || ir1_opnd_is_ymm(opnd));
    *low = ra_alloc_ftemp();
    la_vori_b(*low, ra_alloc_xmm(ir1_opnd_base_reg_num(opnd)), 0);
    if (ymm) {
        *high = load_ymm_high128_shadow(ir1_opnd_base_reg_num(opnd));
    }
}

static void store_avx_cvt_value_lsx(IR2_OPND low, IR2_OPND high, int dest,
                                    bool ymm)
{
    la_vori_b(ra_alloc_xmm(dest), low, 0);
    if (ymm) {
        store_ymm_high128_shadow(high, dest);
    } else {
        clear_ymm_high128_shadow(dest);
    }
}

static void pack_avx_cvt_low64_lsx(IR2_OPND dest, IR2_OPND low,
                                   IR2_OPND high, bool have_high)
{
    IR2_OPND value = ra_alloc_itemp();

    la_vxor_v(dest, dest, dest);
    la_vpickve2gr_du(value, low, 0);
    la_vinsgr2vr_d(dest, value, 0);
    if (have_high) {
        la_vpickve2gr_du(value, high, 0);
        la_vinsgr2vr_d(dest, value, 1);
    }
    ra_free_temp(value);
}

bool translate_vcvtdq2ps_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    IR2_OPND result_high = ymm ? ra_alloc_ftemp() : (IR2_OPND){ 0 };
    IR2_OPND fcsr = set_fpu_fcsr_rounding_field_by_x86();

    load_avx_cvt_value_lsx(ir1_get_opnd(pir1, 1), ymm, &src_low, &src_high);
    la_vffint_s_w(result_low, src_low);
    if (ymm) {
        la_vffint_s_w(result_high, src_high);
    }
    store_avx_cvt_value_lsx(result_low, result_high,
                             ir1_opnd_base_reg_num(dest_opnd), ymm);
    set_fpu_rounding_mode(fcsr);
    ra_free_temp_auto(fcsr);
    ra_free_temp(result_low);
    if (ymm) {
        ra_free_temp(result_high);
        ra_free_temp(src_high);
    }
    ra_free_temp(src_low);
    return true;
}

bool translate_vcvtdq2pd_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    IR2_OPND src;
    IR2_OPND low = ra_alloc_ftemp();
    IR2_OPND high = ymm ? ra_alloc_ftemp() : (IR2_OPND){ 0 };
    IR2_OPND fcsr = set_fpu_fcsr_rounding_field_by_x86();

    load_avx_cvt_value_lsx(ir1_get_opnd(pir1, 1), false, &src,
                           &(IR2_OPND){ 0 });
    la_vffintl_d_w(low, src);
    if (ymm) {
        la_vffinth_d_w(high, src);
    }
    store_avx_cvt_value_lsx(low, high, ir1_opnd_base_reg_num(dest_opnd), ymm);
    set_fpu_rounding_mode(fcsr);
    ra_free_temp_auto(fcsr);
    ra_free_temp(src);
    ra_free_temp(low);
    if (ymm) {
        ra_free_temp(high);
    }
    return true;
}

bool translate_vcvtps2pd_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    IR2_OPND src;
    IR2_OPND low = ra_alloc_ftemp();
    IR2_OPND high = ymm ? ra_alloc_ftemp() : (IR2_OPND){ 0 };
    IR2_OPND fcsr = set_fpu_fcsr_rounding_field_by_x86();

    load_avx_cvt_value_lsx(ir1_get_opnd(pir1, 1), false, &src,
                           &(IR2_OPND){ 0 });
    la_vfcvtl_d_s(low, src);
    if (ymm) {
        la_vfcvth_d_s(high, src);
    }
    store_avx_cvt_value_lsx(low, high, ir1_opnd_base_reg_num(dest_opnd), ymm);
    set_fpu_rounding_mode(fcsr);
    ra_free_temp_auto(fcsr);
    ra_free_temp(src);
    ra_free_temp(low);
    if (ymm) {
        ra_free_temp(high);
    }
    return true;
}

static bool translate_vcvtpd2x_lsx(IR1_INST *pir1, bool truncate)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    bool src_ymm = ir1_opnd_is_ymm(src_opnd) || avx_lsx_vex_256(pir1);
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    IR2_OPND result_high = src_ymm ? ra_alloc_ftemp() : (IR2_OPND){ 0 };
    IR2_OPND dest = ra_alloc_ftemp();
    IR2_OPND fcsr = set_fpu_fcsr_rounding_field_by_x86();

    load_avx_cvt_value_lsx(src_opnd, src_ymm, &src_low, &src_high);
    if (truncate) {
        la_vftintrz_w_d(result_low, src_low, src_low);
        if (src_ymm) {
            la_vftintrz_w_d(result_high, src_high, src_high);
        }
    } else {
        la_vftint_w_d(result_low, src_low, src_low);
        if (src_ymm) {
            la_vftint_w_d(result_high, src_high, src_high);
        }
    }
    {
        IR2_OPND invalid = ra_alloc_ftemp();
        IR2_OPND overflow = ra_alloc_ftemp();
        IR2_OPND mask = ra_alloc_ftemp();
        IR2_OPND value = ra_alloc_itemp();

        /* x86 writes its indefinite integer for NaN and positive overflow. */
        li_d(value, UINT64_C(0x0000000080000000));
        la_vreplgr2vr_w(invalid, value);
        li_d(value, UINT64_C(0x41e0000000000000));
        la_vreplgr2vr_d(overflow, value);
        la_vfcmp_cond_d(mask, overflow, src_low, FCMP_COND_CULE);
        la_vshuf4i_w(mask, mask, 0x88);
        la_vbitsel_v(result_low, result_low, invalid, mask);
        if (src_ymm) {
            la_vfcmp_cond_d(mask, overflow, src_high, FCMP_COND_CULE);
            la_vshuf4i_w(mask, mask, 0x88);
            la_vbitsel_v(result_high, result_high, invalid, mask);
        }
        ra_free_temp(value);
        ra_free_temp(mask);
        ra_free_temp(overflow);
        ra_free_temp(invalid);
    }
    pack_avx_cvt_low64_lsx(dest, result_low, result_high, src_ymm);
    store_avx_cvt_value_lsx(dest, (IR2_OPND){ 0 },
                             ir1_opnd_base_reg_num(dest_opnd), false);
    set_fpu_rounding_mode(fcsr);
    ra_free_temp_auto(fcsr);
    ra_free_temp(dest);
    ra_free_temp(result_low);
    ra_free_temp(src_low);
    if (src_ymm) {
        ra_free_temp(result_high);
        ra_free_temp(src_high);
    }
    return true;
}

bool translate_vcvtpd2dq_lsx(IR1_INST *pir1)
{
    return translate_vcvtpd2x_lsx(pir1, false);
}

bool translate_vcvttpd2dq_lsx(IR1_INST *pir1)
{
    return translate_vcvtpd2x_lsx(pir1, true);
}

bool translate_vcvtpd2ps_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    bool src_ymm = ir1_opnd_is_ymm(src_opnd) || avx_lsx_vex_256(pir1);
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    IR2_OPND result_high = src_ymm ? ra_alloc_ftemp() : (IR2_OPND){ 0 };
    IR2_OPND dest = ra_alloc_ftemp();
    IR2_OPND fcsr = set_fpu_fcsr_rounding_field_by_x86();

    load_avx_cvt_value_lsx(src_opnd, src_ymm, &src_low, &src_high);
    la_vfcvt_s_d(result_low, src_low, src_low);
    if (src_ymm) {
        la_vfcvt_s_d(result_high, src_high, src_high);
    }
    pack_avx_cvt_low64_lsx(dest, result_low, result_high, src_ymm);
    store_avx_cvt_value_lsx(dest, (IR2_OPND){ 0 },
                             ir1_opnd_base_reg_num(dest_opnd), false);
    set_fpu_rounding_mode(fcsr);
    ra_free_temp_auto(fcsr);
    ra_free_temp(dest);
    ra_free_temp(result_low);
    ra_free_temp(src_low);
    if (src_ymm) {
        ra_free_temp(result_high);
        ra_free_temp(src_high);
    }
    return true;
}

static bool translate_vcvtps2dq_lsx_common(IR1_INST *pir1, bool truncate)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    IR2_OPND result_high = ymm ? ra_alloc_ftemp() : (IR2_OPND){ 0 };
    IR2_OPND fcsr = set_fpu_fcsr_rounding_field_by_x86();

    load_avx_cvt_value_lsx(ir1_get_opnd(pir1, 1), ymm, &src_low, &src_high);
    if (truncate) {
        la_vftintrz_w_s(result_low, src_low);
        if (ymm) {
            la_vftintrz_w_s(result_high, src_high);
        }
    } else {
        la_vftint_w_s(result_low, src_low);
        if (ymm) {
            la_vftint_w_s(result_high, src_high);
        }
    }
    {
        IR2_OPND invalid = ra_alloc_ftemp();
        IR2_OPND overflow = ra_alloc_ftemp();
        IR2_OPND mask = ra_alloc_ftemp();
        IR2_OPND value = ra_alloc_itemp();

        li_d(value, UINT64_C(0x0000000080000000));
        la_vreplgr2vr_w(invalid, value);
        li_d(value, UINT64_C(0x000000004f000000));
        la_vreplgr2vr_w(overflow, value);
        la_vfcmp_cond_s(mask, overflow, src_low, FCMP_COND_CULE);
        la_vbitsel_v(result_low, result_low, invalid, mask);
        if (ymm) {
            la_vfcmp_cond_s(mask, overflow, src_high, FCMP_COND_CULE);
            la_vbitsel_v(result_high, result_high, invalid, mask);
        }
        ra_free_temp(value);
        ra_free_temp(mask);
        ra_free_temp(overflow);
        ra_free_temp(invalid);
    }
    store_avx_cvt_value_lsx(result_low, result_high,
                             ir1_opnd_base_reg_num(dest_opnd), ymm);
    set_fpu_rounding_mode(fcsr);
    ra_free_temp_auto(fcsr);
    ra_free_temp(result_low);
    ra_free_temp(src_low);
    if (ymm) {
        ra_free_temp(result_high);
        ra_free_temp(src_high);
    }
    return true;
}

bool translate_vcvtps2dq_lsx(IR1_INST *pir1)
{
    return translate_vcvtps2dq_lsx_common(pir1, false);
}

bool translate_vcvttps2dq_lsx(IR1_INST *pir1)
{
    return translate_vcvtps2dq_lsx_common(pir1, true);
}

bool translate_vcvtsd2ss_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int dest_index = ir1_opnd_base_reg_num(opnd0);
    IR2_OPND src1 = ra_alloc_ftemp();
    IR2_OPND src2;
    IR2_OPND converted = ra_alloc_ftemp();
    IR2_OPND fcsr;
    bool src2_is_temp = false;

    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    lsassert(ir1_opnd_is_xmm(opnd2) ||
             (ir1_opnd_is_mem(opnd2) && ir1_opnd_size(opnd2) == 64));

    la_vori_b(src1, ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1)), 0);
    if (ir1_opnd_is_mem(opnd2)) {
        IR2_OPND value = load_u64_from_ir1_mem_exact(opnd2);

        src2 = ra_alloc_ftemp();
        src2_is_temp = true;
        la_vxor_v(src2, src2, src2);
        la_vinsgr2vr_d(src2, value, 0);
        ra_free_temp(value);
    } else {
        src2 = load_freg128_from_ir1(opnd2);
    }

    fcsr = set_fpu_fcsr_rounding_field_by_x86();
    la_fcvt_s_d(converted, src2);
    set_fpu_rounding_mode(fcsr);
    ra_free_temp_auto(fcsr);
    la_vextrins_w(src1, converted, VEXTRINS_IMM_4_0(0, 0));
    la_vori_b(ra_alloc_xmm(dest_index), src1, 0);
    clear_ymm_high128_shadow(dest_index);

    if (src2_is_temp) {
        ra_free_temp(src2);
    }
    ra_free_temp(converted);
    ra_free_temp(src1);
    return true;
}

bool translate_vcvtsi2sd_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int dest_index = ir1_opnd_base_reg_num(opnd0);
    IR2_OPND src1 = ra_alloc_ftemp();
    IR2_OPND src2;
    IR2_OPND converted = ra_alloc_ftemp();
    IR2_OPND converted_low = ra_alloc_itemp();
    IR2_OPND fcsr;

    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    lsassert((ir1_opnd_is_gpr(opnd2) || ir1_opnd_is_mem(opnd2)) &&
             (ir1_opnd_size(opnd2) == 32 || ir1_opnd_size(opnd2) == 64));

    la_vori_b(src1, ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1)), 0);
    if (ir1_opnd_is_mem(opnd2)) {
        src2 = ir1_opnd_size(opnd2) == 32 ?
            load_u32_from_ir1_mem_exact(opnd2) :
            load_u64_from_ir1_mem_exact(opnd2);
        if (ir1_opnd_size(opnd2) == 32) {
            la_mov32_sx(src2, src2);
        }
    } else {
        src2 = load_ireg_from_ir1(opnd2, SIGN_EXTENSION, false);
    }

    fcsr = set_fpu_fcsr_rounding_field_by_x86();
    la_vxor_v(converted, converted, converted);
    la_vinsgr2vr_d(converted, src2, 0);
    if (ir1_opnd_size(opnd2) == 32) {
        la_vffintl_d_w(converted, converted);
    } else {
        la_vffint_d_l(converted, converted);
    }
    set_fpu_rounding_mode(fcsr);
    ra_free_temp_auto(fcsr);
    la_vpickve2gr_du(converted_low, converted, 0);
    la_vinsgr2vr_d(src1, converted_low, 0);
    la_vori_b(ra_alloc_xmm(dest_index), src1, 0);
    clear_ymm_high128_shadow(dest_index);

    ra_free_temp_auto(src2);
    ra_free_temp(converted_low);
    ra_free_temp(converted);
    ra_free_temp(src1);
    return true;
}

#endif
