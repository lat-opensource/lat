#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate-bd.h"
#include "hbr-bd.h"

bool translate_por_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vor_v(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vor_v(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pxor_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_xmm_bd(dest) && ir1_opnd_is_mem_bd(src)) {
        IR2_OPND temp = ra_alloc_ftemp();
        load_freg128_from_ir1_mem_bd(temp, src);
        la_vxor_v(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                  ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)), temp);
        return true;
    } else if (ir1_opnd_is_xmm_bd(dest) && ir1_opnd_is_xmm_bd(src)) {
        la_vxor_v(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                  ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                  ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)));
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vxor_v(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_packsswb_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) &&
            (ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0)) ==
             ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 1)))) {
            //la_vssrani_b_h(dest, dest, 0);
            // on 3A6000
            // vsrani.b.h  latency 4 IPC 2 
            // vsat.h      latency 2 IPC 2
            // vpickev.b   latency 1 IPC 4
            // latency&IPC from https://jia.je/unofficial-loongarch-intrinsics-guide/
            la_vsat_h(dest, dest, 7);
            la_vpickev_b(dest, dest, dest);
        } else {
            // la_vssrani_b_h(dest, dest, 0);
            // IR2_OPND temp = ra_alloc_ftemp();
            // la_vssrani_b_h(temp, src, 0);
            // la_vextrins_d(dest, temp, VEXTRINS_IMM_4_0(1, 0));
            IR2_OPND temp = ra_alloc_ftemp();
            la_vsat_h(dest, dest, 7);
            la_vsat_h(temp, src, 7);
            la_vpickev_b(dest, temp, dest);
        }
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vpackev_d(dest, src, dest);
        //la_vssrani_b_h(dest, dest, 0);
        la_vsat_h(dest, dest, 7);
        la_vpickev_b(dest, dest, dest);
    }
    return true;
}

bool translate_packssdw_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) &&
            (ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0)) ==
             ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 1)))) {
            la_vssrani_h_w(dest, dest, 0);
        } else {
            la_vssrani_h_w(dest, dest, 0);
            IR2_OPND temp = ra_alloc_ftemp();
            la_vssrani_h_w(temp, src, 0);
            la_vextrins_d(dest, temp, VEXTRINS_IMM_4_0(1, 0));
        }
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vpackev_d(dest, src, dest);
        la_vssrani_h_w(dest, dest, 0);
    }
    return true;
}

bool translate_packuswb_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) &&
            (ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0)) ==
             ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 1)))) {
            la_vssrani_bu_h(dest, dest, 0);
            la_vextrins_d(dest, dest, VEXTRINS_IMM_4_0(1, 0));
        } else {
            la_vssrani_bu_h(dest, dest, 0);
            IR2_OPND temp = ra_alloc_ftemp();
            la_vssrani_bu_h(temp, src, 0);
            la_vextrins_d(dest, temp, VEXTRINS_IMM_4_0(1, 0));
        }
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vpackev_d(dest, src, dest);
        la_vssrani_bu_h(dest, dest, 0);
    }
    return true;
}

bool translate_paddb_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vadd_b(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vadd_b(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vadd_h(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vadd_h(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddd_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vadd_w(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vadd_w(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddsb_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vsadd_b(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vsadd_b(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddsw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vsadd_h(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vsadd_h(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddusb_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vsadd_bu(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vsadd_bu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddusw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vsadd_hu(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vsadd_hu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pand_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vand_v(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vand_v(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pandn_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vandn_v(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vandn_v(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pmaddwd_bd(IR1_INST *pir1)
{
    IR2_OPND temp = ra_alloc_ftemp();
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vxor_v(temp, temp, temp);
        la_vmaddwev_w_h(temp, dest, src);
        la_vmaddwod_w_h(temp, dest, src);
        la_vbsll_v(dest, temp, 0);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest_lo =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src_lo =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vxor_v(temp, temp, temp);
        la_vmaddwev_w_h(temp, dest_lo, src_lo);
        la_vmaddwod_w_h(temp, dest_lo, src_lo);
        la_vbsll_v(dest_lo, temp, 0);
    }
    return true;
}

bool translate_pmulhuw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vmuh_hu(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vmuh_hu(dest, dest, src);
    }
    return true;
}

bool translate_pmulhw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vmuh_h(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vmuh_h(dest, dest, src);
    }
    return true;
}

bool translate_pmullw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vmul_h(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vmul_h(dest, dest, src);
    }
    return true;
}

bool translate_psubb_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vsub_b(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vsub_b(dest, dest, src);
    }
    return true;
}

bool translate_psubw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vsub_h(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vsub_h(dest, dest, src);
    }
    return true;
}

bool translate_psubd_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vsub_w(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vsub_w(dest, dest, src);
    }
    return true;
}

bool translate_psubsb_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vssub_b(dest, dest, src);
        return true;
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vssub_b(dest, dest, src);
    }
    return true;
}

bool translate_psubsw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vssub_h(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vssub_h(dest, dest, src);
    }
    return true;
}

bool translate_psubusb_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vssub_bu(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vssub_bu(dest, dest, src);
    }
    return true;
}

bool translate_psubusw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vssub_hu(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vssub_hu(dest, dest, src);
    }
    return true;
}

bool translate_punpckhbw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vilvh_b(dest, src, dest);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vilvl_b(dest, src, dest);
        la_vextrins_d(dest, dest, VEXTRINS_IMM_4_0(0, 1));
    }
    return true;
}

bool translate_punpckhwd_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vilvh_h(dest, src, dest);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vilvl_h(dest, src, dest);
        la_vextrins_d(dest, dest, VEXTRINS_IMM_4_0(0, 1));
    }
    return true;
}

bool translate_punpckhdq_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vilvh_w(dest, src, dest);
    } else {
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vilvl_w(dest, src, dest);
        la_vextrins_d(dest, dest, VEXTRINS_IMM_4_0(0, 1));
    }
    return true;
}

bool translate_punpcklbw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vilvl_b(dest, src, dest);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vilvl_b(dest, src, dest);
    }
    return true;
}

bool translate_punpcklwd_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vilvl_h(dest, src, dest);
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vilvl_h(dest, src, dest);
    }
    return true;
}

bool translate_punpckldq_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vilvl_w(dest, src, dest);
        return true;
    } else { //mmx
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vilvl_w(dest, src, dest);
    }
    return true;
}

bool translate_addps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vfadd_s(dest, dest, src);
    return true;
}

bool translate_addsd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    if (SHBR_ON_64_BD(pir1)) {
        la_fadd_d(dest, dest, src);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fadd_d(temp, dest, src);
        if (option_enable_lasx) {
            la_xvinsve0_d(dest, temp, 0);
        } else {
            la_vextrins_d(dest, temp, 0);
        }
    }
    return true;
}

bool translate_addss_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    if (SHBR_ON_32_BD(pir1)) {
        la_fadd_s(dest, dest, src);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fadd_s(temp, dest, src);
        if (option_enable_lasx) {
            la_xvinsve0_w(dest, temp, 0);
        } else {
            la_vextrins_w(dest, temp, 0);
        }
    }
    return true;
}

bool translate_andnpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vandn_v(dest, dest, src);
    return true;
}

bool translate_andnps_bd(IR1_INST *pir1)
{
    translate_andnpd_bd(pir1);
    return true;
}

bool translate_andps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vand_v(dest, dest, src);
    return true;
}

bool translate_divpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vfdiv_d(dest, dest, src);
    return true;
}

bool translate_divps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vfdiv_s(dest, dest, src);
    return true;
}

bool translate_divsd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    if (SHBR_ON_64_BD(pir1)) {
        la_fdiv_d(dest, dest, src);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fdiv_d(temp, dest, src);
        if (option_enable_lasx) {
            la_xvinsve0_d(dest, temp, 0);
        } else {
            la_vextrins_d(dest, temp, 0);
        }
    }
    return true;
}

bool translate_divss_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    if (SHBR_ON_32_BD(pir1)) {
        la_fdiv_s(dest, dest, src);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fdiv_s(temp, dest, src);
        if (option_enable_lasx) {
            la_xvinsve0_w(dest, temp, 0);
        } else {
            la_vextrins_w(dest, temp, 0);
        }
    }
    return true;
}

/**
 * @brief translate inst maxpd, maxps, maxsd, maxss
 * IF ((SRC1 = 0.0) and (SRC2 = 0.0)) THEN DEST = SRC2;
 *     ELSE IF (SRC1 = SNaN) THEN DEST = SRC2; FI;
 *     ELSE IF (SRC2 = SNaN) THEN DEST = SRC2; FI;
 *     ELSE IF (SRC1 > SRC2) THEN DEST = SRC1;
 *     ELSE DEST = SRC2;
 * FI;
  */
bool translate_maxpd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        if (ir1_opnd_base_reg_num_bd(opnd0) == ir1_opnd_base_reg_num_bd(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND mask = ra_alloc_ftemp();
    la_vfcmp_cond_d(mask, dest, src, 0xF); // equal, unorder and dest < src, use src
    la_vbitsel_v(dest, dest, src, mask);
    return true;
}

bool translate_maxps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        if (ir1_opnd_base_reg_num_bd(opnd0) == ir1_opnd_base_reg_num_bd(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND mask = ra_alloc_ftemp();
    la_vfcmp_cond_s(mask, dest, src, 0xF); // equal, unorder and dest < src, use src
    la_vbitsel_v(dest, dest, src, mask);
    return true;
}

bool translate_maxsd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        if (ir1_opnd_base_reg_num_bd(opnd0) == ir1_opnd_base_reg_num_bd(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_fcmp_cond_d(fcc0_ir2_opnd, src, dest, 0x3);
    if (SHBR_ON_64_BD(pir1)) {
        la_fsel(dest, src, dest, fcc0_ir2_opnd);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fsel(temp, src, dest, fcc0_ir2_opnd);
        if (option_enable_lasx) {
            la_xvinsve0_d(dest, temp, 0);
        } else {
            la_vextrins_d(dest, temp, 0);
        }
    }

    return true;
}

bool translate_maxss_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        if (ir1_opnd_base_reg_num_bd(opnd0) == ir1_opnd_base_reg_num_bd(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_fcmp_cond_s(fcc0_ir2_opnd, src, dest, 0x3);
    if (SHBR_ON_32_BD(pir1)) {
        la_fsel(dest, src, dest, fcc0_ir2_opnd);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fsel(temp, src, dest, fcc0_ir2_opnd);
        if (option_enable_lasx) {
            la_xvinsve0_w(dest, temp, 0);
        } else {
            la_vextrins_w(dest, temp, 0);
        }
    }

    return true;
}

bool translate_minpd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        if (ir1_opnd_base_reg_num_bd(opnd0) == ir1_opnd_base_reg_num_bd(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND mask = ra_alloc_ftemp();
    // sse minpd:
    //  * MIN(SRC1, SRC2)
    // {
    //     IF ((SRC1 = 0.0) and (SRC2 = 0.0)) THEN DEST := SRC2;
    //         ELSE IF (SRC1 = NaN) THEN DEST := SRC2; FI;
    //         ELSE IF (SRC2 = NaN) THEN DEST := SRC2; FI;
    //         ELSE IF (SRC1 < SRC2) THEN DEST := SRC1;
    //         ELSE DEST := SRC2;
    //     FI;
    // }
    //  SULE = 0xF = un lt eq
    //  un: either operand 1 or 2 is Nan, choose second operand
    //  eq: +0.0 == -0.0 , choose second operand. not zero, either operand 1 or 2 is ok
    //  lt: choose second operand
    la_vfcmp_cond_d(mask, src, dest, 0xF);
    la_vbitsel_v(dest, dest, src, mask);
    return true;
}

bool translate_minps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        if (ir1_opnd_base_reg_num_bd(opnd0) == ir1_opnd_base_reg_num_bd(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND mask = ra_alloc_ftemp();
    // as MINPD
    la_vfcmp_cond_s(mask, src, dest, 0xF);
    la_vbitsel_v(dest, dest, src, mask);
    return true;
}

bool translate_minsd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        if (ir1_opnd_base_reg_num_bd(opnd0) == ir1_opnd_base_reg_num_bd(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_fcmp_cond_d(fcc0_ir2_opnd, dest, src, 0x3);
    if (SHBR_ON_64_BD(pir1)) {
        la_fsel(dest, src, dest, fcc0_ir2_opnd);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fsel(temp, src, dest, fcc0_ir2_opnd);
        if (option_enable_lasx) {
            la_xvinsve0_d(dest, temp, 0);
        } else {
            la_vextrins_d(dest, temp, 0);
        }
    }

    return true;
}

bool translate_minss_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        if (ir1_opnd_base_reg_num_bd(opnd0) == ir1_opnd_base_reg_num_bd(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_fcmp_cond_s(fcc0_ir2_opnd, dest, src, 0x3);
    if (SHBR_ON_32_BD(pir1)) {
        la_fsel(dest, src, dest, fcc0_ir2_opnd);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fsel(temp, src, dest, fcc0_ir2_opnd);
        if (option_enable_lasx) {
            la_xvinsve0_w(dest, temp, 0);
        } else {
            la_vextrins_w(dest, temp, 0);
        }
    }

    return true;
}

bool translate_mulpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vfmul_d(dest, dest, src);
    return true;
}

bool translate_mulps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vfmul_s(dest, dest, src);
    return true;
}

bool translate_mulsd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    if (SHBR_ON_64_BD(pir1)) {
        la_fmul_d(dest, dest, src);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fmul_d(temp, dest, src);
        if (option_enable_lasx) {
            la_xvinsve0_d(dest, temp, 0);
        } else {
            la_vextrins_d(dest, temp, 0);
        }
    }
    return true;
}

bool translate_mulss_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    if (SHBR_ON_32_BD(pir1)) {
        la_fmul_s(dest, dest, src);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fmul_s(temp, dest, src);
        if (option_enable_lasx) {
            la_xvinsve0_w(dest, temp, 0);
        } else {
            la_vextrins_w(dest, temp, 0);
        }
    }

    return true;
}

bool translate_orpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vor_v(dest, dest, src);
    return true;
}

bool translate_orps_bd(IR1_INST *pir1)
{
    translate_orpd_bd(pir1);
    return true;
}

bool translate_paddq_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vadd_d(dest, dest, src);
        return true;
    }
    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vadd_d(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pavgb_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vavgr_bu(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vavgr_bu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pavgw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vavgr_hu(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vavgr_hu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pextrw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        uint8_t imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 2));
        imm &= 7;
        IR2_OPND dest_opnd;
        if(ir1_opnd_is_mem_bd(opnd0)) {
            dest_opnd = ra_alloc_itemp();
        } else {
            /* [MSB:16] will be zeroed, so we simply get gpr */
            dest_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0)));
        }
        la_vpickve2gr_hu(dest_opnd,
            ra_alloc_xmm(ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 1))), imm);
        if(ir1_opnd_is_mem_bd(opnd0)) {
            store_ireg_to_ir1_bd(dest_opnd, ir1_get_opnd_bd(pir1, 0), false);
            ra_free_temp(dest_opnd);
        }
        return true;
    }
    uint8 imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 0) + 2);
    IR2_OPND src_lo = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 0) + 1, UNKNOWN_EXTENSION,
                           false); /* fill default parameter */
    uint8 select = imm & 0x3;
    switch (select) {
    case 0:
        break;
    case 1:
        la_srli_d(src_lo, src_lo, 0x10);
        break;
    case 2:
        la_srli_d(src_lo, src_lo, 0x20);
        break;
    case 3:
        la_srli_d(src_lo, src_lo, 0x30);
        break;
    default:
        fprintf(stderr, "1: invalid imm8<0:1> in PEXTRW : %d\n", select);
        exit(-1);
    }
    la_bstrpick_w(src_lo, src_lo, 15, 0);
    store_ireg_to_ir1_bd(src_lo, ir1_get_opnd_bd(pir1, 0), false); /* fill default */
                                                   /* parameter */
    return true;
}

bool translate_pinsrw_bd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))) {
        uint8_t imm = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 2));
        imm &= 7;
        IR2_OPND src = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1),
                                          UNKNOWN_EXTENSION, false);
        la_vinsgr2vr_h(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0))),
                       src, imm);
        return true;
    }
    IR2_OPND src = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 0) + 1, UNKNOWN_EXTENSION,
                                      false); /* fill default parameter */
    IR2_OPND ftemp = ra_alloc_ftemp();
    la_movgr2fr_d(ftemp, src);
    uint8 imm8 = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 0) + 2);

    IR2_OPND dest =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    uint8 select = imm8 & 0x3;
    switch (select) {
    case 0:
        la_vextrins_h(dest, ftemp, VEXTRINS_IMM_4_0(0, 0));
        break;
    case 1:
        la_vextrins_h(dest, ftemp, VEXTRINS_IMM_4_0(1, 0));
        break;
    case 2:
        la_vextrins_h(dest, ftemp, VEXTRINS_IMM_4_0(2, 0));
        break;
    case 3:
        la_vextrins_h(dest, ftemp, VEXTRINS_IMM_4_0(3, 0));
        break;
    default:
        fprintf(stderr, "1: invalid imm8<0:1> in PINSRW : %d\n", select);
        exit(-1);
    }
    ra_free_temp(ftemp);
    return true;
}

bool translate_pmaxsw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vmax_h(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    if (ir2_opnd_cmp(&dest_lo, &src_lo))
        return true;
    else
        la_vmax_h(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pmaxub_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vmax_bu(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    if (ir2_opnd_cmp(&dest_lo, &src_lo))
        return true;
    else
        la_vmax_bu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pminsw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vmin_h(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    if (ir2_opnd_cmp(&dest_lo, &src_lo))
        return true;
    else
        la_vmin_h(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pminub_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vmin_bu(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    if (ir2_opnd_cmp(&dest_lo, &src_lo))
        return true;
    else
        la_vmin_bu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pmuludq_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vmulwev_d_wu(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vmulwev_d_wu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_psadbw_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vabsd_bu(dest, dest, src);
        la_vhaddw_hu_bu(dest, dest, dest);
        la_vhaddw_wu_hu(dest, dest, dest);
        la_vhaddw_du_wu(dest, dest, dest);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vabsd_bu(dest_lo, dest_lo, src_lo);
    la_vhaddw_hu_bu(dest_lo, dest_lo, dest_lo);
    la_vhaddw_wu_hu(dest_lo, dest_lo, dest_lo);
    la_vhaddw_du_wu(dest_lo, dest_lo, dest_lo);
    return true;
}

bool translate_pshufd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    uint64_t imm8 = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 2));
    la_vshuf4i_w(dest, src, imm8);
    return true;
}

bool translate_pshufw_bd(IR1_INST *pir1)
{
    IR2_OPND dest =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    IR1_OPND_BD *imm8_reg = ir1_get_opnd_bd(pir1, 2);
    uint64_t imm8 = ir1_opnd_uimm_bd(imm8_reg);
    la_vshuf4i_h(dest, src, imm8);
    return true;
}

bool translate_pshufhw_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR1_OPND_BD *dest_reg = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src_reg = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *imm8_reg = ir1_get_opnd_bd(pir1, 2);

    IR2_OPND dest = load_freg128_from_ir1_bd(dest_reg);
    IR2_OPND src = load_freg128_from_ir1_bd(src_reg);
    IR2_OPND temp = ra_alloc_ftemp();

    uint64_t imm8 = ir1_opnd_uimm_bd(imm8_reg);
    if (ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1)) ||
        (ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0)) !=
         ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 1)))) {
        la_vori_b(dest, src, 0);
    }
    la_vori_b(temp, src, 0);
    la_vshuf4i_h(dest, dest, imm8);
    if (option_enable_lasx) {
        la_xvinsve0_d(dest, temp, 0);
    } else {
        la_vextrins_d(dest, temp, 0);
    }
    return true;
}

bool translate_pshuflw_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND temp = ra_alloc_ftemp();
    uint64_t imm8 = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 2));
    if (ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1)) ||
        (ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0)) !=
         ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 1)))) {
        la_vori_b(dest, src, 0);
    }

    la_vori_b(temp, src, 0);
    la_vbsrl_v(temp, temp, 8);
    la_vshuf4i_h(dest, dest, imm8);
    if (option_enable_lasx) {
        la_xvinsve0_d(dest, temp, 1);
    } else {
        la_vextrins_d(dest, temp, 0x1 << 4);
    }
    return true;
}

bool translate_psubq_bd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vsub_d(dest, dest, src);
        return true;
    } else {
        /* transfer_to_mmx_mode_bd */
        transfer_to_mmx_mode_bd();

        IR2_OPND dest_lo =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src_lo =
            load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
        la_vsub_d(dest_lo, dest_lo, src_lo);
    }
    return true;
}

bool translate_punpckhqdq_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vilvh_d(dest, src, dest);
    return true;
}

bool translate_rcpss_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND temp = ra_alloc_ftemp();
    la_vfrecip_s(temp, src);
    if (option_enable_lasx) {
        la_xvinsve0_w(dest, temp, 0);
    } else {
        la_vextrins_w(dest, temp, 0);
    }
    return true;
}

bool translate_rcpps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vfrecip_s(dest, src);
    return true;
}

bool translate_rsqrtss_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND temp = ra_alloc_ftemp();
    la_frsqrt_s(temp, src);
    if (option_enable_lasx) {
        la_xvinsve0_w(dest, temp, 0);
    } else {
        la_vextrins_w(dest, temp, 0);
    }
    return true;
}

bool translate_rsqrtps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vfrsqrt_s(dest, src);
    return true;
}

bool translate_sqrtpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));

    if (option_enable_lasx) {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp0 = ra_alloc_ftemp();
        la_fsqrt_d(temp0, src);
        la_vshuf4i_w(temp, src, 0xee);
        la_fsqrt_d(temp, temp);
        la_xvinsve0_d(dest, temp, 1);
        la_xvinsve0_d(dest, temp0, 0);
    } else {
        la_vfsqrt_d(dest, src);
    }

    return true;
}

bool translate_sqrtps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));

    if (option_enable_lasx) {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp0 = ra_alloc_ftemp();
        la_vfsqrt_s(temp0, src);
        la_vshuf4i_w(temp, src, 0xee);
        la_vfsqrt_s(temp, temp);
        la_xvinsve0_d(dest, temp, 1);
        la_xvinsve0_d(dest, temp0, 0);
    } else {
        la_vfsqrt_s(dest, src);
    }

    return true;
}

bool translate_addpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vfadd_d(dest, dest, src);
    return true;
}

bool translate_andpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vand_v(dest, dest, src);
    return true;
}

bool translate_unpcklps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vilvl_w(dest, src, dest);
    return true;
}

bool translate_unpcklpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vshuf4i_d(dest, src, 0x8);
    return true;
}

bool translate_unpckhpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vshuf4i_d(dest, src, 0xd);
    return true;
}

bool translate_unpckhps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vilvh_w(dest, src, dest);
    return true;
}

bool translate_shufps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    uint64_t imm8 = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 2));
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    la_vshuf4i_w(temp1, dest, imm8);
    la_vshuf4i_w(temp2, src, imm8 >> 4);
    la_vpickev_d(dest , temp2, temp1);
    return true;
}

bool translate_shufpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    uint8_t imm8 = ir1_opnd_uimm_bd(ir1_get_opnd_bd(pir1, 2));
    imm8 &= 3;
    uint8_t shfd_imm8 = 0;
    if (imm8 == 0){
        shfd_imm8 = 0x8;
    }
    else if(imm8 == 1) {
        shfd_imm8 = 0x9;
    }
    else if(imm8 == 2) {
        shfd_imm8 = 0xc;
    }
    else if(imm8 == 3) {
        shfd_imm8 = 0xd;
    }
    else {
        lsassert(0);
    }

    la_vshuf4i_d(dest, src, shfd_imm8);
    return true;
}

bool translate_punpcklqdq_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vilvl_d(dest, src, dest);
    return true;
}

bool translate_xorps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vxor_v(dest, dest, src);
    return true;
}

bool translate_xorpd_bd(IR1_INST *pir1)
{
    translate_xorps_bd(pir1);
    return true;
}

bool translate_subss_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    if (SHBR_ON_32_BD(pir1)) {
        la_fsub_s(dest, dest, src);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fsub_s(temp, dest, src);
        if (option_enable_lasx) {
            la_xvinsve0_w(dest, temp, 0);
        } else {
            la_vextrins_w(dest, temp, 0);
        }
    }

    return true;
}

bool translate_subsd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    if (SHBR_ON_64_BD(pir1)) {
        la_fsub_d(dest, dest, src);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fsub_d(temp, dest, src);
        if (option_enable_lasx) {
            la_xvinsve0_d(dest, temp, 0);
        } else {
            la_vextrins_d(dest, temp, 0);
        }
    }
    return true;
}

bool translate_subps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vfsub_s(dest, dest, src);
    return true;
}

bool translate_subpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vfsub_d(dest, dest, src);
    return true;
}

bool translate_sqrtsd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    if (SHBR_ON_64_BD(pir1)) {
        la_fsqrt_d(dest, src);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fsqrt_d(temp, src);
        if (option_enable_lasx) {
            la_xvinsve0_d(dest, temp, 0);
        } else {
            la_vextrins_d(dest, temp, 0);
        }
    }

    return true;
}

bool translate_sqrtss_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    if (SHBR_ON_32_BD(pir1)) {
        la_fsqrt_s(dest, src);
    } else{
        IR2_OPND temp = ra_alloc_ftemp();
        la_fsqrt_s(temp, src);
        if (option_enable_lasx) {
            la_xvinsve0_w(dest, temp, 0);
        } else {
            la_vextrins_w(dest, temp, 0);
        }
    }

    return true;
}

bool translate_pause_bd(IR1_INST *pir1) { return true; }

/**
 * @brief
 *
 * xmm1 -> LO (dest)
 * xmm2 -> HI (src)
 * +------+------+
 * | xmm2 | xmm1 |
 * +------+------+
 * | xmm2 | xmm1 |
 * +------+------+
 *
 * @param pir1
 * @return true
 * @return false
 */
bool translate_haddpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    la_vpickev_d(temp1, src, dest);
    la_vpickod_d(temp2, src, dest);
    la_vfadd_d(dest, temp1, temp2);

    return true;
}

bool translate_haddps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    /**
     * origin:
     * s3, s2, s1, s0
     * d3, d2, d1, d0
     *
     * after pick:
     * temp1:   s2, s0, d2, d0
     * temp2:   s3, s1, d3, d1
     */
    la_vpickev_w(temp1, src, dest);
    la_vpickod_w(temp2, src, dest);
    la_vfadd_s(dest, temp1, temp2);
    return true;
}

bool translate_hsubpd_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    la_vpickev_d(temp1, src, dest);
    la_vpickod_d(temp2, src, dest);
    la_vfsub_d(dest, temp1, temp2);
    return true;
}

bool translate_hsubps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    la_vpickev_w(temp1, src, dest);
    la_vpickod_w(temp2, src, dest);
    la_vfsub_s(dest, temp1, temp2);
    return true;
}

/* ssse3 */
bool translate_psignb_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vsigncov_b(dest, src, dest);

    return true;
}

bool translate_psignw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vsigncov_h(dest, src, dest);

    return true;
}

bool translate_psignd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vsigncov_w(dest, src, dest);

    return true;
}

bool translate_pabsb_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND vzero = ra_alloc_ftemp();

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vreplgr2vr_d(vzero, zero_ir2_opnd);
    la_vabsd_b(dest, src, vzero);

    return true;
}

bool translate_pabsw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND vzero = ra_alloc_ftemp();

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vreplgr2vr_d(vzero, zero_ir2_opnd);
    la_vabsd_h(dest, src, vzero);

    return true;
}

bool translate_pabsd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND vzero = ra_alloc_ftemp();

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vreplgr2vr_d(vzero, zero_ir2_opnd);
    la_vabsd_w(dest, src, vzero);

    return true;
}

bool translate_palignr_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);

    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        /* fast path */
        if (imm >= 32) {
            la_vxor_v(dest, dest, dest);
        } else if (imm >= 16 && imm < 32) {
            la_vbsrl_v(dest, dest, imm - 16);
        } else {
            /* slow path */
            src = load_freg128_from_ir1_bd(opnd1);
            if (imm == 0) {
                la_vori_b(dest, src, 0);
            } else {
                la_vbsrl_v(temp, src, imm);
                la_vbsll_v(dest, dest, 16 - imm);
                la_vor_v(dest, dest, temp);
            }
        }
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        /* fast path */
        if (imm >= 16) {
            la_vxor_v(dest, dest, dest);
        } else if (imm >= 8 && imm < 16) {
            la_vinsgr2vr_d(dest, zero_ir2_opnd, 0x1);
            la_vbsrl_v(dest, dest, imm - 8);
        } else {
            /* slow path */
            src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
            if (imm == 0) {
                la_vori_b(dest, src, 0);
            } else {
                la_vori_b(temp, src, 0);
                if (option_enable_lasx) {
                    la_xvinsve0_d(temp, dest, 1);
                } else {
                    la_vextrins_d(temp, dest, 0x1 << 4);
                }
                la_vbsrl_v(dest, temp, imm);
            }
        }
    }

    return true;
}

bool translate_pshufb_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND vzero = ra_alloc_ftemp();
    IR2_OPND src_t1 = ra_alloc_ftemp();
    IR2_OPND src_t2 = ra_alloc_ftemp();

    la_vreplgr2vr_d(vzero, zero_ir2_opnd);

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
        la_vandi_b(src_t2, src, 0xf);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
        la_vandi_b(src_t2, src, 0x7);
    }

    la_vandi_b(src_t1, src, 0x80);
    la_vsrli_b(src_t1, src_t1, 3);
    la_vadd_b(src_t1, src_t2, src_t1);
    la_vshuf_b(dest, vzero, dest, src_t1);

    return true;
}

bool translate_pmulhrsw_bd(IR1_INST *pir1)
{
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    if(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
        la_vmulwev_w_h(temp1, dest, src);
        la_vmulwod_w_h(temp2, dest, src);
        la_vsrai_w(temp1, temp1, 14);
        la_vsrai_w(temp2, temp2, 14);
        la_vaddi_wu(temp1, temp1, 1);
        la_vaddi_wu(temp2, temp2, 1);
        la_vsrani_h_w(temp2, temp1, 1);
        la_vshuf4i_w(temp2, temp2, 0xd8); //11 01 10 00
        la_vshuf4i_h(dest, temp2, 0xd8); //11 01 10 00
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    la_vmulwev_w_h(temp1, dest_lo, src_lo);
    la_vmulwod_w_h(temp2, dest_lo, src_lo);
    la_vsrai_w(temp1, temp1, 14);
    la_vsrai_w(temp2, temp2, 14);
    la_vaddi_wu(temp1, temp1, 1);
    la_vaddi_wu(temp2, temp2, 1);
    la_vsrani_h_w(temp2, temp1, 1);
    la_vshuf4i_w(temp2, temp2, 0xd8); //11 01 10 00
    la_vshuf4i_h(dest_lo, temp2, 0xd8); //11 01 10 00
    return true;
}

bool translate_pmaddubsw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    IR2_OPND temp3 = ra_alloc_ftemp();
    IR2_OPND temp4 = ra_alloc_ftemp();
    IR2_OPND temp5 = ra_alloc_ftemp();
    IR2_OPND itmp = ra_alloc_itemp();

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    /* unsigned dest * signed src */

    /* U dest * abs(src) */
    la_vreplgr2vr_d(temp1, zero_ir2_opnd);
    la_vabsd_b(temp3, src, temp1);
    la_vmaddwev_h_bu(temp1, dest, temp3);
    la_vreplgr2vr_d(temp2, zero_ir2_opnd);
    la_vmaddwod_h_bu(temp2, dest, temp3);

    la_ori(itmp, zero_ir2_opnd, 0x1);
    la_vreplgr2vr_b(temp3, itmp);
    la_vsigncov_b(temp4, src, temp3);

    la_vmulwev_h_b(temp5, temp4, temp3);
    la_vmulwod_h_b(temp3, temp4, temp3);

    la_vmul_h(temp1, temp1, temp5);
    la_vmul_h(temp2, temp2, temp3);
    la_vsadd_h(dest, temp2, temp1);

    return true;
}

bool translate_phsubw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vpickev_h(temp1, src, dest);
    la_vpickod_h(temp2, src, dest);
    la_vsub_h(dest, temp1, temp2);
    if (!ir1_opnd_is_xmm_bd(opnd0)) {
        la_vshuf4i_w(dest, dest, 0xd8);
    }
    return true;
}

bool translate_phsubd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vpickev_w(temp1, src, dest);
    la_vpickod_w(temp2, src, dest);
    la_vsub_w(dest, temp1, temp2);
    if (!ir1_opnd_is_xmm_bd(opnd0)) {
        la_vshuf4i_w(dest, dest, 0xd8);
    }
    return true;
}

bool translate_phsubsw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vpickev_h(temp1, src, dest);
    la_vpickod_h(temp2, src, dest);
    la_vssub_h(dest, temp1, temp2);
    if (!ir1_opnd_is_xmm_bd(opnd0)) {
        la_vshuf4i_w(dest, dest, 0xd8);
    }
    return true;
}

bool translate_phaddw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vpickev_h(temp1, src, dest);
    la_vpickod_h(temp2, src, dest);
    la_vadd_h(dest, temp1, temp2);
    if (!ir1_opnd_is_xmm_bd(opnd0)) {
        la_vshuf4i_w(dest, dest, 0xd8);
    }
    return true;
}

bool translate_phaddd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vpickev_w(temp1, src, dest);
    la_vpickod_w(temp2, src, dest);
    la_vadd_w(dest, temp1, temp2);
    if (!ir1_opnd_is_xmm_bd(opnd0)) {
        la_vshuf4i_w(dest, dest, 0xd8);
    }
    return true;
    return false;
}

bool translate_phaddsw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm_bd(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1_bd(opnd0);
        src = load_freg128_from_ir1_bd(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1_bd(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1_bd(opnd1, false, IS_INTEGER);
    }

    la_vpickev_h(temp1, src, dest);
    la_vpickod_h(temp2, src, dest);
    la_vsadd_h(temp2, temp1, temp2);
    if (!ir1_opnd_is_xmm_bd(opnd0)) {
        la_vshuf4i_w(temp2, temp2, 0xd8);
    }
    la_vori_b(dest, temp2, 0);
    return true;
}


/* sse 4.1 fp */
bool translate_dpps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    lsassert(ir1_opnd_is_xmm_bd(opnd0) || ir1_opnd_is_ymm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);
    la_vxor_v(temp1, temp1, temp1);
    la_vxor_v(temp2, temp2, temp2);
    if(imm & 0x10){
        la_vextrins_w(temp1, dest, 0x00);
        la_vextrins_w(temp2, src1, 0x00);
    }
    if(imm & 0x20){
        la_vextrins_w(temp1, dest, 0x11);
        la_vextrins_w(temp2, src1, 0x11);
    }
    if(imm & 0x40){
        la_vextrins_w(temp1, dest, 0x22);
        la_vextrins_w(temp2, src1, 0x22);
    }
    if(imm & 0x80){
        la_vextrins_w(temp1, dest, 0x33);
        la_vextrins_w(temp2, src1, 0x33);
    }
    la_vfmul_s(temp1, temp1, temp2);
    la_vpackod_w(temp2, temp1, temp1);
    la_vpackev_w(temp1, temp1, temp1);
    la_vfadd_s(temp1, temp1, temp2);
    la_vpackod_d(temp2, temp1, temp1);
    la_vpackev_d(temp1, temp1, temp1);
    la_vfadd_s(temp1, temp1, temp2);

    la_vxor_v(dest, dest, dest);
    if(imm & 0x1){
        la_vextrins_w(dest, temp1, 0x00);
    }
    if(imm & 0x2){
        la_vextrins_w(dest, temp1, 0x11);
    }
    if(imm & 0x4){
        la_vextrins_w(dest, temp1, 0x22);
    }
    if(imm & 0x8){
        la_vextrins_w(dest, temp1, 0x33);
    }
    set_high128_xreg_to_zero_bd(dest);
    return true;
}

bool translate_dppd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    la_xvxor_v(temp1, temp1, temp1);
    la_xvxor_v(temp2, temp2, temp2);
    if(imm & 0x10){
        la_vextrins_d(temp1, dest, 0x00);
        la_vextrins_d(temp2, src1, 0x00);
    }
    if(imm & 0x20){
        la_vextrins_d(temp1, dest, 0x11);
        la_vextrins_d(temp2, src1, 0x11);
    }

    la_vfmul_d(temp1, temp1, temp2);
    la_vpackod_d(temp2, temp1, temp1);
    la_vpackev_d(temp1, temp1, temp1);
    la_vfadd_d(temp1, temp1, temp2);
    la_xvxor_v(dest, dest, dest);
    if(imm & 0x1){
        la_xvextrins_d(dest, temp1, 0x00);
    }
    if(imm & 0x2){
        la_xvextrins_d(dest, temp1, 0x11);
    }
    set_high128_xreg_to_zero_bd(dest);
    return true;
}

bool translate_blendps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);
    if(imm & 0x1)
        la_vextrins_w(dest, src, VEXTRINS_IMM_4_0(0, 0));
    if(imm & 0x2)
        la_vextrins_w(dest, src, VEXTRINS_IMM_4_0(1, 1));
    if(imm & 0x4)
        la_vextrins_w(dest, src, VEXTRINS_IMM_4_0(2, 2));
    if(imm & 0x8)
        la_vextrins_w(dest, src, VEXTRINS_IMM_4_0(3, 3));
    return true;
}

bool translate_blendpd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);
    if(imm & 0x1)
        la_vextrins_d(dest, src, VEXTRINS_IMM_4_0(0, 0));
    if(imm & 0x2)
        la_vextrins_d(dest, src, VEXTRINS_IMM_4_0(1, 1));
    return true;
}

bool translate_blendvps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND mask = ra_alloc_xmm(0);
    IR2_OPND temp = ra_alloc_ftemp();
    la_vslti_w(temp, mask, 0);
    la_vbitsel_v(dest, dest, src, temp);
    return true;
}

bool translate_blendvpd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND mask = ra_alloc_xmm(0);
    IR2_OPND temp = ra_alloc_ftemp();
    la_vslti_d(temp, mask, 0);
    la_vbitsel_v(dest, dest, src, temp);
    return true;
}

bool translate_roundps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();
    if(imm & 0x8){
        la_vfcmp_cond_s(temp, src, src, 0x8);
    } else {
        la_vfrint_s(temp, src);
    }
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_save, fcsr, 31, 0);
    la_ld_wu(mxcsr, env_ir2_opnd,
            lsenv_offset_of_mxcsr(lsenv));
    la_bstrins_w(fcsr, zero_ir2_opnd, 4, 0);
    if (imm & 0x4) {
        temp = ra_alloc_itemp();
        la_bstrpick_w(temp, mxcsr, 14, 13);
        IR2_OPND temp_int = ra_alloc_itemp_internal();
        la_andi(temp_int, temp, 0x1);
        IR2_OPND label1 = ra_alloc_label();
        la_beq(temp_int, zero_ir2_opnd, label1);
        la_xori(temp, temp, 0x2);
        la_label(label1);
        la_bstrins_w(fcsr, temp, 9, 8);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
        la_vfrint_s(dest, src);
        set_high128_xreg_to_zero_bd(dest);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

        ra_free_temp(temp);
        ra_free_temp(fcsr);
        ra_free_temp(fcsr_save);
        ra_free_temp(mxcsr);
        return true;
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
    if ((imm & 0x3) == 0x0) {
        la_vfrintrne_s(dest, src);
    } else if ((imm & 0x3) == 0x1) {
        la_vfrintrm_s(dest, src);
    } else if ((imm & 0x3) == 0x2) {
        la_vfrintrp_s(dest, src);
    } else if ((imm & 0x3) == 0x3) {
        la_vfrintrz_s(dest, src);
	}
    set_high128_xreg_to_zero_bd(dest);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_roundss_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1_bd(opnd1);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();
    IR2_OPND src = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();
    la_xvreplve0_w(src, src1);

    if (imm & 0x8) {
        la_vfcmp_cond_s(temp, src, src, 0x8);
    } else {
        la_vfrint_s(temp, src);
    }
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_save, fcsr, 31, 0);
    la_ld_wu(mxcsr, env_ir2_opnd,
            lsenv_offset_of_mxcsr(lsenv));
    la_bstrins_w(fcsr, zero_ir2_opnd, 4, 0);
    if (imm & 0x4) {
        temp = ra_alloc_itemp();
        la_bstrpick_w(temp, mxcsr, 14, 13);
        IR2_OPND temp_int = ra_alloc_itemp_internal();
        la_andi(temp_int, temp, 0x1);
        IR2_OPND label1 = ra_alloc_label();
        la_beq(temp_int, zero_ir2_opnd, label1);
        la_xori(temp, temp, 0x2);
        la_label(label1);
        la_bstrins_w(fcsr, temp, 9, 8);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
        la_vfrint_s(temp_dest, src);
        la_xvinsve0_w(dest, temp_dest, 0);
        set_high128_xreg_to_zero_bd(dest);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

        ra_free_temp(temp);
        ra_free_temp(fcsr);
        ra_free_temp(fcsr_save);
        ra_free_temp(mxcsr);
        return true;
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
    if ((imm & 0x3) == 0x0) {
        la_vfrintrne_s(temp_dest, src);
    } else if ((imm & 0x3) == 0x1) {
        la_vfrintrm_s(temp_dest, src);
    } else if ((imm & 0x3) == 0x2) {
        la_vfrintrp_s(temp_dest, src);
    } else if ((imm & 0x3) == 0x3) {
        la_vfrintrz_s(temp_dest, src);
	}
    la_xvinsve0_w(dest, temp_dest, 0);
    set_high128_xreg_to_zero_bd(dest);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_roundpd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();
    if (imm & 0x8) {
        la_vfcmp_cond_d(temp, src, src, 0x8);
    } else {
        la_vfrint_d(temp, src);
    }
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_save, fcsr, 31, 0);
    la_ld_wu(mxcsr, env_ir2_opnd,
            lsenv_offset_of_mxcsr(lsenv));
    la_bstrins_w(fcsr, zero_ir2_opnd, 4, 0);
    if (imm & 0x4) {
        temp = ra_alloc_itemp();
        la_bstrpick_w(temp, mxcsr, 14, 13);
        IR2_OPND temp_int = ra_alloc_itemp_internal();
        la_andi(temp_int, temp, 0x1);
        IR2_OPND label1 = ra_alloc_label();
        la_beq(temp_int, zero_ir2_opnd, label1);
        la_xori(temp, temp, 0x2);
        la_label(label1);
        la_bstrins_w(fcsr, temp, 9, 8);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
        la_vfrint_d(dest, src);
        set_high128_xreg_to_zero_bd(dest);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

        ra_free_temp(temp);
        ra_free_temp(fcsr);
        ra_free_temp(fcsr_save);
        ra_free_temp(mxcsr);
        return true;
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
    if ((imm & 0x3) == 0x0) {
        la_vfrintrne_d(dest, src);
    } else if ((imm & 0x3) == 0x1) {
        la_vfrintrm_d(dest, src);
    } else if ((imm & 0x3) == 0x2) {
        la_vfrintrp_d(dest, src);
    } else if ((imm & 0x3) == 0x3) {
        la_vfrintrz_d(dest, src);
	}
    set_high128_xreg_to_zero_bd(dest);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_roundsd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1_bd(opnd1);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();
    IR2_OPND src = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();

    la_xvreplve0_d(src, src1);
    if (imm & 0x8) {
        la_vfcmp_cond_d(temp, src, src, 0x8);
    } else {
        la_vfrint_d(temp, src);
    }

    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_save, fcsr, 31, 0);
    la_ld_wu(mxcsr, env_ir2_opnd,
            lsenv_offset_of_mxcsr(lsenv));
    la_bstrins_w(fcsr, zero_ir2_opnd, 4, 0);
    if (imm & 0x4) {
        temp = ra_alloc_itemp();
        la_bstrpick_w(temp, mxcsr, 14, 13);
        IR2_OPND temp_int = ra_alloc_itemp_internal();
        la_andi(temp_int, temp, 0x1);
        IR2_OPND label1 = ra_alloc_label();
        la_beq(temp_int, zero_ir2_opnd, label1);
        la_xori(temp, temp, 0x2);
        la_label(label1);
        la_bstrins_w(fcsr, temp, 9, 8);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
        la_vfrint_d(temp_dest, src);
        la_xvinsve0_d(dest, temp_dest, 0);
        set_high128_xreg_to_zero_bd(dest);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

        ra_free_temp(temp);
        ra_free_temp(fcsr);
        ra_free_temp(fcsr_save);
        ra_free_temp(mxcsr);
        return true;
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
    if ((imm & 0x3) == 0x0) {
        la_vfrintrne_d(temp_dest, src);
    } else if ((imm & 0x3) == 0x1) {
        la_vfrintrm_d(temp_dest, src);
    } else if ((imm & 0x3) == 0x2) {
        la_vfrintrp_d(temp_dest, src);
    } else if ((imm & 0x3) == 0x3) {
        la_vfrintrz_d(temp_dest, src);
	}
    la_xvinsve0_d(dest, temp_dest, 0);
    set_high128_xreg_to_zero_bd(dest);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_insertps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND temp1 = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);
    uint8_t count_s = (imm >> 6) & 0x3;
    uint8_t count_d = (imm >> 4) & 0x3;
    uint8_t zmask = imm & 0xf;
    if (ir1_opnd_is_mem_bd(opnd1)) {
        count_s = 0;
    }
    la_vextrins_w(dest, src, VEXTRINS_IMM_4_0(count_d, count_s));

    la_vxor_v(temp1, temp1, temp1);
    if(zmask & 0x1)
        la_vextrins_w(dest, temp1, VEXTRINS_IMM_4_0(0, 0));
    if(zmask & 0x2)
        la_vextrins_w(dest, temp1, VEXTRINS_IMM_4_0(1, 0));
    if(zmask & 0x4)
        la_vextrins_w(dest, temp1, VEXTRINS_IMM_4_0(2, 0));
    if(zmask & 0x8)
        la_vextrins_w(dest, temp1, VEXTRINS_IMM_4_0(3, 0));
    return true;
}

bool translate_extractps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND temp1 = ra_alloc_ftemp();
    lsassert(ir1_opnd_is_gpr_bd(opnd0) || ir1_opnd_is_mem_bd(opnd0));

    uint8_t imm = ir1_opnd_uimm_bd(opnd2);
    uint8_t src_offset = imm & 0x3;

    la_vxor_v(temp1, temp1, temp1);
    la_vextrins_w(temp1, src, VEXTRINS_IMM_4_0(0, src_offset));

    IR2_OPND dest_opnd = ra_alloc_itemp();
    la_movfr2gr_s(dest_opnd, temp1);
    la_bstrpick_d(dest_opnd, dest_opnd, 31, 0);
    store_ireg_to_ir1_bd(dest_opnd, opnd0, false);
    return true;
}

/* sse 4.1 int */
bool translate_mpsadbw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    IR2_OPND temp3 = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    la_vreplvei_w(temp1, src, imm & 3);
    la_vmepatmsk_v(temp2, 1, imm & 0x4);
    la_vmepatmsk_v(temp3, 2, imm & 0x4);
    la_vshuf_b(temp2, dest, dest, temp2);
    la_vshuf_b(temp3, dest, dest, temp3);
    la_vabsd_bu(temp2, temp2, temp1);
    la_vabsd_bu(temp3, temp3, temp1);
    la_vhaddw_hu_bu(temp2, temp2, temp2);
    la_vhaddw_wu_hu(temp2, temp2, temp2);
    la_vhaddw_hu_bu(temp3, temp3, temp3);
    la_vhaddw_wu_hu(dest, temp3, temp3);
    la_vsrlni_h_w(dest, temp2, 0);
    return true;
}

bool translate_phminposuw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD * opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD * opnd1 = ir1_get_opnd_bd(pir1, 1);

    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    la_xvori_b(temp, src, 0x0);
    la_vsrli_d(temp, temp, 0x10);
    la_vmin_hu(temp, temp, src);
    la_vsrli_d(temp, temp, 0x10);
    la_vmin_hu(temp, temp, src);
    la_vsrli_d(temp, temp, 0x10);
    la_vmin_hu(temp, temp, src);

    la_xvpickve_d(temp2, temp, 1);
    la_vmin_hu(temp, temp, temp2);
    la_xvreplve0_h(temp, temp);
    la_vseq_h(temp1, src, temp);
    la_vfrstpi_h(temp2, temp1, 0);
    la_vpackev_h(dest, temp2, temp);
    la_xvpickve_w(dest, dest, 0);
    return true;
}

bool translate_pmulld_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vmul_w(dest, dest, src);
    return true;
}

bool translate_pmuldq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vmulwev_d_w(dest, dest, src);
    return true;
}

bool translate_pblendvb_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND mask = ra_alloc_xmm(0);
    IR2_OPND temp = ra_alloc_ftemp();

    la_vslti_b(temp, mask, 0);
    la_vbitsel_v(dest, dest, src, temp);
    return true;
}

bool translate_pblendw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    if (imm == 0xff) {
        la_vand_v(dest, src, src);
        return true;
    }
    /* 64 bit fast path */
    if ((imm & 0xf) == 0xf) {
        la_vextrins_d(dest, src, VEXTRINS_IMM_4_0(0, 0));
        imm &= ~0xf;
    }
    if ((imm & 0xf0) == 0xf0) {
        la_vextrins_d(dest, src, VEXTRINS_IMM_4_0(1, 1));
        imm &= ~0xf0;
    }

    /* 32 bit fast path */
    if ((imm & 0x3) == 0x3) {
        la_vextrins_w(dest, src, VEXTRINS_IMM_4_0(0, 0));
        imm &= ~0x3;
    }
    if ((imm & 0xc) == 0xc) {
        la_vextrins_w(dest, src, VEXTRINS_IMM_4_0(1, 1));
        imm &= ~0xc;
    }
    if ((imm & 0x30) == 0x30) {
        la_vextrins_w(dest, src, VEXTRINS_IMM_4_0(2, 2));
        imm &= ~0x30;
    }
    if ((imm & 0xc0) == 0xc0) {
        la_vextrins_w(dest, src, VEXTRINS_IMM_4_0(3, 3));
        imm &= ~0xc0;
    }

    /* 16 bit slow path */
    if (imm & 0x1)
        la_vextrins_h(dest, src, VEXTRINS_IMM_4_0(0, 0));
    if (imm & 0x2)
        la_vextrins_h(dest, src, VEXTRINS_IMM_4_0(1, 1));
    if (imm & 0x4)
        la_vextrins_h(dest, src, VEXTRINS_IMM_4_0(2, 2));
    if (imm & 0x8)
        la_vextrins_h(dest, src, VEXTRINS_IMM_4_0(3, 3));
    if (imm & 0x10)
        la_vextrins_h(dest, src, VEXTRINS_IMM_4_0(4, 4));
    if (imm & 0x20)
        la_vextrins_h(dest, src, VEXTRINS_IMM_4_0(5, 5));
    if (imm & 0x40)
        la_vextrins_h(dest, src, VEXTRINS_IMM_4_0(6, 6));
    if (imm & 0x80)
        la_vextrins_h(dest, src, VEXTRINS_IMM_4_0(7, 7));
    return true;
}

bool translate_pminsb_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    la_vmin_b(dest, dest, src);
    return true;
}

bool translate_pminuw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    la_vmin_hu(dest, dest, src);
    return true;
}

bool translate_pminsd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    la_vmin_w(dest, dest, src);
    return true;
}

bool translate_pminud_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    la_vmin_wu(dest, dest, src);
    return true;
}

bool translate_pmaxsb_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    la_vmax_b(dest, dest, src);
    return true;
}

bool translate_pmaxuw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    la_vmax_hu(dest, dest, src);
    return true;
}

bool translate_pmaxsd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    la_vmax_w(dest, dest, src);
    return true;
}

bool translate_pmaxud_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    la_vmax_wu(dest, dest, src);
    return true;
}

bool translate_pextrb_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);

    IR2_OPND dest;
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    if (ir1_opnd_is_gpr_bd(opnd0)) {
        dest = ra_alloc_gpr(ir1_opnd_base_reg_num_bd(opnd0));
        la_vpickve2gr_bu(dest, src, imm);
    } else {
        dest = ra_alloc_itemp();
        la_vpickve2gr_bu(dest, src, imm);
        store_ireg_to_ir1_bd(dest, opnd0, false);
    }

    return true;
}

bool translate_pextrd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);

    IR2_OPND dest;
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    if (ir1_opnd_is_gpr_bd(opnd0)) {
        dest = ra_alloc_gpr(ir1_opnd_base_reg_num_bd(opnd0));
        la_vpickve2gr_wu(dest, src, imm);
    } else {
        dest = ra_alloc_itemp();
        la_vpickve2gr_wu(dest, src, imm);
        store_ireg_to_ir1_bd(dest, opnd0, false);
    }

    return true;
}

bool translate_pextrq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);

    IR2_OPND dest;
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    if (ir1_opnd_is_gpr_bd(opnd0)) {
        dest = ra_alloc_gpr(ir1_opnd_base_reg_num_bd(opnd0));
        la_vpickve2gr_d(dest, src, imm);
    } else {
        dest = ra_alloc_itemp();
        la_vpickve2gr_d(dest, src, imm);
        store_ireg_to_ir1_bd(dest, opnd0, false);
    }

    return true;
}

bool translate_pinsrb_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);

    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_ireg_from_ir1_bd(opnd1, UNKNOWN_EXTENSION, false);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    la_vinsgr2vr_b(dest, src, imm);

    return true;
}

bool translate_pinsrd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);

    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_ireg_from_ir1_bd(opnd1, UNKNOWN_EXTENSION, false);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    la_vinsgr2vr_w(dest, src, imm);

    return true;
}

bool translate_pinsrq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);

    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_ireg_from_ir1_bd(opnd1, UNKNOWN_EXTENSION, false);
    uint8_t imm = ir1_opnd_uimm_bd(opnd2);

    la_vinsgr2vr_d(dest, src, imm);

    return true;
}

bool translate_pmovsxbw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_h_b(dest, src);
    return true;
}

bool translate_pmovzxbw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_hu_bu(dest, src);
    return true;
}

bool translate_pmovsxbd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_w_b(dest, src);
    return true;
}

bool translate_pmovzxbd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_wu_bu(dest, src);
    return true;
}

bool translate_pmovsxbq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_d_b(dest, src);
    return true;
}

bool translate_pmovzxbq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_du_bu(dest, src);
    return true;
}

bool translate_pmovsxwd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_w_h(dest, src);
    return true;
}

bool translate_pmovzxwd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_wu_hu(dest, src);
    return true;
}

bool translate_pmovsxwq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_d_h(dest, src);
    return true;
}

bool translate_pmovzxwq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_du_hu(dest, src);
    return true;
}

bool translate_pmovsxdq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_d_w(dest, src);
    return true;
}

bool translate_pmovzxdq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);

    la_vext2xv_du_wu(dest, src);
    return true;
}

bool translate_ptest_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND label_1 = ra_alloc_label();
    IR2_OPND label_2 = ra_alloc_label();
    IR2_OPND n4095_opnd = ra_alloc_num_4095();

    la_x86mtflag(zero_ir2_opnd, 0x3f);
    la_vand_v(temp, src, dest);
    la_vseteqz_v(fcc0_ir2_opnd, temp);
    la_bceqz(fcc0_ir2_opnd, label_1);
    la_x86mtflag(n4095_opnd, ZF_USEDEF_BIT);

    la_label(label_1);
    la_vandn_v(temp, dest, src);
    la_vseteqz_v(fcc0_ir2_opnd, temp);
    la_bceqz(fcc0_ir2_opnd, label_2);
    la_x86mtflag(n4095_opnd, CF_USEDEF_BIT);
    la_label(label_2);

    ra_free_num_4095(n4095_opnd);
    return true;
}

bool translate_pcmpeqq_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vseq_d(dest, dest, src);
    return true;
}

bool translate_packusdw_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(opnd0));
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    IR2_OPND temp = ra_alloc_ftemp();
    if (ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) &&
        (ir1_opnd_base_reg_num_bd(opnd0) == ir1_opnd_base_reg_num_bd(opnd1))) {
        la_vslti_w(temp, dest, 0);
        la_vandn_v(dest, temp, dest);
        la_vssrani_hu_w(dest, dest, 0);
        la_vextrins_d(dest, dest, VEXTRINS_IMM_4_0(1, 0));
    } else {
        la_vslti_w(temp, dest, 0);
        la_vandn_v(dest, temp, dest);
        la_vssrani_hu_w(dest, dest, 0);

        IR2_OPND temp1 = ra_alloc_ftemp();
        la_vslti_w(temp1, src, 0);
        la_vandn_v(temp1, temp1, src);
        la_vssrani_hu_w(temp1, temp1, 0);

        /* src packed to high part */
        la_vextrins_d(dest, temp1, VEXTRINS_IMM_4_0(1, 0));
    }
    return true;
}


bool translate_movntdqa_bd(IR1_INST *pir1)
{
    translate_movaps_bd(pir1);
    return true;
}

bool translate_andn_bd(IR1_INST *pir1)
{
    IR2_OPND dest = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 0),
        UNKNOWN_EXTENSION, false);
    IR2_OPND src0 = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1),
        UNKNOWN_EXTENSION, false);
    IR2_OPND src1 = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 2),
        UNKNOWN_EXTENSION, false);
    int opnd0_size = ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND temp = ra_alloc_itemp();
    la_orn(temp, zero_ir2_opnd, src0);
    generate_eflag_calculation_bd(dest, temp, src1, pir1, false);
    la_andn(temp, src1, src0);
    if (opnd0_size == 32) {
        la_bstrpick_d(dest, temp, 31, 0);
    } else {
        la_ori(dest, temp, 0);
    }

    return true;
}

bool translate_movbe_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    bool is_lock = ir1_is_prefix_lock_bd(pir1) && ir1_opnd_is_mem_bd(opnd0);

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        lsassert(0);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;

    opnd0_size = ir1_opnd_size_bd(opnd0);

    /* get src1 */
    src = load_ireg_from_ir1_bd(opnd1, UNKNOWN_EXTENSION, false);

    /* get src0 */
    if (ir1_opnd_is_gpr_bd(opnd0)) {
        dest = convert_gpr_opnd_bd(opnd0, UNKNOWN_EXTENSION);
    } else {
        dest = ra_alloc_itemp();
        mem_opnd = convert_mem_bd(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(dest, mem_opnd, imm, opnd0_size);
    }

    /* set eflag */
    generate_eflag_calculation_bd(dest, dest, src, pir1, true);

    /* calculate */
    IR2_OPND temp = ra_alloc_itemp();
    if (opnd0_size == 16) {
        la_revb_2h(temp, src);
    } else if (opnd0_size == 32) {
        la_revb_2w(temp, src);
    } else if (opnd0_size == 64) {
        la_revb_d(temp, src);
    } else {
        lsassert(0);
    }

    /* write back */
    if (ir1_opnd_is_gpr_bd(opnd0)) {
        if (opnd0_size == 32) {
            la_bstrpick_d(dest, temp, opnd0_size - 1, 0);
        } else {
            la_bstrins_d(dest, temp, opnd0_size - 1, 0);
        }
    } else {
        la_st_by_op_size(temp, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

bool translate_pcmpestri_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    int imm = ir1_opnd_uimm_bd(opnd2);
#ifdef TARGET_X86_64
    /* Presence of REX.W is indicated by bit 8*/
    if (CODEIS64) {
        imm |= ir1_rex_w_bd(pir1) << 8;
    }
#endif
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        int s = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpestri_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 8);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem_bd(src, opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpestri_xmm, d, (d + 1) % 8, imm);
        la_xvor_v(src, temp, temp);
    }
    /* TODO:fix eflags and mem opnd */
    return true;
}

bool translate_pcmpestrm_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    int imm = ir1_opnd_uimm_bd(opnd2);
#ifdef TARGET_X86_64
    /* Presence of REX.W is indicated by bit 8*/
    if (CODEIS64) {
        imm |= ir1_rex_w_bd(pir1) << 8;
    }
#endif
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        int s = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpestrm_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem_bd(src, opnd1);
         tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpestrm_xmm, d,
                                                        (d + 1) % 7 + 1, imm);
        la_xvor_v(src, temp, temp);
    }
    /* TODO:fix eflags and mem opnd */
    return true;
}

bool translate_pcmpistri_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    int imm = ir1_opnd_uimm_bd(opnd2);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        int s = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpistri_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 8);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem_bd(src, opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpistri_xmm, d, (d + 1) % 8, imm);
        la_xvor_v(src, temp, temp);
    }
    /* TODO:fix eflags and mem opnd */
    return true;
}

bool translate_pcmpistrm_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    int imm = ir1_opnd_uimm_bd(opnd2);
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        int s = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpistrm_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem_bd(src, opnd1);
         tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpistrm_xmm, d,
                                                        (d + 1) % 7 + 1, imm);
        la_xvor_v(src, temp, temp);
    }
    /* TODO:fix eflags and mem opnd */
    return true;
}

bool translate_pclmulqdq_bd(IR1_INST * pir1) {
    IR1_OPND_BD * opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD * opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD * opnd2 = ir1_get_opnd_bd(pir1, 2);

    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND temp = ra_alloc_ftemp();
    la_xvori_b(temp,dest,0x0);
    int s0 = ir1_opnd_base_reg_num_bd(opnd0);
    uint8_t ctrl = ir1_opnd_uimm_bd(opnd2);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pclmulqdq_xmm, s0, s1, ctrl);
    } else {
        int s1 = 0;
        while (s1 < 8) {
            if (s1 != s0) {
                break;
            }
            s1++;
        }
        IR2_OPND temp_mem = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp_mem, src, src);
        load_freg128_from_ir1_mem_bd(src, opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pclmulqdq_xmm, s0, s1, ctrl);
        la_xvor_v(src, temp_mem, temp_mem);
    }
    return true;
}

bool translate_aesdec_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_aesdec_xmm, d, s1, 0);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_aesdec_xmm, d, s1, 0);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}

bool translate_aesdeclast_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_aesdeclast_xmm, d, s1, 0);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_aesdeclast_xmm, d, s1, 0);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}

bool translate_aesenc_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_aesenc_xmm, d, s1, 0);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_aesenc_xmm, d, s1, 0);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}

bool translate_aesenclast_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_aesenclast_xmm, d, s1, 0);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_aesenclast_xmm, d, s1, 0);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}

bool translate_aesimc_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        int s = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aesimc_xmm, d, s, 0);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem_bd(src, opnd1);
         tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aesimc_xmm, d,
                                                    (d + 1) % 7 + 1, 0);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: IMM 0 do not need to save */
    return true;
}

bool translate_aeskeygenassist_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    int imm = ir1_opnd_uimm_bd(opnd2);
    if (ir1_opnd_is_xmm_bd(opnd1)) {
        int s = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aeskeygenassist_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem_bd(src, opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aeskeygenassist_xmm, d,
                                                            (d + 1) % 7 + 1, imm);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}
