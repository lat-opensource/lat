#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"
#include "hbr.h"

bool translate_movdq2q(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    la_fmov_d(ra_alloc_mmx(ir1_opnd_base_reg_num(dest)),
                     ra_alloc_xmm(ir1_opnd_base_reg_num(src)));
    // TODO:zero fpu top and tag word
    return true;
}

bool translate_movmskpd(IR1_INST *pir1)
{
    IR1_OPND* dest = ir1_get_opnd(pir1, 0);
    IR1_OPND* src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(src)) {
        IR2_OPND temp = ra_alloc_ftemp();
        la_vmskltz_d(temp,
            ra_alloc_xmm(ir1_opnd_base_reg_num(src)));
        la_movfr2gr_d(ra_alloc_gpr(ir1_opnd_base_reg_num(dest)), temp);
        return true;
    }
    lsassert(0);
    return false;
}

bool translate_movmskps(IR1_INST *pir1)
{
    IR1_OPND* dest = ir1_get_opnd(pir1, 0);
    IR1_OPND* src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(src)) {
        IR2_OPND temp = ra_alloc_ftemp();
        la_vmskltz_w(temp,
            ra_alloc_xmm(ir1_opnd_base_reg_num(src)));
        la_movfr2gr_d(ra_alloc_gpr(ir1_opnd_base_reg_num(dest)), temp);
        return true;
    }
    lsassert(0);
    return false;
}

bool translate_movntdq(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(src)) {
        IR2_OPND src_ir2 = ra_alloc_xmm(ir1_opnd_base_reg_num(src));
        store_freg128_to_ir1_mem(src_ir2, dest);
        return true;
    }
    lsassert(0);
    return false;
}

bool translate_movnti(IR1_INST *pir1)
{
    IR2_OPND src = load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 1, UNKNOWN_EXTENSION,
                                      false);   /* fill default parameter */
    store_ireg_to_ir1(src, ir1_get_opnd(pir1, 0), false); /* fill default parameter */
    return true;
}

bool translate_movntpd(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(src)) {
        IR2_OPND src_ir2 = ra_alloc_xmm(ir1_opnd_base_reg_num(src));
        store_freg128_to_ir1_mem(src_ir2, dest);
        return true;
    }
    lsassert(0);
    return false;
}

bool translate_movntps(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(src)) {
        IR2_OPND src_ir2 = ra_alloc_xmm(ir1_opnd_base_reg_num(src));
        store_freg128_to_ir1_mem(src_ir2, dest);
        return true;
    }
    lsassert(0);
    return false;
}

bool translate_movntq(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(src)) {
        IR2_OPND src_ir2 = ra_alloc_xmm(ir1_opnd_base_reg_num(src));
        store_freg_to_ir1(src_ir2, dest, false, false);
        return true;
    }

    /* transfer_to_mmx_mode */
    transfer_to_mmx_mode();

    IR2_OPND src_lo =
        load_freg_from_ir1_1(ir1_get_opnd(pir1, 1), false, IS_INTEGER);
    store_freg_to_ir1(src_lo, ir1_get_opnd(pir1, 0), false,
                      true); /* fill default parameter */
    return true;
}

bool translate_movq2dq(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    if (option_enable_lasx) {
        la_xvpickve_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                      ra_alloc_mmx(ir1_opnd_base_reg_num(src)), 0);
    } else {
        la_vandi_b(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                   ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), 0);
        la_vextrins_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                      ra_alloc_mmx(ir1_opnd_base_reg_num(src)), 0);
    }
    //TODO:zero fpu top and tag word
    return true;
}

bool translate_pmovmskb(IR1_INST *pir1)
{
    IR1_OPND* dest = ir1_get_opnd(pir1, 0);
    IR1_OPND* src = ir1_get_opnd(pir1, 1);
    IR2_OPND ftemp = ra_alloc_ftemp();
    if (ir1_opnd_is_xmm(src)) {
        la_vmskltz_b(ftemp,
            ra_alloc_xmm(ir1_opnd_base_reg_num(src)));
        la_movfr2gr_d(ra_alloc_gpr(ir1_opnd_base_reg_num(dest)), ftemp);
    } else { //mmx
        IR2_OPND itemp = ra_alloc_itemp();
        la_vmskltz_b(ftemp,
            ra_alloc_mmx(ir1_opnd_base_reg_num(src)));
        la_movfr2gr_d(itemp, ftemp);
        la_andi(itemp, itemp, 0xff);
        store_ireg_to_ir1(itemp, dest, false);
        ra_free_temp(itemp);
        ra_free_temp(ftemp);
    }
    return true;
}

bool translate_maskmovq(IR1_INST *pir1)
{
    IR2_OPND src = ra_alloc_ftemp();
    IR2_OPND mask = ra_alloc_ftemp();
    load_freg_from_ir1_2(src, ir1_get_opnd(pir1, 0), IS_INTEGER);
    load_freg_from_ir1_2(mask, ir1_get_opnd(pir1, 1), IS_INTEGER);
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

bool translate_maskmovdqu(IR1_INST *pir1)
{
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND mask = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
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

bool translate_movupd(IR1_INST *pir1)
{
    translate_movaps(pir1);
    return true;
}

bool translate_movdqa(IR1_INST *pir1)
{
    translate_movaps(pir1);
    return true;
}

bool translate_movdqu(IR1_INST *pir1)
{
    translate_movaps(pir1);
    return true;
}

bool translate_movups(IR1_INST *pir1)
{
    translate_movaps(pir1);
    return true;
}

bool translate_movapd(IR1_INST *pir1)
{
    translate_movaps(pir1);
    return true;
}
bool translate_lddqu(IR1_INST *pir1)
{
    translate_movaps(pir1);
    return true;
}
bool translate_movaps(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_mem(src)) {
        load_freg128_from_ir1_mem(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                                  src);
    } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
        store_freg128_to_ir1_mem(ra_alloc_xmm(ir1_opnd_base_reg_num(src)),
                                 dest);
    } else if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_xmm(src)) {
        la_vori_b(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                  ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
    } else {
        lsassert(0);
    }
    return true;
}

bool translate_movhlps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vilvh_d(dest, dest, src);
    return true;
}

bool translate_movshdup(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)) ||
             ir1_opnd_is_mem(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vpackod_w(dest, src, src);
    return true;
}

bool translate_movsldup(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)) ||
             ir1_opnd_is_mem(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vpackev_w(dest, src, src);
    return true;
}

bool translate_movlhps(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 1)));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    la_vilvl_d(dest, src, dest);
    return true;
}

bool translate_movsd(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_mem(src)) {
        if (SHBR_ON_64(pir1)) {
            IR2_OPND dest_opnd = ra_alloc_xmm(ir1_opnd_base_reg_num(dest));
            load_freg_from_ir1_2(dest_opnd, src, IS_INTEGER);
        } else{
            IR2_OPND temp = load_freg_from_ir1_1(src, false, IS_INTEGER);
            if (option_enable_lasx) {
                la_xvpickve_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), temp, 0);
            } else {
                la_vandi_b(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                           ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), 0);
                la_vextrins_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), temp, 0);
            }
        }
    } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
        store_freg_to_ir1(ra_alloc_xmm(ir1_opnd_base_reg_num(src)), dest,
                          false, false);
    } else if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_xmm(src)) {
        if (ir1_opnd_base_reg_num(dest) == ir1_opnd_base_reg_num(src)) {
            return true;
        } else {
            if (option_enable_lasx) {
                la_xvinsve0_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                              ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            } else {
                la_vextrins_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                              ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            }
        }
    }
    return true;
}

bool translate_movss(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);

    if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_mem(src)) {
        IR2_OPND temp = load_freg_from_ir1_1(src, false, IS_INTEGER);
        IR2_OPND xmm_dest = ra_alloc_xmm(ir1_opnd_base_reg_num(dest));
        la_xvpickve_w(xmm_dest, temp, 0);
        return true;
    } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
        store_freg_to_ir1(ra_alloc_xmm(ir1_opnd_base_reg_num(src)), dest,
                          false, false);
        return true;
    } else if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_xmm(src)) {
        if (ir1_opnd_base_reg_num(dest) == ir1_opnd_base_reg_num(src)) {
            return true;
        } else {
            if (option_enable_lasx) {
                la_xvinsve0_w(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                              ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            } else {
                la_vextrins_w(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                              ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            }
        }
        return true;
    }
    if (ir1_opnd_is_xmm(dest) || ir1_opnd_is_xmm(src)) {
        lsassert(0);
    }
    return true;
}

bool translate_movhpd(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_mem(src) && ir1_opnd_is_xmm(dest)) {
        IR2_OPND temp = load_ireg_from_ir1(src, ZERO_EXTENSION, false);
        la_vinsgr2vr_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), temp, 1);
    } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
        IR2_OPND temp = ra_alloc_itemp();
        la_vpickve2gr_d(temp,
                          ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 1);
        store_ireg_to_ir1(temp, dest, false);
    } else {
        lsassert(0);
    }
    return true;
}

bool translate_movhps(IR1_INST *pir1)
{
    translate_movhpd(pir1);
    return true;
}

bool translate_movlpd(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_mem(src) && ir1_opnd_is_xmm(dest)) {
        IR2_OPND temp = load_ireg_from_ir1(src, ZERO_EXTENSION, false);
        la_vinsgr2vr_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), temp, 0);
        return true;
    } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
        store_freg_to_ir1(ra_alloc_xmm(ir1_opnd_base_reg_num(src)), dest, false, false);
        return true;
    } else {
        lsassert(0);
    }

    if (ir1_opnd_is_mem(src)) {
        IR2_OPND src_lo = load_freg_from_ir1_1(src, false, true);
        store_freg_to_ir1(src_lo, ir1_get_opnd(pir1, 0), false, true);
    } else {
        IR2_OPND src_lo = load_freg_from_ir1_1(src, false, true);
        store_freg_to_ir1(src_lo, ir1_get_opnd(pir1, 0), false, true);
    }

    return true;
}

bool translate_movlps(IR1_INST *pir1)
{
    translate_movlpd(pir1);
    return true;
}

bool translate_movddup(IR1_INST *pir1)
{
    IR2_OPND src_lo =
        load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    if (option_enable_lasx) {
        la_xvreplve0_d(dest, src_lo);
    } else {
        la_vreplvei_d(dest, src_lo, 0);
    }

    return true;
}
