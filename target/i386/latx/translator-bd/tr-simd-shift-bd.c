#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate-bd.h"

bool translate_psllw_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
            ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1))) {
            IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vreplvei_d(temp1, src, 0);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vslei_du(temp2, temp1, 15);

            la_vreplvei_h(temp1, src, 0);
            la_vsll_h(dest, dest, temp1);
            la_vand_v(dest, dest, temp2);
        } else if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 1))) {
            uint8_t imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 1));
            if (imm > 15) {
                la_vxor_v(dest, dest, dest);
            } else {
                la_vslli_h(dest, dest, imm);
            }
        } else {
            lsassert(0);
        }
    } else { //mmx
        /* transfer to mmx mode */
        transfer_to_mmx_mode_bd();

        if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 0) + 1)) {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            uint8 imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 0) + 1);
            if (imm > 15)
                la_vxor_v(dest, dest, dest);
            else {
                la_vslli_h(dest, dest, imm);
            }
        } else {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            IR2_OPND src = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0) + 1, false, IS_INTEGER);
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vslei_du(temp1, src, 15);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vreplvei_h(temp2, src, 0);
            la_vsll_h(dest, dest, temp2);
            la_vand_v(dest, dest, temp1);
        }
    }
    return true;
}

bool translate_pslld_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
            ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1))) {
            IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vreplvei_d(temp1, src, 0);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vslei_du(temp2, temp1, 31);
            la_vreplvei_w(temp1, src, 0);
            la_vsll_w(dest, dest, temp1);
            la_vand_v(dest, dest, temp2);
        } else if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 1))) {
            uint8_t imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 1));
            if (imm > 31) {
                la_vxor_v(dest, dest, dest);
            } else {
                la_vslli_w(dest, dest, imm);
            }
        } else {
            lsassert(0);
        }
    } else { //mmx
        /* transfer to mmx mode */
        transfer_to_mmx_mode_bd();

        if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 0) + 1)) {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            uint8 imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 0) + 1);
            if (imm > 31)
                la_vxor_v(dest, dest, dest);
            else {
                la_vslli_w(dest, dest, imm);
            }
        } else {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            IR2_OPND src = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0) + 1, false, IS_INTEGER);
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vslei_du(temp1, src, 31);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vreplvei_w(temp2, src, 0);
            la_vsll_w(dest, dest, temp2);
            la_vand_v(dest, dest, temp1);
        }
    }
    return true;
}

bool translate_psllq_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
            ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1))) {
            IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vreplvei_d(temp1, src, 0);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vldi(temp2, VLDI_IMM_TYPE0(3, 63));
            IR2_OPND temp3 = ra_alloc_ftemp();
            la_vsle_du(temp3, temp1, temp2);
            la_vreplvei_d(temp1, src, 0);
            la_vsll_d(dest, dest, temp1);
            la_vand_v(dest, dest, temp3);
        } else if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 1))) {
            uint8_t imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 1));
            if (imm > 63) {
                la_vxor_v(dest, dest, dest);
            } else {
                la_vslli_d(dest, dest, imm);
            }
        } else {
            lsassert(0);
        }
    } else { //mmx
        /* transfer to mmx mode */
        transfer_to_mmx_mode_bd();

        if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 0) + 1)) {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            uint8 imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 0) + 1);
            if (imm > 63)
                la_vxor_v(dest, dest, dest);
            else {
                la_vslli_d(dest, dest, imm);
            }
        } else {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            IR2_OPND src = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0) + 1, false, IS_INTEGER);
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vldi(temp1, VLDI_IMM_TYPE0(3, 63));
            la_vsle_du(temp1, src, temp1);
            la_vsll_d(dest, dest, src);
            la_vand_v(dest, dest, temp1);
        }
    }
    return true;
}

bool translate_psrlw_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
            ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1))) {
            IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vreplvei_d(temp1, src, 0);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vslei_du(temp2, temp1, 15);
            la_vreplvei_h(temp1, src, 0);
            la_vsrl_h(dest, dest, temp1);
            la_vand_v(dest, dest, temp2);
        } else if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 1))) {
            uint8_t imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 1));
            if (imm > 15) {
                la_vxor_v(dest, dest, dest);
            } else {
                la_vsrli_h(dest, dest, imm);
            }
        } else {
            lsassert(0);
        }
    } else { //mmx
        /* transfer to mmx mode */
        transfer_to_mmx_mode_bd();

        if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 0) + 1)) {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            uint8 imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 0) + 1);
            if (imm > 15)
                la_vxor_v(dest, dest, dest);
            else {
                la_vsrli_h(dest, dest, imm);
            }
        } else {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            IR2_OPND src = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0) + 1, false, IS_INTEGER);
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vslei_du(temp1, src, 15);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vreplvei_h(temp2, src, 0);
            la_vsrl_h(dest, dest, temp2);
            la_vand_v(dest, dest, temp1);
        }
    }
    return true;
}

bool translate_psrld_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
            ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1))) {
            IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vreplvei_d(temp1, src, 0);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vslei_du(temp2, temp1, 31);
            la_vreplvei_w(temp1, src, 0);
            la_vsrl_w(dest, dest, temp1);
            la_vand_v(dest, dest, temp2);
        } else if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 1))) {
            uint8_t imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 1));
            if (imm > 31) {
                la_vxor_v(dest, dest, dest);
            } else {
                la_vsrli_w(dest, dest, imm);
            }
        } else {
            lsassert(0);
        }
    } else { //mmx
        /* transfer to mmx mode */
        transfer_to_mmx_mode_bd();

        if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 0) + 1)) {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            uint8 imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 0) + 1);
            if (imm > 31)
                la_vxor_v(dest, dest, dest);
            else {
                la_vsrli_w(dest, dest, imm);
            }
        } else {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            IR2_OPND src = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0) + 1, false, IS_INTEGER);
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vslei_du(temp1, src, 31);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vreplvei_w(temp2, src, 0);
            la_vsrl_w(dest, dest, temp2);
            la_vand_v(dest, dest, temp1);
        }
    }
    return true;
}

bool translate_psrlq_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
            ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1))) {
            IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vreplvei_d(temp1, src, 0);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vldi(temp2, VLDI_IMM_TYPE0(3, 63));
            IR2_OPND temp3 = ra_alloc_ftemp();
            la_vsle_du(temp3, temp1, temp2);
            la_vsrl_d(dest, dest, temp1);
            la_vand_v(dest, dest, temp3);
        } else if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 1))) {
            uint8_t imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 1));
            if (imm > 63) {
                la_vxor_v(dest, dest, dest);
            } else {
                la_vsrli_d(dest, dest, imm);
            }
        } else {
            lsassert(0);
        }
    } else { //mmx
        /* transfer to mmx mode */
        transfer_to_mmx_mode_bd();

        if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 0) + 1)) {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            uint8 imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 0) + 1);
            if (imm > 63)
                la_movgr2fr_d(dest, zero_ir2_opnd);
            else {
                IR2_OPND itemp = ra_alloc_itemp();
                li_wu(itemp, imm);
                la_movfr2gr_d(itemp, dest);
                la_srli_d(itemp, itemp, imm);
                la_movgr2fr_d(dest, itemp);
                ra_free_temp(itemp);
            }
        } else {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            IR2_OPND src = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0) + 1, false, IS_INTEGER);
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vldi(temp1, VLDI_IMM_TYPE0(3, 63));
            la_vsle_du(temp1, src, temp1);
            la_vsrl_d(dest, dest, src);
            la_vand_v(dest, dest, temp1);
        }
    }
    return true;
}

bool translate_psraw_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
            ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1))) {
            IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vreplvei_d(temp1, src, 0);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vslei_du(temp2, temp1, 15);
            IR2_OPND temp3 = ra_alloc_ftemp();
            la_vsrai_h(temp3, dest, 15);
            la_vreplvei_h(temp1, src, 0);
            la_vsra_h(dest, dest, temp1);
            la_vbitsel_v(dest, temp3, dest, temp2);
        } else if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 1))) {
            uint8_t imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 1));
            if (imm > 15)
                imm = 15;
            la_vsrai_h(dest, dest, imm);
        } else {
            lsassert(0);
        }
    } else { //mmx
        /* transfer to mmx mode */
        transfer_to_mmx_mode_bd();

        if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 0) + 1)) {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            uint8 imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 0) + 1);
            if (imm > 15)
                imm = 15;
            la_vsrai_h(dest, dest, imm);
        } else {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            IR2_OPND src = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0) + 1, false, IS_INTEGER);

            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vreplvei_d(temp1, src, 0);

            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vslei_du(temp2, temp1, 15);
            la_vreplvei_h(temp1, src, 0);

            IR2_OPND temp3 = ra_alloc_ftemp();
            la_vsrai_h(temp3, dest, 15);
            la_vsra_h(dest, dest, temp1);
            la_vbitsel_v(dest, temp3, dest, temp2);
        }
    }
    return true;
}

bool translate_psrad_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
            ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1))) {
            IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vreplvei_d(temp1, src, 0);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vslei_du(temp2, temp1, 31);
            la_vreplvei_w(temp1, src, 0);
            IR2_OPND temp3 = ra_alloc_ftemp();
            la_vsrai_w(temp3, dest, 31);
            la_vsra_w(dest, dest, temp1);
            la_vbitsel_v(dest, temp3, dest, temp2);
        } else if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 1))) {
            uint8_t imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 1));
            if (imm > 31)
                imm = 31;
            la_vsrai_w(dest, dest, imm);
        } else {
            lsassert(0);
        }
    } else { //mmx
        /* transfer to mmx mode */
        transfer_to_mmx_mode_bd();

        if (ir1_opnd_is_imm_bd(ir1_get_opnd_bd(pir1, 0) + 1)) {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            uint8 imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 0) + 1);
            if (imm > 31)
                imm = 31;
            la_vsrai_w(dest, dest, imm);
        } else {
            IR2_OPND dest = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
            IR2_OPND src = load_freg_from_ir1_1_bd(
                ir1_get_opnd_bd(pir1, 0) + 1, false, IS_INTEGER);
            IR2_OPND temp1 = ra_alloc_ftemp();
            la_vreplvei_d(temp1, src, 0);
            IR2_OPND temp2 = ra_alloc_ftemp();
            la_vslei_du(temp2, temp1, 31);
            la_vreplvei_w(temp1, src, 0);
            IR2_OPND temp3 = ra_alloc_ftemp();
            la_vsrai_w(temp3, dest, 31);
            la_vsra_w(dest, dest, temp1);
            la_vbitsel_v(dest, temp3, dest, temp2);
        }
    }
    return true;
}

bool translate_pslldq_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    uint8_t imm8 = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 1));
    if (imm8 > 15) {
        la_vxor_v(dest, dest, dest);
    } else if (imm8 == 0) {
        return true;
    } else {
        la_vbsll_v(dest, dest, imm8);
    }
    return true;
}

bool translate_psrldq_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    uint8_t imm8 = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 1));
    if (imm8 > 15) {
        la_vxor_v(dest, dest, dest);
    } else if (imm8 == 0) {
        return true;
    } else {
        la_vbsrl_v(dest, dest, imm8);
    }
    return true;
}


bool translate_addsubpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
             ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1)));

    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND temp_dest_sub = ra_alloc_ftemp();
    IR2_OPND ftemp_src_add = ra_alloc_ftemp();
    IR2_OPND ftemp_src_sub = ra_alloc_ftemp();
    IR2_OPND ftemp_dest_add = ra_alloc_ftemp();
    IR2_OPND ftemp_dest_sub = ra_alloc_ftemp();

    la_vbsll_v(ftemp_src_add, src, 0);
    la_vbsll_v(ftemp_src_sub, src, 0);
    la_vbsll_v(ftemp_dest_add, dest, 0);
    la_vbsll_v(ftemp_dest_sub, dest, 0);

    la_vpermi_w(ftemp_src_add, src, 0xee);
    la_vpermi_w(ftemp_dest_add, dest, 0xee);
    la_vpermi_w(ftemp_src_sub, src, 0x44);
    la_vpermi_w(ftemp_dest_sub, dest, 0x44);

    la_vfsub_d(temp_dest_sub, ftemp_dest_sub, ftemp_src_sub);
    la_vfadd_d(dest, ftemp_dest_add, ftemp_src_add);
    if (option_enable_lasx) {
        la_xvinsve0_d(dest, temp_dest_sub, 0);
    } else {
        la_vextrins_d(dest, temp_dest_sub, 0);
    }

    ra_free_temp(temp_dest_sub);
    ra_free_temp(ftemp_src_add);
    ra_free_temp(ftemp_src_sub);
    ra_free_temp(ftemp_dest_add);
    ra_free_temp(ftemp_dest_sub);
    return true;
}

bool translate_addsubps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
             ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1)));

    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND temp_dest_sub = ra_alloc_ftemp();
    IR2_OPND ftemp_src_add = ra_alloc_ftemp();
    IR2_OPND ftemp_src_sub = ra_alloc_ftemp();
    IR2_OPND ftemp_dest_add = ra_alloc_ftemp();
    IR2_OPND ftemp_dest_sub = ra_alloc_ftemp();

    /**
     * The two should not interfere with each other When
     * calculating addition or subtraction, or unexpected FPE
     * would be triggered. See VPERMI in Volume 2 of LOONGARCH for
     * more informations.
     */
    la_vbsll_v(ftemp_src_add, src, 0);
    la_vbsll_v(ftemp_src_sub, src, 0);
    la_vbsll_v(ftemp_dest_add, dest, 0);
    la_vbsll_v(ftemp_dest_sub, dest, 0);

    la_vpermi_w(ftemp_src_add, src, 0xf5);
    la_vpermi_w(ftemp_dest_add, dest, 0xf5);
    la_vpermi_w(ftemp_src_sub, src, 0xa0);
    la_vpermi_w(ftemp_dest_sub, dest, 0xa0);

    la_vfsub_s(temp_dest_sub, ftemp_dest_sub, ftemp_src_sub);
    la_vfadd_s(dest, ftemp_dest_add, ftemp_src_add);

   /*
    * Pick temp_sub data to [0:31] [64:95]
    */
    if (option_enable_lasx) {
        la_xvinsve0_w(dest, temp_dest_sub, 0);
    } else {
        la_vextrins_w(dest, temp_dest_sub, 0);
    }
    la_vbsrl_v(temp_dest_sub, temp_dest_sub, 8);
    if (option_enable_lasx) {
        la_xvinsve0_w(dest, temp_dest_sub, 2);
    } else {
        la_vextrins_w(dest, temp_dest_sub, 0x2 << 4);
    }

    ra_free_temp(temp_dest_sub);
    ra_free_temp(ftemp_src_add);
    ra_free_temp(ftemp_src_sub);
    ra_free_temp(ftemp_dest_add);
    ra_free_temp(ftemp_dest_sub);

    return true;
}
