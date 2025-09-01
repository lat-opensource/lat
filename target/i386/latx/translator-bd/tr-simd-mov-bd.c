#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate-bd.h"
#include "hbr-bd.h"

bool translate_movdq2q_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)));
    la_fmov_d(ra_alloc_mmx(ir1_opnd_base_reg_num_bd(dest)),
                     ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)));
    // TODO:zero fpu top and tag word
    return true;
}

bool translate_movmskpd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD* dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD* src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_xmm_bd(src)) {
        IR2_OPND temp = ra_alloc_ftemp();
        la_vmskltz_d(temp,
            ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)));
        la_movfr2gr_d(ra_alloc_gpr(ir1_opnd_base_reg_num_bd(dest)), temp);
        return true;
    }
    lsassert(0);
    return false;
}

bool translate_movmskps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD* dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD* src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_xmm_bd(src)) {
        IR2_OPND temp = ra_alloc_ftemp();
        la_vmskltz_w(temp,
            ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)));
        la_movfr2gr_d(ra_alloc_gpr(ir1_opnd_base_reg_num_bd(dest)), temp);
        return true;
    }
    lsassert(0);
    return false;
}

bool translate_movntdq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_xmm_bd(src)) {
        IR2_OPND src_ir2 = ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src));
        store_freg128_to_ir1_mem_bd(src_ir2, dest);
        return true;
    }
    lsassert(0);
    return false;
}

bool translate_movnti_bd(IR1_INST *pir1)
{
    IR2_OPND src = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 0) + 1, UNKNOWN_EXTENSION,
                                      false);   /* fill default parameter */
    store_ireg_to_ir1_bd(src, ir1_get_opnd_bd(pir1, 0), false); /* fill default parameter */
    return true;
}

bool translate_movntpd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_xmm_bd(src)) {
        IR2_OPND src_ir2 = ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src));
        store_freg128_to_ir1_mem_bd(src_ir2, dest);
        return true;
    }
    lsassert(0);
    return false;
}

bool translate_movntps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_xmm_bd(src)) {
        IR2_OPND src_ir2 = ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src));
        store_freg128_to_ir1_mem_bd(src_ir2, dest);
        return true;
    }
    lsassert(0);
    return false;
}

bool translate_movntq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_xmm_bd(src)) {
        IR2_OPND src_ir2 = ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src));
        store_freg_to_ir1_bd(src_ir2, dest, false, false);
        return true;
    }

    /* transfer_to_mmx_mode_bd */
    transfer_to_mmx_mode_bd();

    IR2_OPND src_lo =
        load_freg_from_ir1_1_bd(ir1_get_opnd_bd(pir1, 1), false, IS_INTEGER);
    store_freg_to_ir1_bd(src_lo, ir1_get_opnd_bd(pir1, 0), false,
                      true); /* fill default parameter */
    return true;
}

bool translate_movq2dq_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    if (option_enable_lasx) {
        la_xvpickve_d(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                      ra_alloc_mmx(ir1_opnd_base_reg_num_bd(src)), 0);
    } else {
        la_vandi_b(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                   ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)), 0);
        la_vextrins_d(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                      ra_alloc_mmx(ir1_opnd_base_reg_num_bd(src)), 0);
    }
    //TODO:zero fpu top and tag word
    return true;
}

bool translate_pmovmskb_bd(IR1_INST *pir1)
{
    IR1_OPND_BD* dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD* src = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND ftemp = ra_alloc_ftemp();
    if (ir1_opnd_is_xmm_bd(src)) {
        la_vmskltz_b(ftemp,
            ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)));
        la_movfr2gr_d(ra_alloc_gpr(ir1_opnd_base_reg_num_bd(dest)), ftemp);
    } else { //mmx
        IR2_OPND itemp = ra_alloc_itemp();
        la_vmskltz_b(ftemp,
            ra_alloc_mmx(ir1_opnd_base_reg_num_bd(src)));
        la_movfr2gr_d(itemp, ftemp);
        la_andi(itemp, itemp, 0xff);
        store_ireg_to_ir1_bd(itemp, dest, false);
        ra_free_temp(itemp);
        ra_free_temp(ftemp);
    }
    return true;
}

bool translate_maskmovq_bd(IR1_INST *pir1)
{
    IR2_OPND src = ra_alloc_ftemp();
    IR2_OPND mask = ra_alloc_ftemp();
    load_freg_from_ir1_2_bd(src, ir1_get_opnd_bd(pir1, 0), IS_INTEGER);
    load_freg_from_ir1_2_bd(mask, ir1_get_opnd_bd(pir1, 1), IS_INTEGER);
    IR2_OPND zero = ra_alloc_ftemp();
    la_vxor_v(zero, zero, zero);
    /*
     * Mapping to LA 23 -> 30
     */
    IR2_OPND base_opnd = ra_alloc_gpr(edi_index);
    IR2_OPND temp_mask = ra_alloc_ftemp();
    la_vandi_b(temp_mask, mask, 0x80);
    IR2_OPND mem_mask = ra_alloc_ftemp();
    la_vseq_b(mem_mask, temp_mask, zero);
    la_vnor_v(temp_mask, mem_mask, zero);
    IR2_OPND mem_data = ra_alloc_ftemp();
    IR2_OPND xmm_data = ra_alloc_ftemp();
#ifndef TARGET_X86_64
    la_bstrpick_d(base_opnd, base_opnd, 31, 0);
#else
    if(!CODEIS64) {
        la_bstrpick_d(base_opnd, base_opnd, 31, 0);
    }
#endif
    la_fld_d(mem_data, base_opnd, 0);
    la_vand_v(xmm_data, src, temp_mask);
    la_vand_v(mem_data, mem_data, mem_mask);
    la_vor_v(mem_data, mem_data, xmm_data);
    la_fst_d(mem_data, base_opnd, 0);
    return true;
}

bool translate_maskmovdqu_bd(IR1_INST *pir1)
{
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND mask = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND zero = ra_alloc_ftemp();
    la_vxor_v(zero, zero, zero);
    /*
     * Mapping to LA 23 -> 30
     */
    IR2_OPND base_opnd = ra_alloc_gpr(edi_index);
#ifndef TARGET_X86_64
    la_bstrpick_d(base_opnd, base_opnd, 31, 0);
#else
    if(!CODEIS64) {
        la_bstrpick_d(base_opnd, base_opnd, 31, 0);
    }
#endif
    IR2_OPND temp_mask = ra_alloc_ftemp();
    la_vandi_b(temp_mask, mask, 0x80);
    IR2_OPND mem_mask = ra_alloc_ftemp();
    la_vseq_b(mem_mask, temp_mask, zero);
    la_vnor_v(temp_mask, mem_mask, zero);
    IR2_OPND mem_data = ra_alloc_ftemp();
    IR2_OPND xmm_data = ra_alloc_ftemp();
    la_vld(mem_data, base_opnd, 0);
    la_vand_v(xmm_data, src, temp_mask);
    la_vand_v(mem_data, mem_data, mem_mask);
    la_vor_v(mem_data, mem_data, xmm_data);
    la_vst(mem_data, base_opnd, 0);
    return true;
}

bool translate_movupd_bd(IR1_INST *pir1)
{
    translate_movaps_bd(pir1);
    return true;
}

bool translate_movdqa_bd(IR1_INST *pir1)
{
    translate_movaps_bd(pir1);
    return true;
}

bool translate_movdqu_bd(IR1_INST *pir1)
{
    translate_movaps_bd(pir1);
    return true;
}

bool translate_movups_bd(IR1_INST *pir1)
{
    translate_movaps_bd(pir1);
    return true;
}

bool translate_movapd_bd(IR1_INST *pir1)
{
    translate_movaps_bd(pir1);
    return true;
}
bool translate_lddqu_bd(IR1_INST *pir1)
{
    translate_movaps_bd(pir1);
    return true;
}
bool translate_movaps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_xmm_bd(dest) && ir1_opnd_is_mem_bd(src)) {
        load_freg128_from_ir1_mem_bd(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                                  src);
    } else if (ir1_opnd_is_mem_bd(dest) && ir1_opnd_is_xmm_bd(src)) {
        store_freg128_to_ir1_mem_bd(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)),
                                 dest);
    } else if (ir1_opnd_is_xmm_bd(dest) && ir1_opnd_is_xmm_bd(src)) {
        la_vori_b(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                  ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)), 0);
    } else {
        lsassert(0);
    }
    return true;
}

bool translate_movhlps_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);

    if(ir1_opnd_is_mem_bd(opnd1)) {
        return translate_movlps_bd(pir1);
    }

    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    la_vilvh_d(dest, dest, src);
    return true;
}

bool translate_movshdup_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
             ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vpackod_w(dest, src, src);
    return true;
}

bool translate_movsldup_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)) ||
             ir1_opnd_is_mem_bd(ir1_get_opnd_bd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vpackev_w(dest, src, src);
    return true;
}

bool translate_movlhps_bd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 0)));
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    la_vilvl_d(dest, src, dest);
    return true;
}

bool translate_movsd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_xmm_bd(dest) && ir1_opnd_is_mem_bd(src)) {
        if (SHBR_ON_64_BD(pir1)) {
            IR2_OPND dest_opnd = ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest));
            load_freg_from_ir1_2_bd(dest_opnd, src, IS_INTEGER);
        } else{
            IR2_OPND temp = load_freg_from_ir1_1_bd(src, false, IS_INTEGER);
            if (option_enable_lasx) {
                la_xvpickve_d(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)), temp, 0);
            } else {
                la_vandi_b(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                           ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)), 0);
                la_vextrins_d(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)), temp, 0);
            }
        }
    } else if (ir1_opnd_is_mem_bd(dest) && ir1_opnd_is_xmm_bd(src)) {
        store_freg_to_ir1_bd(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)), dest,
                          false, false);
    } else if (ir1_opnd_is_xmm_bd(dest) && ir1_opnd_is_xmm_bd(src)) {
        if (ir1_opnd_base_reg_num_bd(dest) == ir1_opnd_base_reg_num_bd(src)) {
            return true;
        } else {
            if (option_enable_lasx) {
                la_xvinsve0_d(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                              ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)), 0);
            } else {
                la_vextrins_d(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                              ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)), 0);
            }
        }
    }
    return true;
}

bool translate_movss_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);

    if (ir1_opnd_is_xmm_bd(dest) && ir1_opnd_is_mem_bd(src)) {
        IR2_OPND temp = load_freg_from_ir1_1_bd(src, false, IS_INTEGER);
        IR2_OPND xmm_dest = ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest));
        la_xvpickve_w(xmm_dest, temp, 0);
        return true;
    } else if (ir1_opnd_is_mem_bd(dest) && ir1_opnd_is_xmm_bd(src)) {
        store_freg_to_ir1_bd(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)), dest,
                          false, false);
        return true;
    } else if (ir1_opnd_is_xmm_bd(dest) && ir1_opnd_is_xmm_bd(src)) {
        if (ir1_opnd_base_reg_num_bd(dest) == ir1_opnd_base_reg_num_bd(src)) {
            return true;
        } else {
            if (option_enable_lasx) {
                la_xvinsve0_w(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                              ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)), 0);
            } else {
                la_vextrins_w(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)),
                              ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)), 0);
            }
        }
        return true;
    }
    if (ir1_opnd_is_xmm_bd(dest) || ir1_opnd_is_xmm_bd(src)) {
        lsassert(0);
    }
    return true;
}

bool translate_movhpd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_mem_bd(src) && ir1_opnd_is_xmm_bd(dest)) {
        IR2_OPND temp = load_ireg_from_ir1_bd(src, ZERO_EXTENSION, false);
        la_vinsgr2vr_d(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)), temp, 1);
    } else if (ir1_opnd_is_mem_bd(dest) && ir1_opnd_is_xmm_bd(src)) {
        IR2_OPND temp = ra_alloc_itemp();
        la_vpickve2gr_d(temp,
                          ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)), 1);
        store_ireg_to_ir1_bd(temp, dest, false);
    } else {
        lsassert(0);
    }
    return true;
}

bool translate_movhps_bd(IR1_INST *pir1)
{
    translate_movhpd_bd(pir1);
    return true;
}

bool translate_movlpd_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *dest = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *src = ir1_get_opnd_bd(pir1, 1);
    if (ir1_opnd_is_mem_bd(src) && ir1_opnd_is_xmm_bd(dest)) {
        IR2_OPND temp = load_ireg_from_ir1_bd(src, ZERO_EXTENSION, false);
        la_vinsgr2vr_d(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)), temp, 0);
        return true;
    } else if (ir1_opnd_is_mem_bd(dest) && ir1_opnd_is_xmm_bd(src)) {
        store_freg_to_ir1_bd(ra_alloc_xmm(ir1_opnd_base_reg_num_bd(src)), dest, false, false);
        return true;
    } else {
        lsassert(0);
    }

    if (ir1_opnd_is_mem_bd(src)) {
        IR2_OPND src_lo = load_freg_from_ir1_1_bd(src, false, true);
        store_freg_to_ir1_bd(src_lo, ir1_get_opnd_bd(pir1, 0), false, true);
    } else {
        IR2_OPND src_lo = load_freg_from_ir1_1_bd(src, false, true);
        store_freg_to_ir1_bd(src_lo, ir1_get_opnd_bd(pir1, 0), false, true);
    }

    return true;
}

bool translate_movlps_bd(IR1_INST *pir1)
{
    translate_movlpd_bd(pir1);
    return true;
}

bool translate_movddup_bd(IR1_INST *pir1)
{
    IR2_OPND src_lo =
        load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 1));
    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(pir1, 0));
    if (option_enable_lasx) {
        la_xvreplve0_d(dest, src_lo);
    } else {
        la_vreplvei_d(dest, src_lo, 0);
    }

    return true;
}
