/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"
#include "hbr.h"
#include "latx-smc.h"

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

bool translate_movaps_vst_x4(IR1_INST *pir1)
{
    IR1_INST *p0 = pir1;
    IR1_INST *p1 = pir1 + 1;
    IR1_INST *p2 = pir1 + 2;
    IR1_INST *p3 = pir1 + 3;

    IR1_OPND *dest0 = ir1_get_opnd(p0, 0);
    IR1_OPND *dest1 = ir1_get_opnd(p1, 0);
    IR1_OPND *dest2 = ir1_get_opnd(p2, 0);
    IR1_OPND *dest3 = ir1_get_opnd(p3, 0);

    if (!ir1_opnd_is_mem(dest0)) return false;
    if (!ir1_opnd_is_mem(dest1)) return false;
    if (!ir1_opnd_is_mem(dest2)) return false;
    if (!ir1_opnd_is_mem(dest3)) return false;

    if (ir1_opnd_has_index(dest0)) return false;
    if (ir1_opnd_has_index(dest1)) return false;
    if (ir1_opnd_has_index(dest2)) return false;
    if (ir1_opnd_has_index(dest3)) return false;

    int base0 = ir1_opnd_base_reg(dest0);
    int base1 = ir1_opnd_base_reg(dest1);
    int base2 = ir1_opnd_base_reg(dest2);
    int base3 = ir1_opnd_base_reg(dest3);

    if (!(base0 == base1 && base1 == base2 && base2 == base3)) {
        return false;
    }

    int off0 = ir1_opnd_simm(dest0);
    int off1 = ir1_opnd_simm(dest1);
    int off2 = ir1_opnd_simm(dest2);
    int off3 = ir1_opnd_simm(dest3);

#define A16(n)  ((n)+16)
#define A16OK(o0, o1, o2, o3)   \
    (A16(o0) == (o1) && A16(o1) == (o2) && A16(o2) == (o3))
#define B16OK(o0, o1, o2, o3)   \
    (A16(o3) == (o2) && A16(o2) == (o1) && A16(o1) == (o0))
    int a16ok = A16OK(off0, off1, off2, off3);
    int b16ok = B16OK(off0, off1, off2, off3);
    if (!a16ok && !b16ok) return false;

    IR1_OPND *src0 = ir1_get_opnd(p0, 1);
    IR1_OPND *src1 = ir1_get_opnd(p1, 1);
    IR1_OPND *src2 = ir1_get_opnd(p2, 1);
    IR1_OPND *src3 = ir1_get_opnd(p3, 1);

    if (!ir1_opnd_is_xmm(src0)) return false;
    if (!ir1_opnd_is_xmm(src1)) return false;
    if (!ir1_opnd_is_xmm(src2)) return false;
    if (!ir1_opnd_is_xmm(src3)) return false;

    int r0 = ir1_opnd_base_reg_num(src0);
    int r1 = ir1_opnd_base_reg_num(src1);
    int r2 = ir1_opnd_base_reg_num(src2);
    int r3 = ir1_opnd_base_reg_num(src3);

    int xmm = 0;
    xmm |= (0x1 << r0);
    xmm |= (0x1 << r1);
    xmm |= (0x1 << r2);
    xmm |= (0x1 << r3);

    IR2_OPND label_finish = ra_alloc_label();
    IR2_OPND tmp = ra_alloc_itemp();
    IR2_OPND mem;
    int offset;
    if (a16ok) {
        mem = convert_mem(dest0, &offset);
        li_w(tmp, offset);
        la_add_d(tmp, mem, tmp);
    } else if (b16ok) {
        mem = convert_mem(dest3, &offset);
        li_w(tmp, offset);
        la_add_d(tmp, mem, tmp);
    }

    // save context
    tr_save_registers_to_env(0xff, 0x0, xmm & 0xff, 0);
#ifdef TARGET_X86_64
    tr_save_x64_8_registers_to_env(0xff, (xmm >> 8) & 0xff);
#endif

    // call helper
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_mov64(a1_ir2_opnd, tmp);
    if (a16ok) {
        la_ori(a2_ir2_opnd, zero_ir2_opnd, r0);
        la_ori(a3_ir2_opnd, zero_ir2_opnd, r1);
        la_ori(a4_ir2_opnd, zero_ir2_opnd, r2);
        la_ori(a5_ir2_opnd, zero_ir2_opnd, r3);
    } else if (b16ok) {
        la_ori(a2_ir2_opnd, zero_ir2_opnd, r3);
        la_ori(a3_ir2_opnd, zero_ir2_opnd, r2);
        la_ori(a4_ir2_opnd, zero_ir2_opnd, r1);
        la_ori(a5_ir2_opnd, zero_ir2_opnd, r0);
    }
    aot_load_host_addr(tmp, (ADDR)smc_store_helper_vst_x4,
            LOAD_HELPER_SMC_VST_X4, 0);
    la_jirl(ra_ir2_opnd, tmp, 0);

    // restore context
    tr_load_registers_from_env(0xff, 0x0, 0x0, 0);
#ifdef TARGET_X86_64
    tr_load_x64_8_registers_from_env(0xff, 0x0);
#endif

    // beq a0, finish
    la_beq(a0_ir2_opnd, zero_ir2_opnd, label_finish);

    IR2_OPND ir2_opnd_addr;

    store_freg128_to_ir1_mem(ra_alloc_xmm(r0), dest0);

    ir2_opnd_build(&ir2_opnd_addr, IR2_OPND_IMM, ir1_addr(p1));
    la_x86_inst(ir2_opnd_addr);
    store_freg128_to_ir1_mem(ra_alloc_xmm(r1), dest1);

    ir2_opnd_build(&ir2_opnd_addr, IR2_OPND_IMM, ir1_addr(p2));
    la_x86_inst(ir2_opnd_addr);
    store_freg128_to_ir1_mem(ra_alloc_xmm(r2), dest2);

    ir2_opnd_build(&ir2_opnd_addr, IR2_OPND_IMM, ir1_addr(p3));
    la_x86_inst(ir2_opnd_addr);
    store_freg128_to_ir1_mem(ra_alloc_xmm(r3), dest3);

    // --> finish
    la_label(label_finish);

    return true;
}

void smc_hook_get_xmm(void *_env, int reg, uint64_t *val)
{
    CPUX86State *env = _env;
    val[0] = env->xmm_regs[reg]._q_ZMMReg[0];
    val[1] = env->xmm_regs[reg]._q_ZMMReg[1];
}

bool translate_movaps(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_mem(src)) {
        load_freg128_from_ir1_mem(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                                  src);
    } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
#ifdef CONFIG_LATX_SMC_OPT
        if (tb_use_smc_opt(lsenv->tr_data->curr_tb) &&
            (ir1_opcode(pir1) == dt_X86_INS_MOVAPS ||
             ir1_opcode(pir1) == dt_X86_INS_MOVDQA))
        {
            IR2_OPND label_finish = ra_alloc_label();
            IR2_OPND tmp = ra_alloc_itemp();
            int offset;
            IR2_OPND mem = convert_mem(dest, &offset);
            li_w(tmp, offset);
            la_add_d(tmp, mem, tmp);
            // save context
            tr_save_registers_to_env(0xff, 0x0, 0x0, 0);
#ifdef TARGET_X86_64
            tr_save_x64_8_registers_to_env(0xff, 0x0);
#endif
            // call smc_store_helper
            la_mov64(a0_ir2_opnd, env_ir2_opnd);
            la_mov64(a1_ir2_opnd, tmp);
            la_vpickve2gr_d(a2_ir2_opnd,
              ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            la_vpickve2gr_d(a3_ir2_opnd,
              ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 1);
            aot_load_host_addr(tmp, (ADDR)smc_store_helper_vst,
                    LOAD_HELPER_SMC_VST, 0);
            la_jirl(ra_ir2_opnd, tmp, 0);
            // restore context
            tr_load_registers_from_env(0xff, 0x0, 0x0, 0);
#ifdef TARGET_X86_64
            tr_load_x64_8_registers_from_env(0xff, 0x0);
#endif
            // beq a0, finish
            la_beq(a0_ir2_opnd, zero_ir2_opnd, label_finish);
            store_freg128_to_ir1_mem(ra_alloc_xmm(ir1_opnd_base_reg_num(src)),
                                     dest);
            // --> finish
            la_label(label_finish);
        } else {
            store_freg128_to_ir1_mem(ra_alloc_xmm(ir1_opnd_base_reg_num(src)),
                                     dest);
        }
#else
        store_freg128_to_ir1_mem(ra_alloc_xmm(ir1_opnd_base_reg_num(src)),
                                 dest);
#endif
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
        if(option_enable_lasx) {
            la_xvpickve_w(xmm_dest, temp, 0);
        } else {
            la_vandi_b(xmm_dest, xmm_dest, 0);
            la_vextrins_w(xmm_dest, temp, 0);
        }
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
