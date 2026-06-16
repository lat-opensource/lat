/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "translate.h"

#ifdef CONFIG_LATX_AVX_OPT

bool translate_vcvtps2pd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vfcvth_d_s(temp, src);
        la_vfcvtl_d_s(dest, src);
        la_xvpermi_q(dest, temp, XVPERMI_Q_4_0(0, 2));
    } else {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vfcvtl_d_s(temp, src);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vcvtpd2ps(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));

    if (ir1_opnd_size(ir1_get_opnd(pir1, 1)) == 128) {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));

        la_vfcvt_s_d(dest, src, src);
        la_xvpickve_d(dest, dest, 0);
    } else if (ir1_opnd_size(ir1_get_opnd(pir1, 1)) == 256) {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();

        la_xvpermi_q(temp, src, XVPERMI_Q_4_0(1, 1));
        la_vfcvt_s_d(temp, temp, src);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    } else {
        lsassert(0);
    }
    return true;
}

bool translate_vcvtdq2ps(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));

        la_xvffint_s_w(dest, src);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vffint_s_w(temp, src);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

static bool translate_vcvtps2dq_opt(IR1_INST * pir1) {
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    lsassert(ir1_opnd_is_xmm(opnd0) ||
        ir1_opnd_is_ymm(opnd0));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_OPND temp_f = ra_alloc_ftemp();
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm(opnd0)) {
        la_vftint_w_s(temp_f, src);
        la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
        la_vldi(overflow, (0b10011 << 8) | 0x4f); //0x134f
        la_vfcmp_cond_s(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
        la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
        la_vand_v(dest, temp_f, temp_f);
        set_high128_xreg_to_zero(dest);
    } else {
        la_xvftint_w_s(temp_f, src);
        la_xvldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
        la_xvldi(overflow, (0b10011 << 8) | 0x4f); //0x134f
        la_xvfcmp_cond_s(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
        la_xvbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
        la_xvand_v(dest, temp_f, temp_f);
    }
    return true;
}

bool translate_vcvtps2dq(IR1_INST * pir1) {
    if (option_cvt_opt) {
        return translate_vcvtps2dq_opt(pir1);
    }
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    lsassert(ir1_opnd_is_xmm(opnd0) ||
        ir1_opnd_is_ymm(opnd0));

    bool is_same_reg = ir1_opnd_is_same_reg(opnd0, opnd1);

    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND temp_fcsr = ra_alloc_itemp();
        IR2_OPND temp_int = ra_alloc_itemp();
        IR2_OPND temp_operand_count = ra_alloc_itemp();
        IR2_OPND label_over = ra_alloc_label();
        IR2_OPND label_second_operand = ra_alloc_label();
        IR2_OPND label_third_operand = ra_alloc_label();
        IR2_OPND label_fourth_operand = ra_alloc_label();
        IR2_OPND label_fifth_operand = ra_alloc_label();
        IR2_OPND label_sixth_operand = ra_alloc_label();
        IR2_OPND label_seventh_operand = ra_alloc_label();
        IR2_OPND label_eighth_operand = ra_alloc_label();
        IR2_OPND dest = is_same_reg ? ra_alloc_ftemp() : load_freg256_from_ir1(opnd0);
        IR2_OPND src = load_freg256_from_ir1(opnd1);

        la_xvftint_w_s(dest, src);

        /* check if INVALID bit in fcsr is set */
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);

        /* if no INVALID exception happend, convertion done */
        la_beqz(temp_fcsr, label_over);

        /* if INVALID exception did happen, check the four operands separately */
        li_wu(temp_int, 0x80000000);
        IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
        IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();
        IR2_OPND src_h128 = ra_alloc_ftemp();
        la_xvpermi_q(src_h128, src, XVPERMI_Q_4_0(1, 1));

        /* check the first single operand */
        la_xvreplve_w(ftemp_src_temp1, src, zero_ir2_opnd);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_second_operand);
        la_xvinsgr2vr_w(dest, temp_int, 0);

        /* check the second single operand */
        la_label(label_second_operand);
        li_wu(temp_operand_count, 0x1);
        la_xvreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_third_operand);
        la_xvinsgr2vr_w(dest, temp_int, 1);

        /* check the third single operand */
        la_label(label_third_operand);
        li_wu(temp_operand_count, 0x2);
        la_xvreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_fourth_operand);
        la_xvinsgr2vr_w(dest, temp_int, 2);

        /* check the fourth single operand */
        la_label(label_fourth_operand);
        li_wu(temp_operand_count, 0x3);
        la_xvreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_fifth_operand);
        la_xvinsgr2vr_w(dest, temp_int, 3);

        /* check the fifth single operand */
        la_label(label_fifth_operand);
        la_xvreplve_w(ftemp_src_temp1, src_h128, zero_ir2_opnd);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_sixth_operand);
        la_xvinsgr2vr_w(dest, temp_int, 4);

        /* check the sixth single operand */
        la_label(label_sixth_operand);
        li_wu(temp_operand_count, 0x1);
        la_xvreplve_w(ftemp_src_temp1, src_h128, temp_operand_count);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_seventh_operand);
        la_xvinsgr2vr_w(dest, temp_int, 5);

        /* check the seventh single operand */
        la_label(label_seventh_operand);
        li_wu(temp_operand_count, 0x2);
        la_xvreplve_w(ftemp_src_temp1, src_h128, temp_operand_count);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_eighth_operand);
        la_xvinsgr2vr_w(dest, temp_int, 6);

        /* check the eighth single operand */
        la_label(label_eighth_operand);
        li_wu(temp_operand_count, 0x3);
        la_xvreplve_w(ftemp_src_temp1, src_h128, temp_operand_count);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_over);
        la_xvinsgr2vr_w(dest, temp_int, 7);

        la_label(label_over);
        if (is_same_reg) {
            la_xvori_b(load_freg256_from_ir1(opnd0), dest, 0);
        }
        ra_free_temp(temp_fcsr);
        ra_free_temp(temp_int);
        ra_free_temp(temp_operand_count);
        ra_free_temp(ftemp_src_temp1);
        ra_free_temp(ftemp_src_temp2);
    } else {
        IR2_OPND temp_fcsr = ra_alloc_itemp();
        IR2_OPND temp_int = ra_alloc_itemp();
        IR2_OPND temp_operand_count = ra_alloc_itemp();
        IR2_OPND label_over = ra_alloc_label();
        IR2_OPND label_second_operand = ra_alloc_label();
        IR2_OPND label_third_operand = ra_alloc_label();
        IR2_OPND label_fourth_operand = ra_alloc_label();
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src = load_freg128_from_ir1(opnd1);
        IR2_OPND temp = ra_alloc_ftemp();

        la_vftint_w_s(temp, src);

        /* check if INVALID bit in fcsr is set */
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);

        /* if no INVALID exception happend, convertion done */
        la_beqz(temp_fcsr, label_over);

        /* if INVALID exception did happen, check the four operands separately */
        li_wu(temp_int, 0x80000000);
        IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
        IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();

        /* check the first single operand */
        la_vreplve_w(ftemp_src_temp1, src, zero_ir2_opnd);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_second_operand);
        la_vinsgr2vr_w(temp, temp_int, 0);

        /* check the second single operand */
        la_label(label_second_operand);
        li_wu(temp_operand_count, 0x1);
        la_vreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_third_operand);
        la_vinsgr2vr_w(temp, temp_int, 1);

        /* check the third single operand */
        la_label(label_third_operand);
        li_wu(temp_operand_count, 0x2);
        la_vreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_fourth_operand);
        la_vinsgr2vr_w(temp, temp_int, 2);


        /* check the fourth single operand */
        la_label(label_fourth_operand);
        li_wu(temp_operand_count, 0x3);
        la_vreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_over);
        la_vinsgr2vr_w(temp, temp_int, 3);

        la_label(label_over);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
        ra_free_temp(temp_fcsr);
        ra_free_temp(temp_int);
        ra_free_temp(temp_operand_count);
        ra_free_temp(ftemp_src_temp1);
        ra_free_temp(ftemp_src_temp2);
    }
    return true;
}

bool translate_vcvtdq2pd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));
    IR2_OPND fcsr_opnd = set_fpu_fcsr_rounding_field_by_x86();
    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();

        la_vffintl_d_w(temp1, src);
        la_vffinth_d_w(temp2, src);
        la_xvpermi_q(temp1, temp2, XVPERMI_Q_4_0(0, 2));
        la_xvori_b(dest, temp1, 0);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vffintl_d_w(temp, src);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    set_fpu_rounding_mode(fcsr_opnd);
    return true;
}

static bool translate_vcvtpd2dq_opt(IR1_INST * pir1) {
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_OPND temp_f = ra_alloc_ftemp();
    IR2_OPND mxcsr = ra_alloc_itemp();
    IR2_OPND temp_i = ra_alloc_itemp();
    int offset = lsenv_offset_of_mxcsr(lsenv);
    IR2_OPND label_rd_rz = ra_alloc_label();
    IR2_OPND label_ru = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();

    la_ld_w(mxcsr, env_ir2_opnd, offset);
    la_bstrpick_d(temp_i, mxcsr, 13, 13);
    la_bnez(temp_i, label_rd_rz);

    la_bstrpick_d(temp_i, mxcsr, 14, 14);
    la_bnez(temp_i, label_ru);
    /*Round to nearest(00B)*/
    li_d(temp_i, 0x41dfffffffe00000); //2147483647.5
    la_b(label_exit);
    la_label(label_ru);
    /*Round up (10B)*/
    li_d(temp_i, 0x41DFFFFFFFC00001); //2147483647.0000002
    la_b(label_exit);
    la_label(label_rd_rz);
    /*Round down(01B) or Round toward zero(11B)*/
    li_d(temp_i, 0x41E0000000000000); //2147483648.0
    la_label(label_exit);

    if (ir1_opnd_size(opnd1) == 128) {
        la_vftint_w_d(temp_f, src, src);
        la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
        la_vreplgr2vr_d(overflow, temp_i);
        la_vfcmp_cond_d(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
        la_vshuf4i_w(comp_mask, comp_mask, 0x88);
        la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
        la_xvpickve_d(dest, temp_f, 0);
    } else {
        IR2_OPND src_h128 = ra_alloc_ftemp();
        la_xvpermi_q(src_h128, src, XVPERMI_Q_4_0(1, 1));
        la_vftint_w_d(temp_f, src_h128, src);
        la_xvldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
        la_xvreplgr2vr_d(overflow, temp_i);
        la_xvfcmp_cond_d(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
        la_xvshuf4i_w(comp_mask, comp_mask, 0x88);
        la_xvpermi_d(comp_mask, comp_mask, 0x88);
        // la_xvshuf4i_d(comp_mask, comp_mask, 0x88);
        la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
        la_vand_v(dest, temp_f, temp_f);
        set_high128_xreg_to_zero(dest);
    }
    return true;
}
bool translate_vcvtpd2dq(IR1_INST * pir1) {
    if (option_cvt_opt) {
        return translate_vcvtpd2dq_opt(pir1);
    }

    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));

    if (ir1_opnd_size(ir1_get_opnd(pir1, 1)) == 256) {
        IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND src_h128 = ra_alloc_ftemp();
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp_fcsr = ra_alloc_itemp();
        IR2_OPND temp_int = ra_alloc_itemp();
        IR2_OPND label_over = ra_alloc_label();
        IR2_OPND label_second_operand = ra_alloc_label();
        IR2_OPND label_third_operand = ra_alloc_label();
        IR2_OPND label_fourth_operand = ra_alloc_label();

        la_xvpermi_q(src_h128, src, XVPERMI_Q_4_0(1, 1));
        la_vftint_w_d(temp, src_h128, src);

        /* check if INVALID bit in fcsr is set */
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);

        /* if no INVALID exception happend, convertion done */
        la_beqz(temp_fcsr, label_over);

        /* if INVALID exception did happen, check the four operands separately */
        li_wu(temp_int, 0x80000000);
        IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
        IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();

        /* check the first single operand */
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(0, 0, 0, 0));
        la_vftint_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_second_operand);
        la_vinsgr2vr_w(temp, temp_int, 0);

        /* check the second single operand */
        la_label(label_second_operand);
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(1, 1, 1, 1));
        la_vftint_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_third_operand);
        la_vinsgr2vr_w(temp, temp_int, 1);

        /* check the third single operand */
        la_label(label_third_operand);
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(2, 2, 2, 2));
        la_vftint_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_fourth_operand);
        la_vinsgr2vr_w(temp, temp_int, 2);


        /* check the fourth single operand */
        la_label(label_fourth_operand);
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(3, 3, 3, 3));
        la_vftint_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_over);
        la_vinsgr2vr_w(temp, temp_int, 3);

        la_label(label_over);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
        ra_free_temp(temp_fcsr);
        ra_free_temp(temp_int);
        ra_free_temp(ftemp_src_temp1);
        ra_free_temp(ftemp_src_temp2);
    } else if (ir1_opnd_size(ir1_get_opnd(pir1, 1)) == 128) {
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp_fcsr = ra_alloc_itemp();
        IR2_OPND temp_int = ra_alloc_itemp();
        IR2_OPND label_over = ra_alloc_label();
        IR2_OPND label_second_operand = ra_alloc_label();

        la_vftint_w_d(temp, src, src);

        /* check if INVALID bit in fcsr is set */
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);

        /* if no INVALID exception happend, convertion done */
        la_beqz(temp_fcsr, label_over);

        /* if INVALID exception did happen, check the four operands separately */
        li_wu(temp_int, 0x80000000);
        IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
        IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();

        /* check the first single operand */
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(0, 0, 0, 0));
        la_vftint_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_second_operand);
        la_vinsgr2vr_w(temp, temp_int, 0);

        /* check the second single operand */
        la_label(label_second_operand);
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(1, 1, 1, 1));
        la_vftint_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_over);
        la_vinsgr2vr_w(temp, temp_int, 1);

        la_label(label_over);
        la_xvpickve_d(dest, temp, 0);
        ra_free_temp(temp_fcsr);
        ra_free_temp(temp_int);
        ra_free_temp(ftemp_src_temp1);
        ra_free_temp(ftemp_src_temp2);
    } else {
        lsassert(0);
    }

    return true;
}

bool translate_vcvtsd2ss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND fcsr_opnd = set_fpu_fcsr_rounding_field_by_x86();
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND dest_temp = ra_alloc_ftemp();
    la_fcvt_s_d(temp, src2);
    la_vori_b(dest_temp, src1, 0);
    la_xvinsve0_w(dest_temp, temp, 0);
    set_high128_xreg_to_zero(dest_temp);
    la_xvori_b(dest, dest_temp, 0);
    set_fpu_rounding_mode(fcsr_opnd);
    return true;
}

bool translate_vcvtsi2sd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);
    IR2_OPND temp = ra_alloc_ftemp();
    la_movgr2fr_d(temp, src2);
    if (ir1_opnd_size(opnd2) == 32) {
        la_vffintl_d_w(temp, temp);
    } else if (ir1_opnd_size(opnd2) == 64) {
        la_vffint_d_l(temp, temp);
    } else {
        lsassert(0);
    }
    la_vshuf4i_d(temp, src1, 0xc);
    set_high128_xreg_to_zero(temp);
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vcvtss2sd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    la_fcvt_d_s(temp, src2);
    la_vshuf4i_d(temp, src1, 0xc);
    set_high128_xreg_to_zero(temp);
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vcvtsi2ss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR2_OPND fcsr_opnd = set_fpu_fcsr_rounding_field_by_x86();
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);
    IR2_OPND temp = ra_alloc_ftemp();
    la_movgr2fr_d(temp, src2);
    if (ir1_opnd_size(opnd2) == 64) {
        la_ffint_s_l(temp, temp);
    } else {
        la_ffint_s_w(temp, temp);
    }
    if (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) != ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1))) {
        la_xvori_b(dest, src1, 0);
    }
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    set_fpu_rounding_mode(fcsr_opnd);
    return true;
}

static bool translate_vcvttpd2dq_opt(IR1_INST * pir1) {
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);

    IR2_OPND temp_f = ra_alloc_ftemp();
    IR2_OPND temp_i = ra_alloc_itemp();
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();

    li_d(temp_i, 0x41E0000000000000); //0x41E0000000000000 2^31

    if (ir1_opnd_size(opnd1) == 128) {
        la_vftintrz_w_d(temp_f, src, src);
        la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
        la_vreplgr2vr_d(overflow, temp_i);
        la_vfcmp_cond_d(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
        la_vshuf4i_w(comp_mask, comp_mask, 0x88);
        la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
        la_xvpickve_d(dest, temp_f, 0);
    } else {
        IR2_OPND src_h128 = ra_alloc_ftemp();
        la_xvpermi_q(src_h128, src, XVPERMI_Q_4_0(1, 1));
        la_xvftintrz_w_d(temp_f, src_h128, src);
        la_xvldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
        la_xvreplgr2vr_d(overflow, temp_i);
        la_xvfcmp_cond_d(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
        la_xvshuf4i_w(comp_mask, comp_mask, 0x88);
        la_xvpermi_d(comp_mask, comp_mask, 0x88);
        la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
        la_vand_v(dest, temp_f, temp_f);
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vcvttpd2dq(IR1_INST * pir1) {
    if (option_cvt_opt) {
        return translate_vcvttpd2dq_opt(pir1);
    }

    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));

    if (ir1_opnd_size(ir1_get_opnd(pir1, 1)) == 256) {
        IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND src_h128 = ra_alloc_ftemp();
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp_fcsr = ra_alloc_itemp();
        IR2_OPND temp_int = ra_alloc_itemp();
        IR2_OPND label_over = ra_alloc_label();
        IR2_OPND label_second_operand = ra_alloc_label();
        IR2_OPND label_third_operand = ra_alloc_label();
        IR2_OPND label_fourth_operand = ra_alloc_label();

        la_xvpermi_q(src_h128, src, XVPERMI_Q_4_0(1, 1));
        la_vftintrz_w_d(temp, src_h128, src);

        /* check if INVALID bit in fcsr is set */
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);

        /* if no INVALID exception happend, convertion done */
        la_beqz(temp_fcsr, label_over);

        /* if INVALID exception did happen, check the four operands separately */
        li_wu(temp_int, 0x80000000);
        IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
        IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();

        /* check the first single operand */
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(0, 0, 0, 0));
        la_vftintrz_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_second_operand);
        la_vinsgr2vr_w(temp, temp_int, 0);

        /* check the second single operand */
        la_label(label_second_operand);
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(1, 1, 1, 1));
        la_vftintrz_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_third_operand);
        la_vinsgr2vr_w(temp, temp_int, 1);

        /* check the third single operand */
        la_label(label_third_operand);
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(2, 2, 2, 2));
        la_vftintrz_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_fourth_operand);
        la_vinsgr2vr_w(temp, temp_int, 2);


        /* check the fourth single operand */
        la_label(label_fourth_operand);
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(3, 3, 3, 3));
        la_vftintrz_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_over);
        la_vinsgr2vr_w(temp, temp_int, 3);

        la_label(label_over);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
        ra_free_temp(temp_fcsr);
        ra_free_temp(temp_int);
        ra_free_temp(ftemp_src_temp1);
        ra_free_temp(ftemp_src_temp2);
    } else if (ir1_opnd_size(ir1_get_opnd(pir1, 1)) == 128) {
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp_fcsr = ra_alloc_itemp();
        IR2_OPND temp_int = ra_alloc_itemp();
        IR2_OPND label_over = ra_alloc_label();
        IR2_OPND label_second_operand = ra_alloc_label();

        la_vftintrz_w_d(temp, src, src);

        /* check if INVALID bit in fcsr is set */
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);

        /* if no INVALID exception happend, convertion done */
        la_beqz(temp_fcsr, label_over);

        /* if INVALID exception did happen, check the four operands separately */
        li_wu(temp_int, 0x80000000);
        IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
        IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();

        /* check the first single operand */
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(0, 0, 0, 0));
        la_vftintrz_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_second_operand);
        la_vinsgr2vr_w(temp, temp_int, 0);

        /* check the second single operand */
        la_label(label_second_operand);
        la_xvpermi_d(ftemp_src_temp1, src, XVPERMI_D_2_2_2_2(1, 1, 1, 1));
        la_vftintrz_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_over);
        la_vinsgr2vr_w(temp, temp_int, 1);

        la_label(label_over);
        la_xvpickve_d(dest, temp, 0);
        ra_free_temp(temp_fcsr);
        ra_free_temp(temp_int);
        ra_free_temp(ftemp_src_temp1);
        ra_free_temp(ftemp_src_temp2);
    } else {
        lsassert(0);
    }

    return true;
}

static bool translate_vcvttps2dq_opt(IR1_INST * pir1) {
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    lsassert(ir1_opnd_is_xmm(opnd0) ||
        ir1_opnd_is_ymm(opnd0));

    IR2_OPND dest =  load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_OPND temp_f = ra_alloc_ftemp();
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm(opnd0)) {
        la_vftintrz_w_s(temp_f, src);
        la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
        la_vldi(overflow, (0b10011 << 8) | 0x4f); //0x134f 2^31
        la_vfcmp_cond_s(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
        la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
        la_vand_v(dest, temp_f, temp_f);
        set_high128_xreg_to_zero(dest);
    } else {
        la_xvftintrz_w_s(temp_f, src);
        la_xvldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
        la_xvldi(overflow, (0b10011 << 8) | 0x4f); //0x134f 2^31
        la_xvfcmp_cond_s(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
        la_xvbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
        la_xvand_v(dest, temp_f, temp_f);
    }
    return true;
}

bool translate_vcvttps2dq(IR1_INST * pir1) {
    if (option_cvt_opt) {
        return translate_vcvttps2dq_opt(pir1);
    }

    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    lsassert(ir1_opnd_is_xmm(opnd0) ||
        ir1_opnd_is_ymm(opnd0));

    bool is_same_reg = ir1_opnd_is_same_reg(opnd0, opnd1);

    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND temp_fcsr = ra_alloc_itemp();
        IR2_OPND temp_int = ra_alloc_itemp();
        IR2_OPND temp_operand_count = ra_alloc_itemp();
        IR2_OPND label_over = ra_alloc_label();
        IR2_OPND label_second_operand = ra_alloc_label();
        IR2_OPND label_third_operand = ra_alloc_label();
        IR2_OPND label_fourth_operand = ra_alloc_label();
        IR2_OPND label_fifth_operand = ra_alloc_label();
        IR2_OPND label_sixth_operand = ra_alloc_label();
        IR2_OPND label_seventh_operand = ra_alloc_label();
        IR2_OPND label_eighth_operand = ra_alloc_label();
        IR2_OPND dest = is_same_reg ? ra_alloc_ftemp() : load_freg256_from_ir1(opnd0);
        IR2_OPND src = load_freg256_from_ir1(opnd1);

        la_xvftintrz_w_s(dest, src);

        /* check if INVALID bit in fcsr is set */
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);

        /* if no INVALID exception happend, convertion done */
        la_beqz(temp_fcsr, label_over);

        /* if INVALID exception did happen, check the four operands separately */
        li_wu(temp_int, 0x80000000);
        IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
        IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();
        IR2_OPND src_h128 = ra_alloc_ftemp();
        la_xvpermi_q(src_h128, src, XVPERMI_Q_4_0(1, 1));

        /* check the first single operand */
        la_xvreplve_w(ftemp_src_temp1, src, zero_ir2_opnd);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_second_operand);
        la_xvinsgr2vr_w(dest, temp_int, 0);

        /* check the second single operand */
        la_label(label_second_operand);
        li_wu(temp_operand_count, 0x1);
        la_xvreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_third_operand);
        la_xvinsgr2vr_w(dest, temp_int, 1);

        /* check the third single operand */
        la_label(label_third_operand);
        li_wu(temp_operand_count, 0x2);
        la_xvreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_fourth_operand);
        la_xvinsgr2vr_w(dest, temp_int, 2);

        /* check the fourth single operand */
        la_label(label_fourth_operand);
        li_wu(temp_operand_count, 0x3);
        la_xvreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_fifth_operand);
        la_xvinsgr2vr_w(dest, temp_int, 3);

        /* check the fifth single operand */
        la_label(label_fifth_operand);
        la_xvreplve_w(ftemp_src_temp1, src_h128, zero_ir2_opnd);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_sixth_operand);
        la_xvinsgr2vr_w(dest, temp_int, 4);

        /* check the sixth single operand */
        la_label(label_sixth_operand);
        li_wu(temp_operand_count, 0x1);
        la_xvreplve_w(ftemp_src_temp1, src_h128, temp_operand_count);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_seventh_operand);
        la_xvinsgr2vr_w(dest, temp_int, 5);

        /* check the seventh single operand */
        la_label(label_seventh_operand);
        li_wu(temp_operand_count, 0x2);
        la_xvreplve_w(ftemp_src_temp1, src_h128, temp_operand_count);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_eighth_operand);
        la_xvinsgr2vr_w(dest, temp_int, 6);

        /* check the eighth single operand */
        la_label(label_eighth_operand);
        li_wu(temp_operand_count, 0x3);
        la_xvreplve_w(ftemp_src_temp1, src_h128, temp_operand_count);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_over);
        la_xvinsgr2vr_w(dest, temp_int, 7);

        la_label(label_over);
        if (is_same_reg) {
            la_xvori_b(load_freg256_from_ir1(opnd0), dest, 0);
        }
        ra_free_temp(temp_fcsr);
        ra_free_temp(temp_int);
        ra_free_temp(temp_operand_count);
        ra_free_temp(ftemp_src_temp1);
        ra_free_temp(ftemp_src_temp2);
    } else {
        IR2_OPND temp_fcsr = ra_alloc_itemp();
        IR2_OPND temp_int = ra_alloc_itemp();
        IR2_OPND temp_operand_count = ra_alloc_itemp();
        IR2_OPND label_over = ra_alloc_label();
        IR2_OPND label_second_operand = ra_alloc_label();
        IR2_OPND label_third_operand = ra_alloc_label();
        IR2_OPND label_fourth_operand = ra_alloc_label();
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vftintrz_w_s(temp, src);

        /* check if INVALID bit in fcsr is set */
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);

        /* if no INVALID exception happend, convertion done */
        la_beqz(temp_fcsr, label_over);

        /* if INVALID exception did happen, check the four operands separately */
        li_wu(temp_int, 0x80000000);
        IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
        IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();

        /* check the first single operand */
        la_vreplve_w(ftemp_src_temp1, src, zero_ir2_opnd);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_second_operand);
        la_vinsgr2vr_w(temp, temp_int, 0);

        /* check the second single operand */
        la_label(label_second_operand);
        li_wu(temp_operand_count, 0x1);
        la_vreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_third_operand);
        la_vinsgr2vr_w(temp, temp_int, 1);

        /* check the third single operand */
        la_label(label_third_operand);
        li_wu(temp_operand_count, 0x2);
        la_vreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_fourth_operand);
        la_vinsgr2vr_w(temp, temp_int, 2);


        /* check the fourth single operand */
        la_label(label_fourth_operand);
        li_wu(temp_operand_count, 0x3);
        la_vreplve_w(ftemp_src_temp1, src, temp_operand_count);
        la_vftintrz_w_s(ftemp_src_temp2, ftemp_src_temp1);
        la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
        la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_beqz(temp_fcsr, label_over);
        la_vinsgr2vr_w(temp, temp_int, 3);

        la_label(label_over);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
        ra_free_temp(temp_fcsr);
        ra_free_temp(temp_int);
        ra_free_temp(temp_operand_count);
        ra_free_temp(ftemp_src_temp1);
        ra_free_temp(ftemp_src_temp2);
    }
    return true;
}
#endif
