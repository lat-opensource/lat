#include "common.h"
#include "env.h"
#include "reg-alloc.h"
#include "translate.h"
#include "latx-options.h"
#include "hbr.h"

bool translate_das(IR1_INST *pir1)
{
    IR1_OPND *reg_al = &al_ir1_opnd;
    IR2_OPND old_al = load_ireg_from_ir1(reg_al, ZERO_EXTENSION, false);
    IR2_OPND new_al = ra_alloc_itemp();
    IR2_OPND eflag = ra_alloc_itemp();
    IR2_OPND imm_opnd = ra_alloc_itemp();
    IR2_OPND temp_opnd = ra_alloc_itemp();
    IR2_OPND n4095_opnd = ra_alloc_num_4095();
    IR2_OPND label_al_sub_6 = ra_alloc_label();
    IR2_OPND label_al_sub_60 = ra_alloc_label();
    IR2_OPND label_next = ra_alloc_label();
    IR2_OPND label_clear_zf = ra_alloc_label();
    IR2_OPND label_zf = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    /* read af and cf */
    la_x86mfflag(eflag, 0x5);
    /* af == 1 ? */
    la_andi(temp_opnd, eflag, 0x10);
    la_bne(temp_opnd, zero_ir2_opnd, label_al_sub_6);
    /* (AL AND 0FH) > 9 ? */
    la_andi(temp_opnd, old_al, 0xf);
    la_ori(imm_opnd, zero_ir2_opnd, 0xa);
    la_or(new_al, zero_ir2_opnd, old_al);
    la_blt(temp_opnd, imm_opnd, label_next);
    /* al = (al - 0x6) & 0xff and set af */
    la_x86mtflag(n4095_opnd, 0x4);
    la_label(label_al_sub_6);
    la_ori(imm_opnd, zero_ir2_opnd, 0x6);
    la_sub_w(new_al, old_al, imm_opnd);
    la_andi(new_al, new_al, 0xff);
    /* old_al < 6 ? */
    la_bge(old_al, imm_opnd, label_next);
    la_x86mtflag(n4095_opnd, 0x1);

    /* cf == 1 ? */
    la_label(label_next);
    la_andi(temp_opnd, eflag, 0x1);
    la_bne(temp_opnd, zero_ir2_opnd, label_al_sub_60);
    /* old_AL > 99H ? */
    la_ori(imm_opnd, zero_ir2_opnd, 0x9a);
    la_blt(old_al, imm_opnd, label_zf);
    la_x86mtflag(n4095_opnd, 0x1);
    /* al = (al - 0x60) & 0xff */
    la_label(label_al_sub_60);
    la_ori(imm_opnd, zero_ir2_opnd, 0x60);
    la_sub_w(new_al, new_al, imm_opnd);
    la_andi(new_al, new_al, 0xff);
    /* zf */
    la_label(label_zf);
    la_bne(new_al, zero_ir2_opnd, label_clear_zf);
    la_x86mtflag(n4095_opnd, 0x8);
    la_b(label_exit);
    la_label(label_clear_zf);
    la_x86mtflag(zero_ir2_opnd, 0x8);
    /* exit */
    la_label(label_exit);
    store_ireg_to_ir1(new_al, reg_al, false);
    generate_eflag_calculation(new_al, old_al, old_al, pir1, true);
    /* clear OF */
    la_x86mtflag(zero_ir2_opnd, 0x20);

    ra_free_num_4095(n4095_opnd);
    return true;
}

bool translate_aam(IR1_INST *pir1)
{
    IR1_OPND *reg_ah = &ah_ir1_opnd;
    IR1_OPND *reg_al = &al_ir1_opnd;
    IR2_OPND imm_opnd;
    int op_count = ir1_get_opnd_num(pir1);
    if(op_count == 0) {
        imm_opnd = ra_alloc_itemp();
        la_ori(imm_opnd, zero_ir2_opnd, 0xa);
    } else {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        imm_opnd = load_ireg_from_ir1(opnd0, ZERO_EXTENSION, false);
    }
    IR2_OPND al = load_ireg_from_ir1(reg_al, ZERO_EXTENSION, false);
    IR2_OPND old_al = ra_alloc_itemp();
    IR2_OPND ah = ra_alloc_itemp();

    la_or(old_al, zero_ir2_opnd, al);
    la_div_d(ah, al, imm_opnd);
    la_mod_d(al, al, imm_opnd);

    store_ireg_to_ir1(ah, reg_ah, false);
    store_ireg_to_ir1(al, reg_al, false);
    generate_eflag_calculation(al, old_al, imm_opnd, pir1, true);

    return true;
}

bool translate_aaa(IR1_INST *pir1)
{
    IR1_OPND *reg_ax = &ax_ir1_opnd;
    IR1_OPND *reg_al = &al_ir1_opnd;
    IR2_OPND temp_opnd = ra_alloc_itemp();
    IR2_OPND imm_opnd = ra_alloc_itemp();
    IR2_OPND n4095_opnd = ra_alloc_num_4095();
    IR2_OPND set_af_cf = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND ax = load_ireg_from_ir1(reg_ax, ZERO_EXTENSION, false);

    /* af */
    la_x86mfflag(temp_opnd, 0x4);
    /* af == 1 ? */
    la_andi(temp_opnd, temp_opnd, 0x10);
    la_bne(temp_opnd, zero_ir2_opnd, set_af_cf);
    /* (AL AND 0FH) > 9 ? */
    la_andi(temp_opnd, ax, 0xf);
    la_ori(imm_opnd, zero_ir2_opnd, 0xa);
    la_bge(temp_opnd, imm_opnd, set_af_cf);
    /* clear af and cf*/
    la_x86mtflag(zero_ir2_opnd, 0x5);
    la_b(label_exit);
    /* set clear af and cf*/
    la_label(set_af_cf);
    la_x86mtflag(n4095_opnd, 0x5);
    la_addi_d(ax, ax, 0x106);
    /* exit */
    la_label(label_exit);
    la_andi(temp_opnd, ax, 0xf);
    store_ireg_to_ir1(ax, reg_ax, false);
    store_ireg_to_ir1(temp_opnd, reg_al, false);

    ra_free_num_4095(n4095_opnd);

    return true;
}

bool translate_aas(IR1_INST *pir1)
{
    IR1_OPND *reg_ax = &ax_ir1_opnd;
    IR1_OPND *reg_al = &al_ir1_opnd;
    IR1_OPND *reg_ah = &ah_ir1_opnd;
    IR2_OPND imm_opnd = ra_alloc_itemp();
    IR2_OPND temp_opnd = ra_alloc_itemp();
    IR2_OPND n4095_opnd = ra_alloc_num_4095();
    IR2_OPND label_set_af_cf = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_sub_carry = ra_alloc_label();
    IR2_OPND ax_ir2 = load_ireg_from_ir1(reg_ax, ZERO_EXTENSION, false);
    IR2_OPND ah_ir2 = load_ireg_from_ir1(reg_ah, ZERO_EXTENSION, false);
    IR2_OPND al_ir2 = load_ireg_from_ir1(reg_al, ZERO_EXTENSION, false);

    la_x86mfflag(temp_opnd, 0x4);
    /* af == 1 ? */
    la_andi(temp_opnd, temp_opnd, 0x10);
    la_bne(temp_opnd, zero_ir2_opnd, label_set_af_cf);
    /* (AL AND 0FH) > 9 ? */
    la_andi(temp_opnd, ax_ir2, 0xf);
    la_ori(imm_opnd, zero_ir2_opnd, 0xa);
    la_bge(temp_opnd, imm_opnd, label_set_af_cf);
    /* check eflags */
    la_x86mtflag(zero_ir2_opnd, 0x5);
    la_b(label_exit);
    /* check if need sub carry */
    la_label(label_set_af_cf);
    la_ori(imm_opnd, zero_ir2_opnd, 0x6);
    la_bge(al_ir2, imm_opnd, label_sub_carry);
    /* sub carry */
    la_addi_w(ah_ir2, ah_ir2, -1);
    /* without sub carry */
    la_label(label_sub_carry);
    la_addi_w(ax_ir2, ax_ir2, -6);
    la_addi_w(ah_ir2, ah_ir2, -1);
    /* set EFLAGS AF and CF flags */
    la_x86mtflag(n4095_opnd, 0x5);
    la_label(label_exit);
    la_andi(al_ir2, ax_ir2, 0xf);
    store_ireg_to_ir1(al_ir2, reg_al, false);
    store_ireg_to_ir1(ah_ir2, reg_ah, false);

    ra_free_num_4095(n4095_opnd);

    return true;
}

bool translate_aad(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *reg_ah = &ah_ir1_opnd;
    IR1_OPND *reg_al = &al_ir1_opnd;
    IR2_OPND imm_opnd;
    IR2_OPND al = load_ireg_from_ir1(reg_al, ZERO_EXTENSION, false);
    IR2_OPND old_al = load_ireg_from_ir1(reg_al, ZERO_EXTENSION, false);
    IR2_OPND ah = load_ireg_from_ir1(reg_ah, ZERO_EXTENSION, false);
    IR2_OPND temp_opnd = ra_alloc_itemp();

    if (ir1_opnd_type(opnd0) == dt_X86_OP_IMM && ir1_opnd_size(opnd0) != 0) {
        imm_opnd = load_ireg_from_ir1(opnd0, ZERO_EXTENSION, false);
    } else {
        imm_opnd = ra_alloc_itemp();
        la_ori(imm_opnd, zero_ir2_opnd, 0x0A);
    }

    la_mul_d(temp_opnd, ah, imm_opnd);
    la_add_d(temp_opnd, temp_opnd, old_al);
    la_andi(al, temp_opnd, 0xFF);
    la_andi(ah, ah, 0x00);
    store_ireg_to_ir1(ah, reg_ah, false);
    store_ireg_to_ir1(al, reg_al, false);

    generate_eflag_calculation(al, old_al, imm_opnd, pir1, true);
    return true;
}

bool translate_daa(IR1_INST *pir1)
{
    IR1_OPND *reg_al = &al_ir1_opnd;
    IR2_OPND temp_opnd = ra_alloc_itemp();
    IR2_OPND imm_opnd = ra_alloc_itemp();
    IR2_OPND n4095_opnd = ra_alloc_num_4095();
    IR2_OPND not_clear_af = ra_alloc_label();
    IR2_OPND not_clear_cf = ra_alloc_label();
    IR2_OPND label_cf = ra_alloc_label();
    IR2_OPND label_zf = ra_alloc_label();
    IR2_OPND label_clear_zf = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND old_eax = load_ireg_from_ir1(reg_al, ZERO_EXTENSION, false);
    IR2_OPND new_eax = ra_alloc_itemp();

    /* af */
    la_x86mfflag(temp_opnd, 0x4);
    /* af == 1 ? */
    la_andi(temp_opnd, temp_opnd, 0x10);
    la_bne(temp_opnd, zero_ir2_opnd, not_clear_af);
    /* (AL AND 0FH) > 9 ? */
    la_andi(temp_opnd, old_eax, 0xf);
    la_ori(imm_opnd, zero_ir2_opnd, 0xa);
    la_bge(temp_opnd, imm_opnd, not_clear_af);
    /* clear af */
    la_x86mtflag(zero_ir2_opnd, 0x4);
    la_or(new_eax, zero_ir2_opnd, old_eax);
    la_b(label_cf);
    /* not clear af */
    la_label(not_clear_af);
    la_x86mtflag(n4095_opnd, 0x4);
    la_addi_w(new_eax, old_eax, 0x6);

    /* cf */
    la_label(label_cf);
    la_x86mfflag(temp_opnd, 0x1);
    /* cf == 1 ? */
    la_andi(temp_opnd, temp_opnd, 0x1);
    la_bne(temp_opnd, zero_ir2_opnd, not_clear_cf);
    /* old_AL > 99H ? */
    la_ori(imm_opnd, zero_ir2_opnd, 0x9a);
    la_bge(old_eax, imm_opnd, not_clear_cf);
    /* clear cf */
    la_x86mtflag(zero_ir2_opnd, 0x1);
    la_b(label_zf);
    /* not clear cf */
    la_label(not_clear_cf);
    la_x86mtflag(n4095_opnd, 0x1);
    la_addi_w(new_eax, new_eax, 0x60);
    /* zf */
    la_label(label_zf);
    li_wu(temp_opnd, 0xffff);
    la_and(temp_opnd, temp_opnd, new_eax);
    la_bne(temp_opnd, zero_ir2_opnd, label_clear_zf);
    la_x86mtflag(n4095_opnd, 0x8);
    la_b(label_exit);
    la_label(label_clear_zf);
    la_x86mtflag(zero_ir2_opnd, 0x8);
    /* exit */
    la_label(label_exit);
    store_ireg_to_ir1(new_eax, reg_al, false);
    generate_eflag_calculation(new_eax, old_eax, old_eax, pir1, true);

    ra_free_num_4095(n4095_opnd);

    return true;
}

bool translate_add(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_add(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;
    bool opt_imm;

    opnd0_size = ir1_opnd_size(opnd0);
    opt_imm = tr_opt_simm12(pir1);

    /* get src1 */
    if (opt_imm) {
        src1 = zero_ir2_opnd;
    } else {
        src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    }

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
        if (opnd0_size >= 32) {
            dest = src0;
        } else {
            dest = ra_alloc_itemp();
        }
    } else {
        src0 = ra_alloc_itemp();
        dest = src0;
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /* set eflag */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* calculate */
    if (opt_imm) {
        la_addi_d(dest, src0, (int)ir1_opnd_simm(opnd1));
    } else {
        la_add_d(dest, src0, src1);
    }
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

bool translate_adc(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_adc(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;

    opnd0_size = ir1_opnd_size(opnd0);

    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);

    /* alloc dest */
    dest = ra_alloc_itemp();

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
    } else {
        src0 = ra_alloc_itemp();
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /* calculate */
    la_adc_d(dest, src0, src1);
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif

    /* set eflag */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
        store_ireg_to_ir1(dest, opnd0, false);
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

bool translate_inc(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_inc(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size, gpr_num;

    opnd0_size = ir1_opnd_size(opnd0);

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        if (opnd0_size >= 32) {
            gpr_num = ir1_opnd_base_reg_num(opnd0);
            src0 = ra_alloc_gpr(gpr_num);
            dest = src0;
        } else {
            src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
            dest = ra_alloc_itemp();
        }
    } else {
        src0 = ra_alloc_itemp();
        dest = src0;
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /*
     * set eflag
     * The inst la_x86inc requires only one operand src0
     */
    generate_eflag_calculation(dest, src0, src0, pir1, true);

    /* calculate */
#ifndef TARGET_X86_64
    la_addi_w(dest, src0, 1);
#else
    if (CODEIS64) {
        la_addi_d(dest, src0, 1);
    } else {
        la_addi_w(dest, src0, 1);
    }
#endif
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

bool translate_dec(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_dec(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size, gpr_num;

    opnd0_size = ir1_opnd_size(opnd0);

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        if (opnd0_size >= 32) {
            gpr_num = ir1_opnd_base_reg_num(opnd0);
            src0 = ra_alloc_gpr(gpr_num);
            dest = src0;
        } else {
            src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
            dest = ra_alloc_itemp();
        }
    } else {
        src0 = ra_alloc_itemp();
        dest = src0;
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /*
     * set eflag
     * The inst la_x86dec requires only one operand src0
     */
    generate_eflag_calculation(dest, src0, src0, pir1, true);

    /* calculate */
#ifndef TARGET_X86_64
    la_addi_w(dest, src0, -1);
#else
    if (CODEIS64) {
        la_addi_d(dest, src0, -1);
    } else {
        la_addi_w(dest, src0, -1);
    }
#endif
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

bool translate_sub(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_sub(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;
    int src1_imm = 0;
    bool opt_imm = false;

    opnd0_size = ir1_opnd_size(opnd0);

    /* get src1 */
    if (ir1_opnd_is_imm(opnd1) &&
            !ir1_need_calculate_any_flag(pir1)) {
#ifdef CONFIG_LATX_FLAG_REDUCTION
        src1_imm = -ir1_opnd_simm(opnd1);
        if (src1_imm >= -2048 && src1_imm < 2047) {
            src1 = zero_ir2_opnd;
            opt_imm = true;
        } else {
            src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
        }
#endif
    } else {
        src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    }

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
        if (opnd0_size >= 32) {
            dest = src0;
        } else {
            dest = ra_alloc_itemp();
        }
    } else {
        src0 = ra_alloc_itemp();
        dest = src0;
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /* set eflag */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* calculate */
    if (opt_imm) {
        la_addi_d(dest, src0, src1_imm);
    } else {
        la_sub_d(dest, src0, src1);
    }
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

/**
* @brief translate_sbb - DEST ← (DEST – (SRC + CF))
*
* @param pir1
*
* @return
*/
bool translate_sbb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_sbb(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;

    opnd0_size = ir1_opnd_size(opnd0);

    /* alloc dest */
    dest = ra_alloc_itemp();
    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, SIGN_EXTENSION, false);

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        src0 = load_ireg_from_ir1(opnd0, SIGN_EXTENSION, false);
    } else {
        src0 = ra_alloc_itemp();
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /* calculate */
    la_sbc_d(dest, src0, src1);
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif

    /* set eflag */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
        store_ireg_to_ir1(dest, opnd0, false);
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

bool translate_neg(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_neg(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size, gpr_num;

    opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        if (opnd0_size >= 32) {
            gpr_num = ir1_opnd_base_reg_num(opnd0);
            src0 = ra_alloc_gpr(gpr_num);
            dest = src0;
        } else {
            src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
            dest = ra_alloc_itemp();
        }
    } else {
        src0 = ra_alloc_itemp();
        dest = src0;
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /* set eflag */
    generate_eflag_calculation(dest, zero_ir2_opnd, src0, pir1, true);

    /* calculate */
    la_sub_d(dest, zero_ir2_opnd, src0);
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

bool translate_cmp(IR1_INST *pir1)
{
    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0), UNKNOWN_EXTENSION, false);
    IR2_OPND src_opnd_1 =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 1), UNKNOWN_EXTENSION, false);

    /*
     * We can compute the eflags directly since the
     * hardware eflags optimization was enabled default.
     */
    generate_eflag_calculation(src_opnd_0, src_opnd_0, src_opnd_1, pir1, true);

    return true;
}


bool translate_mul(IR1_INST *pir1)
{
    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0), ZERO_EXTENSION, false);
    IR2_OPND dest_opnd = ra_alloc_itemp();
    IR2_OPND src_opnd_1;
    if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 8) {
        src_opnd_1 = load_ireg_from_ir1(&al_ir1_opnd, ZERO_EXTENSION, false);
        generate_eflag_calculation(dest_opnd, src_opnd_0, src_opnd_1, pir1,
                                   true);

        la_mul_w(dest_opnd, src_opnd_0, src_opnd_1);
        store_ireg_to_ir1(dest_opnd, &ax_ir1_opnd, false);
    } else if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 16) {
        src_opnd_1 = load_ireg_from_ir1(&ax_ir1_opnd, ZERO_EXTENSION, false);
        generate_eflag_calculation(dest_opnd, src_opnd_0, src_opnd_1, pir1,
                                   true);
        la_mul_w(dest_opnd, src_opnd_0, src_opnd_1);
        store_ireg_to_ir1(dest_opnd, &ax_ir1_opnd, false);
        la_srli_w(dest_opnd, dest_opnd, 16);
        store_ireg_to_ir1(dest_opnd, &dx_ir1_opnd, false);
    } else if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32) {
        src_opnd_1 = load_ireg_from_ir1(&eax_ir1_opnd, ZERO_EXTENSION, false);
        /*
         * NOTE: for X86 MUL insn, we have to make sure src_opnd_0 data is zero
         * extension, otherwise, mul data will be incorrect.
         */
        la_mov32_zx(src_opnd_0, src_opnd_0);
        generate_eflag_calculation(dest_opnd, src_opnd_0, src_opnd_1, pir1,
                                   true);
        la_mul_d(dest_opnd, src_opnd_0, src_opnd_1);
        store_ireg_to_ir1(dest_opnd, &eax_ir1_opnd, false);
        la_srli_d(dest_opnd, dest_opnd, 32);
        store_ireg_to_ir1(dest_opnd, &edx_ir1_opnd, false);
    } else {
        /* 64-bit mode */
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(&rax_ir1_opnd, ZERO_EXTENSION, false);

        IR2_OPND temp_src_0 = ra_alloc_itemp();
        la_add_d(temp_src_0, src_opnd_0, zero_ir2_opnd);
        generate_eflag_calculation(dest_opnd, src_opnd_0, src_opnd_1, pir1,
                                   true);
        la_mulh_du(ra_alloc_gpr(edx_index),
                               src_opnd_1, temp_src_0);
        la_mul_d(ra_alloc_gpr(eax_index), src_opnd_1,
                               temp_src_0);
        ra_free_temp(temp_src_0);
    }

    ra_free_temp(dest_opnd);
    return true;
}

static bool translate_imul_1_opnd(IR1_INST *pir1)
{
    /*
     * IMUL r/m8, IMUL r/m16, IMUL r/m32, IMUL r/m64
     * 1. IMUL r{8,16,32,64}
     *   1.1. IMUL r{32,64}
     *     1.1.1. IMUL r32              < mulw.d.w temp, reg1, reg2
     *     1.1.2. IMUL r64              < mul{h}.d temp, reg1, reg2
     *   1.2. IMUL r{8,16}
     *     1.2.1 IMUL {ax, al}          < mul{?} temp, temp1, temp1
     *     1.2.2 IMUL {else}            < default
     * 2. IMUL m{8,16,32,64}
     *   2.1 IMUL m{32,64}
     *     2.1.1. IMUL m32              < mulw.d.w temp, reg1, temp1
     *     2.1.2. IMUL m64              < mul{h}.d temp, reg1, temp1
     *   2.2 IMUL m{8,16}               < default
     *
     * ---------- collect all switch ----------
     *
     * IMUL r/m8, IMUL r/m16, IMUL r/m32, IMUL r/m64
     * 1. IMUL r/m{8,16}
     *   1.1 IMUL {ax, al}              < mul{?} temp, temp1, temp1
     *   1.2 IMUL {else}                < default
     * 2. IMUL r/m{32,64}
     *   2.1 IMUL r/m32                 < mulw.d.w temp, reg1, reg2/temp1
     *   2.2 IMUL r/m64                 < mul{h}.d temp, reg1, reg2/temp1
     */
    IR1_OPND *src_ir1 = ir1_get_opnd(pir1, 0);
    int src_size = ir1_opnd_size(src_ir1);

    /*
     * IMUL r/m{32,64}
     *   IMUL r/m32     < mulw.d.w temp, reg1, reg2/temp1
     *   IMUL r/m64     < mul{h}.d temp, reg1, reg2/temp1
     */
#ifdef TARGET_X86_64
    if (CODEIS64 && src_size == 64) {
        /*
         * IMUL r/m64     < mul{h}.d temp, reg1, reg2/temp1
         */
        lsassert(!ir1_opnd_is_gpr(src_ir1) || ir1_opnd_is_64bit(src_ir1));

        IR2_OPND src_opnd_0;
        IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
        IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
        /* NOTE: if src_opnd_0 is RDX, we need save it */
        if (ir1_opnd_is_gpr(src_ir1) &&
            (ir1_opnd_base_reg_num(src_ir1) == edx_index)) {
            src_opnd_0 = ra_alloc_itemp();
            la_mov64(src_opnd_0, edx_opnd);
        } else {
            src_opnd_0 = load_ireg_from_ir1(src_ir1, UNKNOWN_EXTENSION, false);
        }

        generate_eflag_calculation(eax_opnd, src_opnd_0, eax_opnd, pir1, true);
        la_mulh_d(edx_opnd, eax_opnd, src_opnd_0);
        la_mul_d(eax_opnd, eax_opnd, src_opnd_0);

        if (ir2_opnd_is_itemp(&src_opnd_0)) {
            ra_free_temp(src_opnd_0);
        }
        return true;
    } else
#endif
    if (src_size == 32) {
        /*
         * IMUL r/m32     < mulw.d.w temp, reg1, reg2/temp1
         */
        lsassert(!ir1_opnd_is_gpr(src_ir1) || ir1_opnd_is_32bit(src_ir1));

        IR2_OPND src_opnd_0 =
            load_ireg_from_ir1(src_ir1, UNKNOWN_EXTENSION, false);
        IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
        IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
        IR2_OPND dest = ra_alloc_itemp();

        la_mulw_d_w(dest, eax_opnd, src_opnd_0);
        generate_eflag_calculation(dest, src_opnd_0, eax_opnd, pir1, true);
        la_bstrpick_d(eax_opnd, dest, 31, 0);
        la_bstrpick_d(edx_opnd, dest, 63, 32);
        ra_free_temp(dest);
        return true;
    } else {
#ifdef CONFIG_LATX_DEBUG
        if (src_size == 8) {
            lsassert(!ir1_opnd_is_gpr(src_ir1) || ir1_opnd_is_8l(src_ir1) ||
                     ir1_opnd_is_8h(src_ir1));
        } else if (src_size == 16) {
            lsassert(!ir1_opnd_is_gpr(src_ir1) || ir1_opnd_is_16bit(src_ir1));
        } else {
            lsassert(0);
        }
#endif
        /*
         * IMUL r/m{8,16}
         *   IMUL {ax, al}  < mul{?} temp, temp1, temp1
         *   IMUL {else}    < default
         */
        if (ir1_opnd_is_gpr(src_ir1) &&
            (ir1_opnd_base_reg_num(src_ir1) == eax_index)) {
            IR2_OPND src_opnd;
            IR2_OPND dest = ra_alloc_itemp();
            if (src_size == 16) {
                /* IMUL ax */
                src_opnd =
                    load_ireg_from_ir1(&ax_ir1_opnd, SIGN_EXTENSION, false);
                la_mul_w(dest, src_opnd, src_opnd);
                generate_eflag_calculation(dest, src_opnd, src_opnd, pir1,
                                           true);
                store_ireg_to_ir1(dest, &ax_ir1_opnd, false);
                la_srli_w(dest, dest, 16);
                store_ireg_to_ir1(dest, &dx_ir1_opnd, false);
                return true;
            } else if (ir1_opnd_is_8l(src_ir1)) {
                /* IMUL al */
                src_opnd =
                    load_ireg_from_ir1(&al_ir1_opnd, SIGN_EXTENSION, false);
                la_mul_w(dest, src_opnd, src_opnd);
                generate_eflag_calculation(dest, src_opnd, src_opnd, pir1,
                                           true);
                store_ireg_to_ir1(dest, &ax_ir1_opnd, false);
                return true;
            }
            ra_free_temp(dest);
        }
    }

    /* default */

    IR2_OPND src_opnd_0 = load_ireg_from_ir1(src_ir1, SIGN_EXTENSION, false);
    IR2_OPND dest = ra_alloc_itemp();
    if (src_size == 8) {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(&al_ir1_opnd, SIGN_EXTENSION, false);
        la_mul_w(dest, src_opnd_1, src_opnd_0);
        /* calculate eflag */
        generate_eflag_calculation(dest, src_opnd_0, src_opnd_1, pir1, true);
        store_ireg_to_ir1(dest, &ax_ir1_opnd, false);
    } else if (src_size == 16) {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(&ax_ir1_opnd, SIGN_EXTENSION, false);
        la_mul_w(dest, src_opnd_1, src_opnd_0);
        /* calculate eflag */
        generate_eflag_calculation(dest, src_opnd_0, src_opnd_1, pir1, true);
        store_ireg_to_ir1(dest, &ax_ir1_opnd, false);
        la_srli_w(dest, dest, 16);
        store_ireg_to_ir1(dest, &dx_ir1_opnd, false);
    } else {
        lsassert(0);
    }
    ra_free_temp(dest);
    return true;
}

static bool translate_imul_(IR1_INST *pir1,
                            IR1_OPND *dest, IR1_OPND *src0, IR1_OPND *src1)
{
    /*
     * IMUL r{16,32,64}, r{16,32,64}, r/m{16,32,64} <- hahaha
     *
     * IMUL r{16,32,64}, r/m{16,32,64}, imm8
     * IMUL r16,         r/m16,         imm16
     * IMUL r{32,64},    r/m{32,64},    imm32
     *
     * 1. IMUL r{16,32,64},  r{16,32,64}, imm{8,16,32}
     *   1.1. IMUL r{64},    r{64},       imm{8,32}    < mul.d reg1, reg2, temp
     *   1.2. IMUL r{32},    r{32},       imm{8,32}    < mul.w temp, reg2, temp'
     *   1.3. IMUL r{16},    r{16},       imm{8,16}    < default
     * 2. IMUL r{16,32,64},  m{16,32,64}, imm{8,16,32}
     *   1.1. IMUL r{64},    m{64},       imm{8,32}    < mul.d reg1, temp, temp
     *   1.2. IMUL r{16,32}, m{16,32},    imm{8,16,32} < default
     * 3. IMUL r{16,32,64}, r{16,32,64}, r/m{16,32,64}
     *   3.1. IMUL r{64}, r{64}, r{64}                 < mul.d reg1, reg2, reg3
     *   3.2. IMUL r{32}, r{32}, r{32}                 < mul.w temp, reg2, reg3
     *   3.3. IMUL r{64}, r{64}, m{64}                 < mul.d reg1, reg2, temp
     *   3.4. IMUL r{32}, r{32}, m{32}                 < mul.w temp, reg2, temp'
     *   3.5. IMUL r{16}, r{16}, r/m{16}               < default
     *
     * ---------- collect all switch ----------
     * 0. IMUL r{16},    r/m{16},  r/m/imm{?}          < default
     * 1. IMUL r{32,64}, r{32,64}, r{32,64}
     *   1.1. IMUL r{64}, r{64}, r{64}                 < mul.d reg1, reg2, reg3
     *   1.2. IMUL r{32}, r{32}, r{32}                 < mul.w temp, reg2, reg3
     * 2. IMUL r{32,64}, r{32,64}, imm{8,32}/m{32,64}
     *   2.1. IMUL r{64}, r{64}, imm{8,32}/m{64}       < mul.d reg1, reg2, temp
     *   2.2. IMUL r{32}, r{32}, imm{8,32}/m{32}       < mul.w temp, reg2, temp'
     * 3. IMUL r{32,64}, m{32,64}, imm{8,32}
     *   3.1. IMUL r{64}, m{64}, imm{8,32}             < mul.d reg1, temp, temp
     *   3.2. IMUL r{32}, m{32}, imm{8,32}             < default
     */
    lsassert(ir1_opnd_is_gpr(dest));
    int dest_size = ir1_opnd_size(dest);
    IR2_OPND dest_ir2, src0_ir2, src1_ir2;
    if (dest_size == 16) {
        /* 0. IMUL r{16},    r/m{16},  r/m/imm{?} */
        goto _default;
    }
    if (ir1_opnd_is_gpr(src0)) {
        dest_ir2 = ra_alloc_gpr(ir1_opnd_base_reg_num(dest));
        src0_ir2 = ra_alloc_gpr(ir1_opnd_base_reg_num(src0));
        if (ir1_opnd_is_gpr(src1)) {
            /* 1. IMUL r{32,64}, r{32,64}, r{32,64} */
            src1_ir2 = ra_alloc_gpr(ir1_opnd_base_reg_num(src1));
        } else {
            /* 2. IMUL r{32,64}, r{32,64}, imm{8,32}/m{32,64} */
            src1_ir2 =
                load_ireg_from_ir1(src1, SIGN_EXTENSION, false);
        }
        generate_eflag_calculation(dest_ir2, src0_ir2, src1_ir2, pir1, true);
#ifdef TARGET_X86_64
        if (CODEIS64 && dest_size == 64) {
            /* 1.1. IMUL r{64}, r{64}, r{64} */
            /* -> mul.d reg1, reg2, reg3 */
            /* 2.1. IMUL r{64}, r{64}, imm{8,32}/m{64} */
            /* -> mul.d reg1, reg2, temp */
            la_mul_d(dest_ir2, src0_ir2, src1_ir2);
        } else
#endif
        {
            lsassert(dest_size == 32);
            /* 1.2. IMUL r{32}, r{32}, r{32} */
            /* -> mul.w temp, reg2, reg3 */
            /* 2.2. IMUL r{32}, r{32}, imm{8,32}/m{32} */
            /* -> mul.w temp, reg2, temp' */
            IR2_OPND temp = ra_alloc_itemp();
            la_mul_w(temp, src0_ir2, src1_ir2);
            la_mov32_zx(dest_ir2, temp);
            ra_free_temp(temp);
        }
    } else {
        /* 3. IMUL r{32,64}, m{32,64}, imm{8,32} */
        lsassert(ir1_opnd_is_mem(src0) && ir1_opnd_is_imm(src1));
#ifdef TARGET_X86_64
        if (CODEIS64 && dest_size == 64) {
            /* 3.1. IMUL r{64}, m{64}, imm{8,32} */
            /* -> mul.d reg1, temp, temp */
            dest_ir2 = ra_alloc_gpr(ir1_opnd_base_reg_num(dest));
            src0_ir2 = load_ireg_from_ir1(src0, SIGN_EXTENSION, false);
            src1_ir2 = load_ireg_from_ir1(src1, SIGN_EXTENSION, false);
            generate_eflag_calculation(dest_ir2, src0_ir2, src1_ir2, pir1, true);
            la_mul_d(dest_ir2, src0_ir2, src1_ir2);
        } else
#endif
        {
            /* 3.2. IMUL r{32}, m{32}, imm{8,32} */
            lsassert(dest_size == 32);
            goto _default;
        }
    }

    return true;
_default:
    /*
     * IMUL r{16}, r/m{16},  r/m/imm{?}
     * IMUL r{32}, m{32},    imm{8,32}
     */
    src0_ir2 = load_ireg_from_ir1(src0, SIGN_EXTENSION, false);
    src1_ir2 = load_ireg_from_ir1(src1, SIGN_EXTENSION, false);
    dest_ir2 = ra_alloc_itemp();
    la_mul_w(dest_ir2, src0_ir2, src1_ir2);
    generate_eflag_calculation(dest_ir2, src0_ir2, src1_ir2, pir1, true);
    store_ireg_to_ir1(dest_ir2, dest, false);
    ra_free_temp(dest_ir2);
    return true;
}

bool translate_imul(IR1_INST *pir1)
{
    if (ir1_opnd_num(pir1) == 1) {
        return translate_imul_1_opnd(pir1);
    } else if (ir1_opnd_num(pir1) == 2) {
        /*
         * IMUL r{16,32,64}, r/m{16,32,64}
         */
        IR1_OPND *src0 = ir1_get_opnd(pir1, 0);
        IR1_OPND *src1 = ir1_get_opnd(pir1, 1);
        return translate_imul_(pir1, src0, src0, src1);
    } else {
        lsassert(ir1_opnd_num(pir1) == 3);
        IR1_OPND *dest = ir1_get_opnd(pir1, 0);
        IR1_OPND *src0 = ir1_get_opnd(pir1, 1);
        IR1_OPND *src1 = ir1_get_opnd(pir1, 2);
        return translate_imul_(pir1, dest, src0, src1);
    }
}

bool translate_div(IR1_INST *pir1)
{
    IR2_OPND small_opnd =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0), ZERO_EXTENSION, false);
    IR2_OPND result = ra_alloc_itemp();
    IR2_OPND result_remainder = ra_alloc_itemp();

    IR2_OPND label_z = ra_alloc_label();

    la_bne(small_opnd, zero_ir2_opnd, label_z);
    la_break(0x7);
    la_label(label_z);

    if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 8) {
        IR2_OPND large_opnd =
            load_ireg_from_ir1(&ax_ir1_opnd, ZERO_EXTENSION, false);

        la_div_du(result, large_opnd, small_opnd);
        la_mod_du(result_remainder, large_opnd,
                            small_opnd);
        /*  result larger than uint8 would raise an exception */
        // ir2_opnd_set_em(&result, ZERO_EXTENSION, 8);
        // ir2_opnd_set_em(&result_remainder, ZERO_EXTENSION, 8);

        /* set AL and AH at the same time */
        la_slli_d(result_remainder, result_remainder,
                                8);
        la_or(result, result, result_remainder);
        store_ireg_to_ir1(result, &ax_ir1_opnd, false);
    } else if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 16) {
        IR2_OPND large_opnd =
            load_ireg_from_ir1(&ax_ir1_opnd, ZERO_EXTENSION, false);
        IR2_OPND large_opnd_high_bits =
            load_ireg_from_ir1(&dx_ir1_opnd, ZERO_EXTENSION, false);
        la_slli_d(large_opnd_high_bits,
                                large_opnd_high_bits, 16);
        la_or(large_opnd, large_opnd_high_bits,
                               large_opnd);

        la_div_du(result, large_opnd, small_opnd);
        la_mod_du(result_remainder, large_opnd,
                            small_opnd);
        /*  result larger than uint16 would raise an exception */
        // ir2_opnd_set_em(&result, ZERO_EXTENSION, 16);
        // ir2_opnd_set_em(&result_remainder, ZERO_EXTENSION, 16);

        store_ireg_to_ir1(result, &ax_ir1_opnd, false);
        store_ireg_to_ir1(result_remainder, &dx_ir1_opnd, false);
    } else if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32) {
        IR2_OPND large_opnd =
            load_ireg_from_ir1(&eax_ir1_opnd, ZERO_EXTENSION, false);
        IR2_OPND large_opnd_high_bits =
            load_ireg_from_ir1(&edx_ir1_opnd, UNKNOWN_EXTENSION, false);
        la_slli_d(large_opnd_high_bits,
                                large_opnd_high_bits, 32);
        la_or(large_opnd, large_opnd_high_bits,
                               large_opnd);

        IR2_OPND ir2_eax = ra_alloc_gpr(ir1_opnd_base_reg_num(&eax_ir1_opnd));
        IR2_OPND ir2_edx = ra_alloc_gpr(ir1_opnd_base_reg_num(&edx_ir1_opnd));

        /* load_ireg_from_ir1() may return the old register */
        if (ir2_eax._reg_num == large_opnd._reg_num) {
            IR2_OPND temp_large = ra_alloc_itemp();
            la_or(temp_large, large_opnd,
                                   zero_ir2_opnd);
            large_opnd = temp_large;
        }

        la_div_du(ir2_eax, large_opnd, small_opnd);
        la_mod_du(ir2_edx, large_opnd, small_opnd);
    } else {
        /* we cannot use EM because sometimes we will jump forward/behind */
        /* 64 bits div is diffcult */
        IR2_OPND divisor_high = small_opnd;
        IR2_OPND dividend_low = ra_alloc_gpr(eax_index);
        IR2_OPND dividend_high = ra_alloc_gpr(edx_index);
        /* 0. if RDX = 0, using `ddivu` directly */
        IR2_OPND label_begin = ra_alloc_label();
        IR2_OPND label_finish = ra_alloc_label();
        la_bne(dividend_high, zero_ir2_opnd, label_begin);
        /* we need to make divisor not be changed by others */
        la_add_d(result, divisor_high, zero_ir2_opnd);
        la_mod_du(dividend_high, dividend_low, result);
        la_div_du(dividend_low, dividend_low, result);
        la_b(label_finish);
        /* 1. init count result */
    /* label_begin: */
        la_label(label_begin);
        /* first we need make divisor_high as a temp */
        if (!ir2_opnd_is_itemp(&divisor_high)) {
            divisor_high = ra_alloc_itemp();
            /* mov divisor to divisor_high */
            la_add_d(divisor_high, zero_ir2_opnd, small_opnd);
        }
        IR2_OPND count = ra_alloc_itemp();
        IR2_OPND divisor = result_remainder;
        la_addi_d(count, zero_ir2_opnd, 65);
        la_add_d(result, zero_ir2_opnd, zero_ir2_opnd);
        la_add_d(divisor, zero_ir2_opnd, zero_ir2_opnd);

        IR2_OPND dividend_less_divisor = ra_alloc_itemp();
        IR2_OPND label_next_turn = ra_alloc_label();
        IR2_OPND label_do_check = ra_alloc_label();
        IR2_OPND label_do_div = ra_alloc_label();
        IR2_OPND label_save_result = ra_alloc_label();
        /* 2. identify next turn need calculate or not */
    /* label_next_turn: */
        la_label(label_next_turn);
        /* 2.1 make count-- */
        la_addi_d(count, count, -1);
        /* 2.2 for next turn, result will shift left 1 bit */
        la_slli_d(result, result, 1);
        /* 2.3 if dividend < divisor, it means result is zero */
        /*     check if need do next turn, also shift divisor */
        /* 2.3.1 dividend_high < divisor_high */
        la_sltu(dividend_less_divisor,
                            dividend_high, divisor_high);
        la_bne(dividend_less_divisor,
                            zero_ir2_opnd, label_do_check);
        /* 2.3.2 dividend_high = divisor_high but */
        /*       dividend_low < divisor_low */
        la_sltu(dividend_less_divisor,
                            dividend_low, divisor);
        la_bne(dividend_high,
                            divisor_high, label_do_div);
        la_bne(dividend_less_divisor,
                            zero_ir2_opnd, label_do_check);

        /* 3. doing this turn div (sub) */
    /* label_do_div: */
        la_label(label_do_div);
        /* 3.1 result + 1 */
        la_addi_d(result, result, 1);
        /* 3.2 remainder */
        /* 3.2.1 if dividend_low < divisor_low, dividend_high-- */
        /*       `dividend_less_divisor` is caculate before */
        la_sub_d(dividend_high,
                            dividend_high, dividend_less_divisor);
        /* 3.2.2 dividend_low - divisor_low */
        la_sub_d(dividend_low,
                            dividend_low, divisor);
        /* 3.2.2 dividend_high - divisor_high */
        la_sub_d(dividend_high,
                            dividend_high, divisor_high);
        ra_free_temp(dividend_less_divisor);

    /* label_do_check: */
        la_label(label_do_check);
        /* 4. if count == 0, break; */
        la_beq(count, zero_ir2_opnd, label_save_result);

        /* 5. Divisor >> 1 */
        IR2_OPND divisor_first_bit = ra_alloc_itemp();
        la_srli_d(divisor, divisor, 1);
        la_slli_d(divisor_first_bit, divisor_high, 63);
        la_or(divisor, divisor, divisor_first_bit);
        la_srli_d(divisor_high, divisor_high, 1);
        /* 6. branch to next turn */
        la_b(label_next_turn);
        ra_free_temp(divisor_first_bit);

    /* label_save_result: */
        la_label(label_save_result);
        /* 7. save to regs */
        /*    Known: dividend_low maybe RAX */
        /*    So we need save dividend_low to RDX first */
        IR2_OPND ir2_edx = ra_alloc_gpr(edx_index);
        IR2_OPND ir2_eax = ra_alloc_gpr(eax_index);
        la_or(ir2_edx, zero_ir2_opnd, dividend_low);
        la_or(ir2_eax, zero_ir2_opnd, result);
        /* 8. finish the DIV */
    /* label_finish: */
        la_label(label_finish);

        ra_free_temp(count);
        if (ir2_opnd_is_itemp(&divisor_high)) {
            ra_free_temp(divisor_high);
        }
    }

    ra_free_temp(result);
    ra_free_temp(result_remainder);
    return true;
}

bool translate_idiv(IR1_INST *pir1)
{
    IR2_OPND dest_opnd = ra_alloc_itemp();
    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0), SIGN_EXTENSION, false);

    IR2_OPND label_z = ra_alloc_label();
    la_bne(src_opnd_0, zero_ir2_opnd, label_z);
    la_break(0x7);
    la_label(label_z);

    if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 8) {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(&ax_ir1_opnd, SIGN_EXTENSION, false);
        la_mod_d(dest_opnd, src_opnd_1, src_opnd_0);
        la_div_d(src_opnd_1, src_opnd_1, src_opnd_0);

        // ir2_opnd_set_em(&src_opnd_1, SIGN_EXTENSION, 8);
        // ir2_opnd_set_em(&dest_opnd, SIGN_EXTENSION, 8);
        store_ireg_to_ir1(src_opnd_1, &al_ir1_opnd, false);
        store_ireg_to_ir1(dest_opnd, &ah_ir1_opnd, false);
    } else if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 16) {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(&ax_ir1_opnd, ZERO_EXTENSION, false);
        IR2_OPND src_opnd_2 =
            load_ireg_from_ir1(&dx_ir1_opnd, UNKNOWN_EXTENSION, false);
        IR2_OPND temp_src = ra_alloc_itemp();
        IR2_OPND temp1_opnd = ra_alloc_itemp();

        la_slli_w(temp_src, src_opnd_2, 16);
        la_or(temp_src, temp_src, src_opnd_1);
        la_mod_d(temp1_opnd, temp_src, src_opnd_0);
        la_div_d(temp_src, temp_src, src_opnd_0);
        // ir2_opnd_set_em(&temp1_opnd, SIGN_EXTENSION, 16);
        // ir2_opnd_set_em(&temp_src, UNKNOWN_EXTENSION, 32);

        store_ireg_to_ir1(temp_src, &ax_ir1_opnd, false);
        store_ireg_to_ir1(temp1_opnd, &dx_ir1_opnd, false);

        ra_free_temp(temp_src);
        ra_free_temp(temp1_opnd);
    } else if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32) {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(&eax_ir1_opnd, ZERO_EXTENSION, false);
        IR2_OPND src_opnd_2 =
            load_ireg_from_ir1(&edx_ir1_opnd, UNKNOWN_EXTENSION, false);
        IR2_OPND temp_src = ra_alloc_itemp();
        IR2_OPND temp1_opnd = ra_alloc_itemp();

        la_slli_d(temp_src, src_opnd_2, 32);
        la_or(temp_src, temp_src, src_opnd_1);

        la_mod_d(temp1_opnd, temp_src, src_opnd_0);
        la_div_d(temp_src, temp_src, src_opnd_0);

        store_ireg_to_ir1(temp_src, &eax_ir1_opnd, false);
        store_ireg_to_ir1(temp1_opnd, &edx_ir1_opnd, false);

        ra_free_temp(temp_src);
        ra_free_temp(temp1_opnd);
    } else {
        /* first we need make divisor_high a temp */
        IR2_OPND divisor_high = src_opnd_0;
        if (!ir2_opnd_is_itemp(&divisor_high)) {
            divisor_high = ra_alloc_itemp();
            /* mov divisor_temp to divisor */
            la_add_d(divisor_high, zero_ir2_opnd, src_opnd_0);
        }
        /* for IDIV, sign need to be considered */
        IR2_OPND dividend_low = ra_alloc_gpr(eax_index);
        IR2_OPND dividend_high = ra_alloc_gpr(edx_index);
        IR2_OPND flags = ra_alloc_itemp();
        /* clean flags, bit1 is result, bit 2 is remainder */
        la_add_d(flags, zero_ir2_opnd, zero_ir2_opnd);
        IR2_OPND label_check_divisor = ra_alloc_label();
        IR2_OPND label_dividend_low_not_zero = ra_alloc_label();
        IR2_OPND label_div = ra_alloc_label();
        /* 0. pre-work: sign change */
        /* In this part, all the data will change to positive number */
        /* 0.1 check dividend */
        /* 0.1.1 if dividend_high >= 0, mark need not change the dividend */
        la_bge(dividend_high, zero_ir2_opnd, label_check_divisor);
        /* 0.1.2 then dividend_high < 0, mark this is negative number */
        /*       we need change the number to positive */
        /* 0.1.2.1 we can set the flag */
        /*         bit1 and bit2 all set 1 */
        la_xori(flags, flags, 1);
        la_xori(flags, flags, 2);
        /* 0.1.2.2 when dividend_low == 0 */
        /*         dividend_high = ~(dividend_high - 1) */
        /*         dividend_low not change */
        la_bne(dividend_low,
                zero_ir2_opnd, label_dividend_low_not_zero);
        la_addi_d(dividend_high, dividend_high, -1);
    /* label_dividend_low_not_zero: */
        la_label(label_dividend_low_not_zero);
        /* 0.1.2.3 when dividend_low != 0 */
        /*         dividend_high = ~dividend_high */
        /*         dividend_low = -dividend_low */
        la_nor(dividend_high, dividend_high, zero_ir2_opnd);
        la_sub_d(dividend_low, zero_ir2_opnd, dividend_low);
    /* label_check_divisor: */
        /* 0.2 check divisor */
        la_label(label_check_divisor);
        /* 0.2.3 if divisor >= 0, mark need not change  */
        la_bge(divisor_high, zero_ir2_opnd, label_div);
        /* else divisor = -divisor */
        la_sub_d(divisor_high, zero_ir2_opnd, divisor_high);
        /* also mark the flags */
        la_xori(flags, flags, 1);
    /* label_div: */
        la_label(label_div);
        /* begin div */

        /* 1. if RDX = 0, using `ddivu` directly */
        IR2_OPND label_begin = ra_alloc_label();
        IR2_OPND label_finish = ra_alloc_label();
        la_bne(dividend_high, zero_ir2_opnd, label_begin);
        la_mod_du(ra_alloc_gpr(edx_index), dividend_low, divisor_high);
        la_div_du(ra_alloc_gpr(eax_index), dividend_low, divisor_high);

        /* 1.1 if flags & 2, RDX = -RDX */
        IR2_OPND ax_flag = ra_alloc_itemp();
        IR2_OPND label_no_change = ra_alloc_label();
        la_andi(ax_flag, flags, 2);
        la_beq(ax_flag, zero_ir2_opnd, label_no_change);
        /* RDX = -RDX */
        la_sub_d(ra_alloc_gpr(edx_index),
                            zero_ir2_opnd, ra_alloc_gpr(edx_index));
    /* label_no_change: */
        la_label(label_no_change);
        /* 1.2 if flags & 1, RAX = -RAX */
        la_andi(ax_flag, flags, 1);
        la_beq(ax_flag, zero_ir2_opnd, label_finish);
        /* RAX = -RAX */
        la_sub_d(ra_alloc_gpr(eax_index),
                            zero_ir2_opnd, ra_alloc_gpr(eax_index));
        ra_free_temp(ax_flag);
        la_b(label_finish);
        /* 2. init count result */
    /* label_begin: */
        la_label(label_begin);
        IR2_OPND count = ra_alloc_itemp();
        IR2_OPND divisor = ra_alloc_itemp();
        IR2_OPND result = dest_opnd;
        la_addi_d(count, zero_ir2_opnd, 65);
        la_add_d(result, zero_ir2_opnd, zero_ir2_opnd);
        la_add_d(divisor, zero_ir2_opnd, zero_ir2_opnd);

        IR2_OPND dividend_less_divisor = ra_alloc_itemp();
        IR2_OPND label_next_turn = ra_alloc_label();
        IR2_OPND label_do_check = ra_alloc_label();
        IR2_OPND label_do_div = ra_alloc_label();
        IR2_OPND label_save_result = ra_alloc_label();
        /* 3. identify next turn need caculate or not */
    /* label_next_turn: */
        la_label(label_next_turn);
        /* 3.1 make count-- */
        la_addi_d(count, count, -1);
        /* 3.2 for next turn, result will shift left 1 bit */
        la_slli_d(result, result, 1);
        /* 3.3 if dividend < divisor, it means result is zero */
        /*     check if need do next turn, also shift divisor */
        /* 3.3.1 dividend_high < divisor_high */
        la_sltu(dividend_less_divisor,
                            dividend_high, divisor_high);
        la_bne(dividend_less_divisor,
                            zero_ir2_opnd, label_do_check);
        /* 3.3.2 dividend_high = divisor_high but */
        /*       dividend_low < divisor_low */
        /* no d_slot anymore, so exec before branch */
        la_sltu(dividend_less_divisor,
                            dividend_low, divisor);
        la_bne(dividend_high,
                            divisor_high, label_do_div);
        la_bne(dividend_less_divisor,
                            zero_ir2_opnd, label_do_check);

        /* 4. doing this turn div (sub) */
    /* label_do_div: */
        la_label(label_do_div);
        /* 4.1 result + 1 */
        la_addi_d(result, result, 1);
        /* 4.2 remainder */
        /* 4.2.1 if dividend_low < divisor_low, dividend_high-- */
        /*       `dividend_less_divisor` is caculate before */
        la_sub_d(dividend_high,
                            dividend_high, dividend_less_divisor);
        /* 4.2.2 dividend_low - divisor_low */
        la_sub_d(dividend_low,
                            dividend_low, divisor);
        /* 4.2.2 dividend_high - divisor_high */
        la_sub_d(dividend_high,
                            dividend_high, divisor_high);
        ra_free_temp(dividend_less_divisor);

    /* label_do_check: */
        la_label(label_do_check);
        /* 5. if count == 0, break; */
        la_beq(count, zero_ir2_opnd, label_save_result);

        /* 6. Divisor >> 1 */
        IR2_OPND divisor_first_bit = ra_alloc_itemp();
        la_srli_d(divisor, divisor, 1);
        la_slli_d(divisor_first_bit, divisor_high, 63);
        la_or(divisor, divisor, divisor_first_bit);
        /* 7. branch to next turn */
        la_srli_d(divisor_high, divisor_high, 1);
        la_b(label_next_turn);
        ra_free_temp(divisor_first_bit);

    /* label_save_result: */
        la_label(label_save_result);
        /* 8. save to regs */
        /* 8.1 if flags & 2, RDX = -RDX */
        IR2_OPND ex_flag = ra_alloc_itemp();
        IR2_OPND label_rdx_no_change = ra_alloc_label();
        IR2_OPND ir2_edx = ra_alloc_gpr(edx_index);
        IR2_OPND ir2_eax = ra_alloc_gpr(eax_index);
        la_andi(ex_flag, flags, 2);
        la_or(ir2_edx,
                            zero_ir2_opnd, dividend_low);
        la_beq(ex_flag, zero_ir2_opnd, label_rdx_no_change);
        /* RDX = -dividend_low(RDX) */
        la_sub_d(ir2_edx, zero_ir2_opnd, dividend_low);
    /* label_rdx_no_change: */
        la_label(label_rdx_no_change);
        /* 8.2 if flags & 1, RAX = -RAX */
        la_andi(ex_flag, flags, 1);
        la_or(ir2_eax, zero_ir2_opnd, result);
        la_beq(ex_flag, zero_ir2_opnd, label_finish);
        /* RAX = -result(RAX) */
        la_sub_d(ir2_eax, zero_ir2_opnd, result);
        ra_free_temp(ex_flag);
        /* 9. finish the DIV */
    /* label_finish: */
        la_label(label_finish);

        ra_free_temp(flags);
        ra_free_temp(count);
        ra_free_temp(divisor);
        if (ir2_opnd_is_itemp(&divisor_high)) {
            ra_free_temp(divisor_high);
        }
    }

    ra_free_temp(dest_opnd);
    return true;
}

bool translate_xadd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_xadd(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;

    opnd0_size = ir1_opnd_size(opnd0);

    /* get src1 */
    src1 = ra_alloc_itemp();
    load_ireg_from_ir1_2(src1, opnd1, SIGN_EXTENSION, false);

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
        if (opnd0_size >= 32) {
            dest = src0;
        } else {
            dest = ra_alloc_itemp();
        }
    } else {
        src0 = ra_alloc_itemp();
        dest = ra_alloc_itemp();
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /* set eflag */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* calculate */
    if (ir1_opnd_is_gpr(opnd0)) {
        store_ireg_to_ir1(src0, opnd1, false);
    }
    la_add_d(dest, src0, src1);
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
        /* src1 有可能与 mem_opnd 为同一寄存器，因此最后 store */
        store_ireg_to_ir1(src0, opnd1, false);
    }

    return true;
}

bool translate_mulx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest0 = load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);
    IR2_OPND dest1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    IR2_OPND src0 = load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);
#ifdef TARGET_X86_64
    IR1_OPND *opndselect = &edx_ir1_opnd;
    if (CODEIS64) {
        opndselect = &rdx_ir1_opnd;
    }
    IR2_OPND src1 = load_ireg_from_ir1(opndselect, ZERO_EXTENSION, false);
#else
    IR2_OPND src1 = load_ireg_from_ir1(&edx_ir1_opnd, ZERO_EXTENSION, false);
#endif
    /* TODO : if dest == src, this calculate will wrong. */
    if(dest0._val == src0._val || dest1._val == src0._val) {
        IR2_OPND src0_tmp = ra_alloc_itemp();
        la_or(src0_tmp, zero_ir2_opnd, src0);
        src0 = src0_tmp;
    }
    if(dest0._val == src1._val || dest1._val == src1._val) {
        IR2_OPND src1_tmp = ra_alloc_itemp();
        la_or(src1_tmp, zero_ir2_opnd, src1);
        src1 = src1_tmp;
    }

    if (ir1_opnd_size(opnd0) == 32) {
        /*
         * NOTE: for X86 MUL insn, we have to make sure src_opnd_0 data is zero
         * extension, otherwise, mul data will be incorrect.
         * la_mov32_zx(src_opnd_0, src_opnd_0);
         *
         * LYZ： but I think it is right, let us have a try.
         */
        la_mul_w(dest1, src0, src1);
        la_mulh_wu(dest0, src0, src1);
        la_bstrins_d(dest1, zero_ir2_opnd, 63, 32);
        la_bstrins_d(dest0, zero_ir2_opnd, 63, 32);
    } else {
        /* 64-bit mode */
        la_mul_d(dest1, src0, src1);
        la_mulh_du(dest0, src0, src1);
    }
    return true;
}
