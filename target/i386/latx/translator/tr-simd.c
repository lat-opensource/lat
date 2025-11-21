#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"
#include "hbr.h"

bool translate_por(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vor_v(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vor_v(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pxor(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_mem(src)) {
        IR2_OPND temp = ra_alloc_ftemp();
        load_freg128_from_ir1_mem(temp, src);
        la_vxor_v(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                  ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), temp);
        return true;
    } else if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_xmm(src)) {
        la_vxor_v(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                  ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                  ra_alloc_xmm(ir1_opnd_base_reg_num(src)));
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vxor_v(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_packsswb(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)) &&
            (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) ==
             ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1)))) {
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
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vpackev_d(dest, src, dest);
        //la_vssrani_b_h(dest, dest, 0);
        la_vsat_h(dest, dest, 7);
        la_vpickev_b(dest, dest, dest);
    }
    return true;
}

bool translate_packssdw(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)) &&
            (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) ==
             ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1)))) {
            la_vssrani_h_w(dest, dest, 0);
        } else {
            la_vssrani_h_w(dest, dest, 0);
            IR2_OPND temp = ra_alloc_ftemp();
            la_vssrani_h_w(temp, src, 0);
            la_vextrins_d(dest, temp, VEXTRINS_IMM_4_0(1, 0));
        }
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vpackev_d(dest, src, dest);
        la_vssrani_h_w(dest, dest, 0);
    }
    return true;
}

bool translate_packuswb(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)) &&
            (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) ==
             ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1)))) {
            la_vssrani_bu_h(dest, dest, 0);
            la_vextrins_d(dest, dest, VEXTRINS_IMM_4_0(1, 0));
        } else {
            la_vssrani_bu_h(dest, dest, 0);
            IR2_OPND temp = ra_alloc_ftemp();
            la_vssrani_bu_h(temp, src, 0);
            la_vextrins_d(dest, temp, VEXTRINS_IMM_4_0(1, 0));
        }
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vpackev_d(dest, src, dest);
        la_vssrani_bu_h(dest, dest, 0);
    }
    return true;
}

bool translate_paddb(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vadd_b(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vadd_b(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vadd_h(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vadd_h(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vadd_w(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vadd_w(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddsb(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vsadd_b(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vsadd_b(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddsw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vsadd_h(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vsadd_h(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddusb(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vsadd_bu(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vsadd_bu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_paddusw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vsadd_hu(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vsadd_hu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pand(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vand_v(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vand_v(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pandn(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vandn_v(dest, dest, src);
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vandn_v(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pmaddwd(IR1_INST *pir1)
{
    IR2_OPND temp = ra_alloc_ftemp();
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vxor_v(temp, temp, temp);
        la_vmaddwev_w_h(temp, dest, src);
        la_vmaddwod_w_h(temp, dest, src);
        la_vbsll_v(dest, temp, 0);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest_lo =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src_lo =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vxor_v(temp, temp, temp);
        la_vmaddwev_w_h(temp, dest_lo, src_lo);
        la_vmaddwod_w_h(temp, dest_lo, src_lo);
        la_vbsll_v(dest_lo, temp, 0);
    }
    return true;
}

bool translate_pmulhuw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vmuh_hu(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vmuh_hu(dest, dest, src);
    }
    return true;
}

bool translate_pmulhw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vmuh_h(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vmuh_h(dest, dest, src);
    }
    return true;
}

bool translate_pmullw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vmul_h(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vmul_h(dest, dest, src);
    }
    return true;
}

bool translate_psubb(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vsub_b(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vsub_b(dest, dest, src);
    }
    return true;
}

bool translate_psubw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vsub_h(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vsub_h(dest, dest, src);
    }
    return true;
}

bool translate_psubd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vsub_w(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vsub_w(dest, dest, src);
    }
    return true;
}

bool translate_psubsb(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vssub_b(dest, dest, src);
        return true;
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vssub_b(dest, dest, src);
    }
    return true;
}

bool translate_psubsw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vssub_h(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vssub_h(dest, dest, src);
    }
    return true;
}

bool translate_psubusb(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vssub_bu(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vssub_bu(dest, dest, src);
    }
    return true;
}

bool translate_psubusw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vssub_hu(dest, dest, src);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vssub_hu(dest, dest, src);
    }
    return true;
}

bool translate_punpckhbw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vilvh_b(dest, src, dest);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vilvl_b(dest, src, dest);
        la_vextrins_d(dest, dest, VEXTRINS_IMM_4_0(0, 1));
    }
    return true;
}

bool translate_punpckhwd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vilvh_h(dest, src, dest);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vilvl_h(dest, src, dest);
        la_vextrins_d(dest, dest, VEXTRINS_IMM_4_0(0, 1));
    }
    return true;
}

bool translate_punpckhdq(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vilvh_w(dest, src, dest);
    } else {
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vilvl_w(dest, src, dest);
        la_vextrins_d(dest, dest, VEXTRINS_IMM_4_0(0, 1));
    }
    return true;
}

bool translate_punpcklbw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vilvl_b(dest, src, dest);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vilvl_b(dest, src, dest);
    }
    return true;
}

bool translate_punpcklwd(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vilvl_h(dest, src, dest);
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vilvl_h(dest, src, dest);
    }
    return true;
}

bool translate_punpckldq(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vilvl_w(dest, src, dest);
        return true;
    } else { //mmx
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vilvl_w(dest, src, dest);
    }
    return true;
}

bool translate_addps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfadd_s(dest, dest, src);
    return true;
}

bool translate_addsd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    if (SHBR_ON_64(pir1)) {
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

bool translate_addss(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    if (SHBR_ON_32(pir1)) {
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

bool translate_andnpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vandn_v(dest, dest, src);
    return true;
}

bool translate_andnps(IR1_INST *pir1)
{
    translate_andnpd(pir1);
    return true;
}

bool translate_andps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vand_v(dest, dest, src);
    return true;
}

bool translate_divpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfdiv_d(dest, dest, src);
    return true;
}

bool translate_divps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfdiv_s(dest, dest, src);
    return true;
}

bool translate_divsd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    if (SHBR_ON_64(pir1)) {
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

bool translate_divss(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    if (SHBR_ON_32(pir1)) {
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
bool translate_maxpd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    if (ir1_opnd_is_xmm(opnd1)) {
        if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND mask = ra_alloc_ftemp();
    la_vfcmp_cond_d(mask, dest, src, 0xF); // equal, unorder and dest < src, use src
    la_vbitsel_v(dest, dest, src, mask);
    return true;
}

bool translate_maxps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    if (ir1_opnd_is_xmm(opnd1)) {
        if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND mask = ra_alloc_ftemp();
    la_vfcmp_cond_s(mask, dest, src, 0xF); // equal, unorder and dest < src, use src
    la_vbitsel_v(dest, dest, src, mask);
    return true;
}

bool translate_maxsd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    if (ir1_opnd_is_xmm(opnd1)) {
        if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    la_fcmp_cond_d(fcc0_ir2_opnd, src, dest, 0x3);
    if (SHBR_ON_64(pir1)) {
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

bool translate_maxss(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    if (ir1_opnd_is_xmm(opnd1)) {
        if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    la_fcmp_cond_s(fcc0_ir2_opnd, src, dest, 0x3);
    if (SHBR_ON_32(pir1)) {
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

bool translate_minpd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    if (ir1_opnd_is_xmm(opnd1)) {
        if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
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

bool translate_minps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    if (ir1_opnd_is_xmm(opnd1)) {
        if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND mask = ra_alloc_ftemp();
    // as MINPD
    la_vfcmp_cond_s(mask, src, dest, 0xF);
    la_vbitsel_v(dest, dest, src, mask);
    return true;
}

bool translate_minsd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    if (ir1_opnd_is_xmm(opnd1)) {
        if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    la_fcmp_cond_d(fcc0_ir2_opnd, dest, src, 0x3);
    if (SHBR_ON_64(pir1)) {
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

bool translate_minss(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    if (ir1_opnd_is_xmm(opnd1)) {
        if (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1)) {
            return true;
        }
    }
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    la_fcmp_cond_s(fcc0_ir2_opnd, dest, src, 0x3);
    if (SHBR_ON_32(pir1)) {
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

bool translate_mulpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfmul_d(dest, dest, src);
    return true;
}

bool translate_mulps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfmul_s(dest, dest, src);
    return true;
}

bool translate_mulsd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    if (SHBR_ON_64(pir1)) {
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

bool translate_mulss(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    if (SHBR_ON_32(pir1)) {
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

bool translate_orpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vor_v(dest, dest, src);
    return true;
}

bool translate_orps(IR1_INST *pir1)
{
    translate_orpd(pir1);
    return true;
}

bool translate_paddq(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vadd_d(dest, dest, src);
        return true;
    }
    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vadd_d(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pavgb(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vavgr_bu(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vavgr_bu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pavgw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vavgr_hu(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vavgr_hu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pextrw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(opnd1)) {
        uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
        imm &= 7;
        IR2_OPND dest_opnd;
        if(ir1_opnd_is_mem(opnd0)) {
            dest_opnd = ra_alloc_itemp();
        } else {
            /* [MSB:16] will be zeroed, so we simply get gpr */
            dest_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
        }
        la_vpickve2gr_hu(dest_opnd,
            ra_alloc_xmm(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1))), imm);
        if(ir1_opnd_is_mem(opnd0)) {
            store_ireg_to_ir1(dest_opnd, ir1_get_opnd(pir1, 0), false);
            ra_free_temp(dest_opnd);
        }
        return true;
    }
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 0) + 2);
    IR2_OPND src_lo = load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 1, UNKNOWN_EXTENSION,
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
    store_ireg_to_ir1(src_lo, ir1_get_opnd(pir1, 0), false); /* fill default */
                                                   /* parameter */
    return true;
}

bool translate_pinsrw(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))) {
        uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
        imm &= 7;
        IR2_OPND src = load_ireg_from_ir1(ir1_get_opnd(pir1, 1),
                                          UNKNOWN_EXTENSION, false);
        la_vinsgr2vr_h(ra_alloc_xmm(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0))),
                       src, imm);
        return true;
    }
    IR2_OPND src = load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 1, UNKNOWN_EXTENSION,
                                      false); /* fill default parameter */
    IR2_OPND ftemp = ra_alloc_ftemp();
    la_movgr2fr_d(ftemp, src);
    uint8 imm8 = ir1_opnd_uimm(ir1_get_opnd(pir1, 0) + 2);

    IR2_OPND dest =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
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

bool translate_pmaxsw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vmax_h(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    if (ir2_opnd_cmp(&dest_lo, &src_lo))
        return true;
    else
        la_vmax_h(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pmaxub(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vmax_bu(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    if (ir2_opnd_cmp(&dest_lo, &src_lo))
        return true;
    else
        la_vmax_bu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pminsw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vmin_h(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    if (ir2_opnd_cmp(&dest_lo, &src_lo))
        return true;
    else
        la_vmin_h(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pminub(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vmin_bu(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    if (ir2_opnd_cmp(&dest_lo, &src_lo))
        return true;
    else
        la_vmin_bu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_pmuludq(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vmulwev_d_wu(dest, dest, src);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vmulwev_d_wu(dest_lo, dest_lo, src_lo);
    return true;
}

bool translate_psadbw(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vabsd_bu(dest, dest, src);
        la_vhaddw_hu_bu(dest, dest, dest);
        la_vhaddw_wu_hu(dest, dest, dest);
        la_vhaddw_du_wu(dest, dest, dest);
        return true;
    }
    IR2_OPND dest_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    la_vabsd_bu(dest_lo, dest_lo, src_lo);
    la_vhaddw_hu_bu(dest_lo, dest_lo, dest_lo);
    la_vhaddw_wu_hu(dest_lo, dest_lo, dest_lo);
    la_vhaddw_du_wu(dest_lo, dest_lo, dest_lo);
    return true;
}

bool translate_pshufd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    uint64_t imm8 = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
    la_vshuf4i_w(dest, src, imm8);
    return true;
}

bool translate_pshufw(IR1_INST *pir1)
{
    IR2_OPND dest =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    IR1_OPND *imm8_reg = ir1_get_opnd(pir1, 2);
    uint64_t imm8 = ir1_opnd_uimm(imm8_reg);
    la_vshuf4i_h(dest, src, imm8);
    return true;
}

bool translate_pshufhw(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR1_OPND *dest_reg = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_reg = ir1_get_opnd(pir1, 1);
    IR1_OPND *imm8_reg = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = load_freg128_from_ir1(dest_reg);
    IR2_OPND src = load_freg128_from_ir1(src_reg);
    IR2_OPND temp = ra_alloc_ftemp();

    uint64_t imm8 = ir1_opnd_uimm(imm8_reg);
    if (ir1_opnd_is_mem(ir1_get_opnd(pir1, 1)) ||
        (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) !=
         ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1)))) {
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

bool translate_pshuflw(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp = ra_alloc_ftemp();
    uint64_t imm8 = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
    if (ir1_opnd_is_mem(ir1_get_opnd(pir1, 1)) ||
        (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) !=
         ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1)))) {
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

bool translate_psubq(IR1_INST *pir1)
{
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_vsub_d(dest, dest, src);
        return true;
    } else {
        /* transfer_to_mmx_mode */
        transfer_to_mmx_mode();

        IR2_OPND dest_lo =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
        IR2_OPND src_lo =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
        la_vsub_d(dest_lo, dest_lo, src_lo);
    }
    return true;
}

bool translate_punpckhqdq(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vilvh_d(dest, src, dest);
    return true;
}

bool translate_rcpss(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp = ra_alloc_ftemp();
    la_vfrecip_s(temp, src);
    if (option_enable_lasx) {
        la_xvinsve0_w(dest, temp, 0);
    } else {
        la_vextrins_w(dest, temp, 0);
    }
    return true;
}

bool translate_rcpps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfrecip_s(dest, src);
    return true;
}

bool translate_rsqrtss(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp = ra_alloc_ftemp();
    la_frsqrt_s(temp, src);
    if (option_enable_lasx) {
        la_xvinsve0_w(dest, temp, 0);
    } else {
        la_vextrins_w(dest, temp, 0);
    }
    return true;
}

bool translate_rsqrtps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfrsqrt_s(dest, src);
    return true;
}

bool translate_sqrtpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));

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

bool translate_sqrtps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));

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

bool translate_addpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfadd_d(dest, dest, src);
    return true;
}

bool translate_andpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vand_v(dest, dest, src);
    return true;
}

bool translate_unpcklps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vilvl_w(dest, src, dest);
    return true;
}

bool translate_unpcklpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vshuf4i_d(dest, src, 0x8);
    return true;
}

bool translate_unpckhpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vshuf4i_d(dest, src, 0xd);
    return true;
}

bool translate_unpckhps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vilvh_w(dest, src, dest);
    return true;
}

bool translate_shufps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    uint64_t imm8 = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    la_vshuf4i_w(temp1, dest, imm8);
    la_vshuf4i_w(temp2, src, imm8 >> 4);
    la_vpickev_d(dest , temp2, temp1);
    return true;
}

bool translate_shufpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    uint8_t imm8 = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
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

bool translate_punpcklqdq(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vilvl_d(dest, src, dest);
    return true;
}

bool translate_xorps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vxor_v(dest, dest, src);
    return true;
}

bool translate_xorpd(IR1_INST *pir1)
{
    translate_xorps(pir1);
    return true;
}

bool translate_subss(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    if (SHBR_ON_32(pir1)) {
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

bool translate_subsd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    if (SHBR_ON_64(pir1)) {
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

bool translate_subps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfsub_s(dest, dest, src);
    return true;
}

bool translate_subpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vfsub_d(dest, dest, src);
    return true;
}

bool translate_sqrtsd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    if (SHBR_ON_64(pir1)) {
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

bool translate_sqrtss(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    if (SHBR_ON_32(pir1)) {
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

bool translate_pause(IR1_INST *pir1) { return true; }

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
bool translate_haddpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    la_vpickev_d(temp1, src, dest);
    la_vpickod_d(temp2, src, dest);
    la_vfadd_d(dest, temp1, temp2);

    return true;
}

bool translate_haddps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
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

bool translate_hsubpd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    la_vpickev_d(temp1, src, dest);
    la_vpickod_d(temp2, src, dest);
    la_vfsub_d(dest, temp1, temp2);
    return true;
}

bool translate_hsubps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    la_vpickev_w(temp1, src, dest);
    la_vpickod_w(temp2, src, dest);
    la_vfsub_s(dest, temp1, temp2);
    return true;
}

/* ssse3 */
bool translate_psignb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vsigncov_b(dest, src, dest);

    return true;
}

bool translate_psignw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vsigncov_h(dest, src, dest);

    return true;
}

bool translate_psignd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vsigncov_w(dest, src, dest);

    return true;
}

bool translate_pabsb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND vzero = ra_alloc_ftemp();

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vreplgr2vr_d(vzero, zero_ir2_opnd);
    la_vabsd_b(dest, src, vzero);

    return true;
}

bool translate_pabsw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND vzero = ra_alloc_ftemp();

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vreplgr2vr_d(vzero, zero_ir2_opnd);
    la_vabsd_h(dest, src, vzero);

    return true;
}

bool translate_pabsd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND vzero = ra_alloc_ftemp();

    IR2_OPND dest;
    IR2_OPND src;

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vreplgr2vr_d(vzero, zero_ir2_opnd);
    la_vabsd_w(dest, src, vzero);

    return true;
}

bool translate_palignr(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm(opnd2);

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        /* fast path */
        if (imm >= 32) {
            la_vxor_v(dest, dest, dest);
        } else if (imm >= 16 && imm < 32) {
            la_vbsrl_v(dest, dest, imm - 16);
        } else {
            /* slow path */
            src = load_freg128_from_ir1(opnd1);
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
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        /* fast path */
        if (imm >= 16) {
            la_vxor_v(dest, dest, dest);
        } else if (imm >= 8 && imm < 16) {
            la_vinsgr2vr_d(dest, zero_ir2_opnd, 0x1);
            la_vbsrl_v(dest, dest, imm - 8);
        } else {
            /* slow path */
            src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
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

bool translate_pshufb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND vzero = ra_alloc_ftemp();
    IR2_OPND src_t1 = ra_alloc_ftemp();
    IR2_OPND src_t2 = ra_alloc_ftemp();

    la_vreplgr2vr_d(vzero, zero_ir2_opnd);

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
        la_vandi_b(src_t2, src, 0xf);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
        la_vandi_b(src_t2, src, 0x7);
    }

    la_vandi_b(src_t1, src, 0x80);
    la_vsrli_b(src_t1, src_t1, 3);
    la_vadd_b(src_t1, src_t2, src_t1);
    la_vshuf_b(dest, vzero, dest, src_t1);

    return true;
}

bool translate_pmulhrsw(IR1_INST *pir1)
{
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    if(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))){
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
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
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, IS_INTEGER);
    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
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

bool translate_pmaddubsw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    IR2_OPND temp3 = ra_alloc_ftemp();
    IR2_OPND temp4 = ra_alloc_ftemp();
    IR2_OPND temp5 = ra_alloc_ftemp();
    IR2_OPND itmp = ra_alloc_itemp();

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
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

bool translate_phsubw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vpickev_h(temp1, src, dest);
    la_vpickod_h(temp2, src, dest);
    la_vsub_h(dest, temp1, temp2);
    if (!ir1_opnd_is_xmm(opnd0)) {
        la_vshuf4i_w(dest, dest, 0xd8);
    }
    return true;
}

bool translate_phsubd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vpickev_w(temp1, src, dest);
    la_vpickod_w(temp2, src, dest);
    la_vsub_w(dest, temp1, temp2);
    if (!ir1_opnd_is_xmm(opnd0)) {
        la_vshuf4i_w(dest, dest, 0xd8);
    }
    return true;
}

bool translate_phsubsw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vpickev_h(temp1, src, dest);
    la_vpickod_h(temp2, src, dest);
    la_vssub_h(dest, temp1, temp2);
    if (!ir1_opnd_is_xmm(opnd0)) {
        la_vshuf4i_w(dest, dest, 0xd8);
    }
    return true;
}

bool translate_phaddw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vpickev_h(temp1, src, dest);
    la_vpickod_h(temp2, src, dest);
    la_vadd_h(dest, temp1, temp2);
    if (!ir1_opnd_is_xmm(opnd0)) {
        la_vshuf4i_w(dest, dest, 0xd8);
    }
    return true;
}

bool translate_phaddd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vpickev_w(temp1, src, dest);
    la_vpickod_w(temp2, src, dest);
    la_vadd_w(dest, temp1, temp2);
    if (!ir1_opnd_is_xmm(opnd0)) {
        la_vshuf4i_w(dest, dest, 0xd8);
    }
    return true;
    return false;
}

bool translate_phaddsw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest;
    IR2_OPND src;
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (ir1_opnd_is_xmm(opnd0)) {
        /* xmm */
        dest = load_freg128_from_ir1(opnd0);
        src = load_freg128_from_ir1(opnd1);
    } else {
        /* mmx */
        dest = load_freg_from_ir1_1(opnd0, false, IS_INTEGER);
        src = load_freg_from_ir1_1(opnd1, false, IS_INTEGER);
    }

    la_vpickev_h(temp1, src, dest);
    la_vpickod_h(temp2, src, dest);
    la_vsadd_h(temp2, temp1, temp2);
    if (!ir1_opnd_is_xmm(opnd0)) {
        la_vshuf4i_w(temp2, temp2, 0xd8);
    }
    la_vori_b(dest, temp2, 0);
    return true;
}


/* sse 4.1 fp */
bool translate_dpps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0) || ir1_opnd_is_ymm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm(opnd2);
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
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_dppd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm(opnd2);

    if (option_enable_lasx) {
        la_xvxor_v(temp1, temp1, temp1);
        la_xvxor_v(temp2, temp2, temp2);
    } else {
        la_vxor_v(temp1, temp1, temp1);
        la_vxor_v(temp2, temp2, temp2);
    }
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
    if (option_enable_lasx) {
        la_xvxor_v(dest, dest, dest);
    } else {
        la_vxor_v(dest, dest, dest);
    }
    if(imm & 0x1){
        if (option_enable_lasx) {
            la_xvextrins_d(dest, temp1, 0x00);
        } else {
            la_vextrins_d(dest, temp1, 0x00);
        }
    }
    if(imm & 0x2){
        if (option_enable_lasx) {
            la_xvextrins_d(dest, temp1, 0x11);
        } else {
            la_vextrins_d(dest, temp1, 0x11);
        }
    }
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_blendps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);
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

bool translate_blendpd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);
    if(imm & 0x1)
        la_vextrins_d(dest, src, VEXTRINS_IMM_4_0(0, 0));
    if(imm & 0x2)
        la_vextrins_d(dest, src, VEXTRINS_IMM_4_0(1, 1));
    return true;
}

bool translate_blendvps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND mask = ra_alloc_xmm(0);
    IR2_OPND temp = ra_alloc_ftemp();
    la_vslti_w(temp, mask, 0);
    la_vbitsel_v(dest, dest, src, temp);
    return true;
}

bool translate_blendvpd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND mask = ra_alloc_xmm(0);
    IR2_OPND temp = ra_alloc_ftemp();
    la_vslti_d(temp, mask, 0);
    la_vbitsel_v(dest, dest, src, temp);
    return true;
}

bool translate_roundps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);

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
        set_high128_xreg_to_zero(dest);
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
    set_high128_xreg_to_zero(dest);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_roundss(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();
    IR2_OPND src = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();
    if (option_enable_lasx) {
        la_xvreplve0_w(src, src1);
    } else {
        la_vreplvei_w(src, src1, 0);
    }

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
        if (option_enable_lasx) {
            la_xvinsve0_w(dest, temp_dest, 0);
        } else {
            la_vextrins_w(dest, temp_dest, 0);
        }
        set_high128_xreg_to_zero(dest);
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
    if (option_enable_lasx) {
        la_xvinsve0_w(dest, temp_dest, 0);
    } else {
        la_vextrins_w(dest, temp_dest, 0);
    }
    set_high128_xreg_to_zero(dest);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_roundpd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);

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
        set_high128_xreg_to_zero(dest);
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
    set_high128_xreg_to_zero(dest);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_roundsd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();
    IR2_OPND src = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();

    if (option_enable_lasx) {
        la_xvreplve0_d(src, src1);
    } else {
        la_vreplvei_d(src, src1, 0);
    }
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
        if (option_enable_lasx) {
            la_xvinsve0_d(dest, temp_dest, 0);
        } else {
            la_vextrins_d(dest, temp_dest, 0);
        }
        set_high128_xreg_to_zero(dest);
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
    if (option_enable_lasx) {
        la_xvinsve0_d(dest, temp_dest, 0);
    } else {
        la_vextrins_d(dest, temp_dest, 0);
    }
    set_high128_xreg_to_zero(dest);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_insertps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND temp1 = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm(opnd2);
    uint8_t count_s = (imm >> 6) & 0x3;
    uint8_t count_d = (imm >> 4) & 0x3;
    uint8_t zmask = imm & 0xf;
    if (ir1_opnd_is_mem(opnd1)) {
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

bool translate_extractps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND temp1 = ra_alloc_ftemp();
    lsassert(ir1_opnd_is_gpr(opnd0) || ir1_opnd_is_mem(opnd0));

    uint8_t imm = ir1_opnd_uimm(opnd2);
    uint8_t src_offset = imm & 0x3;

    la_vxor_v(temp1, temp1, temp1);
    la_vextrins_w(temp1, src, VEXTRINS_IMM_4_0(0, src_offset));

    IR2_OPND dest_opnd = ra_alloc_itemp();
    la_movfr2gr_s(dest_opnd, temp1);
    la_bstrpick_d(dest_opnd, dest_opnd, 31, 0);
    store_ireg_to_ir1(dest_opnd, opnd0, false);
    return true;
}

/* sse 4.1 int */
bool translate_mpsadbw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    IR2_OPND temp3 = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm(opnd2);

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

bool translate_phminposuw(IR1_INST *pir1)
{
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    if (option_enable_lasx) {
        la_xvori_b(temp, src, 0x0);
    } else {
        la_vori_b(temp, src, 0x0);
    }
    la_vsrli_d(temp, temp, 0x10);
    la_vmin_hu(temp, temp, src);
    la_vsrli_d(temp, temp, 0x10);
    la_vmin_hu(temp, temp, src);
    la_vsrli_d(temp, temp, 0x10);
    la_vmin_hu(temp, temp, src);

    if (option_enable_lasx) {
        la_xvpickve_d(temp2, temp, 1);
    } else {
        la_vandi_b(temp2, temp2, 0);
        la_vextrins_d(temp2, temp, 1);
    }
    la_vmin_hu(temp, temp, temp2);
    if (option_enable_lasx) {
        la_xvreplve0_h(temp, temp);
    } else {
        la_vreplvei_h(temp, temp, 0);
    }
    la_vseq_h(temp1, src, temp);
    la_vfrstpi_h(temp2, temp1, 0);
    la_vpackev_h(dest, temp2, temp);
    if (option_enable_lasx) {
        la_xvpickve_w(dest, dest, 0);
    } else {
        la_vinsgr2vr_d(dest, zero_ir2_opnd, 1);
        la_vinsgr2vr_w(dest, zero_ir2_opnd, 1);
    }
    return true;
}

bool translate_pmulld(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    la_vmul_w(dest, dest, src);
    return true;
}

bool translate_pmuldq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    la_vmulwev_d_w(dest, dest, src);
    return true;
}

bool translate_pblendvb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND mask = ra_alloc_xmm(0);
    IR2_OPND temp = ra_alloc_ftemp();

    la_vslti_b(temp, mask, 0);
    la_vbitsel_v(dest, dest, src, temp);
    return true;
}

bool translate_pblendw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);

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

bool translate_pminsb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    la_vmin_b(dest, dest, src);
    return true;
}

bool translate_pminuw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    la_vmin_hu(dest, dest, src);
    return true;
}

bool translate_pminsd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    la_vmin_w(dest, dest, src);
    return true;
}

bool translate_pminud(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    la_vmin_wu(dest, dest, src);
    return true;
}

bool translate_pmaxsb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    la_vmax_b(dest, dest, src);
    return true;
}

bool translate_pmaxuw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    la_vmax_hu(dest, dest, src);
    return true;
}

bool translate_pmaxsd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    la_vmax_w(dest, dest, src);
    return true;
}

bool translate_pmaxud(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    la_vmax_wu(dest, dest, src);
    return true;
}

bool translate_pextrb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest;
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);

    if (ir1_opnd_is_gpr(opnd0)) {
        dest = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd0));
        la_vpickve2gr_bu(dest, src, imm);
    } else {
        dest = ra_alloc_itemp();
        la_vpickve2gr_bu(dest, src, imm);
        store_ireg_to_ir1(dest, opnd0, false);
    }

    return true;
}

bool translate_pextrd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest;
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);

    if (ir1_opnd_is_gpr(opnd0)) {
        dest = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd0));
        la_vpickve2gr_wu(dest, src, imm);
    } else {
        dest = ra_alloc_itemp();
        la_vpickve2gr_wu(dest, src, imm);
        store_ireg_to_ir1(dest, opnd0, false);
    }

    return true;
}

bool translate_pextrq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest;
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);

    if (ir1_opnd_is_gpr(opnd0)) {
        dest = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd0));
        la_vpickve2gr_d(dest, src, imm);
    } else {
        dest = ra_alloc_itemp();
        la_vpickve2gr_d(dest, src, imm);
        store_ireg_to_ir1(dest, opnd0, false);
    }

    return true;
}

bool translate_pinsrb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    uint8_t imm = ir1_opnd_uimm(opnd2);

    la_vinsgr2vr_b(dest, src, imm);

    return true;
}

bool translate_pinsrd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    uint8_t imm = ir1_opnd_uimm(opnd2);

    la_vinsgr2vr_w(dest, src, imm);

    return true;
}

bool translate_pinsrq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    uint8_t imm = ir1_opnd_uimm(opnd2);

    la_vinsgr2vr_d(dest, src, imm);

    return true;
}

bool translate_pmovsxbw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_h_b(dest, src);
    } else {
        la_vsllwil_h_b(dest, src, 0);
    }
    return true;
}

bool translate_pmovzxbw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_hu_bu(dest, src);
    } else {
        la_vsllwil_hu_bu(dest, src, 0);
    }
    return true;
}

bool translate_pmovsxbd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_w_b(dest, src);
    } else {
        la_vsllwil_h_b(dest, src, 0);
        la_vsllwil_w_h(dest, dest, 0);
    }
    return true;
}

bool translate_pmovzxbd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_wu_bu(dest, src);
    } else {
        la_vsllwil_hu_bu(dest, src, 0);
        la_vsllwil_wu_hu(dest, dest, 0);
    }
    return true;
}

bool translate_pmovsxbq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_d_b(dest, src);
    } else {
        la_vsllwil_h_b(dest, src, 0);
        la_vsllwil_w_h(dest, dest, 0);
        la_vsllwil_d_w(dest, dest, 0);
    }
    return true;
}

bool translate_pmovzxbq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_du_bu(dest, src);
    } else {
        la_vsllwil_hu_bu(dest, src, 0);
        la_vsllwil_wu_hu(dest, dest, 0);
        la_vsllwil_du_wu(dest, dest, 0);
    }
    return true;
}

bool translate_pmovsxwd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_w_h(dest, src);
    } else {
        la_vsllwil_w_h(dest, src, 0);
    }
    return true;
}

bool translate_pmovzxwd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_wu_hu(dest, src);
    } else {
        la_vsllwil_wu_hu(dest, src, 0);
    }
    return true;
}

bool translate_pmovsxwq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_d_h(dest, src);
    } else {
        la_vsllwil_w_h(dest, src, 0);
        la_vsllwil_d_w(dest, dest, 0);
    }
    return true;
}

bool translate_pmovzxwq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_du_hu(dest, src);
    } else {
        la_vsllwil_wu_hu(dest, src, 0);
        la_vsllwil_du_wu(dest, dest, 0);
    }
    return true;
}

bool translate_pmovsxdq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_d_w(dest, src);
    } else {
        la_vsllwil_d_w(dest, src, 0);
    }
    return true;
}

bool translate_pmovzxdq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);

    if (option_enable_lasx) {
        la_vext2xv_du_wu(dest, src);
    } else {
        la_vsllwil_du_wu(dest, src, 0);
    }
    return true;
}

bool translate_ptest(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
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

bool translate_pcmpeqq(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vseq_d(dest, dest, src);
    return true;
}

bool translate_packusdw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND temp = ra_alloc_ftemp();
    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)) &&
        (ir1_opnd_base_reg_num(opnd0) == ir1_opnd_base_reg_num(opnd1))) {
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


bool translate_movntdqa(IR1_INST *pir1)
{
    translate_movaps(pir1);
    return true;
}

#ifdef CONFIG_LATX_AVX_OPT
bool translate_vaddpd(IR1_INST * pir1) {
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src2;

    if (ir1_opnd_is_ymm(opnd0)) {
        src2 = load_freg256_from_ir1(opnd2);
        la_xvfadd_d(dest, src1, src2);
    } else if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND temp = ra_alloc_ftemp();

        src2 = load_freg128_from_ir1(opnd2);
        la_vfadd_d(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vaddps(IR1_INST * pir1) {
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src2;

    if (ir1_opnd_is_ymm(opnd0)) {
        src2 = load_freg256_from_ir1(opnd2);
        la_xvfadd_s(dest, src1, src2);
    } else if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND temp = ra_alloc_ftemp();

        src2 = load_freg128_from_ir1(opnd2);
        la_vfadd_s(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vaddsd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();

    la_fadd_d(temp, src1, src2);
    la_vshuf4i_d(temp, src1, 0xc);
    set_high128_xreg_to_zero(temp);
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vaddss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();

    la_fadd_s(temp, src1, src2);
    if (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) !=
        ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1))) {
        la_xvori_b(dest, src1, 0);
    }
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vsubpd(IR1_INST * pir1) {
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src2;

    if (ir1_opnd_is_ymm(opnd0)) {
        src2 = load_freg256_from_ir1(opnd2);
        la_xvfsub_d(dest, src1, src2);
    } else if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND temp = ra_alloc_ftemp();

        src2 = load_freg128_from_ir1(opnd2);
        la_vfsub_d(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vsubps(IR1_INST * pir1) {
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src2;

    if (ir1_opnd_is_ymm(opnd0)) {
        src2 = load_freg256_from_ir1(opnd2);
        la_xvfsub_s(dest, src1, src2);
    } else if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND temp = ra_alloc_ftemp();

        src2 = load_freg128_from_ir1(opnd2);
        la_vfsub_s(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vsubsd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND src2 = load_freg128_from_ir1(opnd2);
    IR2_OPND temp = ra_alloc_ftemp();
    la_fsub_d(temp, src1, src2);
    la_vshuf4i_d(temp, src1, 0xc);
    la_xvori_b(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vsubss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();

    la_fsub_s(temp, src1, src2);
    if (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) !=
        ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1))) {
        la_xvori_b(dest, src1, 0);
    }
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vmulpd(IR1_INST * pir1) {
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src2;

    if (ir1_opnd_is_ymm(opnd0)) {
        src2 = load_freg256_from_ir1(opnd2);
        la_xvfmul_d(dest, src1, src2);
    } else if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND temp = ra_alloc_ftemp();

        src2 = load_freg128_from_ir1(opnd2);
        la_vfmul_d(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vmulps(IR1_INST * pir1) {
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src2;

    if (ir1_opnd_is_ymm(opnd0)) {
        src2 = load_freg256_from_ir1(opnd2);
        la_xvfmul_s(dest, src1, src2);
    } else if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND temp = ra_alloc_ftemp();

        src2 = load_freg128_from_ir1(opnd2);
        la_vfmul_s(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vmulsd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();

    la_fmul_d(temp, src1, src2);
    la_vshuf4i_d(temp, src1, 0xc);
    set_high128_xreg_to_zero(temp);
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vmulss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();

    la_fmul_s(temp, src1, src2);
    if (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) !=
        ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1))) {
        la_xvori_b(dest, src1, 0);
    }
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vdivpd(IR1_INST * pir1) {
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src2;

    if (ir1_opnd_is_ymm(opnd0)) {
        src2 = load_freg256_from_ir1(opnd2);
        la_xvfdiv_d(dest, src1, src2);
    } else if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND temp = ra_alloc_ftemp();

        src2 = load_freg128_from_ir1(opnd2);
        la_vfdiv_d(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vdivps(IR1_INST * pir1) {
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) && ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    IR2_OPND src2;

    if (ir1_opnd_is_ymm(opnd0)) {
        src2 = load_freg256_from_ir1(opnd2);
        la_xvfdiv_s(dest, src1, src2);
    } else if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND temp = ra_alloc_ftemp();

        src2 = load_freg128_from_ir1(opnd2);
        la_vfdiv_s(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vdivsd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    la_fdiv_d(temp, src1, src2);
    la_vshuf4i_d(temp, src1, 0xc);
    set_high128_xreg_to_zero(temp);
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vdivss(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND src2 = load_freg128_from_ir1(opnd2);
    IR2_OPND temp = ra_alloc_ftemp();
    la_fdiv_s(temp, src1, src2);
    if (ir1_opnd_base_reg_num(opnd0) != ir1_opnd_base_reg_num(opnd1)) {
        la_xvori_b(dest, src1, 0);
    }
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vsqrtpd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));

        la_xvfsqrt_d(dest, src);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vfsqrt_d(temp, src);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vsqrtps(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));

        la_xvfsqrt_s(dest, src);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vfsqrt_s(temp, src);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vsqrtsd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND src2 = load_freg128_from_ir1(opnd2);
    IR2_OPND temp = ra_alloc_ftemp();
    la_fsqrt_d(temp, src2);
    if (ir1_opnd_base_reg_num(opnd0) != ir1_opnd_base_reg_num(opnd1)) {
        la_xvori_b(dest, src1, 0);
    }
    la_xvinsve0_d(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vsqrtss(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND src2 = load_freg128_from_ir1(opnd2);
    IR2_OPND temp = ra_alloc_ftemp();
    la_fsqrt_s(temp, src2);
    if (ir1_opnd_base_reg_num(opnd0) != ir1_opnd_base_reg_num(opnd1)) {
        la_xvori_b(dest, src1, 0);
    }
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vaddsubpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND add_src1 = ra_alloc_ftemp();
    IR2_OPND add_src2 = ra_alloc_ftemp();
    IR2_OPND sub_src1 = ra_alloc_ftemp();
    IR2_OPND sub_src2 = ra_alloc_ftemp();

    la_xvpackev_d(sub_src1, src1, src1);
    la_xvpackev_d(sub_src2, src2, src2);
    la_xvpackod_d(add_src1, src1, src1);
    la_xvpackod_d(add_src2, src2, src2);
    if (ir1_opnd_is_xmm(opnd0)) {
        la_vfsub_d(sub_src1, sub_src1, sub_src2);
        la_vfadd_d(add_src1, add_src1, add_src2);
    } else {
        la_xvfsub_d(sub_src1, sub_src1, sub_src2);
        la_xvfadd_d(add_src1, add_src1, add_src2);
    }
    la_xvpackev_d(dest, add_src1, sub_src1);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vaddsubps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);

    IR2_OPND add_src1 = ra_alloc_ftemp();
    IR2_OPND add_src2 = ra_alloc_ftemp();
    IR2_OPND sub_src1 = ra_alloc_ftemp();
    IR2_OPND sub_src2 = ra_alloc_ftemp();
    la_xvpackev_w(sub_src1, src1, src1);
    la_xvpackev_w(sub_src2, src2, src2);
    la_xvpackod_w(add_src1, src1, src1);
    la_xvpackod_w(add_src2, src2, src2);
    if (ir1_opnd_is_xmm(opnd0)) {
        la_vfsub_s(sub_src1, sub_src1, sub_src2);
        la_vfadd_s(add_src1, add_src1, add_src2);
    } else {
        la_xvfsub_s(sub_src1, sub_src1, sub_src2);
        la_xvfadd_s(add_src1, add_src1, add_src2);
    }
    la_xvpackev_w(dest, add_src1, sub_src1);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }

    return true;
}

bool translate_vhaddpd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();

        la_xvpickev_d(temp1, src2, src1);
        la_xvpickod_d(temp2, src2, src1);
        la_xvfadd_d(dest, temp1, temp2);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();

        la_vpickev_d(temp1, src2, src1);
        la_vpickod_d(temp2, src2, src1);
        la_vfadd_d(temp, temp1, temp2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vhaddps(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();

        la_xvpickev_w(temp1, src2, src1);
        la_xvpickod_w(temp2, src2, src1);
        la_xvfadd_s(dest, temp1, temp2);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();

        la_vpickev_w(temp1, src2, src1);
        la_vpickod_w(temp2, src2, src1);
        la_vfadd_s(temp, temp1, temp2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vhsubpd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();

        la_xvpickev_d(temp1, src2, src1);
        la_xvpickod_d(temp2, src2, src1);
        la_xvfsub_d(dest, temp1, temp2);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();

        la_vpickev_d(temp1, src2, src1);
        la_vpickod_d(temp2, src2, src1);
        la_vfsub_d(temp, temp1, temp2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vhsubps(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();

        la_xvpickev_w(temp1, src2, src1);
        la_xvpickod_w(temp2, src2, src1);
        la_xvfsub_s(dest, temp1, temp2);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();

        la_vpickev_w(temp1, src2, src1);
        la_vpickod_w(temp2, src2, src1);
        la_vfsub_s(temp, temp1, temp2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vandnpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));

        la_xvandn_v(dest, src1, src2);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vandn_v(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vandnps(IR1_INST * pir1) {
    translate_vandnpd(pir1);
    return true;
}

bool translate_vandpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));

        la_xvand_v(dest, src1, src2);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vand_v(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vandps(IR1_INST * pir1) {
    translate_vandpd(pir1);
    return true;
}

bool translate_vorps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));

        la_xvor_v(dest, src1, src2);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vor_v(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vorpd(IR1_INST * pir1) {
    translate_vorps(pir1);
    return true;
}

bool translate_vxorps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));

        la_xvxor_v(dest, src1, src2);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vxor_v(temp, src1, src2);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vxorpd(IR1_INST * pir1) {
    translate_vxorps(pir1);
    return true;
}

bool translate_vminsx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND src2 = load_freg128_from_ir1(opnd2);
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, int);
    IR2_INST * ( * tr_insert)(IR2_OPND, IR2_OPND, int);
    IR2_OPND temp = ra_alloc_ftemp();
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VMINSS:
            tr_inst = la_fcmp_cond_s;
            tr_insert = la_xvinsve0_w;
            break;
        case dt_X86_INS_VMINSD:
            tr_inst = la_fcmp_cond_d;
            tr_insert = la_xvinsve0_d;
            break;
        default:
            tr_inst = NULL;
            tr_insert =NULL;
            lsassert(0);
            break;
    }
    tr_inst(fcc0_ir2_opnd, src1, src2, 0x3);
    la_fsel(temp, src2, src1, fcc0_ir2_opnd);
    if (ir1_opnd_base_reg_num(opnd0) != ir1_opnd_base_reg_num(opnd1)) {
        la_xvori_b(dest, src1, 0);
    }
    tr_insert(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vminpx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_INST * ( * tr_inst_128)(IR2_OPND, IR2_OPND, IR2_OPND, int);
    IR2_INST * ( * tr_inst_256)(IR2_OPND, IR2_OPND, IR2_OPND, int);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND mask = ra_alloc_ftemp();
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VMINPS:
            tr_inst_256 = la_xvfcmp_cond_s;
            tr_inst_128 = la_vfcmp_cond_s;
            break;
        case dt_X86_INS_VMINPD:
            tr_inst_256 = la_xvfcmp_cond_d;
            tr_inst_128 = la_vfcmp_cond_d;
            break;
        default:
            tr_inst_256 = NULL;
            tr_inst_128 = NULL;
            lsassert(0);
            break;
    }
    if (ir1_opnd_is_ymm(opnd0))
        tr_inst_256(mask, src1, src2, 0x3);
    else
        tr_inst_128(mask, src1, src2, 0x3);
    la_xvand_v(temp, src1, mask);
    la_xvandn_v(mask, mask, src2);
    la_xvor_v(dest, temp, mask);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vmaxsx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND src2 = load_freg128_from_ir1(opnd2);
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, int);
    IR2_INST * ( * tr_insert)(IR2_OPND, IR2_OPND, int);
    IR2_OPND temp = ra_alloc_ftemp();
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VMAXSS:
            tr_inst = la_fcmp_cond_s;
            tr_insert = la_xvinsve0_w;
            break;
        case dt_X86_INS_VMAXSD:
            tr_inst = la_fcmp_cond_d;
            tr_insert = la_xvinsve0_d;
            break;
        default:
            tr_inst = NULL;
            tr_insert = NULL;
            lsassert(0);
            break;
    }
    tr_inst(fcc0_ir2_opnd, src2, src1, 0x3);
    la_fsel(temp, src2, src1, fcc0_ir2_opnd);
    if (ir1_opnd_base_reg_num(opnd0) != ir1_opnd_base_reg_num(opnd1)) {
        la_xvori_b(dest, src1, 0);
    }
    tr_insert(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vmaxpx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_INST * ( * tr_inst_128)(IR2_OPND, IR2_OPND, IR2_OPND, int);
    IR2_INST * ( * tr_inst_256)(IR2_OPND, IR2_OPND, IR2_OPND, int);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND mask = ra_alloc_ftemp();
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VMAXPS:
            tr_inst_256 = la_xvfcmp_cond_s;
            tr_inst_128 = la_vfcmp_cond_s;
            break;
        case dt_X86_INS_VMAXPD:
            tr_inst_256 = la_xvfcmp_cond_d;
            tr_inst_128 = la_vfcmp_cond_d;
            break;
        default:
            tr_inst_256 = NULL;
            tr_inst_128 = NULL;
            lsassert(0);
            break;
    }
    if (ir1_opnd_is_ymm(opnd0))
        tr_inst_256(mask, src2, src1, 0x3);
    else
        tr_inst_128(mask, src2, src1, 0x3);
    la_xvand_v(temp, src1, mask);
    la_xvandn_v(mask, mask, src2);
    la_xvor_v(dest, temp, mask);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vblendvpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND * opnd3 = ir1_get_opnd(pir1, 3);

    lsassert((ir1_opnd_is_xmm(opnd0) &&
            ir1_opnd_is_xmm(opnd1) &&
            ir1_opnd_is_xmm(opnd3)) ||
        (ir1_opnd_is_ymm(opnd0) &&
            ir1_opnd_is_ymm(opnd1) &&
            ir1_opnd_is_ymm(opnd3)));
    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND src3 = load_freg256_from_ir1(opnd3);
        IR2_OPND temp = ra_alloc_ftemp();
        la_xvslti_d(temp, src3, 0);
        la_xvbitsel_v(dest, src1, src2, temp);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND src3 = load_freg128_from_ir1(opnd3);
        IR2_OPND temp = ra_alloc_ftemp();
        la_vslti_d(temp, src3, 0);
        la_vbitsel_v(temp, src1, src2, temp);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vblendpd(IR1_INST * pir1) {
    lsassert(ir1_opnd_num(pir1) == 4 &&
        ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
            ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) &&
            ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));
    IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    IR2_OPND temp = ra_alloc_ftemp();
    la_xvori_b(temp, src1, 0);
    if (imm & 0x1)
        la_vextrins_d(temp, src2, VEXTRINS_IMM_4_0(0, 0));
    if (imm & 0x2)
        la_vextrins_d(temp, src2, VEXTRINS_IMM_4_0(1, 1));
    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_xvori_b(temp2, src1, 0);
        if (imm & 0x4)
            la_xvextrins_d(temp2, src2, VEXTRINS_IMM_4_0(0, 0));
        if (imm & 0x8)
            la_xvextrins_d(temp2, src2, VEXTRINS_IMM_4_0(1, 1));
        la_xvpermi_q(temp, temp2, VEXTRINS_IMM_4_0(1, 2));
    }
    la_xvori_b(dest, temp, 0);
    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vblendps(IR1_INST * pir1) {
    lsassert(ir1_opnd_num(pir1) == 4 &&
        ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
            ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) &&
            ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));
    IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND rmask = ra_alloc_itemp();


    uint64_t mask = 0;
    for (int i = 0; i < 8; i++) {
        if (imm & (1 << i)) {
            mask |= (0xFFULL) << (i * 8);
        }
    }

    li_d(rmask, mask);
    la_movgr2fr_d(temp, rmask);
    la_vext2xv_w_b(temp, temp);
    la_xvbitsel_v(dest, src1, src2, temp);

    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vblendvps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND * opnd3 = ir1_get_opnd(pir1, 3);
    lsassert((ir1_opnd_is_xmm(opnd0) &&
            ir1_opnd_is_xmm(opnd1) &&
            ir1_opnd_is_xmm(opnd3)) ||
        (ir1_opnd_is_ymm(opnd0) &&
            ir1_opnd_is_ymm(opnd1) &&
            ir1_opnd_is_ymm(opnd3)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND src3 = load_freg256_from_ir1(opnd3);
    IR2_OPND temp = ra_alloc_ftemp();
    la_xvslti_w(temp, src3, 0);
    la_xvbitsel_v(dest, src1, src2, temp);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vbroadcastsd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_xvreplve0_d(dest, src);
    return true;
}

bool translate_vbroadcastss(IR1_INST * pir1) {
#ifdef CONFIG_LATX_TS
    if (!ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        !ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        return false;
    }
#else
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));
#endif
    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        la_xvreplve0_w(dest, src);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND temp = ra_alloc_ftemp();
        la_xvreplve0_w(temp, src);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vbroadcastf128(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_xvreplve0_q(dest, src);
    return true;
}

bool translate_vbroadcasti128(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_xvreplve0_q(dest, src);
    return true;
}



bool translate_vextractf128(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_mem(ir1_get_opnd(pir1, 0)));
    lsassert(ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1)) &&
        ir1_opnd_is_imm(ir1_get_opnd(pir1, 2)));
    IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2)) & 0x1;
    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND temp = ra_alloc_ftemp();
        if (!imm) {
            la_xvpermi_q(temp, src, VEXTRINS_IMM_4_0(3, 0));
        } else {
            la_xvpermi_q(temp, src, VEXTRINS_IMM_4_0(3, 1));
        }
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    } else {
        if (!imm) {
            store_freg128_to_ir1_mem(src, ir1_get_opnd(pir1, 0));
        } else {
            IR2_OPND temp = ra_alloc_ftemp();
            la_xvpermi_q(temp, src, VEXTRINS_IMM_4_0(3, 1));
            store_freg128_to_ir1_mem(temp, ir1_get_opnd(pir1, 0));
        }
    }
    return true;
}

bool translate_vextracti128(IR1_INST * pir1) {
    translate_vextractf128(pir1);
    return true;
}

bool translate_vextractps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_gpr(opnd0) || ir1_opnd_is_mem(opnd0));
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    IR2_OPND temp = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm(opnd2) & 0x3;
    la_xvpickve_w(temp, src, imm);
    if (ir1_opnd_is_gpr(opnd0)) {
        IR2_OPND dest = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd0));
        la_movfr2gr_d(dest, temp);
    } else {
        store_freg_to_ir1(temp, opnd0, false, false);
    }
    return true;
}

bool translate_vinsertps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_num(pir1) == 4 &&
        ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND src2 = load_freg128_from_ir1(opnd2);
    IR2_OPND temp = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    uint8_t count_s = (imm >> 6) & 0x3;
    uint8_t count_d = (imm >> 4) & 0x3;
    uint8_t zmask = imm & 0xf;
    if (ir1_opnd_is_mem(opnd2)) {
        count_s = 0;
    }
    la_vori_b(temp, src1, 0);
    la_vextrins_w(temp, src2, VEXTRINS_IMM_4_0(count_d, count_s));
    if (zmask & 0x1)
        la_vinsgr2vr_w(temp, zero_ir2_opnd, 0);
    if (zmask & 0x2)
        la_vinsgr2vr_w(temp, zero_ir2_opnd, 1);
    if (zmask & 0x4)
        la_vinsgr2vr_w(temp, zero_ir2_opnd, 2);
    if (zmask & 0x8)
        la_vinsgr2vr_w(temp, zero_ir2_opnd, 3);
    la_vori_b(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vinserti128(IR1_INST * pir1) {
    lsassert(ir1_opnd_num(pir1) == 4);
    lsassert(ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1)) &&
        ir1_opnd_size(ir1_get_opnd(pir1, 2)) == 128 &&
        ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3)) & 0x1;
    IR2_OPND temp = ra_alloc_ftemp();
    la_xvori_b(temp, src1, 0);
    if (!imm) {
        la_xvpermi_q(temp, src2, VEXTRINS_IMM_4_0(3, 0));
    } else {
        la_xvpermi_q(temp, src2, VEXTRINS_IMM_4_0(0, 2));
    }
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vinsertf128(IR1_INST * pir1) {
    lsassert(ir1_opnd_num(pir1) == 4);
    lsassert(ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1)) &&
        ir1_opnd_size(ir1_get_opnd(pir1, 2)) == 128 &&
        ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3)) & 0x1;
    IR2_OPND temp = ra_alloc_ftemp();
    la_xvori_b(temp, src1, 0);
    if (!imm) {
        la_xvpermi_q(temp, src2, VEXTRINS_IMM_4_0(3, 0));
    } else {
        la_xvpermi_q(temp, src2, VEXTRINS_IMM_4_0(0, 2));
    }
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vshufpd(IR1_INST * pir1) {
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
            ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) &&
            ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));

    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src1 = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp_l = ra_alloc_ftemp();
        IR2_OPND temp_h = ra_alloc_ftemp();
        uint8_t imm8 = ir1_opnd_uimm(ir1_get_opnd(pir1, 3)) & 0xf;
        uint8_t l = imm8 & 0x3;
        uint8_t h = imm8 >> 2;
        if (l == 0) {
            l = 0x8;
        } else if (l == 1) {
            l = 0x9;
        } else if (l == 2) {
            l = 0xc;
        } else if (l == 3) {
            l = 0xd;
        } else {
            lsassert(0);
        }

        if (h == 0) {
            h = 0x8;
        } else if (h == 1) {
            h = 0x9;
        } else if (h == 2) {
            h = 0xc;
        } else if (h == 3) {
            h = 0xd;
        } else {
            lsassert(0);
        }
        la_xvori_b(temp_l, src1, 0);
        la_xvori_b(temp_h, src1, 0);
        la_xvshuf4i_d(temp_l, src2, l);
        la_xvshuf4i_d(temp_h, src2, h);
        la_xvpermi_q(temp_h, temp_l, VEXTRINS_IMM_4_0(3, 0));
        la_xvori_b(dest, temp_h, 0);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();
        uint8_t imm8 = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
        imm8 = imm8 & 3;
        if (imm8 == 0) {
            imm8 = 0x8;
        } else if (imm8 == 1) {
            imm8 = 0x9;
        } else if (imm8 == 2) {
            imm8 = 0xc;
        } else if (imm8 == 3) {
            imm8 = 0xd;
        } else {
            lsassert(0);
        }
        la_xvori_b(temp, src1, 0);
        la_vshuf4i_d(temp, src2, imm8);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vshufps(IR1_INST * pir1) {
    lsassert((ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
            ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1))) ||
        (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) &&
            ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1))));
    if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src1 = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
        uint64_t imm8 = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_xvshuf4i_w(temp1, src1, imm8);
        la_xvshuf4i_w(temp2, src2, imm8 >> 4);
        la_xvpickev_d(dest, temp2, temp1);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        uint64_t imm8 = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_vshuf4i_w(temp1, src1, imm8);
        la_vshuf4i_w(temp2, src2, imm8 >> 4);
        la_vpickev_d(temp1, temp2, temp1);
        set_high128_xreg_to_zero(temp1);
        la_xvori_b(dest, temp1, 0);
    }
    return true;
}

bool translate_vunpckhpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();

        la_xvori_b(temp, src1, 0);
        la_xvshuf4i_d(temp, src2, 0xd);
        la_xvori_b(dest, temp, 0);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vori_b(temp, src1, 0);
        la_vshuf4i_d(temp, src2, 0xd);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vunpckhps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));

        la_xvilvh_w(dest, src2, src1);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vilvh_w(temp, src2, src1);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vunpcklpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();

        la_xvori_b(temp, src1, 0);
        la_xvshuf4i_d(temp, src2, 0x8);
        la_xvori_b(dest, temp, 0);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vori_b(temp, src1, 0);
        la_vshuf4i_d(temp, src2, 0x8);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vunpcklps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));

        la_xvilvl_w(dest, src2, src1);
    } else {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        IR2_OPND temp = ra_alloc_ftemp();

        la_vilvl_w(temp, src2, src1);
        set_high128_xreg_to_zero(temp);
        la_xvori_b(dest, temp, 0);
    }
    return true;
}

bool translate_vpabsx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0) || ir1_opnd_is_ymm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_OPND vzero = ra_alloc_ftemp();
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_INST * ( * tr_inst_128)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_INST * ( * tr_inst_256)(IR2_OPND, IR2_OPND, IR2_OPND);
    switch (ir1_opcode(pir1)) {
        case dt_X86_INS_VPABSB:
            tr_inst_256 = la_xvabsd_b;
            tr_inst_128 = la_vabsd_b;
            break;
        case dt_X86_INS_VPABSW:
            tr_inst_256 = la_xvabsd_h;
            tr_inst_128 = la_vabsd_h;
            break;
        case dt_X86_INS_VPABSD:
            tr_inst_256 = la_xvabsd_w;
            tr_inst_128 = la_vabsd_w;
            break;
        default:
            tr_inst_256 = NULL;
            tr_inst_128 = NULL;
            lsassert(0);
            break;
    }

    la_vreplgr2vr_d(vzero, zero_ir2_opnd);
    if (ir1_opnd_is_ymm(opnd0))
        tr_inst_256(temp, src, vzero);
    else
        tr_inst_128(temp, src, vzero);
    la_xvori_b(dest, temp, 0);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);

    return true;
}

bool translate_vpackusxx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2;
    IR1_OPCODE op = ir1_opcode(pir1);
    IR2_INST * ( * cmp_inst)(IR2_OPND, IR2_OPND, int);
    IR2_INST * ( * cvt_inst)(IR2_OPND, IR2_OPND, int);
    switch (op) {
        case dt_X86_INS_VPACKUSDW:
            cmp_inst = la_xvslti_w;
            cvt_inst = la_xvssrani_hu_w;
            break;
        case dt_X86_INS_VPACKUSWB:
            cmp_inst = la_xvslti_h;
            cvt_inst = la_xvssrani_bu_h;
            break;
        default:
            cmp_inst = NULL;
            cvt_inst = NULL;
            lsassert(0);
            break;
    }
    cmp_inst(temp1, src1, 0);
    la_xvandn_v(temp1, temp1, src1);
    if ((ir1_opnd_is_xmm(opnd2) || ir1_opnd_is_ymm(opnd2)) &&
        ir1_opnd_base_reg_num(opnd1) == ir1_opnd_base_reg_num(opnd2)) {
        temp2 = temp1;
    } else {
        temp2 = ra_alloc_ftemp();
        cmp_inst(temp2, src2, 0);
        la_xvandn_v(temp2, temp2, src2);
    }
    cvt_inst(temp2, temp1, 0);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(temp2);
    }
    la_xvori_b(dest, temp2, 0);
    return true;
}

bool translate_vpaddx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR1_OPCODE op = ir1_opcode(pir1);
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    switch (op) {
        case dt_X86_INS_VPADDB:
            tr_inst = la_xvadd_b;
            break;
        case dt_X86_INS_VPADDW:
            tr_inst = la_xvadd_h;
            break;
        case dt_X86_INS_VPADDD:
            tr_inst = la_xvadd_w;
            break;
        case dt_X86_INS_VPADDQ:
            tr_inst = la_xvadd_d;
            break;
        case dt_X86_INS_VPADDSB:
            tr_inst = la_xvsadd_b;
            break;
        case dt_X86_INS_VPADDSW:
            tr_inst = la_xvsadd_h;
            break;
        case dt_X86_INS_VPADDUSB:
            tr_inst = la_xvsadd_bu;
            break;
        case dt_X86_INS_VPADDUSW:
            tr_inst = la_xvsadd_hu;
            break;

        default:
            tr_inst = NULL;
            lsassert(0);
            break;
    }
    tr_inst(dest, src1, src2);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpand(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);

    if (ir1_opnd_is_ymm(opnd0)) {
        la_xvand_v(dest, src1, src2);
        return true;
    }
    IR2_OPND temp = ra_alloc_ftemp();
    la_xvand_v(temp, src1, src2);
    set_high128_xreg_to_zero(temp);
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vpandn(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);

    if (ir1_opnd_is_ymm(opnd0)) {
        la_xvandn_v(dest, src1, src2);
        return true;
    }
    IR2_OPND temp = ra_alloc_ftemp();
    la_xvandn_v(temp, src1, src2);
    set_high128_xreg_to_zero(temp);
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vpblendd(IR1_INST * pir1) {
    lsassert(ir1_opnd_num(pir1) == 4);
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND * opnd3 = ir1_get_opnd(pir1, 3);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    uint8_t imm = ir1_opnd_uimm(opnd3);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND rmask = ra_alloc_itemp();

    if (ir1_opnd_is_xmm(opnd0)) {
        if((imm & 0xf) == 0xf) {
            la_xvori_b(dest, src2, 0);
            set_high128_xreg_to_zero(dest);
            return true;
        } else if((imm & 0xf) == 0x0){
            la_xvori_b(dest, src1, 0);
            set_high128_xreg_to_zero(dest);
            return true;
        }
        uint64_t mask = 0;
        for (int i = 0; i < 4; i++) {
            if (imm & (1 << i)) {
                mask |= (0xFFULL) << (i * 8);
            }
        }
        li_d(rmask, mask);
        la_movgr2fr_d(temp, rmask);
        la_vext2xv_w_b(temp, temp);
        la_xvbitsel_v(dest, src1, src2, temp);

        set_high128_xreg_to_zero(dest);
        return true;
    }
        if((imm & 0xff) == 0xff) {
            la_xvori_b(dest, src2, 0);
            return true;
        } else if((imm & 0xff) == 0x0){
            la_xvori_b(dest, src1, 0);
            return true;
        }
        uint64_t mask = 0;
        for (int i = 0; i < 8; i++) {
            if (imm & (1 << i)) {
                mask |= (0xFFULL) << (i * 8);
            }
        }
        li_d(rmask, mask);
        la_movgr2fr_d(temp, rmask);
        la_vext2xv_w_b(temp, temp);
        la_xvbitsel_v(dest, src1, src2, temp);

    return true;
}

bool translate_vpblendvb(IR1_INST * pir1) {
    lsassert(ir1_opnd_num(pir1) == 4);
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND * opnd3 = ir1_get_opnd(pir1, 3);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest, src1, src2, src3;
    IR2_OPND temp = ra_alloc_ftemp();
    dest = load_freg256_from_ir1(opnd0);
    src1 = load_freg256_from_ir1(opnd1);
    src2 = load_freg256_from_ir1(opnd2);
    src3 = load_freg256_from_ir1(opnd3);
    la_xvslti_b(temp, src3, 0);
    la_xvbitsel_v(temp, src1, src2, temp);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(temp);
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vpblendw(IR1_INST * pir1) {
    lsassert(ir1_opnd_num(pir1) == 4);
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND * opnd3 = ir1_get_opnd(pir1, 3);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    uint8_t imm = ir1_opnd_uimm(opnd3);
    if (imm == 0xff) {
        la_xvand_v(dest, src2, src2);
        if (ir1_opnd_is_xmm(opnd0))
            set_high128_xreg_to_zero(dest);
        return true;
    } else if (imm == 0) {
        la_xvand_v(dest, src1, src1);
        if (ir1_opnd_is_xmm(opnd0))
            set_high128_xreg_to_zero(dest);
        return true;
    }
    IR2_OPND temp = ra_alloc_ftemp();
    la_xvori_b(temp, src1, 0);
    /* 64 bit fast path */
    if ((imm & 0xf) == 0xf) {
        la_xvextrins_d(temp, src2, VEXTRINS_IMM_4_0(0, 0));
        imm &= ~0xf;
    }
    if ((imm & 0xf0) == 0xf0) {
        la_xvextrins_d(temp, src2, VEXTRINS_IMM_4_0(1, 1));
        imm &= ~0xf0;
    }

    /* 32 bit fast path */
    if ((imm & 0x3) == 0x3) {
        la_xvextrins_w(temp, src2, VEXTRINS_IMM_4_0(0, 0));
        imm &= ~0x3;
    }
    if ((imm & 0xc) == 0xc) {
        la_xvextrins_w(temp, src2, VEXTRINS_IMM_4_0(1, 1));
        imm &= ~0xc;
    }
    if ((imm & 0x30) == 0x30) {
        la_xvextrins_w(temp, src2, VEXTRINS_IMM_4_0(2, 2));
        imm &= ~0x30;
    }
    if ((imm & 0xc0) == 0xc0) {
        la_xvextrins_w(temp, src2, VEXTRINS_IMM_4_0(3, 3));
        imm &= ~0xc0;
    }

    /* 16 bit slow path */
    if (imm & 0x1)
        la_xvextrins_h(temp, src2, VEXTRINS_IMM_4_0(0, 0));
    if (imm & 0x2)
        la_xvextrins_h(temp, src2, VEXTRINS_IMM_4_0(1, 1));
    if (imm & 0x4)
        la_xvextrins_h(temp, src2, VEXTRINS_IMM_4_0(2, 2));
    if (imm & 0x8)
        la_xvextrins_h(temp, src2, VEXTRINS_IMM_4_0(3, 3));
    if (imm & 0x10)
        la_xvextrins_h(temp, src2, VEXTRINS_IMM_4_0(4, 4));
    if (imm & 0x20)
        la_xvextrins_h(temp, src2, VEXTRINS_IMM_4_0(5, 5));
    if (imm & 0x40)
        la_xvextrins_h(temp, src2, VEXTRINS_IMM_4_0(6, 6));
    if (imm & 0x80)
        la_xvextrins_h(temp, src2, VEXTRINS_IMM_4_0(7, 7));

    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(temp);
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vperm2f128(IR1_INST * pir1) {
    translate_vperm2i128(pir1);
    return true;
}

bool translate_vperm2i128(IR1_INST * pir1) {
    lsassert(ir1_opnd_num(pir1) == 4);
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND * opnd3 = ir1_get_opnd(pir1, 3);
    lsassert((ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    lsassert(ir1_opnd_is_imm(opnd3));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    uint8 imm = ir1_opnd_uimm(opnd3) & 0xff;
    bool zero_l = (imm >> 3) & 0x1, zero_h = (imm >> 7) & 0x1;
    if (zero_l && zero_h) {
        la_xvandi_b(dest, dest, 0);
        return true;
    }
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND temp = ra_alloc_ftemp();
    la_xvori_b(temp, src2, 0);
    la_xvpermi_q(temp, src1, imm);
    if (zero_l) {
        la_xvinsgr2vr_d(temp, zero_ir2_opnd, 0);
        la_xvinsgr2vr_d(temp, zero_ir2_opnd, 1);
    }
    if (zero_h) {
        la_xvinsgr2vr_d(temp, zero_ir2_opnd, 2);
        la_xvinsgr2vr_d(temp, zero_ir2_opnd, 3);
    }
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vpermd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    la_xvperm_w(dest, src2, src1);
    return true;
}

bool translate_vpermq(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_imm(ir1_get_opnd(pir1, 2)));
    IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2)) & 0xff;
    la_xvpermi_d(dest, src, imm);
    return true;
}

bool translate_vpextrx(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)) &&
        ir1_opnd_is_imm(ir1_get_opnd(pir1, 2)));
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND src = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND dest;
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, int);
    bool is_reg = ir1_opnd_is_gpr(opnd0);
    if (is_reg)
        dest = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd0));
    else
        dest = ra_alloc_itemp();
    switch (ir1_opcode(pir1)) {
        case dt_X86_INS_VPEXTRB:
            tr_inst = la_vpickve2gr_bu;
            imm &= 0xf;
            break;
        case dt_X86_INS_VPEXTRW:
            tr_inst = la_vpickve2gr_hu;
            imm &= 0x7;
            break;
        case dt_X86_INS_VPEXTRD:
            tr_inst = la_vpickve2gr_wu;
            imm &= 0x3;
            break;
        case dt_X86_INS_VPEXTRQ:
            tr_inst = la_vpickve2gr_du;
            imm &= 0x1;
            break;
        default:
            tr_inst = NULL;
            lsassert(0);
            break;
    }
    tr_inst(dest, src, imm);
    if (!is_reg)
        store_ireg_to_ir1(dest, opnd0, false);
    return true;
}

bool translate_vpminux(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    switch (ir1_opcode(pir1)) {
        case dt_X86_INS_VPMINUB:
            tr_inst = la_xvmin_bu;
            break;
        case dt_X86_INS_VPMINUW:
            tr_inst = la_xvmin_hu;
            break;
        case dt_X86_INS_VPMINUD:
            tr_inst = la_xvmin_wu;
            break;
        default:
            tr_inst = NULL;
            lsassert(0);
            break;
    }
    tr_inst(dest, src1, src2);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);

    return true;
}

bool translate_vpmovsxxx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0) || ir1_opnd_is_ymm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VPMOVSXBW:
            tr_inst = la_vext2xv_h_b;
            break;
        case dt_X86_INS_VPMOVSXBD:
            tr_inst = la_vext2xv_w_b;
            break;
        case dt_X86_INS_VPMOVSXBQ:
            tr_inst = la_vext2xv_d_b;
            break;
        case dt_X86_INS_VPMOVSXWD:
            tr_inst = la_vext2xv_w_h;
            break;
        case dt_X86_INS_VPMOVSXWQ:
            tr_inst = la_vext2xv_d_h;
            break;
        case dt_X86_INS_VPMOVSXDQ:
            tr_inst = la_vext2xv_d_w;
            break;
        default:
            tr_inst = NULL;
            lsassert(0);
            break;
    }
    tr_inst(dest, src);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpmovzxxx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0) || ir1_opnd_is_ymm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VPMOVZXBW:
            tr_inst = la_vext2xv_hu_bu;
            break;
        case dt_X86_INS_VPMOVZXBD:
            tr_inst = la_vext2xv_wu_bu;
            break;
        case dt_X86_INS_VPMOVZXBQ:
            tr_inst = la_vext2xv_du_bu;
            break;
        case dt_X86_INS_VPMOVZXWD:
            tr_inst = la_vext2xv_wu_hu;
            break;
        case dt_X86_INS_VPMOVZXWQ:
            tr_inst = la_vext2xv_du_hu;
            break;
        case dt_X86_INS_VPMOVZXDQ:
            tr_inst = la_vext2xv_du_wu;
            break;
        default:
            tr_inst = NULL;
            lsassert(0);
            break;
    }
    tr_inst(dest, src);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpmullx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VPMULLD:
            tr_inst = la_xvmul_w;
            break;
        case dt_X86_INS_VPMULLW:
            tr_inst = la_xvmul_h;
            break;
        case dt_X86_INS_VPMULUDQ:
            tr_inst = la_xvmulwev_d_wu;
            break;
        default:
            tr_inst = NULL;
            lsassert(0);
            break;
    }
    tr_inst(dest, src1, src2);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpor(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    la_xvor_v(dest, src1, src2);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpshufb(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);

    IR2_OPND index = ra_alloc_ftemp();
    IR2_OPND mask = ra_alloc_ftemp();
    la_xvandi_b(index, src2, 0xf);
    la_xvslti_b(mask, src2, 0);
    la_xvshuf_b(dest, src1, src1, index);
    la_xvandn_v(dest, mask, dest);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpsubx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR1_OPCODE op = ir1_opcode(pir1);
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    switch (op) {
        case dt_X86_INS_VPSUBB:
            tr_inst = la_xvsub_b;
            break;
        case dt_X86_INS_VPSUBW:
            tr_inst = la_xvsub_h;
            break;
        case dt_X86_INS_VPSUBD:
            tr_inst = la_xvsub_w;
            break;
        case dt_X86_INS_VPSUBQ:
            tr_inst = la_xvsub_d;
            break;
        case dt_X86_INS_VPSUBSB:
            tr_inst = la_xvssub_b;
            break;
        case dt_X86_INS_VPSUBSW:
            tr_inst = la_xvssub_h;
            break;
        case dt_X86_INS_VPSUBUSB:
            tr_inst = la_xvssub_bu;
            break;
        case dt_X86_INS_VPSUBUSW:
            tr_inst = la_xvssub_hu;
            break;
        default:
            tr_inst = NULL;
            lsassert(0);
            break;
    }
    tr_inst(dest, src1, src2);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vptest(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(opnd0) || ir1_opnd_is_ymm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND label_1 = ra_alloc_label();
    IR2_OPND label_2 = ra_alloc_label();
    IR2_OPND n4095_opnd = ra_alloc_num_4095();

    if (ir1_opnd_is_xmm(opnd0)) {
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
    } else {
        la_x86mtflag(zero_ir2_opnd, 0x3f);
        la_xvand_v(temp, src, dest);
        la_xvseteqz_v(fcc0_ir2_opnd, temp);
        la_bceqz(fcc0_ir2_opnd, label_1);
        la_x86mtflag(n4095_opnd, ZF_USEDEF_BIT);

        la_label(label_1);
        la_xvandn_v(temp, dest, src);
        la_xvseteqz_v(fcc0_ir2_opnd, temp);
        la_bceqz(fcc0_ir2_opnd, label_2);
        la_x86mtflag(n4095_opnd, CF_USEDEF_BIT);
        la_label(label_2);
    }
    ra_free_num_4095(n4095_opnd);
    return true;
}

bool translate_vpunpckhxx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VPUNPCKHBW:
            tr_inst = la_xvilvh_b;
            break;
        case dt_X86_INS_VPUNPCKHWD:
            tr_inst = la_xvilvh_h;
            break;
        case dt_X86_INS_VPUNPCKHDQ:
            tr_inst = la_xvilvh_w;
            break;
        case dt_X86_INS_VPUNPCKHQDQ:
            tr_inst = la_xvilvh_d;
            break;
        default:
            tr_inst = NULL;
            lsassert(0);
            break;
    }
    tr_inst(dest, src2, src1);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpunpcklxx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VPUNPCKLBW:
            tr_inst = la_xvilvl_b;
            break;
        case dt_X86_INS_VPUNPCKLWD:
            tr_inst = la_xvilvl_h;
            break;
        case dt_X86_INS_VPUNPCKLDQ:
            tr_inst = la_xvilvl_w;
            break;
        case dt_X86_INS_VPUNPCKLQDQ:
            tr_inst = la_xvilvl_d;
            break;
        default:
            tr_inst = NULL;
            lsassert(0);
            break;
    }
    tr_inst(dest, src2, src1);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpxor(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    la_xvxor_v(dest, src1, src2);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vfmaddxxxss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFMADD132SS:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMADD231SS:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFMADD213SS:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    la_fmadd_s(temp, temp1, temp2, temp3);
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfmaddxxxsd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFMADD132SD:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMADD231SD:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFMADD213SD:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    la_fmadd_d(temp, temp1, temp2, temp3);
    la_xvinsve0_d(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfmaddxxxpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFMADD132PD:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMADD231PD:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFMADD213PD:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst = ir1_opnd_is_xmm(opnd0) ? la_vfmadd_d : la_xvfmadd_d;
    tr_inst(temp, temp1, temp2, temp3);
    la_xvori_b(dest, temp, 0);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfmaddxxxps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFMADD132PS:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMADD231PS:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFMADD213PS:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst = ir1_opnd_is_xmm(opnd0) ? la_vfmadd_s : la_xvfmadd_s;
    tr_inst(temp, temp1, temp2, temp3);
    la_xvori_b(dest, temp, 0);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfmsubxxxss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFMSUB132SS:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMSUB231SS:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFMSUB213SS:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    la_fmsub_s(temp, temp1, temp2, temp3);
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfmsubxxxsd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFMSUB132SD:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMSUB231SD:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFMSUB213SD:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    la_fmsub_d(temp, temp1, temp2, temp3);
    la_xvinsve0_d(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfmsubxxxpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFMSUB132PD:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMSUB231PD:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFMSUB213PD:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst = ir1_opnd_is_xmm(opnd0) ? la_vfmsub_d : la_xvfmsub_d;
    tr_inst(temp, temp1, temp2, temp3);
    la_xvori_b(dest, temp, 0);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfmsubxxxps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFMSUB132PS:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFMSUB231PS:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFMSUB213PS:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst = ir1_opnd_is_xmm(opnd0) ? la_vfmsub_s : la_xvfmsub_s;
    tr_inst(temp, temp1, temp2, temp3);
    la_xvori_b(dest, temp, 0);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfnmaddxxxss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFNMADD132SS:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFNMADD231SS:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFNMADD213SS:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    la_fneg_s(temp, temp1);
    la_fmadd_s(temp, temp, temp2, temp3);

    IR2_OPND label_over = ra_alloc_label();
    /* check if result is NaN */
    la_fcmp_cond_s(fcc0_ir2_opnd, temp, temp, 0x8);

    /* if no NaN happend, compution done */
    la_bceqz(fcc0_ir2_opnd, label_over);

    /* if INVALID NaN did happen, use original operands to generate correct NaN */
    la_fmsub_s(temp, src1, src2, dest);

    la_label(label_over);
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfnmaddxxxsd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFNMADD132SD:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFNMADD231SD:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFNMADD213SD:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    la_fneg_d(temp, temp1);
    la_fmadd_d(temp, temp, temp2, temp3);

    IR2_OPND label_over = ra_alloc_label();
    /* check if result is NaN */
    la_fcmp_cond_d(fcc0_ir2_opnd, temp, temp, 0x8);

    /* if no NaN happend, compution done */
    la_bceqz(fcc0_ir2_opnd, label_over);

    /* if INVALID NaN did happen, use original operands to generate correct NaN */
    la_fmsub_d(temp, src1, src2, dest);

    la_label(label_over);
    la_xvinsve0_d(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfnmaddxxxpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR2_OPND itemp = ra_alloc_itemp();
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFNMADD132PD:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFNMADD231PD:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFNMADD213PD:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst = ir1_opnd_is_xmm(opnd0) ? la_vfmadd_d : la_xvfmadd_d;

    /* change the first operand sign bit*/
    la_lu52i_d(itemp, zero_ir2_opnd, 0x800);
    la_vinsgr2vr_d(temp, itemp, 0);
    la_xvreplve0_d(temp, temp);
    la_xvxor_v(temp, temp1, temp);
    /* compute the result*/
    tr_inst(temp, temp, temp2, temp3);

    IR2_OPND mask = ra_alloc_ftemp();
    IR2_OPND src1_temp = ra_alloc_ftemp();
    IR2_OPND src2_temp = ra_alloc_ftemp();
    IR2_OPND src3_temp = ra_alloc_ftemp();

    /* check if result is NaN */
    la_xvfcmp_cond_d(mask, temp, temp, 0x8);
    la_xvand_v(src1_temp, mask, dest);
    la_xvand_v(src2_temp, mask, src1);
    la_xvand_v(src3_temp, mask, src2);
    tr_inst(src1_temp, src2_temp, src3_temp, src1_temp);

    la_xvbitsel_v(temp, temp, src1_temp, mask);
    la_xvori_b(dest, temp, 0);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);

    ra_free_temp(src1_temp);
    ra_free_temp(src2_temp);
    ra_free_temp(src3_temp);
    ra_free_temp(temp);
    ra_free_temp(mask);
    return true;
}

bool translate_vfnmaddxxxps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR2_OPND ftemp = ra_alloc_ftemp();
    IR2_OPND itemp = ra_alloc_itemp();
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFNMADD132PS:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFNMADD231PS:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFNMADD213PS:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst = ir1_opnd_is_xmm(opnd0) ? la_vfmadd_s : la_xvfmadd_s;
    la_lu12i_w(itemp, 0x80000);
    la_vinsgr2vr_w(ftemp, itemp, 0);
    la_xvreplve0_w(ftemp, ftemp);
    la_xvxor_v(temp, temp1, ftemp);

    /* compute the result*/
    tr_inst(temp, temp, temp2, temp3);

    IR2_OPND mask = ra_alloc_ftemp();
    IR2_OPND src1_temp = ra_alloc_ftemp();
    IR2_OPND src2_temp = ra_alloc_ftemp();
    IR2_OPND src3_temp = ra_alloc_ftemp();

    /* check if result is NaN */
    la_xvfcmp_cond_s(mask, temp, temp, 0x8);
    la_xvand_v(src1_temp, mask, dest);
    la_xvand_v(src2_temp, mask, src1);
    la_xvand_v(src3_temp, mask, src2);
    tr_inst(src1_temp, src2_temp, src3_temp, src1_temp);

    la_xvbitsel_v(temp, temp, src1_temp, mask);
    la_xvori_b(dest, temp, 0);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    ra_free_temp(src1_temp);
    ra_free_temp(src2_temp);
    ra_free_temp(src3_temp);
    ra_free_temp(temp);
    ra_free_temp(mask);
    return true;
}

bool translate_vfnmsubxxxss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFNMSUB132SS:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFNMSUB231SS:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFNMSUB213SS:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    la_fneg_s(temp, temp1);
    la_fmsub_s(temp, temp, temp2, temp3);

    IR2_OPND label_over = ra_alloc_label();
    /* check if result is NaN */
    la_fcmp_cond_s(fcc0_ir2_opnd, temp, temp, 0x8);

    /* if no NaN happend, compution done */
    la_bceqz(fcc0_ir2_opnd, label_over);

    /* if INVALID NaN did happen, use original operands to generate correct NaN */
    la_fmsub_s(temp, src1, src2, dest);

    la_label(label_over);
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfnmsubxxxsd(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFNMSUB132SD:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFNMSUB231SD:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFNMSUB213SD:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }
    /* if the dest is NaN , translation may take mistake
     * becasue x86 vfnmsub and 3a5000 vfmsub may produce different NaN*/
    la_fneg_d(temp, temp1);
    la_fmsub_d(temp, temp, temp2, temp3);

    IR2_OPND label_over = ra_alloc_label();
    /* check if result is NaN */
    la_fcmp_cond_d(fcc0_ir2_opnd, temp, temp, 0x8);

    /* if no NaN happend, compution done */
    la_bceqz(fcc0_ir2_opnd, label_over);

    /* if INVALID NaN did happen, use original operands to generate correct NaN */
    la_fmsub_d(temp, src1, src2, dest);

    la_label(label_over);
    la_xvinsve0_d(dest, temp, 0);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vfnmsubxxxpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR2_OPND ftemp = ra_alloc_ftemp();
    IR2_OPND itemp = ra_alloc_itemp();
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFNMSUB132PD:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFNMSUB231PD:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFNMSUB213PD:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }
    /* if the dest is NaN , translation may take mistake
     * becasue x86 vfnmsub and 3a5000 vfmsub may produce different NaN*/
    tr_inst = ir1_opnd_is_xmm(opnd0) ? la_vfmsub_d : la_xvfmsub_d;
    la_lu52i_d(itemp, zero_ir2_opnd, 0x800);
    la_vinsgr2vr_d(ftemp, itemp, 0);
    la_xvreplve0_d(ftemp, ftemp);
    la_xvxor_v(temp, temp1, ftemp);
    tr_inst(temp, temp, temp2, temp3);

    IR2_OPND mask = ra_alloc_ftemp();
    IR2_OPND src1_temp = ra_alloc_ftemp();
    IR2_OPND src2_temp = ra_alloc_ftemp();
    IR2_OPND src3_temp = ra_alloc_ftemp();

    /* check if result is NaN */
    la_xvfcmp_cond_d(mask, temp, temp, 0x8);
    la_xvand_v(src1_temp, mask, dest);
    la_xvand_v(src2_temp, mask, src1);
    la_xvand_v(src3_temp, mask, src2);
    tr_inst(src1_temp, src2_temp, src3_temp, src1_temp);

    la_xvbitsel_v(temp, temp, src1_temp, mask);
    la_xvori_b(dest, temp, 0);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    ra_free_temp(src1_temp);
    ra_free_temp(src2_temp);
    ra_free_temp(src3_temp);
    ra_free_temp(temp);
    ra_free_temp(mask);
    return true;
}

bool translate_vfnmsubxxxps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp1, temp2, temp3;
    IR2_OPND ftemp = ra_alloc_ftemp();
    IR2_OPND itemp = ra_alloc_itemp();
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VFNMSUB132PS:
            temp1 = dest, temp2 = src2, temp3 = src1;
            break;
        case dt_X86_INS_VFNMSUB231PS:
            temp1 = src1, temp2 = src2, temp3 = dest;
            break;
        case dt_X86_INS_VFNMSUB213PS:
            temp1 = src1, temp2 = dest, temp3 = src2;
            break;
        default:
            lsassert(0);
            break;
    }

    tr_inst = ir1_opnd_is_xmm(opnd0) ? la_vfmsub_s : la_xvfmsub_s;
    la_lu12i_w(itemp, 0x80000);
    la_vinsgr2vr_w(ftemp, itemp, 0);
    la_xvreplve0_w(ftemp, ftemp);
    la_xvxor_v(temp, temp1, ftemp);
    tr_inst(temp, temp, temp2, temp3);

    IR2_OPND mask = ra_alloc_ftemp();
    IR2_OPND src1_temp = ra_alloc_ftemp();
    IR2_OPND src2_temp = ra_alloc_ftemp();
    IR2_OPND src3_temp = ra_alloc_ftemp();

    /* check if result is NaN */
    la_xvfcmp_cond_s(mask, temp, temp, 0x8);
    la_xvand_v(src1_temp, mask, dest);
    la_xvand_v(src2_temp, mask, src1);
    la_xvand_v(src3_temp, mask, src2);
    tr_inst(src1_temp, src2_temp, src3_temp, src1_temp);

    la_xvbitsel_v(temp, temp, src1_temp, mask);
    la_xvori_b(dest, temp, 0);
    if (ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    ra_free_temp(src1_temp);
    ra_free_temp(src2_temp);
    ra_free_temp(src3_temp);
    ra_free_temp(temp);
    ra_free_temp(mask);
    return true;
}

bool translate_vpbroadcastq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src;
    if (ir1_opnd_is_mem(opnd1)) {
        src = load_freg128_from_ir1(opnd1);
    } else if (ir1_opnd_is_xmm(opnd1)) {
        src = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    } else {
        lsassert(0);
    }

    if (ir1_opnd_is_xmm(opnd0)) {
        la_xvreplve0_d(dest, src);
        set_high128_xreg_to_zero(dest);
    } else if (ir1_opnd_is_ymm(opnd0)) {
        la_xvreplve0_d(dest, src);
        la_xvinsve0_d(dest, dest, 2);
        la_xvinsve0_d(dest, dest, 3);
    } else {
        lsassert(0);
    }
//    if (ir1_opnd_is_xmm(opnd0)) {
//        set_high128_xreg_to_zero(dest);
//    }


    return true;
}

bool translate_vpaddq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));

    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        la_vadd_d(dest, src1, src2);
        set_high128_xreg_to_zero(dest);
    } else if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        la_xvadd_d(dest, src1, src2);
    } else {
        lsassert(0);
    }
    return true;
}

bool translate_vzeroupper(IR1_INST *pir1)
{
    int reg_xmm = 8;
#ifdef TARGET_X86_64
    reg_xmm = 16;
#endif

    for (int i = 0; i < reg_xmm; ++i) {
        IR2_OPND dest = ra_alloc_xmm(i);
        set_high128_xreg_to_zero(dest);
    }

    return true;
}
#if 0
bool translate_vpinsrb(IR1_INST *pir1)
{
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src0 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src1 = load_ireg_from_ir1(ir1_get_opnd(pir1, 2), UNKNOWN_EXTENSION, false);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    la_vand_v(dest, src0, src0);
    la_vinsgr2vr_b(dest, src1, imm);
    set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vpinsrq(IR1_INST *pir1)
{
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src0 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src1 = load_ireg_from_ir1(ir1_get_opnd(pir1, 2), UNKNOWN_EXTENSION, false);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    la_vand_v(dest, src0, src0);
    la_vinsgr2vr_d(dest, src1, imm);
    set_high128_xreg_to_zero(dest);
    return true;
}
#endif

bool translate_xgetbv(IR1_INST *pir1)
{
    tr_gen_call_to_helper_xgetbv();
    return true;
}

bool translate_xsetbv(IR1_INST *pir1)
{
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND ecx_opnd = ra_alloc_gpr(ecx_index);
    IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
    IR2_OPND temp_rfbm = ra_alloc_itemp();

    la_bstrins_d(temp_rfbm, eax_opnd, 31, 0);
    la_bstrins_d(temp_rfbm, edx_opnd, 63, 32);
    tr_gen_call_to_helper_vfll((ADDR)helper_xsetbv, ecx_opnd, temp_rfbm, 0);
    return true;
}

bool translate_xsave(IR1_INST *pir1)
{
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
    IR2_OPND temp_rfbm = ra_alloc_itemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    la_bstrins_d(temp_rfbm, eax_opnd, 31, 0);
    la_bstrins_d(temp_rfbm, edx_opnd, 63, 32);
    tr_gen_call_to_helper_vfll((ADDR)helper_xsave, mem_opnd, temp_rfbm, 1);
    return true;
}

bool translate_xsaveopt(IR1_INST *pir1)
{
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
    IR2_OPND temp_rfbm = ra_alloc_itemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    la_bstrins_d(temp_rfbm, eax_opnd, 31, 0);
    la_bstrins_d(temp_rfbm, edx_opnd, 63, 32);
    tr_gen_call_to_helper_vfll((ADDR)helper_xsaveopt, mem_opnd, temp_rfbm, 1);
    return true;
}

bool translate_xrstor(IR1_INST *pir1)
{
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
    IR2_OPND temp_rfbm = ra_alloc_itemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    la_bstrins_d(temp_rfbm, eax_opnd, 31, 0);
    la_bstrins_d(temp_rfbm, edx_opnd, 63, 32);
    tr_gen_call_to_helper_vfll((ADDR)helper_xrstor, mem_opnd, temp_rfbm, 1);
    return true;
}

#endif


bool translate_andn(IR1_INST *pir1)
{
    IR2_OPND dest = load_ireg_from_ir1(ir1_get_opnd(pir1, 0),
        UNKNOWN_EXTENSION, false);
    IR2_OPND src0 = load_ireg_from_ir1(ir1_get_opnd(pir1, 1),
        UNKNOWN_EXTENSION, false);
    IR2_OPND src1 = load_ireg_from_ir1(ir1_get_opnd(pir1, 2),
        UNKNOWN_EXTENSION, false);
    int opnd0_size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    IR2_OPND temp = ra_alloc_itemp();
    la_orn(temp, zero_ir2_opnd, src0);
    generate_eflag_calculation(dest, temp, src1, pir1, false);
    la_andn(temp, src1, src0);
    if (opnd0_size == 32) {
        la_bstrpick_d(dest, temp, 31, 0);
    } else {
        la_ori(dest, temp, 0);
    }

    return true;
}

bool translate_movbe(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        lsassert(0);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;

    opnd0_size = ir1_opnd_size(opnd0);

    /* get src1 */
    src = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        dest = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
    } else {
        dest = ra_alloc_itemp();
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(dest, mem_opnd, imm, opnd0_size);
    }

    /* set eflag */
    generate_eflag_calculation(dest, dest, src, pir1, true);

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
    if (ir1_opnd_is_gpr(opnd0)) {
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
#ifdef CONFIG_LATX_AVX_OPT
bool translate_vpbroadcastd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src;
    if (ir1_opnd_is_mem(opnd1)) {
        src = load_freg128_from_ir1(opnd1);
    } else if (ir1_opnd_is_xmm(opnd1)) {
        src = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    } else {
        lsassert(0);
    }

    if (ir1_opnd_is_xmm(opnd0)) {
        la_xvreplve0_w(dest, src);
    } else if (ir1_opnd_is_ymm(opnd0)) {
        la_xvreplve0_w(dest, src);
        la_xvinsve0_d(dest, dest, 2);
        la_xvinsve0_d(dest, dest, 3);
    } else {
        lsassert(0);
    }

    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }

    return true;
}
#if 0
bool translate_vpmaxsd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        la_vmax_w(dest, src1, src2);
        set_high128_xreg_to_zero(dest);
    } else if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src1 = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
        la_xvmax_w(dest, src1, src2);
    } else {
        lsassert(0);
    }
    return true;
}

bool translate_vpminsd(IR1_INST *pir1)
{
    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
        la_vmin_w(dest, src1, src2);
        set_high128_xreg_to_zero(dest);
    } else if (ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0))) {
        IR2_OPND dest = load_freg256_from_ir1(ir1_get_opnd(pir1, 0));
        IR2_OPND src1 = load_freg256_from_ir1(ir1_get_opnd(pir1, 1));
        IR2_OPND src2 = load_freg256_from_ir1(ir1_get_opnd(pir1, 2));
        la_xvmin_w(dest, src1, src2);
    } else {
        lsassert(0);
    }
    return true;
}
#endif
bool translate_vrsqrtss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();

    /* this x86 instruction has no exception ,so we need mask all exception */
    la_frsqrt_s(temp, src2);
    la_xvori_b(dest, src1, 0x0);
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);

    return true;
}

bool translate_vrsqrtps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);

    /* this x86 instruction has no exception ,so we need mask all exception */
    if (ir1_opnd_is_xmm(opnd0)) {
        la_vfrsqrt_s(dest, src);
        set_high128_xreg_to_zero(dest);
    } else {
        la_xvfrsqrt_s(dest, src);
    }

    return true;
}

bool translate_vrcpps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);

    /* this x86 instruction has no exception ,so we need mask all exception */
    if (ir1_opnd_is_xmm(opnd0)) {
        la_vfrecip_s(dest, src);
        set_high128_xreg_to_zero(dest);
    } else {
        la_xvfrecip_s(dest, src);
    }
    return true;
}

bool translate_vrcpss(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src1 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src2 = load_freg128_from_ir1(ir1_get_opnd(pir1, 2));
    IR2_OPND temp = ra_alloc_ftemp();

    /* this x86 instruction has no exception ,so we need mask all exception */
    la_frecip_s(temp, src2);
    la_xvori_b(dest, src1, 0x0);
    la_xvinsve0_w(dest, temp, 0);
    set_high128_xreg_to_zero(dest);

    return true;
}

bool translate_vpsignb(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        la_vsigncov_b(dest, src2, src1);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        la_xvsigncov_b(dest, src2, src1);
    }
    return true;
}

bool translate_vpsignd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        la_vsigncov_w(dest, src2, src1);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        la_xvsigncov_w(dest, src2, src1);
    }
    return true;
}

bool translate_vpsignw(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        la_vsigncov_h(dest, src2, src1);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        la_xvsigncov_h(dest, src2, src1);
    }
    return true;
}

bool translate_vpmulhw(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        la_vmuh_h(dest, src1, src2);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        la_xvmuh_h(dest, src1, src2);
    }
    return true;
}

bool translate_vpmulhuw(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        la_vmuh_hu(dest, src1, src2);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        la_xvmuh_hu(dest, src1, src2);
    }
    return true;
}

bool translate_vpmaddwd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp = ra_alloc_ftemp();

        la_vxor_v(temp, temp, temp);
        la_vmaddwev_w_h(temp, src1, src2);
        la_vmaddwod_w_h(temp, src1, src2);
        la_vbsll_v(dest, temp, 0);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp = ra_alloc_ftemp();

        la_xvxor_v(temp, temp, temp);
        la_xvmaddwev_w_h(temp, src1, src2);
        la_xvmaddwod_w_h(temp, src1, src2);
        la_xvbsll_v(dest, temp, 0);
    }
    return true;
}

bool translate_vpmaddubsw(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);

        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        IR2_OPND temp3 = ra_alloc_ftemp();
        IR2_OPND temp4 = ra_alloc_ftemp();
        IR2_OPND temp5 = ra_alloc_ftemp();
        IR2_OPND itmp = ra_alloc_itemp();
        /* unsigned src1 * signed src2 */
        la_vreplgr2vr_d(temp1, zero_ir2_opnd);
        la_vabsd_b(temp3, src2, temp1);
        la_vmaddwev_h_bu(temp1, src1, temp3);
        la_vreplgr2vr_d(temp2, zero_ir2_opnd);
        la_vmaddwod_h_bu(temp2, src1, temp3);

        la_ori(itmp, zero_ir2_opnd, 0x1);
        la_vreplgr2vr_b(temp3, itmp);
        la_vsigncov_b(temp4, src2, temp3);

        la_vmulwev_h_b(temp5, temp4, temp3);
        la_vmulwod_h_b(temp3, temp4, temp3);

        la_vmul_h(temp1, temp1, temp5);
        la_vmul_h(temp2, temp2, temp3);
        la_vsadd_h(dest, temp2, temp1);

        set_high128_xreg_to_zero(dest);

    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        IR2_OPND temp3 = ra_alloc_ftemp();
        IR2_OPND temp4 = ra_alloc_ftemp();
        IR2_OPND temp5 = ra_alloc_ftemp();
        IR2_OPND itmp = ra_alloc_itemp();
        /* unsigned src1 * signed src2 */
        la_xvreplgr2vr_d(temp1, zero_ir2_opnd);
        la_xvabsd_b(temp3, src2, temp1);
        la_xvmaddwev_h_bu(temp1, src1, temp3);
        la_xvreplgr2vr_d(temp2, zero_ir2_opnd);
        la_xvmaddwod_h_bu(temp2, src1, temp3);

        la_ori(itmp, zero_ir2_opnd, 0x1);
        la_xvreplgr2vr_b(temp3, itmp);
        la_xvsigncov_b(temp4, src2, temp3);

        la_xvmulwev_h_b(temp5, temp4, temp3);
        la_xvmulwod_h_b(temp3, temp4, temp3);

        la_xvmul_h(temp1, temp1, temp5);
        la_xvmul_h(temp2, temp2, temp3);
        la_xvsadd_h(dest, temp2, temp1);
    }
    return true;
}

bool translate_vphsubw(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_vhsubw_w_h(temp1, src1, src1);
        la_vneg_h(temp1, temp1);
        la_vhsubw_w_h(temp2, src2, src2);
        la_vneg_h(temp2, temp2);

        la_vpickev_h(dest, temp2, temp1);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_xvhsubw_w_h(temp1, src1, src1);
        la_xvneg_h(temp1, temp1);
        la_xvhsubw_w_h(temp2, src2, src2);
        la_xvneg_h(temp2, temp2);

        la_xvpickev_h(dest, temp2, temp1);
    }
    return true;
}

bool translate_vphsubd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_vhsubw_d_w(temp1, src1, src1);
        la_vneg_w(temp1, temp1);
        la_vhsubw_d_w(temp2, src2, src2);
        la_vneg_w(temp2, temp2);

        la_vpickev_w(dest, temp2, temp1);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_xvhsubw_d_w(temp1, src1, src1);
        la_xvneg_w(temp1, temp1);
        la_xvhsubw_d_w(temp2, src2, src2);
        la_xvneg_w(temp2, temp2);

        la_xvpickev_w(dest, temp2, temp1);
    }
    return true;
}

bool translate_vphsubsw(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        IR2_OPND temp3 = ra_alloc_ftemp();
        IR2_OPND temp4 = ra_alloc_ftemp();

        la_vpickev_h(temp1, src1, src1);
        la_vpickod_h(temp2, src1, src1);
        la_vssub_h(temp3, temp1, temp2); //temp1-temp2

        la_vpickev_h(temp1, src2, src2);
        la_vpickod_h(temp2, src2, src2);
        la_vssub_h(temp4, temp1, temp2); //temp1-temp2

        la_vshuf4i_d(temp4, temp3, 0b00000110);
        la_vori_b(dest, temp4, 0x0);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        IR2_OPND temp3 = ra_alloc_ftemp();
        IR2_OPND temp4 = ra_alloc_ftemp();
        la_xvpickev_h(temp1, src1, src1);
        la_xvpickod_h(temp2, src1, src1);
        la_xvssub_h(temp3, temp1, temp2); //temp1-temp2

        la_xvpickev_h(temp1, src2, src2);
        la_xvpickod_h(temp2, src2, src2);
        la_xvssub_h(temp4, temp1, temp2); //temp1-temp2

        la_xvshuf4i_d(temp4, temp3, 0b00000110);
        la_xvori_b(dest, temp4, 0x0);
    }
    return true;
}


bool translate_vpmaxxx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    tr_inst = NULL;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VPMAXSD:
            tr_inst = la_xvmax_w;
            break;
        case dt_X86_INS_VPMAXSW:
            tr_inst = la_xvmax_h;
            break;
        case dt_X86_INS_VPMAXSB:
            tr_inst = la_xvmax_b;
            break;
        case dt_X86_INS_VPMAXUD:
            tr_inst = la_xvmax_wu;
            break;
        case dt_X86_INS_VPMAXUW:
            tr_inst = la_xvmax_hu;
            break;
        case dt_X86_INS_VPMAXUB:
            tr_inst = la_xvmax_bu;
            break;
        default:
            break;
    }

    if (ir1_opnd_is_xmm(opnd0)) {

        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        tr_inst(dest, src1, src2);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        tr_inst(dest, src1, src2);
    }

    return true;
}

bool translate_vpminxx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    tr_inst = NULL;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VPMINSD:
            tr_inst = la_xvmin_w;
            break;
        case dt_X86_INS_VPMINSW:
            tr_inst = la_xvmin_h;
            break;
        case dt_X86_INS_VPMINSB:
            tr_inst = la_xvmin_b;
            break;
        default:
            break;
    }

    if (ir1_opnd_is_xmm(opnd0)) {

        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        tr_inst(dest, src1, src2);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        tr_inst(dest, src1, src2);
    }

    return true;
}

bool translate_vpmuldq(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));

    if (ir1_opnd_is_xmm(opnd0)) {

        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        la_vmulwev_d_w(dest, src1, src2);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        la_xvmulwev_d_w(dest, src1, src2);
    }

    return true;
}


bool translate_vpshufd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        uint8_t imm = ir1_opnd_uimm(opnd2);
        la_xvori_b(dest, src1, 0x00);
        la_vpermi_w(dest, src1, imm);
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        uint8_t imm = ir1_opnd_uimm(opnd2);
        la_xvori_b(dest, src1, 0x00);
        la_xvpermi_w(dest, src1, imm);
    }
    return true;
}


bool translate_vpmaskmovx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR2_INST * ( * tr_inst1)(IR2_OPND, IR2_OPND);
    IR2_INST * ( * tr_inst2)(IR2_OPND, IR2_OPND, int);
    tr_inst1 = NULL;
    tr_inst2 = NULL;
    IR1_OPCODE op = ir1_opcode(pir1);
    if (ir1_opnd_is_xmm(opnd1)) {
        switch (op) {
            case dt_X86_INS_VPMASKMOVD:
                tr_inst1 = la_vclz_w;
                tr_inst2 = la_vseqi_w;
                break;
            case dt_X86_INS_VPMASKMOVQ:
                tr_inst1 = la_vclz_d;
                tr_inst2 = la_vseqi_d;
                break;
            default:
                break;

        }
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND dest_temp = ra_alloc_ftemp();
        la_xvori_b(dest_temp, dest, 0x0);

        tr_inst1(temp1, src1);
        tr_inst2(temp1, temp1, 0x0);
        la_vand_v(dest, temp1, src2);
        if (ir1_opnd_is_mem(opnd0)) {

            la_vandn_v(temp1, temp1, dest_temp);
            la_vxor_v(dest, dest, temp1);
            store_freg128_to_ir1_mem(dest, opnd0);
        } else {
            set_high128_xreg_to_zero(dest);
        }

    } else if (ir1_opnd_is_ymm(opnd1)) {
        switch (op) {
            case dt_X86_INS_VPMASKMOVD:
                tr_inst1 = la_xvclz_w;
                tr_inst2 = la_xvseqi_w;
                break;
            case dt_X86_INS_VPMASKMOVQ:
                tr_inst1 = la_xvclz_d;
                tr_inst2 = la_xvseqi_d;
                break;
            default:
                break;

        }
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND dest_temp = ra_alloc_ftemp();
        la_xvori_b(dest_temp, dest, 0x0);

        tr_inst1(temp1, src1);
        tr_inst2(temp1, temp1, 0x0);
        la_xvand_v(dest, temp1, src2);
        if (ir1_opnd_is_ymm(opnd2)) {
            la_xvandn_v(temp1, temp1, dest_temp);
            la_xvxor_v(dest, dest, temp1);
            store_freg256_to_ir1_mem(dest, opnd0);
        }
    }
    return true;
}


bool translate_vpinsrx(IR1_INST * pir1) {

    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND * opnd3 = ir1_get_opnd(pir1, 3);

    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND src2 = load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);
    uint8_t imm = ir1_opnd_uimm(opnd3);

    IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, int);
    tr_inst = NULL;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VPINSRB:
            imm %= 0b10000;
            tr_inst = la_vinsgr2vr_b;
            break;

        case dt_X86_INS_VPINSRW:
            imm %= 0b1000;
            tr_inst = la_vinsgr2vr_h;
            break;

        case dt_X86_INS_VPINSRD:
            imm %= 0b100;
            tr_inst = la_vinsgr2vr_w;
            break;

        case dt_X86_INS_VPINSRQ:
            imm %= 0b10;
            tr_inst = la_vinsgr2vr_d;
            break;

        default:
            break;

    }

    la_vori_b(dest, src1, 0x0);
    tr_inst(dest, src2, imm);
    set_high128_xreg_to_zero(dest);
    return true;
}


bool translate_vpsllvd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);

        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_vslei_wu(temp, src2, 31);
        la_vand_v(temp2, temp, src2);
        la_vsll_w(dest, src1, temp2);
        la_vand_v(dest, dest, temp);
        set_high128_xreg_to_zero(dest);
    } else if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);

        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_xvslei_wu(temp, src2, 31);
        la_xvand_v(temp2, temp, src2);
        la_xvsll_w(dest, src1, temp2);
        la_xvand_v(dest, dest, temp);
    }

    return true;
}

bool translate_vpsllvq(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);

        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_vandi_b(temp,temp,0);
        la_vldi(temp,0b0110000111111);

        la_vsle_du(temp, src2, temp);
        la_vand_v(temp2, temp, src2);
        la_vsll_d(dest, src1, temp2);
        la_vand_v(dest, dest, temp);
        set_high128_xreg_to_zero(dest);
    } else if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);

        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        la_xvandi_b(temp,temp,0);
        la_xvldi(temp,0b0110000111111);

        la_xvsle_du(temp, src2, temp);
        la_xvand_v(temp2, temp, src2);
        la_xvsll_d(dest, src1, temp2);
        la_xvand_v(dest, dest, temp);
    }

    return true;
}

bool translate_vpsravd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp01 = ra_alloc_ftemp();
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp1 = ra_alloc_ftemp();

        la_vclz_w(temp01, src1);
        la_vseqi_w(temp01, temp01, 0x0);

        la_vslei_wu(temp, src2, 31);
        la_vand_v(temp1, temp, src2);
        la_vsra_w(temp1, src1, temp1);
        la_vbitsel_v(dest, temp01, temp1, temp);

        set_high128_xreg_to_zero(dest);
    } else if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp01 = ra_alloc_ftemp();
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND temp1 = ra_alloc_ftemp();

        la_xvclz_w(temp01, src1);
        la_xvseqi_w(temp01, temp01, 0x0);

        la_xvslei_wu(temp, src2, 31);
        la_xvand_v(temp1, temp, src2);
        la_xvsra_w(temp1, src1, temp1);
        la_xvbitsel_v(dest, temp01, temp1, temp);
    }

    return true;
}

bool translate_vpermpx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2;
    uint8_t imm;
    IR1_OPCODE op = ir1_opcode(pir1);
    switch (op) {
        case dt_X86_INS_VPERMPD:
            imm = ir1_opnd_uimm(opnd2);
            la_xvpermi_d(dest, src1, imm);
            break;

        case dt_X86_INS_VPERMPS:
            src2 = load_freg256_from_ir1(opnd2);
            la_xvperm_w(dest, src2, src1);
            break;
        default:
            break;
    }

    return true;
}


bool translate_vpermilps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        if (ir1_opnd_is_imm(opnd2)) {
            uint8_t imm = ir1_opnd_uimm(opnd2);
            la_vshuf4i_w(dest, src1, imm);

        } else {
            IR2_OPND src2 = load_freg128_from_ir1(opnd2);
            IR2_OPND temp = ra_alloc_ftemp();
            la_vandi_b(temp, src2, 0b00000011);
            la_vshuf_w(temp, src2, src1);
            la_vori_b(dest, temp, 0x0);
        }
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        if (ir1_opnd_is_imm(opnd2)) {
            uint8_t imm = ir1_opnd_uimm(opnd2);
            la_xvshuf4i_w(dest, src1, imm);

        } else {
            IR2_OPND src2 = load_freg256_from_ir1(opnd2);
            IR2_OPND temp = ra_alloc_ftemp();
            la_xvandi_b(temp, src2, 0b00000011);
            la_xvshuf_w(temp, src2, src1);
            la_xvori_b(dest, temp, 0x0);

        }

    }
    return true;
}

bool translate_vpermilpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        if (ir1_opnd_is_imm(opnd2)) {
            uint8_t imm = ir1_opnd_uimm(opnd2);
            imm = 0b10100000 + ((imm & 0b00001000) << 3) + ((imm & 0b00000100) << 2) + ((imm & 0b00000010) << 1) + (imm & 0b00000001);
            la_xvpermi_d(dest, src1, imm);

        } else {
            IR2_OPND src2 = load_freg128_from_ir1(opnd2);
            IR2_OPND temp = ra_alloc_ftemp();
            la_vsrli_d(temp, src2, 1);
            la_vandi_b(temp, temp, 0b00000001);
            la_vshuf_d(temp, src2, src1);
            la_vori_b(dest, temp, 0x0);

        }
        set_high128_xreg_to_zero(dest);
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        if (ir1_opnd_is_imm(opnd2)) {
            uint8_t imm = ir1_opnd_uimm(opnd2);
            imm = 0b10100000 + ((imm & 0b00001000) << 3) + ((imm & 0b00000100) << 2) + ((imm & 0b00000010) << 1) + (imm & 0b00000001);
            la_xvpermi_d(dest, src1, imm);

        } else {
            IR2_OPND src2 = load_freg256_from_ir1(opnd2);
            IR2_OPND temp = ra_alloc_ftemp();
            la_xvsrli_d(temp, src2, 1);
            la_xvandi_b(temp, temp, 0b00000001);
            la_xvshuf_d(temp, src2, src1);
            la_xvori_b(dest, temp, 0x0);

        }

    }

    return true;
}

bool translate_vpalignr(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND * opnd3 = ir1_get_opnd(pir1, 3);

    uint8_t imm = ir1_opnd_uimm(opnd3);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);

        /* fast path */
        if (imm >= 32) {
            la_vxor_v(dest, dest, dest);
        } else if (imm >= 16 && imm < 32) {
            la_vbsrl_v(dest, src1, imm - 16);
            set_high128_xreg_to_zero(dest);
        } else {
            /* slow path */
            if (imm == 0) {
                la_vori_b(dest, src2, 0);
                set_high128_xreg_to_zero(dest);
            } else {
                IR2_OPND temp_src2 = ra_alloc_ftemp();
                la_vbsrl_v(temp_src2, src2, imm);
                la_vbsll_v(dest, src1, 16 - imm);
                la_vor_v(dest, temp_src2, dest);
                set_high128_xreg_to_zero(dest);
            }
        }
    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);

        /* fast path */
        if (imm >= 32) {
            la_vxor_v(dest, dest, dest);
        } else if (imm >= 16 && imm < 32) {
            la_xvbsrl_v(dest, src1, imm - 16);
        } else {
            /* slow path */
            if (imm == 0) {
                la_vori_b(dest, src2, 0);
            } else {
                IR2_OPND temp_src2 = ra_alloc_ftemp();
                la_xvbsrl_v(temp_src2, src2, imm);
                la_xvbsll_v(dest, src1, 16 - imm);
                la_xvor_v(dest, temp_src2, dest);

            }
        }
    }
    return true;
}

bool translate_vphminposuw(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
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
    la_vandi_b(temp2, temp2, 0x0);
    la_vfrstp_h(temp2, temp1, temp2);
    la_vpackev_h(dest, temp2, temp);
    la_xvpickve_w(dest, dest, 0);
    return true;
}

bool translate_vpmulhrsw(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        IR2_OPND temp3 = ra_alloc_ftemp();
        la_vmulwev_w_h(temp1, src1, src2);
        la_vmulwod_w_h(temp2, src1, src2);

        la_vsrai_w(temp1, temp1, 0xe);
        la_vsrai_w(temp2, temp2, 0xe);

        la_vandi_b(temp3, temp3, 0x0);
        la_vbitseti_w(temp3, temp3, 0);

        la_vadd_w(temp1, temp1, temp3);
        la_vadd_w(temp2, temp2, temp3);

        la_vsrai_w(temp1, temp1, 0x1);
        la_vsrai_w(temp2, temp2, 0x1);

        la_vpackev_h(dest, temp2, temp1);

        set_high128_xreg_to_zero(dest);

    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp1 = ra_alloc_ftemp();
        IR2_OPND temp2 = ra_alloc_ftemp();
        IR2_OPND temp3 = ra_alloc_ftemp();
        la_xvmulwev_w_h(temp1, src1, src2);
        la_xvmulwod_w_h(temp2, src1, src2);

        la_xvsrai_w(temp1, temp1, 0xe);
        la_xvsrai_w(temp2, temp2, 0xe);

        la_xvandi_b(temp3, temp3, 0x0);
        la_xvbitseti_w(temp3, temp3, 0);

        la_xvadd_w(temp1, temp1, temp3);
        la_xvadd_w(temp2, temp2, temp3);

        la_xvsrai_w(temp1, temp1, 0x1);
        la_xvsrai_w(temp2, temp2, 0x1);

        la_xvpackev_h(dest, temp2, temp1);
    }

    return true;
}


bool translate_vzeroall(IR1_INST * pir1) {
    int reg_xmm = 8;
    #ifdef TARGET_X86_64
    reg_xmm = 16;
    #endif

    for (int i = 0; i < reg_xmm; ++i) {
        IR2_OPND dest = ra_alloc_xmm(i);
        la_xvandi_b(dest, dest, 0x0);
    }

    return true;
}

bool translate_vtestps(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src = load_freg128_from_ir1(opnd1);

        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND n4095_opnd = ra_alloc_num_4095();
        IR2_OPND label_1 = ra_alloc_label();
        IR2_OPND label_2 = ra_alloc_label();

        la_x86mtflag(zero_ir2_opnd, 0x3f);
        la_vand_v(temp, dest, src);
        la_vsrli_w(temp, temp, 0x1f);
        la_vseteqz_v(fcc0_ir2_opnd, temp); //temp==0 fcc=1;
        la_bceqz(fcc0_ir2_opnd, label_1); //fcc==1,not jump
        la_x86mtflag(n4095_opnd, ZF_USEDEF_BIT);

        la_label(label_1);
        la_vandn_v(temp, dest, src);
        la_vsrli_w(temp, temp, 0x1f);
        la_vseteqz_v(fcc0_ir2_opnd, temp); //temp==0 fcc=1;
        la_bceqz(fcc0_ir2_opnd, label_2); //fcc==1,not jump
        la_x86mtflag(n4095_opnd, CF_USEDEF_BIT);
        la_label(label_2);

    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src = load_freg256_from_ir1(opnd1);

        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND n4095_opnd = ra_alloc_num_4095();
        IR2_OPND label_1 = ra_alloc_label();
        IR2_OPND label_2 = ra_alloc_label();

        la_x86mtflag(zero_ir2_opnd, 0x3f);
        la_xvand_v(temp, dest, src);
        la_xvsrli_w(temp, temp, 0x1f);
        la_xvseteqz_v(fcc0_ir2_opnd, temp); //temp==0 fcc=1;
        la_bceqz(fcc0_ir2_opnd, label_1); //fcc==1,not jump
        la_x86mtflag(n4095_opnd, ZF_USEDEF_BIT);

        la_label(label_1);
        la_xvandn_v(temp, dest, src);
        la_xvsrli_w(temp, temp, 0x1f);
        la_xvseteqz_v(fcc0_ir2_opnd, temp); //temp==0 fcc=1;
        la_bceqz(fcc0_ir2_opnd, label_2); //fcc==1,not jump
        la_x86mtflag(n4095_opnd, CF_USEDEF_BIT);
        la_label(label_2);
    }

    return true;
}

bool translate_vtestpd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src = load_freg128_from_ir1(opnd1);

        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND n4095_opnd = ra_alloc_num_4095();
        IR2_OPND label_1 = ra_alloc_label();
        IR2_OPND label_2 = ra_alloc_label();

        la_x86mtflag(zero_ir2_opnd, 0x3f);
        la_vand_v(temp, dest, src);
        la_vsrli_d(temp, temp, 0x3f);
        la_vseteqz_v(fcc0_ir2_opnd, temp); //temp==0 fcc=1;
        la_bceqz(fcc0_ir2_opnd, label_1); //fcc==1,not jump
        la_x86mtflag(n4095_opnd, ZF_USEDEF_BIT);

        la_label(label_1);
        la_vandn_v(temp, dest, src);
        la_vsrli_d(temp, temp, 0x3f);
        la_vseteqz_v(fcc0_ir2_opnd, temp); //temp==0 fcc=1;
        la_bceqz(fcc0_ir2_opnd, label_2); //fcc==1,not jump
        la_x86mtflag(n4095_opnd, CF_USEDEF_BIT);
        la_label(label_2);

    } else {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src = load_freg256_from_ir1(opnd1);

        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND n4095_opnd = ra_alloc_num_4095();
        IR2_OPND label_1 = ra_alloc_label();
        IR2_OPND label_2 = ra_alloc_label();

        la_x86mtflag(zero_ir2_opnd, 0x3f);
        la_xvand_v(temp, dest, src);
        la_xvsrli_d(temp, temp, 0x3f);
        la_xvseteqz_v(fcc0_ir2_opnd, temp); //temp==0 fcc=1;
        la_bceqz(fcc0_ir2_opnd, label_1); //fcc==1,not jump
        la_x86mtflag(n4095_opnd, ZF_USEDEF_BIT);

        la_label(label_1);
        la_xvandn_v(temp, dest, src);
        la_xvsrli_d(temp, temp, 0x3f);
        la_xvseteqz_v(fcc0_ir2_opnd, temp); //temp==0 fcc=1;
        la_bceqz(fcc0_ir2_opnd, label_2); //fcc==1,not jump
        la_x86mtflag(n4095_opnd, CF_USEDEF_BIT);
        la_label(label_2);

    }

    return true;
}

bool translate_vpackssxx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND temp;
    IR2_INST * ( * cvt_inst)(IR2_OPND, IR2_OPND, int);
    switch (ir1_opcode(pir1)) {
        case dt_X86_INS_VPACKSSDW:
            cvt_inst = la_xvssrani_h_w;
            break;
        case dt_X86_INS_VPACKSSWB:
            cvt_inst = la_xvssrani_b_h;
            break;
        default:
            cvt_inst = NULL;
            lsassert(0);
            break;
    }
    if (ir1_opnd_is_xmm(opnd2) || ir1_opnd_is_ymm(opnd2)) {
        temp = ra_alloc_ftemp();
        la_xvori_b(temp, src2, 0);
    } else {
        temp = src2;
    }
    cvt_inst(temp, src1, 0);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(temp);
    }
    la_xvori_b(dest, temp, 0);
    return true;
}

bool translate_vpshufhw(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
             ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) );
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_OPND temp = ra_alloc_ftemp();
    uint64_t imm8 = ir1_opnd_uimm(opnd2);
    la_xvshuf4i_h(temp, src, imm8);
    la_xvshuf4i_d(temp, src, 0x66);
    la_xvori_b(dest, temp, 0);
    if(ir1_opnd_is_xmm(opnd0)){
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpshuflw(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
             ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) );
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    IR2_OPND temp = ra_alloc_ftemp();
    uint64_t imm8 = ir1_opnd_uimm(opnd2);
    la_xvshuf4i_h(temp, src, imm8);
    la_xvshuf4i_d(temp, src, 0xcc);
    la_xvori_b(dest, temp, 0);
    if(ir1_opnd_is_xmm(opnd0)){
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpavgb(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    la_xvavgr_bu(dest, src1, src2);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpavgw(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);

    la_xvavgr_hu(dest, src1, src2);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}


bool translate_vdppd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND *opnd3 = ir1_get_opnd(pir1, 3);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm(opnd3);
    la_xvxor_v(temp1, temp1, temp1);
    la_xvxor_v(temp2, temp2, temp2);
    if(imm & 0x10){
        la_xvextrins_d(temp1, src1, 0x00);
        la_xvextrins_d(temp2, src2, 0x00);
    }
    if(imm & 0x20){
        la_xvextrins_d(temp1, src1, 0x11);
        la_xvextrins_d(temp2, src2, 0x11);
    }
    if(ir1_opnd_is_xmm(opnd0))
        la_vfmul_d(temp1, temp1, temp2);
    else
        la_xvfmul_d(temp1, temp1, temp2);
    la_xvpackod_d(temp2, temp1, temp1);
    la_xvpackev_d(temp1, temp1, temp1);
    if(ir1_opnd_is_xmm(opnd0))
        la_vfadd_d(temp1, temp1, temp2);
    else
        la_xvfadd_d(temp1, temp1, temp2);
    la_xvxor_v(dest, dest, dest);
    if(imm & 0x1){
        la_xvextrins_d(dest, temp1, 0x00);
    }
    if(imm & 0x2){
        la_xvextrins_d(dest, temp1, 0x11);
    }
    if(ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vdpps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND *opnd3 = ir1_get_opnd(pir1, 3);
    lsassert(ir1_opnd_is_xmm(opnd0) || ir1_opnd_is_ymm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    uint8_t imm = ir1_opnd_uimm(opnd3);
    la_xvxor_v(temp1, temp1, temp1);
    la_xvxor_v(temp2, temp2, temp2);
    if(imm & 0x10){
        la_xvextrins_w(temp1, src1, 0x00);
        la_xvextrins_w(temp2, src2, 0x00);
    }
    if(imm & 0x20){
        la_xvextrins_w(temp1, src1, 0x11);
        la_xvextrins_w(temp2, src2, 0x11);
    }
    if(imm & 0x40){
        la_xvextrins_w(temp1, src1, 0x22);
        la_xvextrins_w(temp2, src2, 0x22);
    }
    if(imm & 0x80){
        la_xvextrins_w(temp1, src1, 0x33);
        la_xvextrins_w(temp2, src2, 0x33);
    }
    if(ir1_opnd_is_xmm(opnd0))
        la_vfmul_s(temp1, temp1, temp2);
    else
        la_xvfmul_s(temp1, temp1, temp2);
    la_xvpackod_w(temp2, temp1, temp1);
    la_xvpackev_w(temp1, temp1, temp1);
    if(ir1_opnd_is_xmm(opnd0))
        la_vfadd_s(temp1, temp1, temp2);
    else
        la_xvfadd_s(temp1, temp1, temp2);
    la_xvpackod_d(temp2, temp1, temp1);
    la_xvpackev_d(temp1, temp1, temp1);
    if(ir1_opnd_is_xmm(opnd0))
        la_vfadd_s(temp1, temp1, temp2);
    else
        la_xvfadd_s(temp1, temp1, temp2);

    la_xvxor_v(dest, dest, dest);
    if(imm & 0x1){
        la_xvextrins_w(dest, temp1, 0x00);
    }
    if(imm & 0x2){
        la_xvextrins_w(dest, temp1, 0x11);
    }
    if(imm & 0x4){
        la_xvextrins_w(dest, temp1, 0x22);
    }
    if(imm & 0x8){
        la_xvextrins_w(dest, temp1, 0x33);
    }
    if(ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vmpsadbw(IR1_INST *pir1)
{

    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND *opnd3 = ir1_get_opnd(pir1, 3);
    lsassert(ir1_opnd_is_xmm(opnd0) || ir1_opnd_is_ymm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    uint8_t imm = ir1_opnd_uimm(opnd3);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    IR2_OPND temp3 = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();

    la_vreplvei_w(temp1, src2, imm & 3);
    la_vmepatmsk_v(temp2, 1, imm & 0x4);
    la_vmepatmsk_v(temp3, 2, imm & 0x4);
    la_vshuf_b(temp2, src1, src1, temp2);
    la_vshuf_b(temp3, src1, src1, temp3);
    la_vabsd_bu(temp2, temp2, temp1);
    la_vabsd_bu(temp3, temp3, temp1);
    la_vhaddw_hu_bu(temp2, temp2, temp2);
    la_vhaddw_wu_hu(temp2, temp2, temp2);
    la_vhaddw_hu_bu(temp3, temp3, temp3);
    la_vhaddw_wu_hu(temp_dest, temp3, temp3);
    la_vsrlni_h_w(temp_dest, temp2, 0);
    if(ir1_opnd_is_xmm(opnd0)){
        set_high128_xreg_to_zero(temp_dest);
        la_xvori_b(dest, temp_dest, 0);
        return true;
    }
    imm = imm>>3;
    la_vmepatmsk_v(temp2, 1, imm & 0x4);
    la_vmepatmsk_v(temp3, 2, imm & 0x4);
    la_xvpermi_q(temp1, src1, 0x11);
    la_vshuf_b(temp2, temp1, temp1, temp2);
    la_vshuf_b(temp3, temp1, temp1, temp3);
    la_xvpermi_q(temp1, src2, 0x11);
    la_vreplvei_w(temp1, temp1, imm & 3);
    la_vabsd_bu(temp2, temp2, temp1);
    la_vabsd_bu(temp3, temp3, temp1);
    la_vhaddw_hu_bu(temp2, temp2, temp2);
    la_vhaddw_wu_hu(temp2, temp2, temp2);
    la_vhaddw_hu_bu(temp3, temp3, temp3);
    la_vhaddw_wu_hu(temp3, temp3, temp3);
    la_vsrlni_h_w(temp3, temp2, 0);
    la_xvpermi_q(temp3, temp_dest, 0x20);
    la_xvori_b(dest, temp3, 0);
    return true;
}

bool translate_vphaddw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    la_xvpickev_h(temp1, src2, src1);
    la_xvpickod_h(temp2, src2, src1);
    la_xvadd_h(dest, temp1, temp2);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vphaddd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    la_xvpickev_w(temp1, src2, src1);
    la_xvpickod_w(temp2, src2, src1);
    la_xvadd_w(dest, temp1, temp2);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vphaddsw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();

    la_xvpickev_h(temp1, src2, src1);
    la_xvpickod_h(temp2, src2, src1);
    la_xvsadd_h(dest, temp1, temp2);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vpsadbw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    la_xvabsd_bu(dest, src1, src2);
    la_xvhaddw_hu_bu(dest, dest, dest);
    la_xvhaddw_wu_hu(dest, dest, dest);
    la_xvhaddw_du_wu(dest, dest, dest);
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }

    return true;
}

bool translate_vroundps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0) || ir1_opnd_is_ymm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();
    bool is_xmm = ir1_opnd_is_xmm(opnd0);
    if(imm & 0x8){
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, int);
        tr_inst = is_xmm ? la_vfcmp_cond_s : la_xvfcmp_cond_s;
        tr_inst(temp, src, src, 0x8);
    }
    else{
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrint_s : la_xvfrint_s;
        tr_inst(temp, src);
    }
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_save, fcsr, 31, 0);
    la_ld_wu(mxcsr, env_ir2_opnd,
            lsenv_offset_of_mxcsr(lsenv));
    la_bstrins_w(fcsr, zero_ir2_opnd, 4, 0);
    if (imm & 0x4) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrint_s : la_xvfrint_s;

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
        tr_inst(dest, src);
        if (ir1_opnd_is_xmm(opnd0)) {
            set_high128_xreg_to_zero(dest);
        }
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

        ra_free_temp(temp);
        ra_free_temp(fcsr);
        ra_free_temp(fcsr_save);
        ra_free_temp(mxcsr);
        return true;
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
    if ((imm & 0x3) == 0x0) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrne_s : la_xvfrintrne_s;
        tr_inst(dest, src);
    } else if ((imm & 0x3) == 0x1) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrm_s : la_xvfrintrm_s;
        tr_inst(dest, src);
    } else if ((imm & 0x3) == 0x2) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrp_s : la_xvfrintrp_s;
        tr_inst(dest, src);
    } else if ((imm & 0x3) == 0x3) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrz_s : la_xvfrintrz_s;
        tr_inst(dest, src);
	}
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_vroundpd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert(ir1_opnd_is_xmm(opnd0) || ir1_opnd_is_ymm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src = load_freg256_from_ir1(opnd1);
    uint8_t imm = ir1_opnd_uimm(opnd2);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();
    bool is_xmm = ir1_opnd_is_xmm(opnd0);
    if(imm & 0x8){
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, int);
        tr_inst = is_xmm ? la_vfcmp_cond_d : la_xvfcmp_cond_d;
        tr_inst(temp, src, src, 0x8);
    }
    else{
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrint_d : la_xvfrint_d;
        tr_inst(temp, src);
    }
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_save, fcsr, 31, 0);
    la_ld_wu(mxcsr, env_ir2_opnd,
            lsenv_offset_of_mxcsr(lsenv));
    la_bstrins_w(fcsr, zero_ir2_opnd, 4, 0);
    if (imm & 0x4) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrint_d : la_xvfrint_d;

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
        tr_inst(dest, src);
        if (ir1_opnd_is_xmm(opnd0)) {
            set_high128_xreg_to_zero(dest);
        }
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

        ra_free_temp(temp);
        ra_free_temp(fcsr);
        ra_free_temp(fcsr_save);
        ra_free_temp(mxcsr);
        return true;
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
    if ((imm & 0x3) == 0x0) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrne_d : la_xvfrintrne_d;
        tr_inst(dest, src);
    } else if ((imm & 0x3) == 0x1) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrm_d : la_xvfrintrm_d;
        tr_inst(dest, src);
    } else if ((imm & 0x3) == 0x2) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrp_d : la_xvfrintrp_d;
        tr_inst(dest, src);
    } else if ((imm & 0x3) == 0x3) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrz_d : la_xvfrintrz_d;
        tr_inst(dest, src);
	}
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_vroundss(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND *opnd3 = ir1_get_opnd(pir1, 3);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    uint8_t imm = ir1_opnd_uimm(opnd3);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();
    IR2_OPND src = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();
    bool is_xmm = ir1_opnd_is_xmm(opnd0);
    la_xvreplve0_w(src, src2);
    if(imm & 0x8){
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, int);
        tr_inst = is_xmm ? la_vfcmp_cond_s : la_xvfcmp_cond_s;
        tr_inst(temp, src, src, 0x8);
    }
    else{
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrint_s : la_xvfrint_s;
        tr_inst(temp, src);
    }
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_save, fcsr, 31, 0);
    la_ld_wu(mxcsr, env_ir2_opnd,
            lsenv_offset_of_mxcsr(lsenv));
    la_bstrins_w(fcsr, zero_ir2_opnd, 4, 0);
    if (imm & 0x4) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrint_s : la_xvfrint_s;

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
        tr_inst(temp_dest, src);
        if (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) !=
            ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1))) {
            la_xvori_b(dest, src1, 0);
        }
        la_xvinsve0_w(dest, temp_dest, 0);
        set_high128_xreg_to_zero(dest);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

        ra_free_temp(temp);
        ra_free_temp(fcsr);
        ra_free_temp(fcsr_save);
        ra_free_temp(mxcsr);
        return true;
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
    if ((imm & 0x3) == 0x0) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrne_s : la_xvfrintrne_s;
        tr_inst(temp_dest, src);
    } else if ((imm & 0x3) == 0x1) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrm_s : la_xvfrintrm_s;
        tr_inst(temp_dest, src);
    } else if ((imm & 0x3) == 0x2) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrp_s : la_xvfrintrp_s;
        tr_inst(temp_dest, src);
    } else if ((imm & 0x3) == 0x3) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrz_s : la_xvfrintrz_s;
        tr_inst(temp_dest, src);
	}
    if (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) !=
        ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1))) {
        la_xvori_b(dest, src1, 0);
    }
    la_xvinsve0_w(dest, temp_dest, 0);
    set_high128_xreg_to_zero(dest);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}

bool translate_vroundsd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND *opnd3 = ir1_get_opnd(pir1, 3);
    lsassert(ir1_opnd_is_xmm(opnd0));
    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    IR2_OPND src1 = load_freg256_from_ir1(opnd1);
    IR2_OPND src2 = load_freg256_from_ir1(opnd2);
    uint8_t imm = ir1_opnd_uimm(opnd3);

    IR2_OPND temp = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();
    IR2_OPND src = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();
    bool is_xmm = ir1_opnd_is_xmm(opnd0);
    la_xvreplve0_d(src, src2);
    if(imm & 0x8){
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND, IR2_OPND, int);
        tr_inst = is_xmm ? la_vfcmp_cond_d : la_xvfcmp_cond_d;
        tr_inst(temp, src, src, 0x8);
    }
    else{
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrint_d : la_xvfrint_d;
        tr_inst(temp, src);
    }
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_save, fcsr, 31, 0);
    la_ld_wu(mxcsr, env_ir2_opnd,
            lsenv_offset_of_mxcsr(lsenv));
    la_bstrins_w(fcsr, zero_ir2_opnd, 4, 0);
    if (imm & 0x4) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrint_d : la_xvfrint_d;

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
        tr_inst(temp_dest, src);
        if (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) !=
            ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1))) {
            la_xvori_b(dest, src1, 0);
        }
        la_xvinsve0_d(dest, temp_dest, 0);
        set_high128_xreg_to_zero(dest);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

        ra_free_temp(temp);
        ra_free_temp(fcsr);
        ra_free_temp(fcsr_save);
        ra_free_temp(mxcsr);
        return true;
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
    if ((imm & 0x3) == 0x0) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrne_d : la_xvfrintrne_d;
        tr_inst(temp_dest, src);
    } else if ((imm & 0x3) == 0x1) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrm_d : la_xvfrintrm_d;
        tr_inst(temp_dest, src);
    } else if ((imm & 0x3) == 0x2) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrp_d : la_xvfrintrp_d;
        tr_inst(temp_dest, src);
    } else if ((imm & 0x3) == 0x3) {
        IR2_INST * ( * tr_inst)(IR2_OPND, IR2_OPND);
        tr_inst = is_xmm ? la_vfrintrz_d : la_xvfrintrz_d;
        tr_inst(temp_dest, src);
	}
    if (ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)) !=
        ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 1))) {
        la_xvori_b(dest, src1, 0);
    }
    la_xvinsve0_d(dest, temp_dest, 0);
    set_high128_xreg_to_zero(dest);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);

    ra_free_temp(temp);
    ra_free_temp(fcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(mxcsr);
    return true;
}
#endif

#ifndef CONFIG_LATX_AVX_OPT
bool translate_pcmpestri(IR1_INST *pir1)
#else
bool translate_vpcmpestri(IR1_INST *pir1)
#endif
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int imm = ir1_opnd_uimm(opnd2);
#ifdef TARGET_X86_64
    /* Presence of REX.W is indicated by bit 8*/
    imm |= ir1_rex_w(pir1) << 8;
#endif
    int d = ir1_opnd_base_reg_num(opnd0);
    if (ir1_opnd_is_xmm(opnd1)) {
        int s = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpestri_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 8);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        load_freg128_from_ir1_mem(src, opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpestri_xmm, d, (d + 1) % 8, imm);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO:fix eflags and mem opnd */
    return true;
}

bool translate_pcmpestrm(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int imm = ir1_opnd_uimm(opnd2);
#ifdef TARGET_X86_64
    /* Presence of REX.W is indicated by bit 8*/
    imm |= ir1_rex_w(pir1) << 8;
#endif
    if (ir1_opnd_is_xmm(opnd1)) {
        int s = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpestrm_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        load_freg128_from_ir1_mem(src, opnd1);
         tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpestrm_xmm, d,
                                                        (d + 1) % 7 + 1, imm);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO:fix eflags and mem opnd */
    return true;
}

#ifdef CONFIG_LATX_AVX_OPT
bool translate_vpcmpestrm(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int imm = ir1_opnd_uimm(opnd2);
#ifdef TARGET_X86_64
    /* Presence of REX.W is indicated by bit 8*/
    imm |= ir1_rex_w(pir1) << 8;
#endif
    if (ir1_opnd_is_xmm(opnd1)) {
        int s = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpestrm_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem(src, opnd1);
         tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpestrm_xmm, d,
                                                        (d + 1) % 7 + 1, imm);
        la_xvor_v(src, temp, temp);
    }
    set_high128_xreg_to_zero(ra_alloc_xmm(0));
    /* TODO:fix eflags and mem opnd */
    return true;
}
#endif

#ifndef CONFIG_LATX_AVX_OPT
bool translate_pcmpistri(IR1_INST *pir1)
#else
bool translate_vpcmpistri(IR1_INST *pir1)
#endif
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int imm = ir1_opnd_uimm(opnd2);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (ir1_opnd_is_xmm(opnd1)) {
        int s = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpistri_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 8);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        load_freg128_from_ir1_mem(src, opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpistri_xmm, d, (d + 1) % 8, imm);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO:fix eflags and mem opnd */
    return true;
}

bool translate_pcmpistrm(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int imm = ir1_opnd_uimm(opnd2);
    if (ir1_opnd_is_xmm(opnd1)) {
        int s = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpistrm_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        load_freg128_from_ir1_mem(src, opnd1);
         tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpistrm_xmm, d,
                                                        (d + 1) % 7 + 1, imm);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO:fix eflags and mem opnd */
    return true;
}

#ifdef CONFIG_LATX_AVX_OPT
bool translate_vpcmpistrm(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int imm = ir1_opnd_uimm(opnd2);
    if (ir1_opnd_is_xmm(opnd1)) {
        int s = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpistrm_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem(src, opnd1);
         tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_pcmpistrm_xmm, d,
                                                        (d + 1) % 7 + 1, imm);
        la_xvor_v(src, temp, temp);
    }
    set_high128_xreg_to_zero(ra_alloc_xmm(0));
    /* TODO:fix eflags and mem opnd */
    return true;
}
#endif

static void cal_pclmulqdq(IR2_OPND d, IR2_OPND v, IR2_OPND s, uint8_t ctrl)
{
    IR2_OPND ctrlp = ra_alloc_itemp();
    IR2_OPND a = ra_alloc_itemp();// a = al
    IR2_OPND b = ra_alloc_itemp();
    IR2_OPND ftemp = ra_alloc_ftemp();

    /* 选取操作数 */
    li_d(ctrlp, ctrl);
    la_andi(a, ctrlp, 1);// ((ctrl  1) != 0)
    la_vreplve_d( ftemp, v, a);
    la_vpickve2gr_d( a, ftemp, 0);
    la_bstrpick_d( b, ctrlp, 4, 4);
    la_vreplve_d( ftemp, s, b);
    la_vpickve2gr_d( b, ftemp, 0);
    ra_free_temp(ctrlp);

    IR2_OPND ah = ra_alloc_itemp();
    IR2_OPND resl = ra_alloc_itemp();
    IR2_OPND resh = ra_alloc_itemp();
    IR2_OPND all_label = ra_alloc_label();
    IR2_OPND bit_label = ra_alloc_label();
    IR2_OPND end_label = ra_alloc_label();

    /* 开始运算 */
    la_and( resl, resl, zero_ir2_opnd);
    la_and( resh, resh, zero_ir2_opnd);
    la_and( ah, ah, zero_ir2_opnd);
    la_beqz( b, end_label);
    la_label( all_label);
    la_andi( a0_ir2_opnd, b, 1);// b  1
    la_srli_d( b, b, 1);
    la_beqz(a0_ir2_opnd, bit_label);
    la_xor( resl, resl, a);
    la_xor( resh, resh, ah);
    la_label( bit_label);
    la_slli_d(a1_ir2_opnd, ah, 1);
    la_bstrpick_d(ah, a, 63, 63);
    la_or( ah, a1_ir2_opnd, ah);
    la_slli_d( a, a, 0x1);
    la_bnez( b, all_label);
    la_label( end_label);
    la_vinsgr2vr_d( d, resl, 0);
    la_vinsgr2vr_d( d, resh, 1);
}

bool translate_pclmulqdq(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);

    int s0 = ir1_opnd_base_reg_num(opnd0);
    uint8_t ctrl = ir1_opnd_uimm(opnd2);
    IR2_OPND dest = ra_alloc_xmm(s0);
    IR2_OPND src;

    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        src = ra_alloc_xmm(s1);
    } else {
        src = ra_alloc_ftemp();
        load_freg128_from_ir1_mem(src, opnd1);
    }

    cal_pclmulqdq(dest, dest, src, ctrl);
    return true;
}

#ifdef CONFIG_LATX_AVX_OPT
bool translate_vpclmulqdq(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND * opnd3 = ir1_get_opnd(pir1, 3);

    IR2_OPND dest = load_freg256_from_ir1(opnd0);
    int s0 = ir1_opnd_base_reg_num(opnd0);
    int s1 = ir1_opnd_base_reg_num(opnd1);
    uint8_t ctrl = ir1_opnd_uimm(opnd3);

    ADDR helper_func;

    if (ir1_opnd_is_ymm(opnd0)) {
        helper_func = (ADDR)helper_vpclmulqdq_ymm;
    } else {
        helper_func = (ADDR)helper_vpclmulqdq_xmm;
    }

    if (!ir1_opnd_is_mem(opnd2)) {
        int s2 = ir1_opnd_base_reg_num(opnd2);
        tr_gen_call_to_helper_pclmulqdq((ADDR)helper_func, s0, s1, s2, ctrl, 0);
    } else {
        int s2 = 0;
        while (s2 < 8) {
            if (s2 != s0 && s2 != s1) {
                break;
            }
            s2++;
        }
        IR2_OPND temp_mem = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s2);
        la_xvor_v(temp_mem, src, src);
        if (ir1_opnd_size(opnd2) == 128) {
            load_freg128_from_ir1_mem(src, opnd2);
        } else {
            load_freg256_from_ir1_mem(src, opnd2);
        }
        tr_gen_call_to_helper_pclmulqdq((ADDR)helper_func, s0, s1, s2, ctrl, 0);
        la_xvor_v(src, temp_mem, temp_mem);
    }
    if (ir1_opnd_size(opnd2) == 128)
        set_high128_xreg_to_zero(dest);
    return true;
}
#endif

bool translate_aesdec(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_aesdec_xmm, d, s1, 0);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_aesdec_xmm, d, s1, 0);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

bool translate_aesdeclast(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_aesdeclast_xmm, d, s1, 0);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_aesdeclast_xmm, d, s1, 0);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

bool translate_aesenc(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_aesenc_xmm, d, s1, 0);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_aesenc_xmm, d, s1, 0);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

bool translate_aesenclast(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_aesenclast_xmm, d, s1, 0);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_aesenclast_xmm, d, s1, 0);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

bool translate_aesimc(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (ir1_opnd_is_xmm(opnd1)) {
        int s = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aesimc_xmm, d, s, 0);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        load_freg128_from_ir1_mem(src, opnd1);
         tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aesimc_xmm, d,
                                                    (d + 1) % 7 + 1, 0);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: IMM 0 do not need to save */
    return true;
}

bool translate_aeskeygenassist(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int imm = ir1_opnd_uimm(opnd2);
    if (ir1_opnd_is_xmm(opnd1)) {
        int s = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aeskeygenassist_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        load_freg128_from_ir1_mem(src, opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aeskeygenassist_xmm, d,
                                                            (d + 1) % 7 + 1, imm);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

#ifdef CONFIG_LATX_AVX_OPT
static void adjust_vsib_index(IR2_OPND dest, IR2_OPND base,
                    IR2_OPND index, int scale)
{
    IR2_INST *(*la_alsl)(IR2_OPND, IR2_OPND, IR2_OPND, int);
#ifdef TARGET_X86_64
    la_alsl = &la_alsl_d;
#else
    la_alsl = &la_alsl_wu;
#endif
    switch (scale) {
    case 1:
        la_add(dest, base, index);
        return;
    case 2:
        la_alsl(dest, index, base, 0);
        break;
    case 4:
        la_alsl(dest, index, base, 1);
        break;
    case 8:
        la_alsl(dest, index, base, 2);
        break;
    default:
        lsassert(0);
    }
}

bool translate_vaesdec(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int s1 = ir1_opnd_base_reg_num(opnd1);

    ADDR helper_func;

    if (ir1_opnd_is_ymm(opnd0)) {
        helper_func = (ADDR)helper_vaesdec_ymm;
    } else {
        helper_func = (ADDR)helper_vaesdec_xmm;
    }

    if (!ir1_opnd_is_mem(opnd2)) {
        int s2 = ir1_opnd_base_reg_num(opnd2);
        tr_gen_call_to_helper_aes((ADDR)helper_func, d, s1, s2);
    } else {
        int s2 = 0;
        while (s2 < 8) {
            if (s2 != d && s2 != s1) {
                break;
            }
            s2++;
        }
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s2);
        la_xvor_v(temp, src, src);
        if (ir1_opnd_size(opnd2) == 128) {
            load_freg128_from_ir1_mem(src, opnd2);
        } else {
            load_freg256_from_ir1_mem(src, opnd2);
        }
        tr_gen_call_to_helper_aes((ADDR)helper_func, d, s1, s2);
        la_xvor_v(src, temp, temp);
    }
    if (!ir1_opnd_is_ymm(opnd0)) {
        set_high128_xreg_to_zero(ra_alloc_xmm(d));
    }
    /* TODO: need to check */
    return true;
}

bool translate_vaesdeclast(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int s1 = ir1_opnd_base_reg_num(opnd1);

    ADDR helper_func;

    if (ir1_opnd_is_ymm(opnd0)) {
        helper_func = (ADDR)helper_vaesdeclast_ymm;
    } else {
        helper_func = (ADDR)helper_vaesdeclast_xmm;
    }

    if (!ir1_opnd_is_mem(opnd2)) {
        int s2 = ir1_opnd_base_reg_num(opnd2);
        tr_gen_call_to_helper_aes((ADDR)helper_func, d, s1, s2);
    } else {
        int s2 = 0;
        while (s2 < 8) {
            if (s2 != d && s2 != s1) {
                break;
            }
            s2++;
        }
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s2);
        la_xvor_v(temp, src, src);
        if (ir1_opnd_size(opnd2) == 128) {
            load_freg128_from_ir1_mem(src, opnd2);
        } else {
            load_freg256_from_ir1_mem(src, opnd2);
        }
        tr_gen_call_to_helper_aes((ADDR)helper_func, d, s1, s2);
        la_xvor_v(src, temp, temp);
    }
    if (!ir1_opnd_is_ymm(opnd0)) {
        set_high128_xreg_to_zero(ra_alloc_xmm(d));
    }
    /* TODO: need to check */
    return true;
}

bool translate_vaesenc(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int s1 = ir1_opnd_base_reg_num(opnd1);

    ADDR helper_func;

    if (ir1_opnd_is_ymm(opnd0)) {
        helper_func = (ADDR)helper_vaesenc_ymm;
    } else {
        helper_func = (ADDR)helper_vaesenc_xmm;
    }

    if (!ir1_opnd_is_mem(opnd2)) {
        int s2 = ir1_opnd_base_reg_num(opnd2);
        tr_gen_call_to_helper_aes((ADDR)helper_func, d, s1, s2);
    } else {
        int s2 = 0;
        while (s2 < 8) {
            if (s2 != d && s2 != s1) {
                break;
            }
            s2++;
        }
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s2);
        la_xvor_v(temp, src, src);
        if (ir1_opnd_size(opnd2) == 128) {
            load_freg128_from_ir1_mem(src, opnd2);
        } else {
            load_freg256_from_ir1_mem(src, opnd2);
        }
        tr_gen_call_to_helper_aes((ADDR)helper_func, d, s1, s2);
        la_xvor_v(src, temp, temp);
    }
    if (!ir1_opnd_is_ymm(opnd0)) {
        set_high128_xreg_to_zero(ra_alloc_xmm(d));
    }
    /* TODO: need to check */
    return true;
}

bool translate_vaesenclast(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int s1 = ir1_opnd_base_reg_num(opnd1);

    ADDR helper_func;

    if (ir1_opnd_is_ymm(opnd0)) {
        helper_func = (ADDR)helper_vaesenclast_ymm;
    } else {
        helper_func = (ADDR)helper_vaesenclast_xmm;
    }

    if (!ir1_opnd_is_mem(opnd2)) {
        int s2 = ir1_opnd_base_reg_num(opnd2);
        tr_gen_call_to_helper_aes((ADDR)helper_func, d, s1, s2);
    } else {
        int s2 = 0;
        while (s2 < 8) {
            if (s2 != d && s2 != s1) {
                break;
            }
            s2++;
        }
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s2);
        la_xvor_v(temp, src, src);
        if (ir1_opnd_size(opnd2) == 128) {
            load_freg128_from_ir1_mem(src, opnd2);
        } else {
            load_freg256_from_ir1_mem(src, opnd2);
        }
        tr_gen_call_to_helper_aes((ADDR)helper_func, d, s1, s2);
        la_xvor_v(src, temp, temp);
    }
    if (!ir1_opnd_is_ymm(opnd0)) {
        set_high128_xreg_to_zero(ra_alloc_xmm(d));
    }
    /* TODO: need to check */
    return true;
}

bool translate_vaesimc(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (ir1_opnd_is_xmm(opnd1)) {
        int s = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aesimc_xmm, d, s, 0);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem(src, opnd1);
         tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aesimc_xmm, d,
                                                    (d + 1) % 7 + 1, 0);
        la_xvor_v(src, temp, temp);
    }
    set_high128_xreg_to_zero(ra_alloc_xmm(d));
    /* TODO: IMM 0 do not need to save */
    return true;
}

bool translate_vaeskeygenassist(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int imm = ir1_opnd_uimm(opnd2);
    if (ir1_opnd_is_xmm(opnd1)) {
        int s = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aeskeygenassist_xmm, d, s, imm);
    } else {
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm((d + 1) % 7 + 1);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem(src, opnd1);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_aeskeygenassist_xmm, d,
                                                            (d + 1) % 7 + 1, imm);
        la_xvor_v(src, temp, temp);
    }
    set_high128_xreg_to_zero(ra_alloc_xmm(d));
    /* TODO: need to check */
    return true;
}

bool translate_vpsrlvd(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp = ra_alloc_ftemp();

        la_vslei_wu(temp, src2, 31);
        la_vsrl_w(dest, src1, src2);
        la_vand_v(dest, dest, temp);
        set_high128_xreg_to_zero(dest);
    } else if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp = ra_alloc_ftemp();

        la_xvslei_wu(temp, src2, 31);
        la_xvsrl_w(dest, src1, src2);
        la_xvand_v(dest, dest, temp);
    }

    return true;
}

bool translate_vpsrlvq(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND temp = ra_alloc_ftemp();

        la_vldi(temp,0b0110000111111);
        la_vsle_du(temp, src2, temp);
        la_vsrl_d(dest, src1, src2);
        la_vand_v(dest, dest, temp);
        set_high128_xreg_to_zero(dest);
    } else if (ir1_opnd_is_ymm(opnd0)) {
        IR2_OPND dest = load_freg256_from_ir1(opnd0);
        IR2_OPND src1 = load_freg256_from_ir1(opnd1);
        IR2_OPND src2 = load_freg256_from_ir1(opnd2);
        IR2_OPND temp = ra_alloc_ftemp();

        la_xvldi(temp,0b0110000111111);
        la_xvsle_du(temp, src2, temp);
        la_xvsrl_d(dest, src1, src2);
        la_xvand_v(dest, dest, temp);
    }

    return true;
}
#endif


#ifdef CONFIG_LATX_AVX_OPT
bool translate_vpgatherdd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd2)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd2)));
    lsassert(ir1_opnd_is_mem(opnd1) && ir1_opnd_has_index(opnd1));
    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND mask = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd2));
    IR2_OPND index_op = ra_alloc_xmm(ir1_opnd_vsib_index_reg_num(opnd1));
    IR2_OPND temp_addr = ra_alloc_itemp();
    IR2_OPND temp_index = ra_alloc_itemp();
    IR2_OPND temp_mask = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();
    bool has_base = ir1_opnd_has_base(opnd1);
    longx offset = ir1_opnd_simm(opnd1);

    la_xvslti_w(temp_mask, mask, 0);
    li_guest_addr(temp_addr, offset);
    if(has_base){
        IR2_OPND base_op = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
        la_add(temp_addr, temp_addr, base_op);
    }

    /* fetch the first element in vsib */
    la_xvpickve2gr_w(temp_index, index_op, 0);
    adjust_vsib_index(temp_index, temp_addr, temp_index,
                    ir1_opnd_scale(opnd1));
    la_ld_w(temp_index, temp_index, 0);
    la_xvinsgr2vr_w(temp_dest, temp_index, 0);

    /* fetch the second element in vsib */
    la_xvpickve2gr_w(temp_index, index_op, 1);
    adjust_vsib_index(temp_index, temp_addr, temp_index,
                    ir1_opnd_scale(opnd1));
    la_ld_w(temp_index, temp_index, 0);
    la_xvinsgr2vr_w(temp_dest, temp_index, 1);

    /* fetch the third element in vsib */
    la_xvpickve2gr_w(temp_index, index_op, 2);
    adjust_vsib_index(temp_index, temp_addr, temp_index,
                    ir1_opnd_scale(opnd1));
    la_ld_w(temp_index, temp_index, 0);
    la_xvinsgr2vr_w(temp_dest, temp_index, 2);

    /* fetch the fourth element in vsib */
    la_xvpickve2gr_w(temp_index, index_op, 3);
    adjust_vsib_index(temp_index, temp_addr, temp_index,
                    ir1_opnd_scale(opnd1));
    la_ld_w(temp_index, temp_index, 0);
    la_xvinsgr2vr_w(temp_dest, temp_index, 3);

    if(ir1_opnd_is_ymm(opnd0)){
        /* fetch the fifth element in vsib */
        la_xvpickve2gr_w(temp_index, index_op, 4);
        adjust_vsib_index(temp_index, temp_addr, temp_index,
                        ir1_opnd_scale(opnd1));
        la_ld_w(temp_index, temp_index, 0);
        la_xvinsgr2vr_w(temp_dest, temp_index, 4);

        /* fetch the sixth element in vsib */
        la_xvpickve2gr_w(temp_index, index_op, 5);
        adjust_vsib_index(temp_index, temp_addr, temp_index,
                        ir1_opnd_scale(opnd1));
        la_ld_w(temp_index, temp_index, 0);
        la_xvinsgr2vr_w(temp_dest, temp_index, 5);

        /* fetch the seventh element in vsib */
        la_xvpickve2gr_w(temp_index, index_op, 6);
        adjust_vsib_index(temp_index, temp_addr, temp_index,
                        ir1_opnd_scale(opnd1));
        la_ld_w(temp_index, temp_index, 0);
        la_xvinsgr2vr_w(temp_dest, temp_index, 6);

        /* fetch the eighth element in vsib */
        la_xvpickve2gr_w(temp_index, index_op, 7);
        adjust_vsib_index(temp_index, temp_addr, temp_index,
                        ir1_opnd_scale(opnd1));
        la_ld_w(temp_index, temp_index, 0);
        la_xvinsgr2vr_w(temp_dest, temp_index, 7);
    }
    la_xvbitsel_v(dest, dest, temp_dest, temp_mask);
    la_xvxor_v(mask, mask, mask);
    if(ir1_opnd_is_xmm(opnd0))
        set_high128_xreg_to_zero(dest);
    return true;
}

bool translate_vpgatherqd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd2)));
    lsassert(ir1_opnd_is_mem(opnd1) && ir1_opnd_has_index(opnd1));
    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND mask = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd2));
    IR2_OPND index_op = ra_alloc_xmm(ir1_opnd_vsib_index_reg_num(opnd1));
    IR2_OPND temp_addr = ra_alloc_itemp();
    IR2_OPND temp_index = ra_alloc_itemp();
    IR2_OPND temp_mask = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();
    bool has_base = ir1_opnd_has_base(opnd1);
    longx offset = ir1_opnd_simm(opnd1);

    la_xvslti_w(temp_mask, mask, 0);
    li_guest_addr(temp_addr, offset);
    if(has_base){
        IR2_OPND base_op = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
        la_add(temp_addr, temp_addr, base_op);
    }

    /* fetch the first element in vsib */
    la_xvpickve2gr_d(temp_index, index_op, 0);
    adjust_vsib_index(temp_index, temp_addr, temp_index,
                    ir1_opnd_scale(opnd1));
    la_ld_w(temp_index, temp_index, 0);
    la_xvinsgr2vr_w(temp_dest, temp_index, 0);

    /* fetch the second element in vsib */
    la_xvpickve2gr_d(temp_index, index_op, 1);
    adjust_vsib_index(temp_index, temp_addr, temp_index,
                    ir1_opnd_scale(opnd1));
    la_ld_w(temp_index, temp_index, 0);
    la_xvinsgr2vr_w(temp_dest, temp_index, 1);

    if(ir1_index_reg_is_ymm(opnd1)){
        /* fetch the third element in vsib */
        la_xvpickve2gr_d(temp_index, index_op, 2);
        adjust_vsib_index(temp_index, temp_addr, temp_index,
                        ir1_opnd_scale(opnd1));
        la_ld_w(temp_index, temp_index, 0);
        la_xvinsgr2vr_w(temp_dest, temp_index, 2);

        /* fetch the fourth element in vsib */
        la_xvpickve2gr_d(temp_index, index_op, 3);
        adjust_vsib_index(temp_index, temp_addr, temp_index,
                        ir1_opnd_scale(opnd1));
        la_ld_w(temp_index, temp_index, 0);
        la_xvinsgr2vr_w(temp_dest, temp_index, 3);
    }
    if(ir1_index_reg_is_ymm(opnd1)) {
        la_xvbitsel_v(dest, dest, temp_dest, temp_mask);
        set_high128_xreg_to_zero(dest);
    }
    else {
        la_xvbitsel_v(temp_dest, dest, temp_dest, temp_mask);
        la_xvxor_v(dest, dest, dest);
        la_xvinsve0_d(dest, temp_dest, 0);
    }
    la_xvxor_v(mask, mask, mask);

    return true;
}

bool translate_vpgatherdq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd2)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd2)));
    lsassert(ir1_opnd_is_mem(opnd1) && ir1_opnd_has_index(opnd1));
    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND mask = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd2));
    IR2_OPND index_op = ra_alloc_xmm(ir1_opnd_vsib_index_reg_num(opnd1));
    IR2_OPND temp_addr = ra_alloc_itemp();
    IR2_OPND temp_index = ra_alloc_itemp();
    IR2_OPND temp_mask = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();
    bool has_base = ir1_opnd_has_base(opnd1);
    longx offset = ir1_opnd_simm(opnd1);

    la_xvslti_d(temp_mask, mask, 0);
    li_guest_addr(temp_addr, offset);
    if(has_base){
        IR2_OPND base_op = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
        la_add(temp_addr, temp_addr, base_op);
    }

    /* fetch the first element in vsib */
    la_xvpickve2gr_w(temp_index, index_op, 0);
    adjust_vsib_index(temp_index, temp_addr, temp_index,
                    ir1_opnd_scale(opnd1));
    la_ld_d(temp_index, temp_index, 0);
    la_xvinsgr2vr_d(temp_dest, temp_index, 0);

    /* fetch the second element in vsib */
    la_xvpickve2gr_w(temp_index, index_op, 1);
    adjust_vsib_index(temp_index, temp_addr, temp_index,
                    ir1_opnd_scale(opnd1));
    la_ld_d(temp_index, temp_index, 0);
    la_xvinsgr2vr_d(temp_dest, temp_index, 1);

    if(ir1_opnd_is_ymm(opnd0)){
        /* fetch the third element in vsib */
        la_xvpickve2gr_w(temp_index, index_op, 2);
        adjust_vsib_index(temp_index, temp_addr, temp_index,
                        ir1_opnd_scale(opnd1));
        la_ld_d(temp_index, temp_index, 0);
        la_xvinsgr2vr_d(temp_dest, temp_index, 2);

        /* fetch the fourth element in vsib */
        la_xvpickve2gr_w(temp_index, index_op, 3);
        adjust_vsib_index(temp_index, temp_addr, temp_index,
                        ir1_opnd_scale(opnd1));
        la_ld_d(temp_index, temp_index, 0);
        la_xvinsgr2vr_d(temp_dest, temp_index, 3);
    }
    la_xvbitsel_v(dest, dest, temp_dest, temp_mask);
    if(ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    la_xvxor_v(mask, mask, mask);

    return true;
}

bool translate_vpgatherqq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd2)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd2)));
    lsassert(ir1_opnd_is_mem(opnd1) && ir1_opnd_has_index(opnd1));
    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND mask = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd2));
    IR2_OPND index_op = ra_alloc_xmm(ir1_opnd_vsib_index_reg_num(opnd1));
    IR2_OPND temp_addr = ra_alloc_itemp();
    IR2_OPND temp_index = ra_alloc_itemp();
    IR2_OPND temp_mask = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();
    bool has_base = ir1_opnd_has_base(opnd1);
    longx offset = ir1_opnd_simm(opnd1);

    la_xvslti_d(temp_mask, mask, 0);
    li_guest_addr(temp_addr, offset);
    if(has_base){
        IR2_OPND base_op = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
        la_add(temp_addr, temp_addr, base_op);
    }

    /* fetch the first element in vsib */
    la_xvpickve2gr_d(temp_index, index_op, 0);
    adjust_vsib_index(temp_index, temp_addr, temp_index,
                    ir1_opnd_scale(opnd1));
    la_ld_d(temp_index, temp_index, 0);
    la_xvinsgr2vr_d(temp_dest, temp_index, 0);

    /* fetch the second element in vsib */
    la_xvpickve2gr_d(temp_index, index_op, 1);
    adjust_vsib_index(temp_index, temp_addr, temp_index,
                    ir1_opnd_scale(opnd1));
    la_ld_d(temp_index, temp_index, 0);
    la_xvinsgr2vr_d(temp_dest, temp_index, 1);

    if(ir1_opnd_is_ymm(opnd0)){
        /* fetch the third element in vsib */
        la_xvpickve2gr_d(temp_index, index_op, 2);
        adjust_vsib_index(temp_index, temp_addr, temp_index,
                        ir1_opnd_scale(opnd1));
        la_ld_d(temp_index, temp_index, 0);
        la_xvinsgr2vr_d(temp_dest, temp_index, 2);

        /* fetch the fourth element in vsib */
        la_xvpickve2gr_d(temp_index, index_op, 3);
        adjust_vsib_index(temp_index, temp_addr, temp_index,
                        ir1_opnd_scale(opnd1));
        la_ld_d(temp_index, temp_index, 0);
        la_xvinsgr2vr_d(temp_dest, temp_index, 3);
    }
    la_xvbitsel_v(dest, dest, temp_dest, temp_mask);
    if(ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    la_xvxor_v(mask, mask, mask);

    return true;
}

bool translate_vpbroadcastb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src;
    if (ir1_opnd_is_mem(opnd1)) {
        src = load_freg128_from_ir1(opnd1);
    } else if (ir1_opnd_is_xmm(opnd1)) {
        src = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    } else {
        lsassert(0);
    }

    if (ir1_opnd_is_xmm(opnd0)) {
        la_xvreplve0_b(dest, src);
    } else if (ir1_opnd_is_ymm(opnd0)) {
        la_xvreplve0_b(dest, src);
        la_xvinsve0_d(dest, dest, 2);
        la_xvinsve0_d(dest, dest, 3);
    } else {
        lsassert(0);
    }

    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }

    return true;
}

bool translate_vpbroadcastw(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src;
    if (ir1_opnd_is_mem(opnd1)) {
        src = load_freg128_from_ir1(opnd1);
    } else if (ir1_opnd_is_xmm(opnd1)) {
        src = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    } else {
        lsassert(0);
    }

    if (ir1_opnd_is_xmm(opnd0)) {
        la_xvreplve0_h(dest, src);
    } else if (ir1_opnd_is_ymm(opnd0)) {
        la_xvreplve0_h(dest, src);
        la_xvinsve0_d(dest, dest, 2);
        la_xvinsve0_d(dest, dest, 3);
    } else {
        lsassert(0);
    }

    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }

    return true;
}
#endif
