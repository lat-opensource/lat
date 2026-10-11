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
bool translate_vpslldq(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    lsassert(ir1_opnd_is_imm(opnd2));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);
    if (imm > 15) {
        la_xvandi_b(dest, src, 0);
        return true;
    }
    la_xvbsll_v(dest, src, imm);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpsllx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_INST * ( * rep_inst)(IR2_OPND, IR2_OPND);
    IR2_INST * ( * tr_inst_i)(IR2_OPND, IR2_OPND, int);
    IR2_INST * ( * tr_inst_r)(IR2_OPND, IR2_OPND, IR2_OPND);
    int max_count;
    switch (ir1_opcode(pir1)) {
        case dt_X86_INS_VPSLLW:
            rep_inst = la_xvreplve0_h;
            tr_inst_i = la_xvslli_h;
            tr_inst_r = la_xvsll_h;
            max_count = 15;
            break;
        case dt_X86_INS_VPSLLD:
            rep_inst = la_xvreplve0_w;
            tr_inst_i = la_xvslli_w;
            tr_inst_r = la_xvsll_w;
            max_count = 31;
            break;
        case dt_X86_INS_VPSLLQ:
            rep_inst = la_xvreplve0_d;
            tr_inst_i = la_xvslli_d;
            tr_inst_r = la_xvsll_d;
            max_count = 63;
            break;
        default:
            rep_inst = NULL;
            tr_inst_i = NULL;
            tr_inst_r = NULL;
            max_count = 0;
            lsassert(0);
            break;
    }
    if (ir1_opnd_is_imm(opnd2)) {
        uint8_t imm = ir1_opnd_uimm(opnd2);
        if (imm > max_count) {
            la_xvxor_v(dest, dest, dest);
        } else {
            tr_inst_i(dest, src, imm);
        }
    } else {
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND mask = ra_alloc_ftemp();
        IR2_OPND temp = ra_alloc_ftemp();
        if (max_count == 63) {
            la_xvreplve0_d(mask, src2);
            la_xvldi(temp, VLDI_IMM_TYPE0(3, 63));
            la_xvsle_du(mask, mask, temp);
        } else {
            la_xvreplve0_d(mask, src2);
            la_xvslei_du(mask, mask, max_count);
        }
        rep_inst(temp, src2);
        tr_inst_r(dest, src, temp);
        la_xvand_v(dest, dest, mask);
    }
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpsrldq(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    lsassert(ir1_opnd_is_imm(opnd2));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);
    if (imm > 15) {
        la_xvxor_v(dest, dest, dest);
        return true;
    }
    la_xvbsrl_v(dest, src, imm);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpsrlx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_INST * ( * rep_inst)(IR2_OPND, IR2_OPND);
    IR2_INST * ( * tr_inst_i)(IR2_OPND, IR2_OPND, int);
    IR2_INST * ( * tr_inst_r)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_shift = ra_alloc_label();
    int max_count;
    switch (ir1_opcode(pir1)) {
        case dt_X86_INS_VPSRLW:
            rep_inst = la_xvreplve0_h;
            tr_inst_i = la_xvsrli_h;
            tr_inst_r = la_xvsrl_h;
            max_count = 15;
            break;
        case dt_X86_INS_VPSRLD:
            rep_inst = la_xvreplve0_w;
            tr_inst_i = la_xvsrli_w;
            tr_inst_r = la_xvsrl_w;
            max_count = 31;
            break;
        case dt_X86_INS_VPSRLQ:
            rep_inst = la_xvreplve0_d;
            tr_inst_i = la_xvsrli_d;
            tr_inst_r = la_xvsrl_d;
            max_count = 63;
            break;
        default:
            rep_inst = NULL;
            tr_inst_i = NULL;
            tr_inst_r = NULL;
            max_count = 0;
            lsassert(0);
            break;
    }
    if (ir1_opnd_is_imm(opnd2)) {
        uint8_t imm = ir1_opnd_uimm(opnd2);
        if (imm > max_count) {
            la_xvxor_v(dest, dest, dest);
        } else {
            tr_inst_i(dest, src, imm);
        }
    } else {
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND mask = ra_alloc_ftemp();
        IR2_OPND temp = ra_alloc_ftemp();

        IR2_OPND count = ra_alloc_itemp();
        IR2_OPND max = ra_alloc_itemp();
        la_addi_d(max, zero_ir2_opnd, max_count);
        la_vpickve2gr_d(count, src2, 0);
        la_blt(count, max, label_shift);
        la_xvxor_v(dest, dest, dest);
        la_b(label_exit);

        la_label(label_shift);
        if (max_count == 63) {
            la_xvreplve0_d(mask, src2);
            la_xvldi(temp, VLDI_IMM_TYPE0(3, 63));
            la_xvsle_du(mask, mask, temp);
        } else {
            la_xvreplve0_d(mask, src2);
            la_xvslei_du(mask, mask, max_count);
        }
        rep_inst(temp, src2);
        tr_inst_r(dest, src, temp);
        la_xvand_v(dest, dest, mask);
    }
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    la_label(label_exit);
    return true;
}

bool translate_vpsrax(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_INST * ( * rep_inst)(IR2_OPND, IR2_OPND);
    IR2_INST * ( * tr_inst_i)(IR2_OPND, IR2_OPND, int);
    IR2_INST * ( * tr_inst_r)(IR2_OPND, IR2_OPND, IR2_OPND);
    int max_count;
    switch (ir1_opcode(pir1)) {
        case dt_X86_INS_VPSRAW:
            rep_inst = la_xvreplve0_h;
            tr_inst_i = la_xvsrai_h;
            tr_inst_r = la_xvsra_h;
            max_count = 15;
            break;
        case dt_X86_INS_VPSRAD:
            rep_inst = la_xvreplve0_w;
            tr_inst_i = la_xvsrai_w;
            tr_inst_r = la_xvsra_w;
            max_count = 31;
            break;
        default:
            rep_inst = NULL;
            tr_inst_i = NULL;
            tr_inst_r = NULL;
            max_count = 0;
            lsassert(0);
            break;
    }
    if (ir1_opnd_is_imm(opnd2)) {
        uint8_t imm = ir1_opnd_uimm(opnd2);
        if (imm > max_count)
            imm = max_count;
        tr_inst_i(dest, src, imm);
    } else {
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND mask = ra_alloc_ftemp();
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp_sign = ra_alloc_ftemp();
        if (max_count == 63) {
            la_xvreplve0_d(mask, src2);
            la_vldi(temp, VLDI_IMM_TYPE0(3, 63));
            la_xvsle_du(mask, mask, temp);
        } else {
            la_xvreplve0_d(mask, src2);
            la_xvslei_du(mask, mask, max_count);
        }
        tr_inst_i(temp_sign, src, max_count);
        rep_inst(temp, src2);
        tr_inst_r(temp, src, temp);
        la_xvbitsel_v(dest, temp_sign, temp, mask);
    }
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}
#endif
