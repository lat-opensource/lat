/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"

#ifdef CONFIG_LATX_AVX_OPT
bool translate_vfmaddsubxxxps(IR1_INST * pir1) {
    if (!option_enable_lasx) {
        return translate_vfmaddsubxxxps_lsx(pir1);
    }

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND original_dest = ra_alloc_ftemp();
    IR2_INST * ( * tr_inst_fmadd)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_INST * ( * tr_inst_fmsub)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp_add = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);

    la_xvori_b(original_dest, dest, 0);
    if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
        src1 = original_dest;
    }
    if ((ir1_opnd_is_xmm(opnd2) || ir1_opnd_is_ymm(opnd2)) &&
        ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd2)) {
        src2 = original_dest;
    }
    switch (op) {
        case dt_X86_INS_VFMADDSUB132PS:
            temp1 = original_dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMADDSUB231PS:
            temp1 = src1, temp2 = src2, temp3 = original_dest;
            break;
        case dt_X86_INS_VFMADDSUB213PS:
            temp1 = src1, temp2 = original_dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst_fmadd = ir1_opnd_is_xmm(opnd0) ? la_vfmadd_s : la_xvfmadd_s;
    tr_inst_fmsub = ir1_opnd_is_xmm(opnd0) ? la_vfmsub_s : la_xvfmsub_s;
    tr_inst_fmadd(temp_add, temp1, temp2, temp3);
    tr_inst_fmsub(dest, temp1, temp2, temp3);

    /* add[7] add[6] add[5] add[4] add[3] add[2] add[1] add[0] */
	/*  10     11     00     01    */
    /* add[6] add[7] add[4] add[5] add[2] add[3] add[0] add[1] */
    la_xvshuf4i_w(temp_add, temp_add, 0xb1);

    la_xvpackev_w(dest, temp_add, dest);
    lasx_fp_fix_fma_nan(dest, temp1, temp2, temp3, false,
                        ir1_opnd_is_xmm(opnd0) ? 4 : 8);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    ra_free_temp(original_dest);
    return true;
}

bool translate_vfmaddsubxxxpd(IR1_INST * pir1) {
    if (!option_enable_lasx) {
        return translate_vfmaddsubxxxpd_lsx(pir1);
    }

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND original_dest = ra_alloc_ftemp();
    IR2_INST * ( * tr_inst_fmadd)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_INST * ( * tr_inst_fmsub)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp_add = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);

    la_xvori_b(original_dest, dest, 0);
    if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
        src1 = original_dest;
    }
    if ((ir1_opnd_is_xmm(opnd2) || ir1_opnd_is_ymm(opnd2)) &&
        ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd2)) {
        src2 = original_dest;
    }
    switch (op) {
        case dt_X86_INS_VFMADDSUB132PD:
            temp1 = original_dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMADDSUB231PD:
            temp1 = src1, temp2 = src2, temp3 = original_dest;
            break;
        case dt_X86_INS_VFMADDSUB213PD:
            temp1 = src1, temp2 = original_dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst_fmadd = ir1_opnd_is_xmm(opnd0) ? la_vfmadd_d : la_xvfmadd_d;
    tr_inst_fmsub = ir1_opnd_is_xmm(opnd0) ? la_vfmsub_d : la_xvfmsub_d;
    tr_inst_fmadd(temp_add, temp1, temp2, temp3);
    tr_inst_fmsub(dest, temp1, temp2, temp3);

    /* add[3] add[2] sub[3] sub[2] add[1] add[0] sub[1] sub[0] */
	/*  11     00     11     00    */
    /* add[3] sub[2] add[1] sub[0] */
    la_xvshuf4i_d(dest, temp_add, 0xc);
    lasx_fp_fix_fma_nan(dest, temp1, temp2, temp3, true,
                        ir1_opnd_is_xmm(opnd0) ? 2 : 4);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    ra_free_temp(original_dest);
    return true;
}

bool translate_vfmsubaddxxxps(IR1_INST * pir1) {
    if (!option_enable_lasx) {
        return translate_vfmsubaddxxxps_lsx(pir1);
    }

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND original_dest = ra_alloc_ftemp();
    IR2_INST * ( * tr_inst_fmadd)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_INST * ( * tr_inst_fmsub)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp_add = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);

    la_xvori_b(original_dest, dest, 0);
    if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
        src1 = original_dest;
    }
    if ((ir1_opnd_is_xmm(opnd2) || ir1_opnd_is_ymm(opnd2)) &&
        ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd2)) {
        src2 = original_dest;
    }
    switch (op) {
        case dt_X86_INS_VFMSUBADD132PS:
            temp1 = original_dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMSUBADD231PS:
            temp1 = src1, temp2 = src2, temp3 = original_dest;
            break;
        case dt_X86_INS_VFMSUBADD213PS:
            temp1 = src1, temp2 = original_dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst_fmadd = ir1_opnd_is_xmm(opnd0) ? la_vfmadd_s : la_xvfmadd_s;
    tr_inst_fmsub = ir1_opnd_is_xmm(opnd0) ? la_vfmsub_s : la_xvfmsub_s;
    tr_inst_fmadd(temp_add, temp1, temp2, temp3);
    tr_inst_fmsub(dest, temp1, temp2, temp3);

    /* add[7] add[6] add[5] add[4] add[3] add[2] add[1] add[0] */
	/*  10     11     00     01    */
    /* add[6] add[7] add[4] add[5] add[2] add[3] add[0] add[1] */
    la_xvshuf4i_w(temp_add, temp_add, 0xb1);

    la_xvpackod_w(dest, dest, temp_add);
    lasx_fp_fix_fma_nan(dest, temp1, temp2, temp3, false,
                        ir1_opnd_is_xmm(opnd0) ? 4 : 8);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    ra_free_temp(original_dest);
    return true;
}

bool translate_vfmsubaddxxxpd(IR1_INST * pir1) {
    if (!option_enable_lasx) {
        return translate_vfmsubaddxxxpd_lsx(pir1);
    }

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND original_dest = ra_alloc_ftemp();
    IR2_INST * ( * tr_inst_fmadd)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_INST * ( * tr_inst_fmsub)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp_add = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);

    la_xvori_b(original_dest, dest, 0);
    if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
        src1 = original_dest;
    }
    if ((ir1_opnd_is_xmm(opnd2) || ir1_opnd_is_ymm(opnd2)) &&
        ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd2)) {
        src2 = original_dest;
    }
    switch (op) {
        case dt_X86_INS_VFMSUBADD132PD:
            temp1 = original_dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMSUBADD231PD:
            temp1 = src1, temp2 = src2, temp3 = original_dest;
            break;
        case dt_X86_INS_VFMSUBADD213PD:
            temp1 = src1, temp2 = original_dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst_fmadd = ir1_opnd_is_xmm(opnd0) ? la_vfmadd_d : la_xvfmadd_d;
    tr_inst_fmsub = ir1_opnd_is_xmm(opnd0) ? la_vfmsub_d : la_xvfmsub_d;
    tr_inst_fmadd(temp_add, temp1, temp2, temp3);
    tr_inst_fmsub(dest, temp1, temp2, temp3);

    /* add[3] add[2] sub[3] sub[2] add[1] add[0] sub[1] sub[0] */
	/*  01     10     01     10    */
    /* sub[3] add[2] sub[1] add[0] */
    la_xvshuf4i_d(dest, temp_add, 0x6);
    lasx_fp_fix_fma_nan(dest, temp1, temp2, temp3, true,
                        ir1_opnd_is_xmm(opnd0) ? 2 : 4);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    ra_free_temp(original_dest);
    return true;
}
#endif
