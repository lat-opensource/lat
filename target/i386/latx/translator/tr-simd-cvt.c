#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "translate.h"

bool translate_cvtdq2pd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND fcsr_opnd = set_fpu_fcsr_rounding_field_by_x86();
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vffintl_d_w(dest, src);
    set_fpu_rounding_mode(fcsr_opnd);
    return true;
}

bool translate_cvtdq2ps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vffint_s_w(dest, src);
    return true;
}

static void tr_x87_to_mmx(void)
{
    //reset top
    lsenv->tr_data->curr_top = 0;
    la_st_w(zero_ir2_opnd, env_ir2_opnd,
                      lsenv_offset_of_top(lsenv));

    tr_gen_top_mode_init();
}

/**
 * @brief translate inst cvttpd2dq and cvttps2dq
 * -# cvtpd2dq
 * Convert Packed Double-Precision Floating-Point
 * to Packed Doubleword Integer, Truncated
 * -# cvtps2dq
 * Convert Packed Single-Precision Floating-Point
 * to Packed Doubleword Integers, Truncated
 *
 * @see translate_cvtpx2dq
 *
 * @param pir1
 * @return true
 * @return false
 */
static bool translate_cvttpx2dq_opt(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src = load_freg128_from_ir1(opnd2);
    IR2_OPND temp_f = ra_alloc_ftemp();
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();

    if (ir1_opcode(pir1) == dt_X86_INS_CVTTPS2DQ) {
        la_vftintrz_w_s(temp_f, src);
        la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
        la_vldi(overflow, (0b10011 << 8) | 0x4f); //0x134f 2^31
        la_vfcmp_cond_s(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
        la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
        la_vand_v(dest, temp_f, temp_f);
    } else if (ir1_opcode(pir1) == dt_X86_INS_CVTTPD2DQ) {
        IR2_OPND temp_i = ra_alloc_itemp();
        la_vftintrz_w_d(temp_f, src, src);
        la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
        li_d(temp_i, 0x41E0000000000000); //0x41E0000000000000 2^31
        la_vreplgr2vr_d(overflow, temp_i);
        la_vfcmp_cond_d(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
        la_vshuf4i_w(comp_mask, comp_mask, 0x88);
        la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
        la_vand_v(dest, temp_f, temp_f);
        la_vinsgr2vr_d(dest, zero_ir2_opnd, 1);
    } else {
        lsassert(0);
    }
    return true;
}

bool translate_cvttpx2dq(IR1_INST *pir1)
{
    if (option_cvt_opt) {
        return translate_cvttpx2dq_opt(pir1);
    }

    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);
    IR1_OPCODE op = ir1_opcode(pir1);
    IR2_INST *(*split_inst)(IR2_OPND, IR2_OPND, int) = la_vextrins_w;
    IR2_INST *(*cvt_inst)(IR2_OPND, IR2_OPND) = la_ftintrz_w_s;

    switch (op) {
    case dt_X86_INS_CVTTPS2DQ:
        break;
    case dt_X86_INS_CVTTPD2DQ:
        split_inst = la_vextrins_d;
        cvt_inst = la_ftintrz_w_d;
        break;
    default:
        lsassert(0);
        break;
    }

    IR2_OPND src;
    IR2_OPND src_lo;
    IR2_OPND src_hi;
    src = load_freg128_from_ir1(opnd2);
    src_hi = ra_alloc_ftemp();
    split_inst(src_hi, src, VEXTRINS_IMM_4_0(0, 1));

    /* low 32/64 bit */
    IR2_OPND ftemp_src_lo = ra_alloc_ftemp();
    cvt_inst(ftemp_src_lo, src);
    /* load fcsr */
    IR2_OPND fcsr_lo = ra_alloc_itemp();
    IR2_OPND vtemps = ra_alloc_ftemp();
    la_movfcsr2gr(fcsr_lo, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_lo, fcsr_lo, FCSR_OFF_CAUSE_V,
                          FCSR_OFF_CAUSE_V);
    la_vinsgr2vr_w(vtemps, fcsr_lo, 0);

    /* high 32/64 bit */
    IR2_OPND ftemp_src_hi = ra_alloc_ftemp();
    cvt_inst(ftemp_src_hi, src_hi);
    ra_free_temp(src_hi);
    IR2_OPND fcsr_hi = ra_alloc_itemp();
    la_movfcsr2gr(fcsr_hi, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_hi, fcsr_hi, FCSR_OFF_CAUSE_V,
                          FCSR_OFF_CAUSE_V);
    la_vinsgr2vr_w(vtemps, fcsr_hi, 1);

    /* shuf data */
    IR2_OPND vtemp1 = ra_alloc_ftemp();
    la_vilvl_w(vtemp1, ftemp_src_hi, ftemp_src_lo);

    if (op == dt_X86_INS_CVTTPS2DQ) {
        /* CVTPS2DQ will use 4 part */
        src_lo = ra_alloc_ftemp();
        split_inst(src_lo, src, VEXTRINS_IMM_4_0(0, 2));
        cvt_inst(ftemp_src_lo, src_lo);
        ra_free_temp(src_lo);

        src_hi = ra_alloc_ftemp();
        split_inst(src_hi, src, VEXTRINS_IMM_4_0(0, 3));

        /* load fcsr */
        la_movfcsr2gr(fcsr_lo, fcsr_ir2_opnd);
        la_bstrpick_w(fcsr_lo, fcsr_lo,
                              FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_vinsgr2vr_w(vtemps, fcsr_lo, 2);

        /* high 32/64 bit */
        cvt_inst(ftemp_src_hi, src_hi);
        ra_free_temp(src_hi);
        la_movfcsr2gr(fcsr_hi, fcsr_ir2_opnd);
        la_bstrpick_w(fcsr_hi, fcsr_hi,
                              FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
        la_vinsgr2vr_w(vtemps, fcsr_hi, 3);

        /* shuf data */
        la_vextrins_w(vtemp1, ftemp_src_lo,
                             VEXTRINS_IMM_4_0(2, 0));
        la_vextrins_w(vtemp1, ftemp_src_hi,
                             VEXTRINS_IMM_4_0(3, 0));
    }
    ra_free_temp(ftemp_src_hi);
    ra_free_temp(ftemp_src_lo);
    ra_free_temp(fcsr_lo);
    ra_free_temp(fcsr_hi);

    /* set vtemps */
    la_vseqi_w(vtemps, vtemps, 1);

    /* add 0x80..0 */
    IR2_OPND vtemp2 = ra_alloc_ftemp();
    /* ui13: 13'b1 0011 [1000 0000] */
    la_vldi(vtemp2, 0x1380);

    /* calc result */
    IR2_OPND dest = load_freg128_from_ir1(opnd1);
    la_vbitsel_v(dest, vtemp1, vtemp2, vtemps);
    if (op == dt_X86_INS_CVTTPD2DQ) {
        if (option_enable_lasx) {
            la_xvpickve_d(dest, dest, 0);
        } else {
            la_vinsgr2vr_d(dest, zero_ir2_opnd, 1);
        }
    }

    ra_free_temp(vtemp1);
    ra_free_temp(vtemp2);
    ra_free_temp(vtemps);

    return true;
}

/**
 * Refer to cvtps2pi
 */
static bool translate_cvtpd2dq_opt(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src = load_freg128_from_ir1(opnd2);
    IR2_OPND temp_f = ra_alloc_ftemp();
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();
    IR2_OPND temp_i = ra_alloc_itemp();
    int offset = lsenv_offset_of_mxcsr(lsenv);
    IR2_OPND mxcsr = ra_alloc_itemp();
    IR2_OPND label_rd_rz = ra_alloc_label();
    IR2_OPND label_ru = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    la_vftint_w_d(temp_f, src, src);

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

    la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
    la_vreplgr2vr_d(overflow, temp_i);
    la_vfcmp_cond_d(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
    la_vshuf4i_w(comp_mask, comp_mask, 0x88);
    la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);

    la_vand_v(dest, temp_f, temp_f);
    la_vinsgr2vr_d(dest, zero_ir2_opnd, 1);

    // IR2_OPND cun_mask = ra_alloc_ftemp();
    // IR2_OPND overflow_mask = ra_alloc_ftemp();
    // la_vftint_w_d(temp_f, src, src);
    // la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x138
    // la_vfcmp_cond_d(cun_mask, src, src, 0x8); // get Nan mark 0x8=cUN
    // la_vshuf4i_w(cun_mask, cun_mask, 0x88);
    // la_vxor_v(overflow_mask, overflow_mask, overflow_mask);
    // la_vsubi_wu(overflow_mask, overflow_mask, 1);
    // la_vsrli_w(overflow_mask, overflow_mask, 1);//0x7fffffffffffffff, 0x7fffffffffffffff
    // la_vseq_w(overflow_mask, temp_f, overflow_mask);
    // la_vor_v(cun_mask, cun_mask, overflow_mask);
    // la_vbitsel_v(temp_f, temp_f, sse_invalid, cun_mask);
    // la_vand_v(dest, temp_f, temp_f);
    // la_vinsgr2vr_d(dest, zero_ir2_opnd, 1);

    return true;
}

bool translate_cvtpd2dq(IR1_INST *pir1)
{
    if (option_cvt_opt) {
        return translate_cvtpd2dq_opt(pir1);
    }
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));

    IR2_OPND src_lo;
    IR2_OPND src_hi;
    src_lo = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    src_hi = ra_alloc_ftemp();
    la_vextrins_d(src_hi, src_lo, VEXTRINS_IMM_4_0(0, 1));

    IR2_OPND temp_int = ra_alloc_itemp();
    IR2_OPND temp_fcsr = ra_alloc_itemp();
    IR2_OPND ftemp_src1 = ra_alloc_ftemp();
    IR2_OPND ftemp_src2 = ra_alloc_ftemp();
    IR2_OPND label_for_flow1 = ra_alloc_label();
    IR2_OPND label_for_flow2 = ra_alloc_label();
    IR2_OPND label_over = ra_alloc_label();
    IR2_OPND label_high = ra_alloc_label();

    /**
     * verify the first double scalar operand is unorder or overflow
     * or under flow?
     */
    IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
    IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();

    /* convertion has to be done first, or INVALID exception may be missed */
    la_vreplve_d(ftemp_src_temp1, src_lo, zero_ir2_opnd);
    la_vftint_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);

    /* check if INVALID bit in fcsr is set */
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_bnez(temp_fcsr, label_for_flow1);

    /* ##if## no INVALID exception happend, save the lower bits result */
    la_fmov_d(ftemp_src1, ftemp_src_temp2);
    la_b(label_high);

    /* ##else## INVALID exception did happen, load 0x80000000 manually*/
    la_label(label_for_flow1);
    li_wu(temp_int, 0x80000000);
    la_movgr2fr_d(ftemp_src1, temp_int);
    la_label(label_high);

    /**
     * verify the second double scalar operand is unorder or overflow
     * or under flow?
     */
    la_vreplve_d(ftemp_src_temp1, src_hi, zero_ir2_opnd);
    la_vftint_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);
    ra_free_temp(ftemp_src_temp1);

    /* check if INVALID bit in fcsr is set */
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_bnez(temp_fcsr, label_for_flow2);

    /* ##if## no INVALID exception happend, save the higher bits result */
    la_fmov_d(ftemp_src2, ftemp_src_temp2);
    la_b(label_over);

    /* ##else## INVALID exception did happen, load 0x80000000 manually*/
    la_label(label_for_flow2);
    li_wu(temp_int, 0x80000000);
    la_movgr2fr_d(ftemp_src2, temp_int);
    la_label(label_over);

    /**
     * merge, use ftemp_final as transfer register to prevent damaging the
     * 64 bits of the dest.
     */
    IR2_OPND ftemp_final = ra_alloc_ftemp();
    la_vilvl_w(ftemp_final, ftemp_src2, ftemp_src1);
    if (option_enable_lasx) {
        la_xvpickve_d(dest, ftemp_final, 0);
    } else {
        la_vandi_b(dest, dest, 0);
        la_vextrins_d(dest, ftemp_final, 0);
    }

    ra_free_temp(ftemp_src_temp2);
    ra_free_temp(ftemp_final);
    ra_free_temp(ftemp_src1);
    ra_free_temp(ftemp_src2);
    ra_free_temp(src_hi);
    ra_free_temp(temp_int);
    return true;

}

/**
 * @brief translate inst cvtps2dq
 * -# cvtps2dq
 * Convert Packed Single-Precision Floating-Point
 * to Packed Doubleword Integers
 *
 * 1. do the convertion with la_ftint_w_s
 * 2. if no INVALID exception happend, convertion done
 * 3. otherwise, check the four operands separately
 *
 * @param pir1
 * @return true
 * @return false
 */

static bool translate_cvtps2dq_opt(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src = load_freg128_from_ir1(opnd2);

    IR2_OPND temp_f = ra_alloc_ftemp();
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();
    la_vftint_w_s(temp_f, src);
    la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
    la_vldi(overflow, (0b10011 << 8) | 0x4f); //0x134f
    la_vfcmp_cond_s(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
    la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
    la_vand_v(dest, temp_f, temp_f);
    return true;
}

bool translate_cvtps2dq(IR1_INST *pir1)
{
    if (option_cvt_opt) {
        return translate_cvtps2dq_opt(pir1);
    }
    IR2_OPND temp_fcsr = ra_alloc_itemp();
    IR2_OPND temp_int = ra_alloc_itemp();
    IR2_OPND temp_operand_count = ra_alloc_itemp();
    IR2_OPND label_over = ra_alloc_label();
    IR2_OPND label_second_operand = ra_alloc_label();
    IR2_OPND label_third_operand = ra_alloc_label();
    IR2_OPND label_fourth_operand = ra_alloc_label();
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));

    la_vftint_w_s(dest, src);

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
    la_vinsgr2vr_w(dest, temp_int, 0);

    /* check the second single operand */
    la_label(label_second_operand);
    li_wu(temp_operand_count, 0x1);
    la_vreplve_w(ftemp_src_temp1, src, temp_operand_count);
    la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_beqz(temp_fcsr, label_third_operand);
    la_vinsgr2vr_w(dest, temp_int, 1);


    /* check the third single operand */
    la_label(label_third_operand);
    li_wu(temp_operand_count, 0x2);
    la_vreplve_w(ftemp_src_temp1, src, temp_operand_count);
    la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_beqz(temp_fcsr, label_fourth_operand);
    la_vinsgr2vr_w(dest, temp_int, 2);


    /* check the fourth single operand */
    la_label(label_fourth_operand);
    li_wu(temp_operand_count, 0x3);
    la_vreplve_w(ftemp_src_temp1, src, temp_operand_count);
    la_vftint_w_s(ftemp_src_temp2, ftemp_src_temp1);
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_beqz(temp_fcsr, label_over);
    la_vinsgr2vr_w(dest, temp_int, 3);

    la_label(label_over);
    ra_free_temp(temp_fcsr);
    ra_free_temp(temp_int);
    ra_free_temp(temp_operand_count);
    ra_free_temp(ftemp_src_temp1);
    ra_free_temp(ftemp_src_temp2);
    return true;
}

static bool translate_cvtpd2pi_opt(IR1_INST *pir1)
{
    tr_x87_to_mmx();

    IR2_OPND dest = ra_alloc_mmx(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp_i = ra_alloc_itemp();
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();
    IR2_OPND temp_f = ra_alloc_ftemp();
    int offset = lsenv_offset_of_mxcsr(lsenv);
    IR2_OPND mxcsr = ra_alloc_itemp();
    IR2_OPND label_rd_rz = ra_alloc_label();
    IR2_OPND label_ru = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    la_vftint_w_d(temp_f, src, src);

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

    la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all
    la_vreplgr2vr_d(overflow, temp_i);
    la_vfcmp_cond_d(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
    la_vshuf4i_w(comp_mask, comp_mask, 0x88);
    la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);

    la_vextrins_d(dest, temp_f, VEXTRINS_IMM_4_0(0, 0));
    return true;
}

/* refer to cvtps2pi */
bool translate_cvtpd2pi(IR1_INST *pir1)
{
    if (option_cvt_opt) {
        return translate_cvtpd2pi_opt(pir1);
    }
    tr_x87_to_mmx();

    IR2_OPND dest = ra_alloc_mmx(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));

    IR2_OPND src_lo;
    IR2_OPND src_hi;
    src_lo = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    src_hi = ra_alloc_ftemp();
    la_vextrins_d(src_hi, src_lo, VEXTRINS_IMM_4_0(0, 1));

    IR2_OPND temp_int = ra_alloc_itemp();
    IR2_OPND temp_fcsr = ra_alloc_itemp();
    IR2_OPND ftemp_src1 = ra_alloc_ftemp();
    IR2_OPND ftemp_src2 = ra_alloc_ftemp();
    IR2_OPND ftemp_final = ra_alloc_ftemp();
    IR2_OPND label_for_flow1 = ra_alloc_label();
    IR2_OPND label_for_flow2 = ra_alloc_label();
    IR2_OPND label_over = ra_alloc_label();
    IR2_OPND label_high = ra_alloc_label();

    /**
     * verify the first double scalar operand is unorder or overflow
     * or under flow?
     */
    IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
    IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();

    /* convertion has to be done first, or INVALID exception may be missed */
    la_vreplve_d(ftemp_src_temp1, src_lo, zero_ir2_opnd);
    la_vftint_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);

    /* check if INVALID bit in fcsr is set */
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_bnez(temp_fcsr, label_for_flow1);

    /* ##if## no INVALID exception happend, save the lower bits result */
    la_fmov_d(ftemp_src1, ftemp_src_temp2);
    la_b(label_high);

    /* ##else## INVALID exception did happen, load 0x80000000 manually*/
    la_label(label_for_flow1);
    li_wu(temp_int, 0x80000000);
    la_movgr2fr_d(ftemp_src1, temp_int);
    la_label(label_high);

    /**
     * verify the second double scalar operand is unorder or overflow
     * or under flow?
     */
    la_vreplve_d(ftemp_src_temp1, src_hi, zero_ir2_opnd);
    la_vftint_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);

    /* check if INVALID bit in fcsr is set */
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_bnez(temp_fcsr, label_for_flow2);

    /* ##if## no INVALID exception happend, save the higher bits result */
    la_fmov_d(ftemp_src2, ftemp_src_temp2);
    la_b(label_over);

    /* ##else## INVALID exception did happen, load 0x80000000 manually*/
    la_label(label_for_flow2);
    li_wu(temp_int, 0x80000000);
    la_movgr2fr_d(ftemp_src2, temp_int);
    la_label(label_over);

    /**
     * merge, use ftemp_final as transfer register to prevent damaging the
     * 64 bits of the dest.
     */
    la_vilvl_w(ftemp_final, ftemp_src2, ftemp_src1);
    la_vextrins_d(dest, ftemp_final, VEXTRINS_IMM_4_0(0, 0));

    ra_free_temp(ftemp_src_temp1);
    ra_free_temp(ftemp_src_temp2);
    ra_free_temp(ftemp_final);
    ra_free_temp(ftemp_src1);
    ra_free_temp(ftemp_src2);
    ra_free_temp(src_hi);
    ra_free_temp(temp_int);
    return true;
}

static bool translate_cvttpd2pi_opt(IR1_INST *pir1)
{
    tr_x87_to_mmx();
    IR2_OPND dest = ra_alloc_mmx(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));

    IR2_OPND temp_f = ra_alloc_ftemp();
    IR2_OPND temp_i = ra_alloc_itemp();
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();

    la_vftintrz_w_d(temp_f, src, src);
    la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
    li_d(temp_i, 0x41E0000000000000); //0x41E0000000000000
    la_vreplgr2vr_d(overflow, temp_i);
    la_vfcmp_cond_d(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
    la_vshuf4i_w(comp_mask, comp_mask, 0x88);
    la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
    la_vextrins_d(dest, temp_f, VEXTRINS_IMM_4_0(0, 0));
    return true;
}

bool translate_cvttpd2pi(IR1_INST *pir1)
{
    if (option_cvt_opt) {
        return translate_cvttpd2pi_opt(pir1);
    }
    tr_x87_to_mmx();

    IR2_OPND dest = ra_alloc_mmx(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));

    IR2_OPND src_lo;
    IR2_OPND src_hi;
    src_lo = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    src_hi = ra_alloc_ftemp();
    la_vextrins_d(src_hi, src_lo, VEXTRINS_IMM_4_0(0, 1));

    IR2_OPND temp_int = ra_alloc_itemp();
    IR2_OPND temp_fcsr = ra_alloc_itemp();
    IR2_OPND ftemp_src1 = ra_alloc_ftemp();
    IR2_OPND ftemp_src2 = ra_alloc_ftemp();
    IR2_OPND ftemp_final = ra_alloc_ftemp();
    IR2_OPND label_for_flow1 = ra_alloc_label();
    IR2_OPND label_for_flow2 = ra_alloc_label();
    IR2_OPND label_over = ra_alloc_label();
    IR2_OPND label_high = ra_alloc_label();

    /**
     * verify the first double scalar operand is unorder or overflow
     * or under flow?
     */
    IR2_OPND ftemp_src_temp1 = ra_alloc_ftemp();
    IR2_OPND ftemp_src_temp2 = ra_alloc_ftemp();

    /* convertion has to be done first, or INVALID exception may be missed */
    la_vreplve_d(ftemp_src_temp1, src_lo, zero_ir2_opnd);
    la_vftintrz_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);

    /* check if INVALID bit in fcsr is set */
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_bnez(temp_fcsr, label_for_flow1);

    /* ##if## no INVALID exception happend, save the lower bits result */
    la_fmov_d(ftemp_src1, ftemp_src_temp2);
    la_b(label_high);

    /* ##else## INVALID exception did happen, load 0x80000000 manually*/
    la_label(label_for_flow1);
    li_wu(temp_int, 0x80000000);
    la_movgr2fr_d(ftemp_src1, temp_int);
    la_label(label_high);

    /**
     * verify the second double scalar operand is unorder or overflow
     * or under flow?
     */
    la_vreplve_d(ftemp_src_temp1, src_hi, zero_ir2_opnd);
    la_vftintrz_w_d(ftemp_src_temp2, ftemp_src_temp1, ftemp_src_temp1);

    /* check if INVALID bit in fcsr is set */
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_bnez(temp_fcsr, label_for_flow2);

    /* ##if## no INVALID exception happend, save the higher bits result */
    la_fmov_d(ftemp_src2, ftemp_src_temp2);
    la_b(label_over);

    /* ##else## INVALID exception did happen, load 0x80000000 manually*/
    la_label(label_for_flow2);
    li_wu(temp_int, 0x80000000);
    la_movgr2fr_d(ftemp_src2, temp_int);
    la_label(label_over);

    /**
     * merge, use ftemp_final as transfer register to prevent damaging the
     * 64 bits of the dest.
     */
    la_vilvl_w(ftemp_final, ftemp_src2, ftemp_src1);
    la_vextrins_d(dest, ftemp_final, VEXTRINS_IMM_4_0(0, 0));

    ra_free_temp(ftemp_src_temp1);
    ra_free_temp(ftemp_src_temp2);
    ra_free_temp(ftemp_final);
    ra_free_temp(ftemp_src1);
    ra_free_temp(ftemp_src2);
    ra_free_temp(src_hi);
    ra_free_temp(temp_int);
    return true;
}

bool translate_cvtpd2ps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfcvt_s_d(dest, src, src);
    if (option_enable_lasx) {
        la_xvpickve_d(dest, dest, 0);
    } else {
        la_vinsgr2vr_d(dest, zero_ir2_opnd, 1);
    }
    return true;
}

bool translate_cvtpi2ps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    tr_x87_to_mmx();
    IR2_OPND fcsr_opnd = set_fpu_fcsr_rounding_field_by_x86();
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    IR2_OPND temp = ra_alloc_ftemp();

    la_vreplve_d(temp, src, zero_ir2_opnd);
    la_vffint_s_w(temp, temp);
    la_vextrins_d(dest, temp, VEXTRINS_IMM_4_0(0, 0));
    set_fpu_rounding_mode(fcsr_opnd);
    return true;
}

bool translate_cvtpi2pd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    tr_x87_to_mmx();
    IR2_OPND fcsr_opnd = set_fpu_fcsr_rounding_field_by_x86();
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp0 = ra_alloc_ftemp();
    la_ffint_d_w(temp0, src);
    la_vshuf4i_w(temp, src, 0x55);
    la_ffint_d_w(temp, temp);
    if (option_enable_lasx) {
        la_xvinsve0_d(dest, temp, 1);
        la_xvinsve0_d(dest, temp0, 0);
    } else {
        la_vextrins_d(dest, temp, 1 << 4);
        la_vextrins_d(dest, temp0, 0);
    }
    set_fpu_rounding_mode(fcsr_opnd);
    return true;
}

/**
 * Convert single-precision floating-point values to signed int integers,
 * when the invalid exception is masked, the approaches taken by the LA
 * and X86 are different if a converted result is larger than the maximum
 * signed int integer:
 *
 * X86: 80000000H is returned
 * LA : 7FFFFFFFH is returned
 *
 * So we can not use 'la_vftint_w_s' directly due to the above difference.
 */
static bool translate_cvtps2pi_opt(IR1_INST *pir1)
{
    tr_x87_to_mmx();
    IR2_OPND dest = ra_alloc_mmx(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
    IR2_OPND src_lo;                                         
    src_lo = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND temp = ra_alloc_ftemp();

    la_vreplve_d(comp_mask, src_lo, zero_ir2_opnd);
    la_vftint_w_s(temp, comp_mask);
    la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
    la_vldi(overflow, (0b10011 << 8) | 0x4f); //0x134f
    la_vfcmp_cond_s(comp_mask, overflow, comp_mask, 0xE); // get Nan mark 0xE=cULE
    la_vbitsel_v(temp, temp, sse_invalid, comp_mask);
    la_vextrins_d(dest, temp, VEXTRINS_IMM_4_0(0, 0));

    return true;
}

bool translate_cvtps2pi(IR1_INST *pir1)
{
    if (option_cvt_opt) {
        return translate_cvtps2pi_opt(pir1);
    }
    tr_x87_to_mmx();

    IR2_OPND dest = ra_alloc_mmx(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
    IR2_OPND src_lo;
    src_lo = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp_int = ra_alloc_itemp();
    IR2_OPND temp_fcsr = ra_alloc_itemp();
    IR2_OPND ftemp_src_temp = ra_alloc_ftemp();
    IR2_OPND ftemp_final = ra_alloc_ftemp();
    IR2_OPND label_for_flow1 = ra_alloc_label();
    IR2_OPND label_for_flow2 = ra_alloc_label();
    IR2_OPND label_over = ra_alloc_label();
    IR2_OPND label_high = ra_alloc_label();

    /**
     * verify the first single scalar operand is unorder or overflow
     * or under flow?
     */
    IR2_OPND ftemp_src1 = ra_alloc_ftemp();

    /* convertion has to be done first, or INVALID exception may be missed */
    la_vreplve_w(ftemp_src_temp, src_lo, zero_ir2_opnd);
    la_vftint_w_s(ftemp_src_temp, ftemp_src_temp);

    /* check if INVALID bit in fcsr is set */
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_bnez(temp_fcsr, label_for_flow1);

    /* ##if## no INVALID exception happend, save the lower bits result */
    la_fmov_s(ftemp_src1, ftemp_src_temp);
    la_b(label_high);

    /* ##else## INVALID exception did happen, load 0x80000000 manually*/
    la_label(label_for_flow1);
    li_wu(temp_int, 0x80000000);
    la_movgr2fr_d(ftemp_src1, temp_int);
    la_label(label_high);

    /**
     * verify the second single scalar operand is unorder or overflow
     * or under flow?
     */
    IR2_OPND ftemp_src2 = ra_alloc_ftemp();
    la_vextrins_w(ftemp_src2, src_lo, VEXTRINS_IMM_4_0(0, 1));

    /* convertion has to be done first, or INVALID exception may be missed */
    la_vreplve_w(ftemp_src_temp, ftemp_src2, zero_ir2_opnd);
    la_vftint_w_s(ftemp_src_temp, ftemp_src_temp);

    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_bnez(temp_fcsr, label_for_flow2);

    /* ##if## no INVALID exception happend, save the higher bits result */
    la_fmov_s(ftemp_src2, ftemp_src_temp);
    la_b(label_over);

    /* ##else## INVALID exception did happen, load 0x80000000 manually*/
    la_label(label_for_flow2);
    li_wu(temp_int, 0x80000000);
    la_movgr2fr_d(ftemp_src2, temp_int);
    la_label(label_over);

    /**
     * merge, use ftemp_final as transfer register to prevent damaging the
     * 64 bits of the dest.
     */
    la_vilvl_w(ftemp_final, ftemp_src2, ftemp_src1);
    la_vextrins_d(dest, ftemp_final, VEXTRINS_IMM_4_0(0, 0));

    ra_free_temp(ftemp_src1);
    ra_free_temp(ftemp_src2);
    ra_free_temp(ftemp_final);
    ra_free_temp(ftemp_src_temp);
    ra_free_temp(temp_int);
    ra_free_temp(temp_fcsr);
    return true;
}

static bool translate_cvttps2pi_opt(IR1_INST *pir1)
{
    tr_x87_to_mmx();
    IR2_OPND dest = ra_alloc_mmx(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
    IR2_OPND src_lo;                                         
    src_lo = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND sse_invalid = ra_alloc_ftemp();
    IR2_OPND comp_mask = ra_alloc_ftemp();
    IR2_OPND overflow = ra_alloc_ftemp();
    IR2_OPND temp = ra_alloc_ftemp();

    la_vreplve_d(comp_mask, src_lo, zero_ir2_opnd);
    la_vftintrz_w_s(temp, comp_mask);
    la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
    la_vldi(overflow, (0b10011 << 8) | 0x4f); //0x134f
    la_vfcmp_cond_s(comp_mask, overflow, comp_mask, 0xE); // get Nan mark 0xE=cULE
    la_vbitsel_v(temp, temp, sse_invalid, comp_mask);
    la_vextrins_d(dest, temp, VEXTRINS_IMM_4_0(0, 0));

    return true;

}

bool translate_cvttps2pi(IR1_INST *pir1)
{
    if (option_cvt_opt) {
        return translate_cvttps2pi_opt(pir1);
    }
    tr_x87_to_mmx();

    IR2_OPND dest = ra_alloc_mmx(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
    IR2_OPND src_lo;
    src_lo = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp_int = ra_alloc_itemp();
    IR2_OPND temp_fcsr = ra_alloc_itemp();
    IR2_OPND ftemp_src_temp = ra_alloc_ftemp();
    IR2_OPND ftemp_final = ra_alloc_ftemp();
    IR2_OPND label_for_flow1 = ra_alloc_label();
    IR2_OPND label_for_flow2 = ra_alloc_label();
    IR2_OPND label_over = ra_alloc_label();
    IR2_OPND label_high = ra_alloc_label();

    /**
     * verify the first single scalar operand is unorder or overflow
     * or under flow?
     */
    IR2_OPND ftemp_src1 = ra_alloc_ftemp();

    /* convertion has to be done first, or INVALID exception may be missed */
    la_vreplve_w(ftemp_src_temp, src_lo, zero_ir2_opnd);
    la_vftintrz_w_s(ftemp_src_temp, ftemp_src_temp);

    /* check if INVALID bit in fcsr is set */
    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_bnez(temp_fcsr, label_for_flow1);

    /* ##if## no INVALID exception happend, save the lower bits result */
    la_fmov_s(ftemp_src1, ftemp_src_temp);
    la_b(label_high);

    /* ##else## INVALID exception did happen, load 0x80000000 manually*/
    la_label(label_for_flow1);
    li_wu(temp_int, 0x80000000);
    la_movgr2fr_d(ftemp_src1, temp_int);
    la_label(label_high);

    /**
     * verify the second single scalar operand is unorder or overflow
     * or under flow?
     */
    IR2_OPND ftemp_src2 = ra_alloc_ftemp();
    la_vextrins_w(ftemp_src2, src_lo, VEXTRINS_IMM_4_0(0, 1));

    /* convertion has to be done first, or INVALID exception may be missed */
    la_vreplve_w(ftemp_src_temp, ftemp_src2, zero_ir2_opnd);
    la_vftintrz_w_s(ftemp_src_temp, ftemp_src_temp);

    la_movfcsr2gr(temp_fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(temp_fcsr, temp_fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_bnez(temp_fcsr, label_for_flow2);

    /* ##if## no INVALID exception happend, save the higher bits result */
    la_fmov_s(ftemp_src2, ftemp_src_temp);
    la_b(label_over);

    /* ##else## INVALID exception did happen, load 0x80000000 manually*/
    la_label(label_for_flow2);
    li_wu(temp_int, 0x80000000);
    la_movgr2fr_d(ftemp_src2, temp_int);
    la_label(label_over);

    /**
     * merge, use ftemp_final as transfer register to prevent damaging the
     * 64 bits of the dest.
     */
    la_vilvl_w(ftemp_final, ftemp_src2, ftemp_src1);
    la_vextrins_d(dest, ftemp_final, VEXTRINS_IMM_4_0(0, 0));

    ra_free_temp(ftemp_src1);
    ra_free_temp(ftemp_src2);
    ra_free_temp(ftemp_final);
    ra_free_temp(ftemp_src_temp);
    ra_free_temp(temp_int);
    ra_free_temp(temp_fcsr);
    return true;
}

/**
 * @brief translate cvtsd2ss
 * cvtsd2ss
 * Convert Scalar Double-Precision Floating-Point
 * to Scalar Single-Precision Floating-Point
 *
 * @note We ignore the RM because Glibc will set RM both at fcsr and mxcsr
 *
 * @param pir1
 * @return true
 * @return false
 */
bool translate_cvtsd2ss(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND fcsr_opnd = set_fpu_fcsr_rounding_field_by_x86();
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp = ra_alloc_ftemp();
    la_fcvt_s_d(temp, src);
    if (option_enable_lasx) {
        la_xvinsve0_w(dest, temp, 0);
    } else {
        la_vextrins_w(dest, temp, 0);
    }
    set_fpu_rounding_mode(fcsr_opnd);
    return true;
}

bool translate_cvtsi2sd(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);
    /* For si2sd, 32-bit int can convert to FP64 without Round */
    lsassert(ir1_opnd_is_xmm(opnd1));
    IR2_OPND dest = load_freg128_from_ir1(opnd1);
    IR2_OPND src = load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);
    IR2_OPND temp_src = ra_alloc_ftemp();
    la_movgr2fr_d(temp_src, src);
    if (ir1_opnd_size(opnd2) == 64) {
        la_ffint_d_l(temp_src, temp_src);
    } else {
        la_ffint_d_w(temp_src, temp_src);
    }
    la_vextrins_d(dest, temp_src, VEXTRINS_IMM_4_0(0, 0));
    return true;
}

/**
 * @brief translate cvtsi2ss
 * cvtsi2ss
 * Convert Signed Doubleword Integer
 * to Scalar Single-Precision Floating-Point
 *
 * @note We ignore the RM because Glibc will set RM both at fcsr and mxcsr
 *
 * @param pir1
 * @return true
 * @return false
 */
bool translate_cvtsi2ss(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd1));
    IR2_OPND fcsr_opnd = set_fpu_fcsr_rounding_field_by_x86();
    IR2_OPND dest = load_freg128_from_ir1(opnd1);
    IR2_OPND src = load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);
    IR2_OPND temp_src = ra_alloc_ftemp();
    la_movgr2fr_d(temp_src, src);
    if (ir1_opnd_size(opnd2) == 64) {
        la_ffint_s_l(temp_src, temp_src);
    } else {
        la_ffint_s_w(temp_src, temp_src);
    }
    la_vextrins_w(dest, temp_src, VEXTRINS_IMM_4_0(0, 0));
    set_fpu_rounding_mode(fcsr_opnd);
    return true;
}

bool translate_cvtss2sd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp = ra_alloc_ftemp();
    la_fcvt_d_s(temp, src);
    if (option_enable_lasx) {
        la_xvinsve0_d(dest, temp, 0);
    } else {
        la_vextrins_d(dest, temp, 0);
    }
    return true;
}

bool translate_cvtps2pd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    //TODO:simply
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp0 = ra_alloc_ftemp();
    la_fcvt_d_s(temp0, src);
    la_vshuf4i_w(temp, src, 0x55);
    la_fcvt_d_s(temp, temp);
    if (option_enable_lasx) {
        la_xvinsve0_d(dest, temp, 1);
        la_xvinsve0_d(dest, temp0, 0);
    } else {
        la_vextrins_d(dest, temp, 1 << 4);
        la_vextrins_d(dest, temp0, 0);
    }
    return true;
}

/**
 * @brief translate inst cvtss2si and cvtsd2si
 * -# cvtsd2si
 *    Convert Scalar Double-Precision Floating-Point
 *    to Signed Double-Integer
 * -# cvtss2si
 *    Convert Scalar Single-Precision Floating-Point
 *    to Signed Double-Integer
 *
 * @note We ignore the RM because Glibc will set RM both at fcsr and mxcsr
 * @see translate_cvttsx2si
 *
 * @param pir1
 * @return true
 * @return false
 */
static bool translate_cvtsx2si_opt(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);

    IR2_OPND src = load_freg128_from_ir1(opnd2);
    IR2_OPND dest = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
    int opnd0_size = ir1_opnd_size(opnd1);

    IR2_OPND temp_f = ra_alloc_ftemp();
    IR2_OPND temp_i = ra_alloc_itemp();
    IR2_OPND overflow = ra_alloc_ftemp();

    if (ir1_opcode(pir1) == dt_X86_INS_CVTSD2SI) {
        if (opnd0_size == 32) {
            // la_ftint_w_d(temp_f, src);
            // la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
            // li_d(temp_i, 0x41E0000000000000); //0x41E0000000000000
            // la_vreplgr2vr_d(overflow, temp_i);
            // la_vfcmp_cond_d(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
            // la_vshuf4i_w(comp_mask, comp_mask, 0x88);
            // la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
            // la_movfr2gr_s(temp_i, temp_f);
            // la_bstrpick_d(dest, temp_i, 31, 0);
            IR2_OPND temp_dest = ra_alloc_itemp();
            int offset = lsenv_offset_of_mxcsr(lsenv);
            IR2_OPND mxcsr = ra_alloc_itemp();
            IR2_OPND label_rd_rz = ra_alloc_label();
            IR2_OPND label_ru = ra_alloc_label();
            IR2_OPND label_exit = ra_alloc_label();

            la_ftint_w_d(temp_f, src);

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

            la_movgr2fr_d(overflow, temp_i);
            la_fcmp_cond_d(fcc0_ir2_opnd, overflow, src, 0xE); // get Nan mark 0xE=cULE
            la_movcf2gr(temp_i, fcc0_ir2_opnd);
            la_slli_d(temp_i, temp_i, opnd0_size - 1);
            la_movfr2gr_d(temp_dest, temp_f);
            la_masknez(temp_dest, temp_dest, temp_i);
            la_or(temp_dest, temp_dest, temp_i);
            la_bstrpick_d(dest, temp_dest, 31, 0);
        } else if (opnd0_size == 64) {
            la_ftint_l_d(temp_f, src);
            li_d(temp_i, 0x43E0000000000000);
            la_movgr2fr_d(overflow, temp_i);
            la_fcmp_cond_d(fcc0_ir2_opnd, overflow, src, 0xE); // get Nan mark 0xE=cULE
            la_movcf2gr(temp_i, fcc0_ir2_opnd);
            la_slli_d(temp_i, temp_i, opnd0_size - 1);
            la_movfr2gr_d(dest, temp_f);
            la_masknez(dest, dest, temp_i);
            la_or(dest, dest, temp_i);
        } else {
            lsassert(0);
        }
    } else if (ir1_opcode(pir1) == dt_X86_INS_CVTSS2SI){
        IR2_OPND overflow = ra_alloc_ftemp();

        if (opnd0_size == 32) {
            IR2_OPND temp_dest = ra_alloc_itemp();
            la_ftint_w_s(temp_f, src);
            li_w(temp_i, 0x4f000000);
            la_movgr2fr_w(overflow, temp_i);
            la_fcmp_cond_s(fcc0_ir2_opnd, overflow, src, 0xE); // get Nan mark 0xE=cULE
            la_movcf2gr(temp_i, fcc0_ir2_opnd);
            la_slli_d(temp_i, temp_i, opnd0_size - 1);
            la_movfr2gr_s(temp_dest, temp_f);
            la_masknez(temp_dest, temp_dest, temp_i);
            la_or(temp_dest, temp_dest, temp_i);
            la_bstrpick_d(dest, temp_dest, 31, 0);
        } else if (opnd0_size == 64) {
            la_ftint_l_s(temp_f, src);
            li_d(temp_i, 0x5F000000);
            la_movgr2fr_w(overflow, temp_i);
            la_fcmp_cond_s(fcc0_ir2_opnd, overflow, src, 0xE); // get Nan mark 0xE=cULE
            la_movcf2gr(temp_i, fcc0_ir2_opnd);
            la_slli_d(temp_i, temp_i, opnd0_size - 1);
            la_movfr2gr_d(dest, temp_f);
            la_masknez(dest, dest, temp_i);
            la_or(dest, dest, temp_i);
        } else {
            lsassert(0);
        }
    } else {
        return false;
    }
    return true;
}

bool translate_cvtsx2si(IR1_INST *pir1)
{
    if (option_cvt_opt) {
        return translate_cvtsx2si_opt(pir1);
    }

    IR2_OPND fcsr_opnd = set_fpu_fcsr_rounding_field_by_x86();
    IR2_OPND src_lo = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp1 = ra_alloc_itemp();
    int opnd0_size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    IR1_OPCODE op = ir1_opcode(pir1);
    IR2_INST *(*tr_inst)(IR2_OPND, IR2_OPND)
                = (opnd0_size == 64) ? la_ftint_l_d : la_ftint_w_d;
    switch (op) {
    case dt_X86_INS_CVTSD2SI:
#ifdef CONFIG_LATX_AVX_OPT
    case dt_X86_INS_VCVTSD2SI:
#endif
        break;
    case dt_X86_INS_CVTSS2SI:
#ifdef CONFIG_LATX_AVX_OPT
    case dt_X86_INS_VCVTSS2SI:
#endif
        tr_inst = (opnd0_size == 64) ? la_ftint_l_s : la_ftint_w_s;
        break;
    default:
        lsassert(0);
        break;
    }

    IR2_OPND dest = ra_alloc_ftemp();
    tr_inst(dest, src_lo);

    /* check fcsr to get overflow etc. */
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND overflow = ra_alloc_itemp();
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr, fcsr, FCSR_OFF_CAUSE_V, FCSR_OFF_CAUSE_V);
    la_slli_d(overflow, fcsr, opnd0_size - 1);

    /* mov the result */
    la_movfr2gr_d(temp1, dest);

    la_masknez(fcsr, temp1, fcsr);
    la_or(temp1, overflow, fcsr);

    set_fpu_rounding_mode(fcsr_opnd);
    store_ireg_to_ir1(temp1, ir1_get_opnd(pir1, 0), false);

    ra_free_temp(temp1);
    ra_free_temp(fcsr);
    ra_free_temp(overflow);

    ra_free_temp(dest);

    return true;
}

/**
 * @brief translate inst cvttsd2si and cvttss2si
 * -# cvttsd2si
 *    Convert Scalar Double-Precision Floating-Point
 *    to Signed Double-Integer, Truncated
 * -# cvttss2si
 *    Convert Scalar Single-Precision Floating-Point
 *    to Signed Double-Integer, Truncated
 *
 * For x86, if value is
 * - a NaN,
 * - infinity,
 * - result of the conversion is larger than the `dest ability`
 * it will set the value 0x8000...0
 *
 * For LA, if value is
 * - a NaN,             0
 * - +oo, larger than   0x7fff...f
 * - -oo, lower than    0x8000...0
 *
 * So, we need deal Nan, +oo, larger than.
 *
 * - Nan:               // 0x0
 *  1. fcc <- cmp               // normal: 0,   Nan:    1
 *  2. x <- fcc << (size - 1)   // normal: 0,   Nan:    0x8000...0
 *  3. res <- x or res          // normal: res, Nan:    0x8000...0
 * - +oo, larger than:  // 0x7fff...f
 *  1. fcc <- cmp               // normal: 0,   +oo..:  1
 *  2. res <- res + fcc         // normal: res, +oo..:  0x8000...0
 *
 * Also, if current operation cause Nan or etc. , it will set Cause.V
 * 1. tmp  <- fcsr
 * 2. tmp  <- tmp[Cause.V]
 * 3. tmp' <- tmp << (size - 1) // normal: 0,   others: 0x8000...0
 * 4. tmp  <- masknez tmp, res  // normal: res, others: 0
 * 5. res  <- tmp' or tmp       // normal: res, others: 0x8000...0
 *
 * @note
 * For FP64
 * over_flow:
 * if 32 bits, check data is 0x41e0_0000_0000_0000
 * if 64 bits, check data is 0x43e0_0000_0000_0000
 *
 * under_flow:
 * if 32 bits, check data is 0xc1e0_0000_0000_0000
 * if 64 bits, check data is 0xc3e0_0000_0000_0000
 *
 * For FP32
 * over_flow:
 * if 32 bits, check data is 0x4f00_0000
 * if 64 bits, check data is 0x5f00_0000
 *
 * under_flow:
 * if 32 bits, check data is 0xcf00_0000
 * if 64 bits, check data is 0xdf00_0000
 *
 * @param pir1
 * @return true
 * @return false
 */
static bool translate_cvttsx2si_opt(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);

    IR2_OPND src = load_freg128_from_ir1(opnd2);
    IR2_OPND dest = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
    int opnd0_size = ir1_opnd_size(opnd1);

    IR2_OPND temp_f = ra_alloc_ftemp();
    IR2_OPND temp_i = ra_alloc_itemp();
    IR2_OPND overflow = ra_alloc_ftemp();

    if (ir1_opcode(pir1) == dt_X86_INS_CVTTSD2SI) {
        if (opnd0_size == 32) {
            
            // la_ftintrz_w_d(temp_f, src);
            // la_vldi(sse_invalid, 0b1001110000000); // broadcast 0x80000000 to all 0x1380
            // li_d(temp_i, 0x41E0000000000000); //0x41E0000000000000
            // la_vreplgr2vr_d(overflow, temp_i);
            // la_vfcmp_cond_d(comp_mask, overflow, src, 0xE); // get Nan mark 0xE=cULE
            // la_vshuf4i_w(comp_mask, comp_mask, 0x88);
            // la_vbitsel_v(temp_f, temp_f, sse_invalid, comp_mask);
            // la_movfr2gr_s(temp_i, temp_f);
            // la_bstrpick_d(dest, temp_i, 31, 0);

            IR2_OPND temp_dest = ra_alloc_itemp();
            la_ftintrz_w_d(temp_f, src);
            li_d(temp_i, 0x41E0000000000000); //0x41E0000000000000
            la_movgr2fr_d(overflow, temp_i);
            la_fcmp_cond_d(fcc0_ir2_opnd, overflow, src, 0xE); // get Nan mark 0xE=cULE
            la_movcf2gr(temp_i, fcc0_ir2_opnd);
            la_slli_d(temp_i, temp_i, opnd0_size - 1);
            la_movfr2gr_d(temp_dest, temp_f);
            la_masknez(temp_dest, temp_dest, temp_i);
            la_or(temp_dest, temp_dest, temp_i);
            la_bstrpick_d(dest, temp_dest, 31, 0);
        } else if (opnd0_size == 64) {
            la_ftintrz_l_d(temp_f, src);
            li_d(temp_i, 0x43E0000000000000);
            la_movgr2fr_d(overflow, temp_i);
            la_fcmp_cond_d(fcc0_ir2_opnd, overflow, src, 0xE); // get Nan mark 0xE=cULE
            la_movcf2gr(temp_i, fcc0_ir2_opnd);
            la_slli_d(temp_i, temp_i, opnd0_size - 1);
            la_movfr2gr_d(dest, temp_f);
            la_masknez(dest, dest, temp_i);
            la_or(dest, dest, temp_i);
        } else {
            lsassert(0);
        }
    } else if (ir1_opcode(pir1) == dt_X86_INS_CVTTSS2SI){
        if (opnd0_size == 32) {
            IR2_OPND temp_dest = ra_alloc_itemp();
            la_ftintrz_w_s(temp_f, src);
            li_w(temp_i, 0x4f000000);
            la_movgr2fr_w(overflow, temp_i);
            la_fcmp_cond_s(fcc0_ir2_opnd, overflow, src, 0xE); // get Nan mark 0xE=cULE
            la_movcf2gr(temp_i, fcc0_ir2_opnd);
            la_slli_d(temp_i, temp_i, opnd0_size - 1);
            la_movfr2gr_s(temp_dest, temp_f);
            la_masknez(temp_dest, temp_dest, temp_i);
            la_or(temp_dest, temp_dest, temp_i);
            la_bstrpick_d(dest, temp_dest, 31, 0);
        } else if (opnd0_size == 64) {
            la_ftintrz_l_s(temp_f, src);
            li_d(temp_i, 0x5F000000);
            la_movgr2fr_w(overflow, temp_i);
            la_fcmp_cond_s(fcc0_ir2_opnd, overflow, src, 0xE); // get Nan mark 0xE=cULE
            la_movcf2gr(temp_i, fcc0_ir2_opnd);
            la_slli_d(temp_i, temp_i, opnd0_size - 1);
            la_movfr2gr_d(dest, temp_f);
            la_masknez(dest, dest, temp_i);
            la_or(dest, dest, temp_i);
        } else {
            lsassert(0);
        }
    } else {
        return false;
    }
    return true;
}

bool translate_cvttsx2si(IR1_INST *pir1)
{
    if (option_cvt_opt) {
        return translate_cvttsx2si_opt(pir1);
    }
    IR2_OPND src_lo = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp1 = ra_alloc_itemp();
    int opnd0_size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    IR1_OPCODE op = ir1_opcode(pir1);
    IR2_INST *(*tr_inst)(IR2_OPND, IR2_OPND)
                = (opnd0_size == 64) ? la_ftintrz_l_d : la_ftintrz_w_d;
    switch (op) {
    case dt_X86_INS_CVTTSD2SI:
#ifdef CONFIG_LATX_AVX_OPT
    case dt_X86_INS_VCVTTSD2SI:
#endif
        break;
    case dt_X86_INS_CVTTSS2SI:
#ifdef CONFIG_LATX_AVX_OPT
    case dt_X86_INS_VCVTTSS2SI:
#endif
        tr_inst = (opnd0_size == 64) ? la_ftintrz_l_s : la_ftintrz_w_s;
        break;
    default:
        lsassert(0);
        break;
    }

    IR2_OPND dest = ra_alloc_ftemp();
    tr_inst(dest, src_lo);

    /* check fcsr to get overflow etc. */
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND overflow = ra_alloc_itemp();
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr, fcsr, FCSR_OFF_CAUSE_V,
                          FCSR_OFF_CAUSE_V);
    la_slli_d(overflow, fcsr, opnd0_size - 1);

    /* mov the result */
    la_movfr2gr_d(temp1, dest);

    la_masknez(fcsr, temp1, fcsr);
    la_or(temp1, overflow, fcsr);

    store_ireg_to_ir1(temp1, ir1_get_opnd(pir1, 0), false);

    ra_free_temp(temp1);
    ra_free_temp(fcsr);
    ra_free_temp(overflow);

    ra_free_temp(dest);

    return true;
}
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

bool translate_vcvtps2dq(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
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
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));

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

bool translate_vcvtpd2dq(IR1_INST * pir1) {
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

bool translate_vcvttpd2dq(IR1_INST * pir1) {
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

bool translate_vcvttps2dq(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
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
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));

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
