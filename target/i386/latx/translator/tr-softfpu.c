/**
 * @file tr-softfpu.c
 * @author Hanlu Li <heuleehanlu@gmail.com>
 *         yuerengan <y347812075@163.com>
 * @brief softfpu translation functions
 */
#include <math.h>
#include "common.h"
#include "imm-cache.h"
#include "la-append.h"
#include "la-ir2.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "reg-map.h"
#include "translate.h"

/**
* @brief - Compatible with different disassemblers
*
* @param pir1
*
* @return - src opnd index
*/
static int get_src_index(IR1_INST *pir1)
{
    switch (ir1_get_opnd_num(pir1)) {
    case 1:
        return 0;
    case 2:
        return 1;
    default:
        lsassert(0);
        return -1;
    }
}

static int is_data32(IR1_INST *pir1)
{
    int dflag = 32;
#ifdef TARGET_X86_64
    if (CODEIS64) {
        if (ir1_rex(pir1) & 0x8) {
            dflag = 64;
        } else if (ir1_prefix_opnd_size(pir1)) {
            dflag = 16;
        }
    } else {
        if (ir1_prefix_opnd_size(pir1)) {
            dflag = 16;
        }
    }
#else
    if (ir1_prefix_opnd_size(pir1)) {
        dflag = 16;
    }
#endif
    return dflag == 32 ? 1 : 0;
}

static void save_eflags(IR1_INST *pir1)
{
    IR2_OPND eflags_opnd = ra_alloc_eflags();
    IR2_OPND eflags_temp;

    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_FCMOVNB:
    case dt_X86_INS_FCMOVB:
    case dt_X86_INS_FCMOVNU:
    case dt_X86_INS_FCMOVU:
    case dt_X86_INS_FCMOVE:
    case dt_X86_INS_FCMOVNE:
    case dt_X86_INS_FCMOVBE:
    case dt_X86_INS_FCMOVNBE:
    case dt_X86_INS_FCOMI:
    case dt_X86_INS_FCOMIP:
    case dt_X86_INS_FUCOMI:
    case dt_X86_INS_FUCOMIP:
        eflags_temp = ra_alloc_itemp();
        la_x86mfflag(eflags_temp, 0x3f);
        la_or(eflags_opnd, eflags_opnd, eflags_temp);
        ra_free_temp(eflags_temp);
        break;
    default:
        break;
    }
    la_st_w(eflags_opnd, env_ir2_opnd,
                          lsenv_offset_of_eflags(lsenv));
}

static void restore_eflags(IR1_INST *pir1)
{
    IR2_OPND eflags_opnd = ra_alloc_eflags();
    la_ld_w(eflags_opnd, env_ir2_opnd,
                        lsenv_offset_of_eflags(lsenv));

    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_FCMOVNB:
    case dt_X86_INS_FCMOVB:
    case dt_X86_INS_FCMOVNU:
    case dt_X86_INS_FCMOVU:
    case dt_X86_INS_FCMOVE:
    case dt_X86_INS_FCMOVNE:
    case dt_X86_INS_FCMOVBE:
    case dt_X86_INS_FCMOVNBE:
    case dt_X86_INS_FCOMI:
    case dt_X86_INS_FCOMIP:
    case dt_X86_INS_FUCOMI:
    case dt_X86_INS_FUCOMIP:
        la_x86mtflag(eflags_opnd, 0x3f);
        la_andi(eflags_opnd, eflags_opnd, 0x702);
        break;
    default:
        break;
    }
}

static void save_fpr(void)
{
    /* save fpr */
    for (int i = 0; i < 8; i++) {
            IR2_OPND mmx_opnd = ra_alloc_mmx(i);
            la_fst_d(mmx_opnd, env_ir2_opnd,
                                lsenv_offset_of_mmx(lsenv, i));
    }
}

static void restore_fpr(void)
{
    /* restore fpr */
    for (int i = 0; i < 8; i++) {
            IR2_OPND mmx_opnd = ra_alloc_mmx(i);
            la_fld_d(mmx_opnd, env_ir2_opnd,
                              lsenv_offset_of_mmx(lsenv, i));
    }
}

static void save_gpr(void)
{
#ifdef TARGET_X86_64
    for (int i = 8; i < 16; i++) {
        IR2_OPND gpr_opnd = ra_alloc_gpr(i);
        la_st_d(gpr_opnd, env_ir2_opnd,
                lsenv_offset_of_gpr(lsenv, i));
    }
#endif
}

static void restore_gpr(void)
{
#ifdef TARGET_X86_64
    for (int i = 8; i < 16; i++) {
        IR2_OPND gpr_opnd = ra_alloc_gpr(i);
        la_ld_d(gpr_opnd, env_ir2_opnd,
                lsenv_offset_of_gpr(lsenv, i));
    }
#endif
}

static void save_xmm(void)
{
    IR2_OPND tmp_env = ra_alloc_itemp();
    la_addi_d(tmp_env, env_ir2_opnd, 0x7f0);
#ifdef TARGET_X86_64
    int save_no = 16;
#else
    int save_no = 8;
#endif
    for (int i = 0; i < save_no; i++) {
        la_vst(ra_alloc_xmm(i), tmp_env,
                lsenv_offset_of_xmm(lsenv, i) - 0x7f0);
    }
    ra_free_temp(tmp_env);
}

static void restore_xmm(void)
{
    IR2_OPND tmp_env = ra_alloc_itemp();
    la_addi_d(tmp_env, env_ir2_opnd, 0x7f0);
#ifdef TARGET_X86_64
    int save_no = 16;
#else
    int save_no = 8;
#endif
    for (int i = 0; i < save_no; i++) {
        la_vld(ra_alloc_xmm(i), tmp_env,
                lsenv_offset_of_xmm(lsenv, i) - 0x7f0);
    }
    ra_free_temp(tmp_env);
}

#ifdef CONFIG_LATX_IMM_REG
void save_imm_cache(void)
{
    for (int i = 0; i < 4; i++) {
        if (imm_cache_is_cached_at(lsenv->tr_data->imm_cache, i)) {
            int reg_num = imm_cache_la_reg_num_at(lsenv->tr_data->imm_cache, i);
            IR2_OPND target_opnd = get_la_ir2_opnd(reg_num);
            la_st_d(target_opnd, env_ir2_opnd,
                    lsenv_offset_of_imm_value(lsenv, i));
        }
    }
}

void restore_imm_cache(void)
{
    for (int i = 0; i < 4; i++) {
        if (imm_cache_is_cached_at(lsenv->tr_data->imm_cache, i)) {
            int reg_num = imm_cache_la_reg_num_at(lsenv->tr_data->imm_cache, i);
            IR2_OPND target_opnd = get_la_ir2_opnd(reg_num);
            la_ld_d(target_opnd, env_ir2_opnd,
                    lsenv_offset_of_imm_value(lsenv, i));
        }
    }
}
#endif

static void la_fpu_push(void)
{
    IR2_OPND top_opnd = ra_alloc_itemp();

    la_ld_w(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));
    la_addi_d(top_opnd, top_opnd, -1);
    la_bstrpick_d(top_opnd, top_opnd, 2, 0);
    la_st_w(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));

    IR2_OPND temp1 = ra_alloc_itemp();
    IR2_OPND fpu_tag = ra_alloc_itemp();

    /* 2. mark st(0) as valid */
    int tag_offset = lsenv_offset_of_tag_word(lsenv);
    lsassert(tag_offset <= 0x7ff);
    la_ld_d(fpu_tag, env_ir2_opnd, tag_offset);

    li_d(temp1, (uint64)(0xffULL));
    la_slli_d(top_opnd, top_opnd, 3);
    la_sll_d(temp1, temp1, top_opnd);
    la_nor(temp1, zero_ir2_opnd, temp1);
    la_and(fpu_tag, fpu_tag, temp1);
    la_st_d(fpu_tag, env_ir2_opnd, tag_offset);

    ra_free_temp(fpu_tag);
    ra_free_temp(temp1);
    ra_free_temp(top_opnd);
}

static void la_fpu_pop(void)
{
    IR2_OPND top_opnd = ra_alloc_itemp();
    IR2_OPND temp1 = ra_alloc_itemp();
    IR2_OPND temp2 = ra_alloc_itemp();
    IR2_OPND fpu_tag = ra_alloc_itemp();


    la_ld_w(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));
    la_addi_d(temp1, top_opnd, 1);
    la_bstrpick_d(temp1, temp1, 2, 0);
    la_st_w(temp1, env_ir2_opnd, lsenv_offset_of_top(lsenv));

    /* 1. mark st(0) as empty */
    int tag_offset = lsenv_offset_of_tag_word(lsenv);
    lsassert(tag_offset <= 0x7ff);
    la_ld_d(fpu_tag, env_ir2_opnd, tag_offset);

    li_d(temp1, (uint64)(0xffULL));
    li_d(temp2, (uint64)(0x1ULL));
    la_slli_d(top_opnd, top_opnd, 3);

    la_sll_d(temp1, temp1, top_opnd);
    la_sll_d(temp2, temp2, top_opnd);
    la_nor(temp1, zero_ir2_opnd, temp1);
    la_and(fpu_tag, fpu_tag, temp1);
    la_or(fpu_tag, fpu_tag, temp2);
    la_st_d(fpu_tag, env_ir2_opnd, tag_offset);

    ra_free_temp(fpu_tag);
    ra_free_temp(temp1);
    ra_free_temp(temp2);
    ra_free_temp(top_opnd);
}

static IR2_OPND get_stn_opnd_addr(int stn)
{
    IR2_OPND stn_opnd_addr = ra_alloc_itemp();

    la_ld_wu(stn_opnd_addr, env_ir2_opnd, lsenv_offset_of_top(lsenv));
    if (0 != stn) {
        la_addi_d(stn_opnd_addr, stn_opnd_addr, stn);
        la_bstrpick_d(stn_opnd_addr, stn_opnd_addr, 2, 0);
    }

    la_slli_d(stn_opnd_addr, stn_opnd_addr, 4);
    la_add_d(stn_opnd_addr, stn_opnd_addr, env_ir2_opnd);
    la_addi_d(stn_opnd_addr, stn_opnd_addr, lsenv_offset_of_fpr(lsenv, 0));

    return stn_opnd_addr;
}

static void save_float80_to_stn(IR2_OPND stm_opnd, int stn)
{
    IR2_OPND stn_opnd_addr = get_stn_opnd_addr(stn);

    la_vst(stm_opnd, stn_opnd_addr, 0);

    ra_free_temp(stn_opnd_addr);
}

static void get_float80_from_stn(IR2_OPND stm_opnd, int stn)
{
    IR2_OPND stn_opnd_addr = get_stn_opnd_addr(stn);

    la_vld(stm_opnd, stn_opnd_addr, 0);

    ra_free_temp(stn_opnd_addr);
}

static void fmov_STN_STM(int stn, int stm)
{
    if (stn == stm) {
        return;
    }
    IR2_OPND stm_opnd = ra_alloc_ftemp();

    get_float80_from_stn(stm_opnd, stm);
    save_float80_to_stn(stm_opnd, stn);

    ra_free_temp(stm_opnd);
}

static void la_update_fp_status(IR2_OPND cw_opnd)
{
    IR2_OPND itemp = ra_alloc_itemp();
    IR2_OPND tmp_fcsr = ra_alloc_itemp();

    IR2_OPND label_float64_float80 = ra_alloc_label();

    int fp_status_offset = lsenv_offset_of_fp_status(lsenv);
    int round_mode_offset = fp_status_offset +
                            offsetof(float_status, float_rounding_mode);
    int round_precision_offset = fp_status_offset +
                            offsetof(float_status, floatx80_rounding_precision);

    la_movfcsr2gr(tmp_fcsr, fcsr_ir2_opnd);
    /* Rounding Control (11, 10)*/
    la_ori(itemp, cw_opnd, 0);
    update_fcsr_rm(itemp, tmp_fcsr);
    la_bstrpick_d(itemp, cw_opnd, 11, 10);
    la_st_b(itemp, env_ir2_opnd, round_mode_offset);

    /*
     *turn x86 pc to env pc
     *          x86      env
     *
     * 32       00       10
     * 64       10       01
     * 80       11       10
     *
     */

    /* Precision Control (9, 8)*/
    la_bstrpick_d(itemp, cw_opnd, 9, 8);
    la_bne(itemp, zero_ir2_opnd, label_float64_float80);
    li_wu(itemp, 1);

    la_label(label_float64_float80);
    la_xori(itemp, itemp, 3);
    la_st_b(itemp, env_ir2_opnd, round_precision_offset);

    ra_free_temp(itemp);
    ra_free_temp(tmp_fcsr);
}

__attribute__((unused))
void gen_softfpu_helper_prologue(IR1_INST *pir1)
{
    save_eflags(pir1);
    save_gpr();

    if (option_softfpu == 1) {
        save_fpr();
    }
    save_xmm();
#ifdef LATX_DEBUG_SOFTFPU
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)convert_fpregs_64_to_x80);
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
#endif
}

__attribute__((unused))
void gen_softfpu_helper_epilogue(IR1_INST *pir1)
{
#ifdef LATX_DEBUG_SOFTFPU
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)convert_fpregs_x80_to_64);
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
#endif
    restore_xmm();
    if (option_softfpu == 1) {
        restore_fpr();
    }
    restore_gpr();
    restore_eflags(pir1);
}

__attribute__((unused))
static void gen_softfpu_helper1(ADDR func)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    save_imm_cache();
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    restore_imm_cache();
}

__attribute__((unused))
static void gen_softfpu_helper2i(ADDR func, int arg1)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    li_d(a1_ir2_opnd, arg1);
    /* load func_addr and jmp */
    save_imm_cache();
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    restore_imm_cache();
}

__attribute__((unused))
static void gen_softfpu_helper2m_16s(ADDR func, IR2_OPND mem_opnd)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_ld_h(a1_ir2_opnd, mem_opnd, 0);
    /* load func_addr and jmp */
    save_imm_cache();
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    restore_imm_cache();
}

__attribute__((unused))
static void gen_softfpu_helper2m_16u(ADDR func, IR2_OPND mem_opnd)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_ld_hu(a1_ir2_opnd, mem_opnd, 0);
    /* load func_addr and jmp */
    save_imm_cache();
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    restore_imm_cache();
}

__attribute__((unused))
static void gen_softfpu_helper2m_32s(ADDR func, IR2_OPND mem_opnd)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_ld_w(a1_ir2_opnd, mem_opnd, 0);
    /* load func_addr and jmp */
    save_imm_cache();
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    restore_imm_cache();
}

__attribute__((unused))
static void gen_softfpu_helper2m_32u(ADDR func, IR2_OPND mem_opnd)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_ld_wu(a1_ir2_opnd, mem_opnd, 0);
    /* load func_addr and jmp */
    save_imm_cache();
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    restore_imm_cache();
}

__attribute__((unused))
static void gen_softfpu_helper2m_64(ADDR func, IR2_OPND mem_opnd)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_ld_d(a1_ir2_opnd, mem_opnd, 0);
    /* load func_addr and jmp */
    save_imm_cache();
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    restore_imm_cache();
}

__attribute__((unused))
static void gen_softfpu_helper2m_ptr(ADDR func, IR2_OPND ptr)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_mov64(a1_ir2_opnd, ptr);
    /* load func_addr and jmp */
    save_imm_cache();
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    restore_imm_cache();
}

__attribute__((unused))
static void gen_softfpu_helper3i(ADDR func, IR2_OPND arg1,
                                int arg2)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_mov64(a1_ir2_opnd, arg1);
    li_d(a2_ir2_opnd, arg2);
    /* load func_addr and jmp */
    save_imm_cache();
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    restore_imm_cache();
}

__attribute__((unused))
static void gen_softfpu_helper3_ll(ADDR func, IR2_OPND arg1, IR2_OPND arg2)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_mov64(a1_ir2_opnd, arg1);
    la_mov64(a2_ir2_opnd, arg2);
    /* load func_addr and jmp */
    save_imm_cache();
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    restore_imm_cache();
}


static bool translate_wait_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fwait);
    return true;
}

static bool translate_f2xm1_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_f2xm1);
    return true;
}

static bool translate_fabs_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND high_opnd = ra_alloc_itemp();

        IR2_OPND stn_opnd_addr = get_stn_opnd_addr(0);
        la_ld_hu(high_opnd, stn_opnd_addr, 8);
        la_bstrpick_d(high_opnd, high_opnd, 14, 0);
        la_st_h(high_opnd, stn_opnd_addr, 8);
    } else {
        gen_softfpu_helper1((ADDR)helper_fabs_ST0);
    }
    return true;
}

static void la_calcute_float64(IR1_INST *pir1, IR2_OPND float80_dest,
                               IR2_OPND src_opnd, bool src_is_float80)
{
    IR2_OPND ftemp = ra_alloc_ftemp();

    la_vextrins_h(ftemp, float80_dest, 0x4);
    la_fcvt_d_ld(float80_dest, float80_dest, ftemp);

    if (src_is_float80) {
        la_vextrins_h(ftemp, src_opnd, 0x4);
        la_fcvt_d_ld(src_opnd, src_opnd, ftemp);
    }

    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_FADD:
    case dt_X86_INS_FADDP:
    case dt_X86_INS_FIADD:
        la_fadd_d(float80_dest, float80_dest, src_opnd);
        break;
    case dt_X86_INS_FSUB:
    case dt_X86_INS_FSUBP:
    case dt_X86_INS_FISUB:
        la_fsub_d(float80_dest, float80_dest, src_opnd);
        break;
    case dt_X86_INS_FSUBR:
    case dt_X86_INS_FSUBRP:
    case dt_X86_INS_FISUBR:
        la_fsub_d(float80_dest, src_opnd, float80_dest);
        break;
    case dt_X86_INS_FMUL:
    case dt_X86_INS_FMULP:
    case dt_X86_INS_FIMUL:
        la_fmul_d(float80_dest, float80_dest, src_opnd);
        break;
    case dt_X86_INS_FDIV:
    case dt_X86_INS_FDIVP:
    case dt_X86_INS_FIDIV:
        la_fdiv_d(float80_dest, float80_dest, src_opnd);
        break;
    case dt_X86_INS_FDIVR:
    case dt_X86_INS_FDIVRP:
    case dt_X86_INS_FIDIVR:
        la_fdiv_d(float80_dest, src_opnd, float80_dest);
        break;
    case dt_X86_INS_FSQRT:
        la_fsqrt_d(float80_dest, src_opnd);
        break;
    case dt_X86_INS_FRNDINT:
        la_frint_d(float80_dest, src_opnd);
        break;
    case dt_X86_INS_FSCALE:
        la_ftintrz_l_d(ftemp, src_opnd);
        la_fscaleb_d(float80_dest, float80_dest, ftemp);
        break;
    default:
        lsassert(0);
        break;
    }

    la_fcvt_ud_d(ftemp, float80_dest);
    la_fcvt_ld_d(float80_dest, float80_dest);
    la_vextrins_h(float80_dest, ftemp, 0x40);

    ra_free_temp(ftemp);

}

static void gen_float64_ST0_MEM(IR1_INST *pir1, IR2_OPND mem_opnd,
                                int opnd_size, bool is_integer)
{
    IR2_OPND st0_opnd = ra_alloc_ftemp();
    IR2_OPND src_opnd = ra_alloc_ftemp();

    if (opnd_size == 64) {
        la_fld_d(src_opnd, mem_opnd, 0);
        if (is_integer) {
            la_ffint_d_l(src_opnd, src_opnd);
        }
    } else if (opnd_size == 32) {
        la_fld_s(src_opnd, mem_opnd, 0);
        if (is_integer) {
            la_ffint_d_w(src_opnd, src_opnd);
        } else {
            la_fcvt_d_s(src_opnd, src_opnd);
        }
    } else if (opnd_size == 16) {
        IR2_OPND itemp = ra_alloc_itemp();

        la_ld_h(itemp, mem_opnd, 0);
        if (is_integer) {
            la_movgr2fr_d(src_opnd, itemp);
            la_ffint_d_w(src_opnd, src_opnd);
        } else {
            lsassertm(!is_integer, "convert 16-bit floating point?\n");
        }
    } else {
        lsassert(0);
    }

    get_float80_from_stn(st0_opnd, 0);
    la_calcute_float64(pir1, st0_opnd, src_opnd, false);
    save_float80_to_stn(st0_opnd, 0);

    ra_free_temp(st0_opnd);
    ra_free_temp(src_opnd);
}

static void gen_float64_STN_ST0(IR1_INST *pir1, int opnd_num, int stn)
{
    IR2_OPND stn_opnd = ra_alloc_ftemp();
    IR2_OPND st0_opnd = ra_alloc_ftemp();

    get_float80_from_stn(stn_opnd, stn);
    get_float80_from_stn(st0_opnd, 0);
    if (opnd_num == 1) {
        la_calcute_float64(pir1, st0_opnd, stn_opnd, true);
        save_float80_to_stn(st0_opnd, 0);
    } else if (opnd_num == 2) {
        la_calcute_float64(pir1, stn_opnd, st0_opnd, true);
        save_float80_to_stn(stn_opnd, stn);
    }

    ra_free_temp(stn_opnd);
    ra_free_temp(st0_opnd);
}

static void gen_float64_STN_ST0_pop(IR1_INST *pir1, int stn)
{
    IR2_OPND stn_opnd = ra_alloc_ftemp();
    IR2_OPND st0_opnd = ra_alloc_ftemp();

    get_float80_from_stn(stn_opnd, stn);
    get_float80_from_stn(st0_opnd, 0);
    la_calcute_float64(pir1, stn_opnd, st0_opnd, true);
    save_float80_to_stn(stn_opnd, stn);
    la_fpu_pop();

    ra_free_temp(stn_opnd);
    ra_free_temp(st0_opnd);
}

static bool translate_faddp_softfpu(IR1_INST *pir1)
{
    IR1_OPND *src = ir1_get_opnd(pir1, 0);
	int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x1) {
        gen_float64_STN_ST0_pop(pir1, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fadd_STN_ST0, stn);
        gen_softfpu_helper1((ADDR)helper_fpop);
    }
    return true;
}

static bool translate_fadd_softfpu(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    if (pir1->info->x86.opcode[0] == 0xde) {
        translate_faddp_softfpu(pir1);
        return true;
    }

    IR1_OPND *src = ir1_get_opnd(pir1, 0);
    if (ir1_opnd_is_mem(src)) {
        int opnd_size = ir1_opnd_size(src);
        IR2_OPND mem_opnd = convert_mem_no_offset(src);

        if (option_softfpu == 2 && option_softfpu_fast & 0x2) {
            gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, false);
        } else {
            if (opnd_size == 32) {
                gen_softfpu_helper2m_32s((ADDR)helper_flds_FT0, mem_opnd);
            } else {
                gen_softfpu_helper2m_64((ADDR)helper_fldl_FT0, mem_opnd);
            }
            gen_softfpu_helper1((ADDR)helper_fadd_ST0_FT0);
        }
        return true;
    }

    int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x2) {
        gen_float64_STN_ST0(pir1, opnd_num, stn);
        return true;
    } else {
        if (opnd_num == 1) {
            gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
            gen_softfpu_helper1((ADDR)helper_fadd_ST0_FT0);
            return true;
        } else if (opnd_num == 2) {
            gen_softfpu_helper2i((ADDR)helper_fadd_STN_ST0, stn);
            return true;
        }
    }
    lsassert(0);
    return false;
}

static bool translate_fbld_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    gen_softfpu_helper2m_ptr((ADDR)helper_fbld_ST0, mem_opnd);
    return true;
}

static bool translate_fbstp_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    gen_softfpu_helper2m_ptr((ADDR)helper_fbst_ST0, mem_opnd);
    gen_softfpu_helper1((ADDR)helper_fpop);
    return true;
}

static bool translate_fchs_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND high_opnd = ra_alloc_itemp();
        IR2_OPND itemp = ra_alloc_itemp();

        IR2_OPND stn_opnd_addr = get_stn_opnd_addr(0);

        la_ld_hu(high_opnd, stn_opnd_addr, 8);
        li_wu(itemp, 0x8000);
        la_xor(high_opnd, high_opnd, itemp);
        la_st_h(high_opnd, stn_opnd_addr, 8);
    } else {
        gen_softfpu_helper1((ADDR)helper_fchs_ST0);
    }
    return true;
}

static bool translate_fcmovnb_softfpu(IR1_INST *pir1)
{
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND flag_opnd = ra_alloc_itemp();
    la_x86mfflag(flag_opnd, CF_USEDEF_BIT);
    la_bne(flag_opnd, zero_ir2_opnd, label_exit);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
	int stn = ir1_opnd_base_reg_num(opnd1);

    if (option_softfpu == 2) {
        fmov_STN_STM(0, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fmov_ST0_STN, stn);
    }
    la_label(label_exit);
    return true;
}

static bool translate_fcmovb_softfpu(IR1_INST *pir1)
{
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND flag_opnd = ra_alloc_itemp();
    la_x86mfflag(flag_opnd, CF_USEDEF_BIT);
    la_beq(flag_opnd, zero_ir2_opnd, label_exit);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
	int stn = ir1_opnd_base_reg_num(opnd1);

    if (option_softfpu == 2) {
        fmov_STN_STM(0, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fmov_ST0_STN, stn);
    }
    la_label(label_exit);
    return true;
}

static bool translate_fcmovnu_softfpu(IR1_INST *pir1)
{
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND flag_opnd = ra_alloc_itemp();
    la_x86mfflag(flag_opnd, PF_USEDEF_BIT);
    la_bne(flag_opnd, zero_ir2_opnd, label_exit);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
	int stn = ir1_opnd_base_reg_num(opnd1);

    if (option_softfpu == 2) {
        fmov_STN_STM(0, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fmov_ST0_STN, stn);
    }
    la_label(label_exit);
    return true;
}

static bool translate_fcmovu_softfpu(IR1_INST *pir1)
{
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND flag_opnd = ra_alloc_itemp();
    la_x86mfflag(flag_opnd, PF_USEDEF_BIT);
    la_beq(flag_opnd, zero_ir2_opnd, label_exit);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
	int stn = ir1_opnd_base_reg_num(opnd1);

    if (option_softfpu == 2) {
        fmov_STN_STM(0, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fmov_ST0_STN, stn);
    }
    la_label(label_exit);
    return true;
}

static bool translate_fcmove_softfpu(IR1_INST *pir1)
{
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND flag_opnd = ra_alloc_itemp();
    la_x86mfflag(flag_opnd, ZF_USEDEF_BIT);
    la_beq(flag_opnd, zero_ir2_opnd, label_exit);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
	int stn = ir1_opnd_base_reg_num(opnd1);

    if (option_softfpu == 2) {
        fmov_STN_STM(0, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fmov_ST0_STN, stn);
    }
    la_label(label_exit);
    return true;
}

static bool translate_fcmovne_softfpu(IR1_INST *pir1)
{
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND flag_opnd = ra_alloc_itemp();
    la_x86mfflag(flag_opnd, ZF_USEDEF_BIT);
    la_bne(flag_opnd, zero_ir2_opnd, label_exit);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
	int stn = ir1_opnd_base_reg_num(opnd1);

    if (option_softfpu == 2) {
        fmov_STN_STM(0, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fmov_ST0_STN, stn);
    }
    la_label(label_exit);
    return true;
}

static bool translate_fcmovbe_softfpu(IR1_INST *pir1)
{
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND flag_opnd = ra_alloc_itemp();
    la_x86mfflag(flag_opnd, ZF_USEDEF_BIT | CF_USEDEF_BIT);
    la_beq(flag_opnd, zero_ir2_opnd, label_exit);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
	int stn = ir1_opnd_base_reg_num(opnd1);

    if (option_softfpu == 2) {
        fmov_STN_STM(0, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fmov_ST0_STN, stn);
    }
    la_label(label_exit);
    return true;
}

static bool translate_fcmovnbe_softfpu(IR1_INST *pir1)
{
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND flag_opnd = ra_alloc_itemp();
    la_x86mfflag(flag_opnd, ZF_USEDEF_BIT | CF_USEDEF_BIT);
    la_bne(flag_opnd, zero_ir2_opnd, label_exit);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
	int stn = ir1_opnd_base_reg_num(opnd1);

    if (option_softfpu == 2) {
        fmov_STN_STM(0, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fmov_ST0_STN, stn);
    }
    la_label(label_exit);
    return true;
}

static bool translate_fcmovcc_softfpu(IR1_INST *pir1)
{
    bool ret;

    switch (ir1_opcode(pir1)) {
        case dt_X86_INS_FCMOVNB:
            ret = translate_fcmovnb_softfpu(pir1);
            break;
        case dt_X86_INS_FCMOVB:
            ret = translate_fcmovb_softfpu(pir1);
            break;
        case dt_X86_INS_FCMOVNU:
            ret = translate_fcmovnu_softfpu(pir1);
            break;
        case dt_X86_INS_FCMOVU:
            ret = translate_fcmovu_softfpu(pir1);
            break;
        case dt_X86_INS_FCMOVE:
            ret = translate_fcmove_softfpu(pir1);
            break;
        case dt_X86_INS_FCMOVNE:
            ret = translate_fcmovne_softfpu(pir1);
            break;
        case dt_X86_INS_FCMOVBE:
            ret =  translate_fcmovbe_softfpu(pir1);
            break;
        case dt_X86_INS_FCMOVNBE:
            ret = translate_fcmovnbe_softfpu(pir1);
            break;
        default:
            ret = false;
            break;
    }

    return ret;
}

static bool translate_fcom_softfpu(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    int opnd_size = ir1_opnd_size(opnd0);
    if (ir1_opnd_is_fpr(opnd0)) {
        int stn = 1;
        if (opnd_num) {
            stn = ir1_opnd_base_reg_num(opnd0);
        }
        gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
        gen_softfpu_helper1((ADDR)helper_fcom_ST0_FT0);
    } else if (ir1_opnd_is_mem(opnd0) && opnd_size == 32) {
        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
        gen_softfpu_helper2m_32s((ADDR)helper_flds_FT0, mem_opnd);
        gen_softfpu_helper1((ADDR)helper_fcom_ST0_FT0);
    } else if (ir1_opnd_is_mem(opnd0) && opnd_size == 64) {
        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
        gen_softfpu_helper2m_64((ADDR)helper_fldl_FT0, mem_opnd);
        gen_softfpu_helper1((ADDR)helper_fcom_ST0_FT0);
    } else {
        lsassert(0);
        return false;
	}
    return true;
}

static bool translate_fcomi_softfpu(IR1_INST *pir1)
{
    int index = get_src_index(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    int stn = ir1_opnd_base_reg_num(opnd0);
    gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
    gen_softfpu_helper1((ADDR)helper_fcomi_ST0_FT0);
    return true;
}

static bool translate_fcomip_softfpu(IR1_INST *pir1)
{
    int index = get_src_index(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    int stn = ir1_opnd_base_reg_num(opnd0);
    gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
    gen_softfpu_helper1((ADDR)helper_fcomi_ST0_FT0);
    gen_softfpu_helper1((ADDR)helper_fpop);
    return true;
}

static bool translate_fcomp_softfpu(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    int opnd_size = ir1_opnd_size(opnd0);
    if (ir1_opnd_is_fpr(opnd0)) {
        int stn = 1;
        if (opnd_num) {
            stn = ir1_opnd_base_reg_num(opnd0);
        }
        gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
        gen_softfpu_helper1((ADDR)helper_fcom_ST0_FT0);
    } else if (ir1_opnd_is_mem(opnd0) && opnd_size == 32) {
        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
        gen_softfpu_helper2m_32s((ADDR)helper_flds_FT0, mem_opnd);
        gen_softfpu_helper1((ADDR)helper_fcom_ST0_FT0);
    } else if (ir1_opnd_is_mem(opnd0) && opnd_size == 64) {
        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
        gen_softfpu_helper2m_64((ADDR)helper_fldl_FT0, mem_opnd);
        gen_softfpu_helper1((ADDR)helper_fcom_ST0_FT0);
    } else {
        lsassert(0);
        return false;
	}
    gen_softfpu_helper1((ADDR)helper_fpop);
    return true;
}

static bool translate_fcompp_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, 1);
    gen_softfpu_helper1((ADDR)helper_fcom_ST0_FT0);
    gen_softfpu_helper1((ADDR)helper_fpop);
    gen_softfpu_helper1((ADDR)helper_fpop);
    return true;
}

static bool translate_fcos_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fcos);
    return true;
}

static bool translate_fdecstp_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND top_opnd = ra_alloc_itemp();

        la_ld_w(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));
        la_addi_d(top_opnd, top_opnd, -1);
        la_bstrpick_d(top_opnd, top_opnd, 2, 0);
        la_st_w(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));

        /* increments the top-of-stack pointer */
        IR2_OPND value_status = ra_alloc_itemp();
        la_ld_h(value_status, env_ir2_opnd,
                          lsenv_offset_of_status_word(lsenv)); /* status_word */

        /* set C3 C2 C1 C0 flag to 0 */
        la_bstrins_d(value_status, zero_ir2_opnd, 10, 8);
        la_bstrins_d(value_status, zero_ir2_opnd, 14, 14);
        la_st_h(value_status, env_ir2_opnd,
                          lsenv_offset_of_status_word(lsenv)); /* status_word */
    } else {
        /* optimize */
        gen_softfpu_helper1((ADDR)helper_fdecstp);

    }
    return true;
}

static bool translate_fdiv_softfpu(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    IR1_OPND *src = ir1_get_opnd(pir1, 0);
    if (ir1_opnd_is_mem(src)) {
        int opnd_size = ir1_opnd_size(src);
        IR2_OPND mem_opnd = convert_mem_no_offset(src);
        if (option_softfpu == 2 && option_softfpu_fast & 0x4) {
            gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, false);
        } else {
            if (opnd_size == 32) {
                gen_softfpu_helper2m_32s((ADDR)helper_flds_FT0, mem_opnd);
            } else {
                gen_softfpu_helper2m_64((ADDR)helper_fldl_FT0, mem_opnd);
            }
            gen_softfpu_helper1((ADDR)helper_fdiv_ST0_FT0);
        }
        return true;
    }

    int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x4) {
        gen_float64_STN_ST0(pir1, opnd_num, stn);
        return true;
    } else {
        if (opnd_num == 1) {
            gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
            gen_softfpu_helper1((ADDR)helper_fdiv_ST0_FT0);
            return true;
        } else if (opnd_num == 2) {
            gen_softfpu_helper2i((ADDR)helper_fdiv_STN_ST0, stn);
            return true;
        }
    }
    lsassert(0);
    return false;
}

static bool translate_fdivp_softfpu(IR1_INST *pir1)
{
    IR1_OPND *src = ir1_get_opnd(pir1, 0);
	int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x8) {
        gen_float64_STN_ST0_pop(pir1, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fdiv_STN_ST0, stn);
        gen_softfpu_helper1((ADDR)helper_fpop);
    }
    return true;
}

static bool translate_fdivr_softfpu(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    IR1_OPND *src = ir1_get_opnd(pir1, 0);
    if (ir1_opnd_is_mem(src)) {
        int opnd_size = ir1_opnd_size(src);
        IR2_OPND mem_opnd = convert_mem_no_offset(src);
        if (option_softfpu == 2 && option_softfpu_fast & 0x10) {
            gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, false);
        } else {
            if (opnd_size == 32) {
                gen_softfpu_helper2m_32s((ADDR)helper_flds_FT0, mem_opnd);
            } else {
                gen_softfpu_helper2m_64((ADDR)helper_fldl_FT0, mem_opnd);
            }
            gen_softfpu_helper1((ADDR)helper_fdivr_ST0_FT0);
        }
        return true;
    }

    int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x10) {
        gen_float64_STN_ST0(pir1, opnd_num, stn);
        return true;
    } else {
        if (opnd_num == 1) {
            gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
            gen_softfpu_helper1((ADDR)helper_fdivr_ST0_FT0);
            return true;
        } else if (opnd_num == 2) {
            gen_softfpu_helper2i((ADDR)helper_fdivr_STN_ST0, stn);
            return true;
        }
    }
    lsassert(0);
    return false;
}

static bool translate_fdivrp_softfpu(IR1_INST *pir1)
{
    IR1_OPND *src = ir1_get_opnd(pir1, 0);
	int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x20) {
        gen_float64_STN_ST0_pop(pir1, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fdivr_STN_ST0, stn);
        gen_softfpu_helper1((ADDR)helper_fpop);
    }
    return true;
}

static bool translate_ffree_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND fpu_tag = ra_alloc_itemp();
        IR2_OPND top_opnd = ra_alloc_itemp();
        IR2_OPND temp1 = ra_alloc_itemp();
        IR2_OPND temp2 = ra_alloc_itemp();
        IR1_OPND *sti_reg = ir1_get_opnd(pir1, 0);

        /* get TOP */
        la_ld_wu(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));

        /* get fpu tag word */
        int tag_offset = lsenv_offset_of_tag_word(lsenv);
        lsassert(tag_offset <= 0x7ff);
        la_ld_d(fpu_tag, env_ir2_opnd, tag_offset);

        li_d(temp1, (uint64)(0xffULL));
        li_d(temp2, (uint64)(0x1ULL));
        la_slli_d(top_opnd, top_opnd, 3);

        switch (sti_reg->reg) {
            case dt_X86_REG_ST0: {
                break;
            }
            case dt_X86_REG_ST1: {
                la_addi_d(top_opnd, top_opnd, 8);
                break;
            }
            case dt_X86_REG_ST2: {
                la_addi_d(top_opnd, top_opnd, 16);
                break;
            }
            case dt_X86_REG_ST3: {
                la_addi_d(top_opnd, top_opnd, 24);
                break;
            }
            case dt_X86_REG_ST4: {
                la_addi_d(top_opnd, top_opnd, 32);
                break;
            }
            case dt_X86_REG_ST5: {
                la_addi_d(top_opnd, top_opnd, 40);
                break;
            }
            case dt_X86_REG_ST6: {
                la_addi_d(top_opnd, top_opnd, 48);
                break;
            }
            case dt_X86_REG_ST7: {
                la_addi_d(top_opnd, top_opnd, 56);
                break;
            }
            default: {
                lsassertm(0, "invalid operand of FXAM\n");
                break;
            }
        }

        la_sll_d(temp1, temp1, top_opnd);
        la_sll_d(temp2, temp2, top_opnd);
        la_nor(temp1, zero_ir2_opnd, temp1);
        la_and(fpu_tag, fpu_tag, temp1);
        la_or(fpu_tag, fpu_tag, temp2);
        la_st_d(fpu_tag, env_ir2_opnd, tag_offset);

        ra_free_temp(fpu_tag);
        ra_free_temp(top_opnd);
        ra_free_temp(temp1);
        ra_free_temp(temp2);

    } else {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        int stn = ir1_opnd_base_reg_num(opnd0);

        gen_softfpu_helper2i((ADDR)helper_ffree_STN, stn);

    }
    return true;
}

static bool translate_ffreep_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        translate_ffree_softfpu(pir1);
        la_fpu_pop();
    } else {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        int stn = ir1_opnd_base_reg_num(opnd0);

        gen_softfpu_helper2i((ADDR)helper_ffree_STN, stn);
        gen_softfpu_helper1((ADDR)helper_fpop);
    }
    return true;
}

static bool translate_fiadd_softfpu(IR1_INST *pir1)
{
    int index = get_src_index(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    int opnd_size = ir1_opnd_size(opnd0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    if (option_softfpu == 2 && option_softfpu_fast & 0x40) {
        gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, true);
    } else {
        if (opnd_size == 32) {
            gen_softfpu_helper2m_32s((ADDR)helper_fildl_FT0, mem_opnd);
        } else if (opnd_size == 16) {
            gen_softfpu_helper2m_16s((ADDR)helper_fildl_FT0, mem_opnd);
        } else {
            lsassert(0);
        }
        gen_softfpu_helper1((ADDR)helper_fadd_ST0_FT0);
    }
    return true;
}

static bool translate_ficom_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    int opnd_size = ir1_opnd_size(opnd0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    if (opnd_size == 32) {
        gen_softfpu_helper2m_32s((ADDR)helper_fildl_FT0, mem_opnd);
    } else if (opnd_size == 16) {
        gen_softfpu_helper2m_16s((ADDR)helper_fildl_FT0, mem_opnd);
    } else {
    	lsassert(0);
	}
    gen_softfpu_helper1((ADDR)helper_fcom_ST0_FT0);
    return true;
}

static bool translate_ficomp_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    int opnd_size = ir1_opnd_size(opnd0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    if (opnd_size == 32) {
        gen_softfpu_helper2m_32s((ADDR)helper_fildl_FT0, mem_opnd);
    } else if (opnd_size == 16) {
        gen_softfpu_helper2m_16s((ADDR)helper_fildl_FT0, mem_opnd);
    } else {
    	lsassert(0);
	}
    gen_softfpu_helper1((ADDR)helper_fcom_ST0_FT0);
    gen_softfpu_helper1((ADDR)helper_fpop);
    return true;
}

static bool translate_fidiv_softfpu(IR1_INST *pir1)
{
    int index = get_src_index(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    int opnd_size = ir1_opnd_size(opnd0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    if (option_softfpu == 2 && option_softfpu_fast & 0x80) {
        gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, true);
    } else {
        if (opnd_size == 32) {
            gen_softfpu_helper2m_32s((ADDR)helper_fildl_FT0, mem_opnd);
        } else if (opnd_size == 16) {
            gen_softfpu_helper2m_16s((ADDR)helper_fildl_FT0, mem_opnd);
        } else {
            lsassert(0);
        }
        gen_softfpu_helper1((ADDR)helper_fdiv_ST0_FT0);
	}
    return true;
}

static bool translate_fidivr_softfpu(IR1_INST *pir1)
{
    int index = get_src_index(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    int opnd_size = ir1_opnd_size(opnd0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    if (option_softfpu == 2 && option_softfpu_fast & 0x100) {
        gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, true);
    } else {
        if (opnd_size == 32) {
            gen_softfpu_helper2m_32s((ADDR)helper_fildl_FT0, mem_opnd);
        } else if (opnd_size == 16) {
            gen_softfpu_helper2m_16s((ADDR)helper_fildl_FT0, mem_opnd);
        } else {
            lsassert(0);
        }
        gen_softfpu_helper1((ADDR)helper_fdivr_ST0_FT0);
	}
    return true;
}

static bool translate_fild_softfpu(IR1_INST *pir1)
{
    int index = get_src_index(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    int opnd_size = ir1_opnd_size(opnd0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    if (option_softfpu == 2) {
        IR2_OPND src_opnd = ra_alloc_itemp();
        IR2_OPND src_ftemp = ra_alloc_ftemp();

        IR2_OPND st0_opnd = ra_alloc_ftemp();
        IR2_OPND ftemp_opnd = ra_alloc_ftemp();

        IR2_OPND label_exit = ra_alloc_label();
        if (opnd_size == 64) {
            IR2_OPND itemp1 = ra_alloc_itemp();
            IR2_OPND itemp2 = ra_alloc_itemp();
            IR2_OPND label_hard = ra_alloc_label();
            IR2_OPND label_positive = ra_alloc_label();

            la_ld_d(src_opnd, mem_opnd, 0);
            la_ori(itemp1, src_opnd, 0);
            la_bge(itemp1, zero_ir2_opnd, label_positive);
            la_sub_d(itemp1, zero_ir2_opnd, src_opnd);

            /* src_opnd >=0 */
            la_label(label_positive);
            la_clz_d(itemp1, itemp1);
            li_d(itemp2, 12);
            la_bge(itemp1, itemp2, label_hard);

            gen_softfpu_helper2m_64((ADDR)helper_fildll_ST0, mem_opnd);
            la_b(label_exit);

            la_label(label_hard);
            la_movgr2fr_d(src_ftemp, src_opnd);
            la_ffint_d_l(src_ftemp, src_ftemp);

            ra_free_temp(itemp1);
            ra_free_temp(itemp2);

        } else if (opnd_size == 32) {
            la_ld_w(src_opnd, mem_opnd, 0);
            la_movgr2fr_d(src_ftemp, src_opnd);
            la_ffint_d_w(src_ftemp, src_ftemp);
        } else {
            la_ld_h(src_opnd, mem_opnd, 0);
            la_movgr2fr_d(src_ftemp, src_opnd);
            la_ffint_d_w(src_ftemp, src_ftemp);
        }

        la_fcvt_ld_d(st0_opnd, src_ftemp);
        la_fcvt_ud_d(ftemp_opnd, src_ftemp);
        la_vextrins_h(st0_opnd, ftemp_opnd, 0x40);

        la_fpu_push();
        save_float80_to_stn(st0_opnd, 0);

        la_label(label_exit);

    } else {
        if (opnd_size == 64) {
            gen_softfpu_helper2m_64((ADDR)helper_fildll_ST0, mem_opnd);
        } else if (opnd_size == 32) {
            gen_softfpu_helper2m_32s((ADDR)helper_fildl_ST0, mem_opnd);
        } else {
            gen_softfpu_helper2m_16s((ADDR)helper_fildl_ST0, mem_opnd);
        }
    }

    return true;
}

static bool translate_fimul_softfpu(IR1_INST *pir1)
{
    int index = get_src_index(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    int opnd_size = ir1_opnd_size(opnd0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    if (option_softfpu == 2 && option_softfpu_fast & 0x200) {
        gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, true);
    } else {
        if (opnd_size == 32) {
            gen_softfpu_helper2m_32s((ADDR)helper_fildl_FT0, mem_opnd);
        } else if (opnd_size == 16) {
            gen_softfpu_helper2m_16s((ADDR)helper_fildl_FT0, mem_opnd);
        } else {
            lsassert(0);
        }
        gen_softfpu_helper1((ADDR)helper_fmul_ST0_FT0);
	}
    return true;
}

static bool translate_fincstp_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND top_opnd = ra_alloc_itemp();

        la_ld_w(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));
        la_addi_d(top_opnd, top_opnd, 1);
        la_bstrpick_d(top_opnd, top_opnd, 2, 0);
        la_st_w(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));

        /* increments the top-of-stack pointer */
        IR2_OPND value_status = ra_alloc_itemp();
        la_ld_h(value_status, env_ir2_opnd,
                          lsenv_offset_of_status_word(lsenv)); /* status_word */

        /* set C3 C2 C1 C0 flag to 0 */
        la_bstrins_d(value_status, zero_ir2_opnd, 10, 8);
        la_bstrins_d(value_status, zero_ir2_opnd, 14, 14);
        la_st_h(value_status, env_ir2_opnd,
                          lsenv_offset_of_status_word(lsenv)); /* status_word */
    } else {
        /* optimize */
        gen_softfpu_helper1((ADDR)helper_fincstp);
    }
    return true;
}

#define FLOAT80_EXPONENT_MAX 0x3fff
#define FLOAT64_EXPONENT_MAX 51
#define FLOAT32_EXPONENT_MAX 23

static bool translate_fist_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    int opnd_size = ir1_opnd_size(opnd0);

    if (option_softfpu == 2 && option_softfpu_fast & 0x200000) {
        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

        IR2_OPND itemp = ra_alloc_itemp();
        IR2_OPND itemp2 = ra_alloc_itemp();

        IR2_OPND label_exit = ra_alloc_label();
        IR2_OPND label_softfpu = ra_alloc_label();

        int round_mode_offset = lsenv_offset_of_fp_status(lsenv) +
                                offsetof(float_status, float_rounding_mode);

        la_ld_bu(itemp, env_ir2_opnd, round_mode_offset);
        la_bstrpick_d(itemp2, itemp, 2, 2);
        la_bne(itemp2, zero_ir2_opnd, label_softfpu);

        IR2_OPND fpr_st0_low_opnd = ra_alloc_ftemp();
        IR2_OPND fpr_st0_high_opnd = ra_alloc_ftemp();
        IR2_OPND st0_opnd = fpr_st0_low_opnd;

        get_float80_from_stn(fpr_st0_low_opnd, 0);
        la_vextrins_h(fpr_st0_high_opnd, fpr_st0_low_opnd, 0x4);
        la_movfr2gr_d(itemp, fpr_st0_high_opnd);
        la_bstrpick_d(itemp, itemp, 14, 0);
        li_d(itemp2, FLOAT80_EXPONENT_MAX + FLOAT32_EXPONENT_MAX + 1);
        la_bge(itemp, itemp2, label_softfpu);

        if (opnd_size == 16) {
            la_fcvt_d_ld(st0_opnd, fpr_st0_low_opnd, fpr_st0_high_opnd);
            la_ftint_w_d(st0_opnd, st0_opnd);
            la_movfr2gr_s(itemp, st0_opnd);
            la_st_h(itemp, mem_opnd, 0);

        } else if (opnd_size == 32) {
            la_fcvt_d_ld(st0_opnd, fpr_st0_low_opnd, fpr_st0_high_opnd);
            la_ftint_w_d(st0_opnd, st0_opnd);
            la_movfr2gr_s(itemp, st0_opnd);
            la_st_w(itemp, mem_opnd, 0);

        }
        la_b(label_exit);


        la_label(label_softfpu);
        if (opnd_size == 16) {
            gen_softfpu_helper1((ADDR)helper_fist_ST0);
        } else if (opnd_size == 32) {
            gen_softfpu_helper1((ADDR)helper_fistl_ST0);
        }

        /* v0 */
        if (option_mem_test) {
            la_mov64(itemp, a0_ir2_opnd);
            store_ireg_to_ir1(itemp, opnd0, false);
        } else {
            store_ireg_to_ir1(a0_ir2_opnd, opnd0, false);
        }

        la_label(label_exit);

    } else {
        gen_softfpu_helper_prologue(pir1);
        if (opnd_size == 16) {
            gen_softfpu_helper1((ADDR)helper_fist_ST0);
        } else if (opnd_size == 32) {
            gen_softfpu_helper1((ADDR)helper_fistl_ST0);
        }
        gen_softfpu_helper_epilogue(pir1);

        /* v0 */
        if (option_mem_test) {
            IR2_OPND itemp = ra_alloc_itemp();
            la_mov64(itemp, a0_ir2_opnd);
            store_ireg_to_ir1(itemp, opnd0, false);
        } else {
            store_ireg_to_ir1(a0_ir2_opnd, opnd0, false);
        }
    }
    return true;
}

static bool translate_fistp_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2 && option_softfpu_fast & 0x400000) {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        int opnd_size = ir1_opnd_size(opnd0);

        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

        IR2_OPND itemp = ra_alloc_itemp();
        IR2_OPND itemp2 = ra_alloc_itemp();

        IR2_OPND label_exit = ra_alloc_label();
        IR2_OPND label_softfpu = ra_alloc_label();

        int round_mode_offset = lsenv_offset_of_fp_status(lsenv) +
                                offsetof(float_status, float_rounding_mode);

        la_ld_bu(itemp, env_ir2_opnd, round_mode_offset);
        la_bstrpick_d(itemp2, itemp, 2, 2);
        la_bne(itemp2, zero_ir2_opnd, label_softfpu);

        IR2_OPND fpr_st0_low_opnd = ra_alloc_ftemp();
        IR2_OPND fpr_st0_high_opnd = ra_alloc_ftemp();
        IR2_OPND st0_opnd = fpr_st0_low_opnd;

        get_float80_from_stn(fpr_st0_low_opnd, 0);
        la_vextrins_h(fpr_st0_high_opnd, fpr_st0_low_opnd, 0x4);
        la_movfr2gr_d(itemp, fpr_st0_high_opnd);
        la_bstrpick_d(itemp, itemp, 14, 0);

        if (opnd_size == 16) {
            li_d(itemp2, FLOAT80_EXPONENT_MAX + FLOAT32_EXPONENT_MAX + 1);
            la_bge(itemp, itemp2, label_softfpu);

            la_fcvt_d_ld(st0_opnd, fpr_st0_low_opnd, fpr_st0_high_opnd);
            la_ftint_w_d(st0_opnd, st0_opnd);
            la_movfr2gr_s(itemp, st0_opnd);
            la_st_h(itemp, mem_opnd, 0);

        } else if (opnd_size == 32) {
            li_d(itemp2, FLOAT80_EXPONENT_MAX + FLOAT32_EXPONENT_MAX + 1);
            la_bge(itemp, itemp2, label_softfpu);

            la_fcvt_d_ld(st0_opnd, fpr_st0_low_opnd, fpr_st0_high_opnd);
            la_ftint_w_d(st0_opnd, st0_opnd);
            la_movfr2gr_s(itemp, st0_opnd);
            la_st_w(itemp, mem_opnd, 0);

        } else if (opnd_size == 64) {
            li_d(itemp2, FLOAT80_EXPONENT_MAX + FLOAT64_EXPONENT_MAX + 1);
            la_bge(itemp, itemp2, label_softfpu);

            la_fcvt_d_ld(st0_opnd, fpr_st0_low_opnd, fpr_st0_high_opnd);
            la_ftint_l_d(st0_opnd, st0_opnd);
            la_movfr2gr_d(itemp, st0_opnd);
            la_st_d(itemp, mem_opnd, 0);
        }

        la_fpu_pop();
        la_b(label_exit);

        la_label(label_softfpu);
        if (opnd_size == 16) {
            gen_softfpu_helper1((ADDR)helper_fist_ST0);
        } else if (opnd_size == 32) {
            gen_softfpu_helper1((ADDR)helper_fistl_ST0);
        } else if (opnd_size == 64) {
            gen_softfpu_helper1((ADDR)helper_fistll_ST0);
        }

        /* v0 */
        if (option_mem_test) {
            la_mov64(itemp, a0_ir2_opnd);
            store_ireg_to_ir1(itemp, opnd0, false);
        } else {
            store_ireg_to_ir1(a0_ir2_opnd, opnd0, false);
        }
        la_fpu_pop();

        la_label(label_exit);

    } else {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        int opnd_size = ir1_opnd_size(opnd0);

        gen_softfpu_helper_prologue(pir1);
        if (opnd_size == 16) {
            gen_softfpu_helper1((ADDR)helper_fist_ST0);
        } else if (opnd_size == 32) {
            gen_softfpu_helper1((ADDR)helper_fistl_ST0);
        } else if (opnd_size == 64) {
            gen_softfpu_helper1((ADDR)helper_fistll_ST0);
        }

        /* v0 */
        restore_gpr();
        if (option_mem_test) {
            IR2_OPND itemp = ra_alloc_itemp();
            la_mov64(itemp, a0_ir2_opnd);
            store_ireg_to_ir1(itemp, opnd0, false);
        } else {
            store_ireg_to_ir1(a0_ir2_opnd, opnd0, false);
        }
        save_gpr();

        gen_softfpu_helper1((ADDR)helper_fpop);
        gen_softfpu_helper_epilogue(pir1);
    }
    return true;
}

static bool translate_fisttp_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    int opnd_size = ir1_opnd_size(opnd0);

    gen_softfpu_helper_prologue(pir1);
    if (opnd_size == 16) {
        gen_softfpu_helper1((ADDR)helper_fistt_ST0);
    } else if (opnd_size == 32) {
        gen_softfpu_helper1((ADDR)helper_fisttl_ST0);
    } else if (opnd_size == 64) {
        gen_softfpu_helper1((ADDR)helper_fisttll_ST0);
    }

    /* v0 */
    restore_gpr();
    if (option_mem_test) {
        IR2_OPND itemp = ra_alloc_itemp();
        la_mov64(itemp, a0_ir2_opnd);
        store_ireg_to_ir1(itemp, opnd0, false);
    } else {
        store_ireg_to_ir1(a0_ir2_opnd, opnd0, false);
    }
    save_gpr();

    gen_softfpu_helper1((ADDR)helper_fpop);
    gen_softfpu_helper_epilogue(pir1);

    return true;
}

static bool translate_fisub_softfpu(IR1_INST *pir1)
{
    int index = get_src_index(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    int opnd_size = ir1_opnd_size(opnd0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    if (option_softfpu == 2 && option_softfpu_fast & 0x400) {
        gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, true);
    } else {
        if (opnd_size == 32) {
            gen_softfpu_helper2m_32s((ADDR)helper_fildl_FT0, mem_opnd);
        } else if (opnd_size == 16) {
            gen_softfpu_helper2m_16s((ADDR)helper_fildl_FT0, mem_opnd);
        } else {
            lsassert(0);
        }
        gen_softfpu_helper1((ADDR)helper_fsub_ST0_FT0);
	}
    return true;
}

static bool translate_fisubr_softfpu(IR1_INST *pir1)
{
    int index = get_src_index(pir1);
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    int opnd_size = ir1_opnd_size(opnd0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    if (option_softfpu == 2 && option_softfpu_fast & 0x800) {
        gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, true);
    } else {
        if (opnd_size == 32) {
            gen_softfpu_helper2m_32s((ADDR)helper_fildl_FT0, mem_opnd);
        } else if (opnd_size == 16) {
            gen_softfpu_helper2m_16s((ADDR)helper_fildl_FT0, mem_opnd);
        } else {
            lsassert(0);
        }
        gen_softfpu_helper1((ADDR)helper_fsubr_ST0_FT0);
	}
    return true;
}

static bool translate_fld1_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();
        IR2_OPND ftemp = ra_alloc_ftemp();
        IR2_OPND itemp = ra_alloc_itemp();

        la_fpu_push();
        li_d(itemp, 0x8000000000000000LL);
        la_movgr2fr_d(st0_opnd, itemp);
        li_wu(itemp, 0x3fff);
        la_movgr2fr_d(ftemp, itemp);
        la_vextrins_h(st0_opnd, ftemp, 0x40);

        save_float80_to_stn(st0_opnd, 0);
    } else {
        gen_softfpu_helper1((ADDR)helper_fpush);
        gen_softfpu_helper1((ADDR)helper_fld1_ST0);
    }
    return true;
}

static bool translate_fld_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        int index = get_src_index(pir1);
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
        int opnd_size = ir1_opnd_size(opnd0);

        IR2_OPND st0_opnd = ra_alloc_ftemp();
        IR2_OPND ftemp = ra_alloc_ftemp();

        if (ir1_opnd_is_fpr(opnd0)) {
            get_float80_from_stn(st0_opnd, ir1_opnd_base_reg_num(opnd0));
            la_fpu_push();
            save_float80_to_stn(st0_opnd, 0);
            return true;
        } else {
            IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
            IR2_OPND src_opnd = ra_alloc_ftemp();

            if (opnd_size == 32) {
                la_fld_s(src_opnd, mem_opnd, 0);

                la_fcvt_d_s(src_opnd, src_opnd);
                la_fcvt_ud_d(ftemp, src_opnd);
                la_fcvt_ld_d(st0_opnd, src_opnd);
                la_vextrins_h(st0_opnd, ftemp, 0x40);
            } else if (opnd_size == 64) {
                la_fld_d(src_opnd, mem_opnd, 0);
                la_fcvt_ud_d(ftemp, src_opnd);
                la_fcvt_ld_d(st0_opnd, src_opnd);
                la_vextrins_h(st0_opnd, ftemp, 0x40);
            } else if (opnd_size == 80) {
                IR2_OPND high_itemp = ra_alloc_itemp();
                la_fld_d(st0_opnd, mem_opnd, 0);
                la_ld_hu(high_itemp, mem_opnd, 8);
                la_movgr2fr_d(ftemp, high_itemp);
                la_vextrins_h(st0_opnd, ftemp, 0x40);

                ra_free_temp(high_itemp);
            }
        }

        la_fpu_push();
        save_float80_to_stn(st0_opnd, 0);

        ra_free_temp(st0_opnd);
        ra_free_temp(ftemp);
    } else {
        int index = get_src_index(pir1);
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
        int opnd_size = ir1_opnd_size(opnd0);

        if (ir1_opnd_is_fpr(opnd0)) {
            gen_softfpu_helper1((ADDR)helper_fpush);
            gen_softfpu_helper2i((ADDR)helper_fmov_ST0_STN,
                    (ir1_opnd_base_reg_num(opnd0) + 1) & 7);
            return true;
        }

        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

        if (opnd_size == 32) {
            gen_softfpu_helper2m_32u((ADDR)helper_flds_ST0, mem_opnd);
        } else if (opnd_size == 64) {
            gen_softfpu_helper2m_64((ADDR)helper_fldl_ST0, mem_opnd);
        } else if (opnd_size == 80) {
            gen_softfpu_helper2m_ptr((ADDR)helper_fldt_ST0, mem_opnd);
        }
    }
    return true;
}

static bool translate_fldcw_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    if (option_softfpu == 2) {
        IR2_OPND new_cw = ra_alloc_itemp();
        la_ld_hu(new_cw, mem_opnd, 0);
        la_st_h(new_cw, env_ir2_opnd, lsenv_offset_of_control_word(lsenv));
        la_update_fp_status(new_cw);
        ra_free_temp(new_cw);

    } else {
        gen_softfpu_helper2m_16u((ADDR)helper_fldcw, mem_opnd);
    }
    return true;
}

static bool translate_fldenv_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    int data32 = is_data32(pir1);

    if (option_softfpu == 2) {
        IR2_OPND cw_opnd = ra_alloc_itemp();
        IR2_OPND sw_opnd = ra_alloc_itemp();
        IR2_OPND tag_opnd = ra_alloc_itemp();

        IR2_OPND label_exit = ra_alloc_label();
        IR2_OPND label_restore_fpus = ra_alloc_label();

        if (data32) {
            la_ld_hu(cw_opnd, mem_opnd, 0);
            la_ld_hu(sw_opnd, mem_opnd, 4);
            la_ld_hu(tag_opnd, mem_opnd, 8);
        } else {
            la_ld_hu(cw_opnd, mem_opnd, 0);
            la_ld_hu(sw_opnd, mem_opnd, 2);
            la_ld_hu(tag_opnd, mem_opnd, 4);
        }

        /* restore fpuc */
        la_st_h(cw_opnd, env_ir2_opnd, lsenv_offset_of_control_word(lsenv));
        la_update_fp_status(cw_opnd);

        IR2_OPND tag_save = ra_alloc_itemp();
        IR2_OPND itemp = ra_alloc_itemp();
        IR2_OPND itemp2 = ra_alloc_itemp();
        /* restore top */
        la_bstrpick_d(itemp, sw_opnd, 13, 11);
        la_st_w(itemp, env_ir2_opnd, lsenv_offset_of_top(lsenv));

        /* restore fpus */
        li_d(itemp, 0x47ff);
        la_and(sw_opnd, sw_opnd, itemp);
        la_bstrpick_d(itemp, sw_opnd, 7, 7);
        la_beq(itemp, zero_ir2_opnd, label_restore_fpus);

        /* set fpus busy*/
        li_d(itemp, 0x8000);
        la_or(sw_opnd, sw_opnd, itemp);

        la_label(label_restore_fpus);
        li_d(itemp, 0x3f);
        la_andn(itemp, itemp, cw_opnd);
        la_and(itemp, sw_opnd, itemp);
        la_beq(itemp, zero_ir2_opnd, label_exit);
        li_d(itemp, 0x8080);
        la_or(sw_opnd, sw_opnd, itemp);

        la_label(label_exit);
        la_st_h(sw_opnd, env_ir2_opnd, lsenv_offset_of_status_word(lsenv));

        li_wu(tag_save, 0);
        li_wu(itemp2, 0x3);

        for (int i = 7; i >= 0; i--) {
            IR2_OPND label_no_empty = ra_alloc_label();
            la_slli_d(tag_save, tag_save, 8);
            la_bstrpick_d(itemp, tag_opnd, 2 * i + 1, 2 * i);
            la_bne(itemp, itemp2, label_no_empty);
            la_ori(tag_save, tag_save, 0x1);

            la_label(label_no_empty);
        }
        la_st_d(tag_save, env_ir2_opnd, lsenv_offset_of_tag_word(lsenv));


        ra_free_temp(cw_opnd);
        ra_free_temp(sw_opnd);
        ra_free_temp(tag_opnd);
        ra_free_temp(tag_save);
        ra_free_temp(itemp);
        ra_free_temp(itemp2);
        ra_free_temp(mem_opnd);

    } else {
        gen_softfpu_helper3i((ADDR)helper_fldenv, mem_opnd, data32);
    }
    return true;
}

static bool translate_fldl2e_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();
        IR2_OPND ftemp = ra_alloc_ftemp();
        IR2_OPND itemp = ra_alloc_itemp();
        IR2_OPND cw_opnd = ra_alloc_itemp();

        IR2_OPND label_default = ra_alloc_label();
        IR2_OPND label_exit = ra_alloc_label();

        la_fpu_push();
        li_wu(itemp, 0x3fff);
        la_movgr2fr_d(ftemp, itemp);
        la_ld_hu(cw_opnd, env_ir2_opnd, lsenv_offset_of_control_word(lsenv));
        la_bstrpick_d(cw_opnd, cw_opnd, 10, 10);

        la_beq(cw_opnd, zero_ir2_opnd, label_default);
        li_d(itemp, 0xb8aa3b295c17f0bbLL);
        la_b(label_exit);

        la_label(label_default);
        li_d(itemp, 0xb8aa3b295c17f0bcLL);

        la_label(label_exit);
        la_movgr2fr_d(st0_opnd, itemp);
        la_vextrins_h(st0_opnd, ftemp, 0x40);
        save_float80_to_stn(st0_opnd, 0);

    } else {
        gen_softfpu_helper1((ADDR)helper_fpush);
        gen_softfpu_helper1((ADDR)helper_fldl2e_ST0);

    }
    return true;
}

static bool translate_fldl2t_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();
        IR2_OPND ftemp = ra_alloc_ftemp();
        IR2_OPND itemp = ra_alloc_itemp();
        IR2_OPND cw_opnd = ra_alloc_itemp();

        IR2_OPND label_default = ra_alloc_label();
        IR2_OPND label_exit = ra_alloc_label();

        la_fpu_push();
        li_wu(itemp, 0x4000);
        la_movgr2fr_d(ftemp, itemp);
        la_ld_hu(cw_opnd, env_ir2_opnd, lsenv_offset_of_control_word(lsenv));
        la_bstrpick_d(cw_opnd, cw_opnd, 11, 11);

        la_beq(cw_opnd, zero_ir2_opnd, label_default);
        li_d(itemp, 0xd49a784bcd1b8affLL);
        la_b(label_exit);

        la_label(label_default);
        li_d(itemp, 0xd49a784bcd1b8afeLL);

        la_label(label_exit);
        la_movgr2fr_d(st0_opnd, itemp);
        la_vextrins_h(st0_opnd, ftemp, 0x40);
        save_float80_to_stn(st0_opnd, 0);
    } else {
        gen_softfpu_helper1((ADDR)helper_fpush);
        gen_softfpu_helper1((ADDR)helper_fldl2t_ST0);

    }
    return true;
}

static bool translate_fldlg2_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();
        IR2_OPND ftemp = ra_alloc_ftemp();
        IR2_OPND itemp = ra_alloc_itemp();
        IR2_OPND cw_opnd = ra_alloc_itemp();

        IR2_OPND label_default = ra_alloc_label();
        IR2_OPND label_exit = ra_alloc_label();

        la_fpu_push();
        li_wu(itemp, 0x3ffd);
        la_movgr2fr_d(ftemp, itemp);
        la_ld_hu(cw_opnd, env_ir2_opnd, lsenv_offset_of_control_word(lsenv));
        la_bstrpick_d(cw_opnd, cw_opnd, 10, 10);

        la_beq(cw_opnd, zero_ir2_opnd, label_default);
        li_d(itemp, 0x9a209a84fbcff798LL);
        la_b(label_exit);

        la_label(label_default);
        li_d(itemp, 0x9a209a84fbcff799LL);

        la_label(label_exit);
        la_movgr2fr_d(st0_opnd, itemp);
        la_vextrins_h(st0_opnd, ftemp, 0x40);
        save_float80_to_stn(st0_opnd, 0);

    }  else {
        gen_softfpu_helper1((ADDR)helper_fpush);
        gen_softfpu_helper1((ADDR)helper_fldlg2_ST0);

    }
    return true;
}

static bool translate_fldln2_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();
        IR2_OPND ftemp = ra_alloc_ftemp();
        IR2_OPND itemp = ra_alloc_itemp();
        IR2_OPND cw_opnd = ra_alloc_itemp();

        IR2_OPND label_default = ra_alloc_label();
        IR2_OPND label_exit = ra_alloc_label();

        la_fpu_push();
        li_wu(itemp, 0x3ffe);
        la_movgr2fr_d(ftemp, itemp);
        la_ld_hu(cw_opnd, env_ir2_opnd, lsenv_offset_of_control_word(lsenv));
        la_bstrpick_d(cw_opnd, cw_opnd, 10, 10);

        la_beq(cw_opnd, zero_ir2_opnd, label_default);
        li_d(itemp, 0xb17217f7d1cf79abLL);
        la_b(label_exit);

        la_label(label_default);
        li_d(itemp, 0xb17217f7d1cf79acLL);

        la_label(label_exit);
        la_movgr2fr_d(st0_opnd, itemp);
        la_vextrins_h(st0_opnd, ftemp, 0x40);
        save_float80_to_stn(st0_opnd, 0);

    } else {
        gen_softfpu_helper1((ADDR)helper_fpush);
        gen_softfpu_helper1((ADDR)helper_fldln2_ST0);

    }
    return true;
}

static bool translate_fldpi_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();
        IR2_OPND ftemp = ra_alloc_ftemp();
        IR2_OPND itemp = ra_alloc_itemp();
        IR2_OPND cw_opnd = ra_alloc_itemp();

        IR2_OPND label_default = ra_alloc_label();
        IR2_OPND label_exit = ra_alloc_label();

        la_fpu_push();
        li_wu(itemp, 0x4000);
        la_movgr2fr_d(ftemp, itemp);
        la_ld_hu(cw_opnd, env_ir2_opnd, lsenv_offset_of_control_word(lsenv));
        la_bstrpick_d(cw_opnd, cw_opnd, 10, 10);

        la_beq(cw_opnd, zero_ir2_opnd, label_default);
        li_d(itemp, 0xc90fdaa22168c234LL);
        la_b(label_exit);

        la_label(label_default);
        li_d(itemp, 0xc90fdaa22168c235LL);

        la_label(label_exit);
        la_movgr2fr_d(st0_opnd, itemp);
        la_vextrins_h(st0_opnd, ftemp, 0x40);
        save_float80_to_stn(st0_opnd, 0);

    } else {
        gen_softfpu_helper1((ADDR)helper_fpush);
        gen_softfpu_helper1((ADDR)helper_fldpi_ST0);

    }
    return true;
}

static bool translate_fldz_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();

        la_fpu_push();
        la_vandi_b(st0_opnd, st0_opnd, 0);
        save_float80_to_stn(st0_opnd, 0);

    } else {
        gen_softfpu_helper1((ADDR)helper_fpush);
        gen_softfpu_helper1((ADDR)helper_fldz_ST0);

    }
    return true;
}

static bool translate_fmul_softfpu(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    IR1_OPND *src = ir1_get_opnd(pir1, 0);
    if (ir1_opnd_is_mem(src)) {
        int opnd_size = ir1_opnd_size(src);
        IR2_OPND mem_opnd = convert_mem_no_offset(src);
        if (option_softfpu == 2 && option_softfpu_fast & 0x1000) {
            gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, false);
        } else {
            if (opnd_size == 32) {
                gen_softfpu_helper2m_32s((ADDR)helper_flds_FT0, mem_opnd);
            } else {
                gen_softfpu_helper2m_64((ADDR)helper_fldl_FT0, mem_opnd);
            }
            gen_softfpu_helper1((ADDR)helper_fmul_ST0_FT0);
        }
        return true;
    }

    int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x1000) {
        gen_float64_STN_ST0(pir1, opnd_num, stn);
        return true;
    } else {
        if (opnd_num == 1) {
            gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
            gen_softfpu_helper1((ADDR)helper_fmul_ST0_FT0);
            return true;
        } else if (opnd_num == 2) {
            gen_softfpu_helper2i((ADDR)helper_fmul_STN_ST0, stn);
            return true;
        }
    }
    lsassert(0);
    return false;
}

static bool translate_fmulp_softfpu(IR1_INST *pir1)
{
    IR1_OPND *src = ir1_get_opnd(pir1, 0);
	int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x2000) {
        gen_float64_STN_ST0_pop(pir1, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fmul_STN_ST0, stn);
        gen_softfpu_helper1((ADDR)helper_fpop);
    }
    return true;
}

static bool translate_fnclex_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND sw_value = ra_alloc_itemp();

        la_ld_hu(sw_value, env_ir2_opnd, lsenv_offset_of_status_word(lsenv));
        la_bstrpick_d(sw_value, sw_value, 14, 8);
        la_slli_d(sw_value, sw_value, 8);
        la_st_h(sw_value, env_ir2_opnd, lsenv_offset_of_status_word(lsenv));

        ra_free_temp(sw_value);

    } else {
        gen_softfpu_helper1((ADDR)helper_fclex);
    }
    return true;
}

static bool translate_fninit_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND temp = ra_alloc_itemp();

        /* clear status and set control*/
        int offset = lsenv_offset_of_status_word(lsenv);
        lsassert(offset <= 0x7ff);

        li_wu(temp, 0x37f0000ULL);
        la_st_w(temp, env_ir2_opnd, offset);
        la_srli_d(temp, temp, 16);
        la_update_fp_status(temp);

        /* clear top */
        la_st_w(zero_ir2_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));

        /* set fptags */
        offset = lsenv_offset_of_tag_word(lsenv);
        lsassert(offset <= 0x7ff);
        li_d(temp, 0x101010101010101ULL);
        la_st_d(temp, env_ir2_opnd, offset);

        ra_free_temp(temp);
    } else {
        gen_softfpu_helper1((ADDR)helper_fninit);
    }
    return true;
}

static bool translate_fnstenv_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    int data32 = is_data32(pir1);

    if (option_softfpu == 2) {
        IR2_OPND sw_cw_opnd = ra_alloc_itemp();
        IR2_OPND tag_opnd = ra_alloc_itemp();
        IR2_OPND tag_save = ra_alloc_itemp();
        IR2_OPND gpr_stn_low_opnd = ra_alloc_itemp();
        IR2_OPND gpr_stn_high_opnd = ra_alloc_itemp();
        IR2_OPND itemp = ra_alloc_itemp();

        /* update sw_opnd by tag */
        int offset = lsenv_offset_of_status_word(lsenv);
        lsassert(offset <= 0x7ff);
        la_ld_hu(sw_cw_opnd, env_ir2_opnd, offset);
        li_d(itemp, 0xc7ff);
        la_and(sw_cw_opnd, sw_cw_opnd, itemp);

        la_ld_wu(itemp, env_ir2_opnd, lsenv_offset_of_top(lsenv));
        la_slli_w(itemp, itemp, 11);
        la_or(sw_cw_opnd, sw_cw_opnd, itemp);
        la_slli_d(sw_cw_opnd, sw_cw_opnd, 16);

        offset = lsenv_offset_of_control_word(lsenv);
        lsassert(offset <= 0x7ff);
        la_ld_hu(itemp, env_ir2_opnd, offset);
        la_bstrins_d(sw_cw_opnd, itemp, 15, 0);

        offset = lsenv_offset_of_tag_word(lsenv);
        lsassert(offset <= 0x7ff);
        la_ld_d(tag_opnd, env_ir2_opnd, offset);

        li_d(tag_save, 0);
        for (int i = 7; i >= 0; i--) {
            IR2_OPND label_exit = ra_alloc_label();
            IR2_OPND label_empty = ra_alloc_label();
            IR2_OPND label_no_zero = ra_alloc_label();
            IR2_OPND label_no_valid = ra_alloc_label();

            la_slli_d(tag_save, tag_save, 2);
            la_bstrpick_d(itemp, tag_opnd, 8 * (i + 1) - 1, 8 * i);
            la_bne(itemp, zero_ir2_opnd, label_empty);

            offset = lsenv_offset_of_fpr(lsenv, i);
            lsassert(offset <= 0x7ff);
            la_ld_d(gpr_stn_low_opnd, env_ir2_opnd, offset);
            la_ld_hu(gpr_stn_high_opnd, env_ir2_opnd, offset + 8);


            la_bne(gpr_stn_high_opnd, zero_ir2_opnd, label_no_zero);
            la_bne(gpr_stn_low_opnd, zero_ir2_opnd, label_no_zero);

            /* zero */
            la_ori(tag_save, tag_save, 1);
            la_b(label_exit);

            la_label(label_no_zero);
            la_beq(gpr_stn_high_opnd, zero_ir2_opnd, label_no_valid);
            li_wu(itemp, 0x7fff);
            la_beq(gpr_stn_high_opnd, itemp, label_no_valid);
            li_d(itemp, 0x8000000000000000ULL);
            la_and(itemp, gpr_stn_low_opnd, itemp);
            la_beq(itemp, zero_ir2_opnd, label_no_valid);
            la_b(label_exit);

            la_label(label_no_valid);
            la_ori(tag_save, tag_save, 2);
            la_b(label_exit);

            la_label(label_empty);
            la_ori(tag_save, tag_save, 3);

            la_label(label_exit);
        }

        if (data32) {
            /* 32 bit */
            la_bstrpick_d(itemp, sw_cw_opnd, 15, 0);
            la_srli_d(sw_cw_opnd, sw_cw_opnd, 16);
            la_bstrins_d(itemp, sw_cw_opnd, 47, 32);
            la_st_d(itemp, mem_opnd, 0);
            la_st_w(tag_save, mem_opnd, 8);
            la_st_d(zero_ir2_opnd, mem_opnd, 12);
            la_st_d(zero_ir2_opnd, mem_opnd, 20);
        } else {
            /* 16 bit */
            la_st_w(sw_cw_opnd, mem_opnd, 0);
            la_st_h(tag_save, mem_opnd, 4);
            la_st_d(zero_ir2_opnd, mem_opnd, 6);
        }
        ra_free_temp(sw_cw_opnd);
        ra_free_temp(tag_opnd);
        ra_free_temp(tag_save);
        ra_free_temp(gpr_stn_low_opnd);
        ra_free_temp(gpr_stn_high_opnd);
        ra_free_temp(itemp);
        ra_free_temp(mem_opnd);

    } else {
        gen_softfpu_helper3i((ADDR)helper_fstenv, mem_opnd, data32);
    }
    return true;
}

static bool translate_fnop_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fwait);
    return true;
}

static bool translate_fnsave_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    int data32 = is_data32(pir1);

    if (option_softfpu == 2) {
        translate_fnstenv_softfpu(pir1);

        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
        IR2_OPND gpr_stn_high_opnd = ra_alloc_itemp();
        IR2_OPND gpr_stn_low_opnd = ra_alloc_itemp();

        /* store fpu data registers stack */
        int ptr = 14 << data32;
        for (int i = 0; i <= 7; i++) {
            int offset = lsenv_offset_of_fpr(lsenv, i);
            la_ld_d(gpr_stn_low_opnd, env_ir2_opnd, offset);
            la_ld_hu(gpr_stn_high_opnd, env_ir2_opnd, offset + 8);

            la_st_d(gpr_stn_low_opnd, mem_opnd, ptr + 10 * i);
            la_st_h(gpr_stn_high_opnd, mem_opnd, ptr + 10 * i + 8);
        }

        ra_free_temp(gpr_stn_low_opnd);
        ra_free_temp(gpr_stn_high_opnd);
        translate_fninit_softfpu(pir1);

    } else {
        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
        gen_softfpu_helper3i((ADDR)helper_fsave, mem_opnd, data32);
    }
    return true;
}

static bool translate_fnstcw_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        translate_fnstcw(pir1);
    } else {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        gen_softfpu_helper_prologue(pir1);
        gen_softfpu_helper1((ADDR)helper_fnstcw);
        gen_softfpu_helper_epilogue(pir1);
        /* v0 */
        if (ir1_opnd_is_mem(opnd0)) {
            int mem_imm;
            IR2_OPND mem_opnd = convert_mem(opnd0, &mem_imm);
            la_st_h(a0_ir2_opnd, mem_opnd, mem_imm);
        } else {
            if (option_mem_test) {
                IR2_OPND itemp = ra_alloc_itemp();
                la_mov64(itemp, a0_ir2_opnd);
                store_ireg_to_ir1(itemp, opnd0, false);
            } else {
                store_ireg_to_ir1(a0_ir2_opnd, opnd0, false);
            }
        }
    }
    return true;
}

static bool translate_fnstsw_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

        IR2_OPND sw_opnd = ra_alloc_itemp();
        IR2_OPND top_opnd = ra_alloc_itemp();
        IR2_OPND tmp_opnd = ra_alloc_itemp();

        int status_offset = lsenv_offset_of_status_word(lsenv);
        lsassert(status_offset <= 0x7ff);
        la_ld_h(sw_opnd, env_ir2_opnd, status_offset);

        la_ld_wu(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));
        la_slli_w(top_opnd, top_opnd, 11);

        la_lu12i_w(tmp_opnd, 0xc);
        la_ori(tmp_opnd, tmp_opnd, 0x7ff);
        la_and(sw_opnd, sw_opnd, tmp_opnd);

        la_or(sw_opnd, sw_opnd, top_opnd);

        if (ir1_opnd_is_mem(opnd0)) {
            int mem_imm;
            IR2_OPND mem_opnd = convert_mem(opnd0, &mem_imm);
            la_st_h(sw_opnd, mem_opnd, mem_imm);
        } else {
            store_ireg_to_ir1(sw_opnd, opnd0, false);
        }

        ra_free_temp(sw_opnd);

    } else {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        gen_softfpu_helper_prologue(pir1);
        gen_softfpu_helper1((ADDR)helper_fnstsw);
        gen_softfpu_helper_epilogue(pir1);
        /* v0 */
        if (ir1_opnd_is_mem(opnd0)) {
            int mem_imm;
            IR2_OPND mem_opnd = convert_mem(opnd0, &mem_imm);
            la_st_h(a0_ir2_opnd, mem_opnd, mem_imm);
        } else {
            if (option_mem_test) {
                IR2_OPND itemp = ra_alloc_itemp();
                la_mov64(itemp, a0_ir2_opnd);
                store_ireg_to_ir1(itemp, opnd0, false);
            } else {
                store_ireg_to_ir1(a0_ir2_opnd, opnd0, false);
            }
        }
    }
    return true;
}

static bool translate_fpatan_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fpatan);
    return true;
}

static bool translate_fprem1_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fprem1);
    return true;
}

static bool translate_fprem_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fprem);
    return true;
}

static bool translate_fptan_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fptan);
    return true;
}

static bool translate_frndint_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2 && option_softfpu_fast & 0x4000) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();

        get_float80_from_stn(st0_opnd, 0);
        la_calcute_float64(pir1, st0_opnd, st0_opnd, false);
        save_float80_to_stn(st0_opnd, 0);

        ra_free_temp(st0_opnd);
    } else {
        gen_softfpu_helper1((ADDR)helper_frndint);
    }
    return true;
}

static bool translate_frstor_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    int data32 = is_data32(pir1);

    if (option_softfpu == 2) {
        translate_fldenv_softfpu(pir1);

        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
        IR2_OPND gpr_stn_low_opnd = ra_alloc_itemp();
        IR2_OPND gpr_stn_high_opnd = ra_alloc_itemp();

        int ptr = 14 << data32;
        for (int i = 0; i <= 7; i++) {
            int offset = lsenv_offset_of_fpr(lsenv, i);

            la_ld_d(gpr_stn_low_opnd, mem_opnd, ptr + 10 * i);
            la_ld_hu(gpr_stn_high_opnd, mem_opnd, ptr + 10 * i + 8);

            la_st_d(gpr_stn_low_opnd, env_ir2_opnd, offset);
            la_st_h(gpr_stn_high_opnd, env_ir2_opnd, offset + 8);
        }
        ra_free_temp(gpr_stn_low_opnd);
        ra_free_temp(gpr_stn_high_opnd);

    } else {
        IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
        gen_softfpu_helper3i((ADDR)helper_frstor, mem_opnd, data32);

    }
    return true;
}

static bool translate_fscale_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2 && option_softfpu_fast & 0x8000) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();
        IR2_OPND st1_opnd = ra_alloc_ftemp();

        get_float80_from_stn(st0_opnd, 0);
        get_float80_from_stn(st1_opnd, 1);
        la_calcute_float64(pir1, st0_opnd, st1_opnd, true);
        save_float80_to_stn(st0_opnd, 0);

        ra_free_temp(st0_opnd);
        ra_free_temp(st1_opnd);
    } else {
        gen_softfpu_helper1((ADDR)helper_fscale);
    }
    return true;
}

static bool translate_fsetpm_softfpu(IR1_INST *pir1)
{
    return false;
}

static bool translate_fsin_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fsin);
    return true;
}

static bool translate_fsincos_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fsincos);
    return true;
}

static bool translate_fsqrt_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2 && option_softfpu_fast & 0x10000) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();

        get_float80_from_stn(st0_opnd, 0);
        la_calcute_float64(pir1, st0_opnd, st0_opnd, false);
        save_float80_to_stn(st0_opnd, 0);

        ra_free_temp(st0_opnd);
    } else {
        gen_softfpu_helper1((ADDR)helper_fsqrt);
    }
    return true;
}

static bool translate_fst_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2 && option_softfpu_fast & 0x800000) {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        int opnd_size = ir1_opnd_size(opnd0);

        if (ir1_opnd_is_fpr(opnd0)) {
            int opnd0_base_num = ir1_opnd_base_reg_num(opnd0);

            fmov_STN_STM(opnd0_base_num, 0);
        } else {
            IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

            IR2_OPND low_opnd = ra_alloc_ftemp();
            IR2_OPND high_opnd = ra_alloc_ftemp();

            get_float80_from_stn(low_opnd, 0);
            la_vextrins_h(high_opnd, low_opnd, 0x4);

            if (opnd_size == 80) {
                IR2_OPND gpr_high_opnd = ra_alloc_itemp();

                la_movfr2gr_d(gpr_high_opnd, high_opnd);
                la_fst_d(low_opnd, mem_opnd, 0);
                la_st_h(gpr_high_opnd, mem_opnd, 8);
                ra_free_temp(low_opnd);
                ra_free_temp(high_opnd);
                ra_free_temp(gpr_high_opnd);
                return true;
            }

            if (opnd_size == 32) {
                la_fcvt_d_ld(low_opnd, low_opnd, high_opnd);
                la_fcvt_s_d(low_opnd, low_opnd);
                la_fst_s(low_opnd, mem_opnd, 0);
            } else if (opnd_size == 64) {
                la_fcvt_d_ld(low_opnd, low_opnd, high_opnd);
                la_fst_d(low_opnd, mem_opnd, 0);
            }
        }

    } else {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        int opnd_size = ir1_opnd_size(opnd0);
        int opnd0_base_num = ir1_opnd_base_reg_num(opnd0);

        gen_softfpu_helper_prologue(pir1);
        if (ir1_opnd_is_fpr(opnd0)) {
            gen_softfpu_helper2i((ADDR)helper_fmov_STN_ST0,
                                    opnd0_base_num);
            gen_softfpu_helper_epilogue(pir1);
            return true;
        }

        if (opnd_size == 32) {
            gen_softfpu_helper1((ADDR)helper_fsts_ST0);
        } else if (opnd_size == 64) {
            gen_softfpu_helper1((ADDR)helper_fstl_ST0);
        }
        gen_softfpu_helper_epilogue(pir1);

        /* v0 */
        if (option_mem_test) {
            IR2_OPND itemp = ra_alloc_itemp();
            la_mov64(itemp, a0_ir2_opnd);
            store_ireg_to_ir1(itemp, opnd0, false);
        } else {
            store_ireg_to_ir1(a0_ir2_opnd, opnd0, false);
        }
    }
    return true;
}

static bool translate_fstp_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2 && option_softfpu_fast & 0x800000) {
        translate_fst_softfpu(pir1);
        la_fpu_pop();

    } else {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        int opnd_size = ir1_opnd_size(opnd0);

        gen_softfpu_helper_prologue(pir1);
        if (ir1_opnd_is_fpr(opnd0)) {
            int opnd0_base_num = ir1_opnd_base_reg_num(opnd0);
            gen_softfpu_helper2i((ADDR)helper_fmov_STN_ST0,
                                    opnd0_base_num);
            gen_softfpu_helper1((ADDR)helper_fpop);
            gen_softfpu_helper_epilogue(pir1);
            return true;
        }


        if (opnd_size == 80) {
            IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
            gen_softfpu_helper2m_ptr((ADDR)helper_fstt_ST0, mem_opnd);
            gen_softfpu_helper1((ADDR)helper_fpop);
            gen_softfpu_helper_epilogue(pir1);
            return true;
        }

        if (opnd_size == 32) {
            gen_softfpu_helper1((ADDR)helper_fsts_ST0);
        } else if (opnd_size == 64) {
            gen_softfpu_helper1((ADDR)helper_fstl_ST0);
        }
        restore_gpr();
        if (option_mem_test) {
            IR2_OPND itemp = ra_alloc_itemp();
            la_mov64(itemp, a0_ir2_opnd);
            store_ireg_to_ir1(itemp, opnd0, false);
        } else {
            store_ireg_to_ir1(a0_ir2_opnd, opnd0, false);
        }
        save_gpr();
        gen_softfpu_helper1((ADDR)helper_fpop);
        gen_softfpu_helper_epilogue(pir1);

    }
    return true;
}

static bool translate_fsub_softfpu(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    IR1_OPND *src = ir1_get_opnd(pir1, 0);
    if (ir1_opnd_is_mem(src)) {
        int opnd_size = ir1_opnd_size(src);
        IR2_OPND mem_opnd = convert_mem_no_offset(src);
        if (option_softfpu == 2 && option_softfpu_fast & 0x20000) {
            gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, false);
        } else {
            if (opnd_size == 32) {
                gen_softfpu_helper2m_32s((ADDR)helper_flds_FT0, mem_opnd);
            } else {
                gen_softfpu_helper2m_64((ADDR)helper_fldl_FT0, mem_opnd);
            }
            gen_softfpu_helper1((ADDR)helper_fsub_ST0_FT0);
        }
        return true;
    }

    int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x20000) {
        gen_float64_STN_ST0(pir1, opnd_num, stn);
        return true;
    } else {
        if (opnd_num == 1) {
            gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
            gen_softfpu_helper1((ADDR)helper_fsub_ST0_FT0);
            return true;
        } else if (opnd_num == 2) {
            gen_softfpu_helper2i((ADDR)helper_fsub_STN_ST0, stn);
            return true;
        }
    }
    lsassert(0);
    return false;
}

static bool translate_fsubp_softfpu(IR1_INST *pir1)
{
    IR1_OPND *src = ir1_get_opnd(pir1, 0);
	int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x40000) {
        gen_float64_STN_ST0_pop(pir1, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fsub_STN_ST0, stn);
        gen_softfpu_helper1((ADDR)helper_fpop);
    }
    return true;
}

static bool translate_fsubr_softfpu(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    IR1_OPND *src = ir1_get_opnd(pir1, 0);
    if (ir1_opnd_is_mem(src)) {
        int opnd_size = ir1_opnd_size(src);
        IR2_OPND mem_opnd = convert_mem_no_offset(src);
        if (option_softfpu == 2 && option_softfpu_fast & 0x80000) {
            gen_float64_ST0_MEM(pir1, mem_opnd, opnd_size, false);
        } else {
            if (opnd_size == 32) {
                gen_softfpu_helper2m_32s((ADDR)helper_flds_FT0, mem_opnd);
            } else {
                gen_softfpu_helper2m_64((ADDR)helper_fldl_FT0, mem_opnd);
            }
            gen_softfpu_helper1((ADDR)helper_fsubr_ST0_FT0);
        }
        return true;
    }

    int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x80000) {
        gen_float64_STN_ST0(pir1, opnd_num, stn);
        return true;
    } else {
        if (opnd_num == 1) {
            gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
            gen_softfpu_helper1((ADDR)helper_fsubr_ST0_FT0);
            return true;
        } else if (opnd_num == 2) {
            gen_softfpu_helper2i((ADDR)helper_fsubr_STN_ST0, stn);
            return true;
        }
    }
    lsassert(0);
    return false;
}

static bool translate_fsubrp_softfpu(IR1_INST *pir1)
{
    IR1_OPND *src = ir1_get_opnd(pir1, 0);
	int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2 && option_softfpu_fast & 0x100000) {
        gen_float64_STN_ST0_pop(pir1, stn);
    } else {
        gen_softfpu_helper2i((ADDR)helper_fsubr_STN_ST0, stn);
        gen_softfpu_helper1((ADDR)helper_fpop);
    }
    return true;
}

static bool translate_ftst_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fldz_FT0);
    gen_softfpu_helper1((ADDR)helper_fcom_ST0_FT0);
    return true;
}

static bool translate_fucom_softfpu(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    int stn = 1;
    if (opnd_num) {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        stn = ir1_opnd_base_reg_num(opnd0);
    }
    gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
    gen_softfpu_helper1((ADDR)helper_fucom_ST0_FT0);
    return true;
}

static bool translate_fucomi_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    int stn = ir1_opnd_base_reg_num(opnd0);
    gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
    gen_softfpu_helper1((ADDR)helper_fucomi_ST0_FT0);
    return true;
}

static bool translate_fucomip_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    int stn = ir1_opnd_base_reg_num(opnd0);
    gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
    gen_softfpu_helper1((ADDR)helper_fucomi_ST0_FT0);
    gen_softfpu_helper1((ADDR)helper_fpop);
    return true;
}

static bool translate_fucomp_softfpu(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    int stn = 1;
    if (opnd_num) {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        stn = ir1_opnd_base_reg_num(opnd0);
    }
    gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, stn);
    gen_softfpu_helper1((ADDR)helper_fucom_ST0_FT0);
    gen_softfpu_helper1((ADDR)helper_fpop);
    return true;
}

static bool translate_fucompp_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper2i((ADDR)helper_fmov_FT0_STN, 1);
    gen_softfpu_helper1((ADDR)helper_fucom_ST0_FT0);
    gen_softfpu_helper1((ADDR)helper_fpop);
    gen_softfpu_helper1((ADDR)helper_fpop);
    return true;
}

static bool translate_fxam_softfpu(IR1_INST *pir1)
{
    if (option_softfpu == 2) {
        IR2_OPND fpr_st0_low_opnd = ra_alloc_ftemp();
        IR2_OPND ftemp = ra_alloc_ftemp();
        IR2_OPND gpr_st0_high_opnd = ra_alloc_itemp();

        IR2_OPND fpu_tag = ra_alloc_itemp();
        IR2_OPND top_opnd = ra_alloc_itemp();
        IR2_OPND fpus = ra_alloc_itemp();
        IR2_OPND temp1 = ra_alloc_itemp();
        IR2_OPND temp2 = ra_alloc_itemp();
        int status_offset = lsenv_offset_of_status_word(lsenv);
        IR2_OPND not_set_c1 = ra_alloc_label();
        IR2_OPND label_next = ra_alloc_label();
        IR2_OPND label_exit = ra_alloc_label();
        IR2_OPND label_infinity_nan = ra_alloc_label();
        IR2_OPND label_zero_denormal = ra_alloc_label();
        IR2_OPND label_infinity = ra_alloc_label();
        IR2_OPND label_zero = ra_alloc_label();


        get_float80_from_stn(fpr_st0_low_opnd, 0);
        la_movgr2fr_d(ftemp, zero_ir2_opnd);
        la_vextrins_h(ftemp, fpr_st0_low_opnd, 0x4);
        la_movfr2gr_d(gpr_st0_high_opnd, ftemp);
        /*
         * C3, C2, C1, C0 ← 0000
         */
        li_wu(temp1, 0xb8ff);
        la_ld_h(fpus, env_ir2_opnd, status_offset);
        la_and(fpus, fpus, temp1);

        /*
         * C1 ← sign bit of ST0
         */
        li_d(temp1, 0x8000ULL);
        la_and(temp2, gpr_st0_high_opnd, temp1);
        la_bne(temp2, temp1, not_set_c1);
        la_ori(fpus, fpus, 0x200);
        la_label(not_set_c1);

        /* empty is judged by FPU tag word */
        int tag_offset = lsenv_offset_of_tag_word(lsenv);
        lsassert(tag_offset <= 0x7ff);
        la_ld_d(fpu_tag, env_ir2_opnd, tag_offset);

        la_ld_wu(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));
        li_d(temp1, (uint64)(0xffULL));
        la_slli_d(top_opnd, top_opnd, 3);
        la_srl_d(fpu_tag, fpu_tag, top_opnd);
        la_and(fpu_tag, fpu_tag, temp1);
        la_beq(fpu_tag, zero_ir2_opnd, label_next);

        li_wu(temp1, 0x4100UL);
        la_or(fpus, fpus, temp1);
        la_b(label_exit);

        ra_free_temp(fpu_tag);
        /*
         * slow path
         */
        IR2_OPND gpr_st0_low_opnd = ra_alloc_itemp();

        la_label(label_next);
        li_wu(temp1, 0x7fffULL);
        la_and(temp2, gpr_st0_high_opnd, temp1);
        la_beq(temp2, temp1, label_infinity_nan);
        la_beq(temp2, zero_ir2_opnd, label_zero_denormal);
        li_d(temp1, 0x8000000000000000ULL);
        la_movfr2gr_d(gpr_st0_low_opnd, fpr_st0_low_opnd);
        la_and(temp2, gpr_st0_low_opnd, temp1);
        /* Unsupported number */
        la_beq(temp2, zero_ir2_opnd, label_exit);
        /* Normal finite number */
        la_ori(fpus, fpus, 0x400);
        la_b(label_exit);

        la_label(label_infinity_nan);
        li_d(temp1, 0x8000000000000000ULL);
        la_movfr2gr_d(gpr_st0_low_opnd, fpr_st0_low_opnd);
        la_beq(gpr_st0_low_opnd, temp1, label_infinity);
        la_and(temp2, gpr_st0_low_opnd, temp1);
        /* Unsupported number */
        la_beq(temp2, zero_ir2_opnd, label_exit);
        /* NaN */
        la_ori(fpus, fpus, 0x100);
        la_b(label_exit);

        /* Infinity */
        la_label(label_infinity);
        la_ori(fpus, fpus, 0x500);
        la_b(label_exit);

        la_label(label_zero_denormal);
        li_d(temp1, 0x8000000000000000ULL);
        la_movfr2gr_d(gpr_st0_low_opnd, fpr_st0_low_opnd);
        la_beq(gpr_st0_low_opnd, zero_ir2_opnd, label_zero);
        /* Denormal */
        li_wu(temp1, 0x4400UL);
        la_or(fpus, fpus, temp1);
        la_b(label_exit);
        /* Zero */
        la_label(label_zero);
        li_w(temp1, 0x4000UL);
        la_or(fpus, fpus, temp1);
        la_b(label_exit);
        /*
         * exit
         */
        la_label(label_exit);
        la_st_h(fpus, env_ir2_opnd, status_offset);

        ra_free_temp(temp1);
        ra_free_temp(temp2);

    } else {
        gen_softfpu_helper1((ADDR)helper_fxam_ST0_softfpu);

    }
    return true;
}

static bool translate_fxch_softfpu(IR1_INST *pir1)
{
    int index = get_src_index(pir1);

    IR1_OPND *src = ir1_get_opnd(pir1, index);
	int stn = ir1_opnd_base_reg_num(src);
    if (option_softfpu == 2) {
        IR2_OPND st0_opnd = ra_alloc_ftemp();
        IR2_OPND stn_opnd = ra_alloc_ftemp();

        get_float80_from_stn(st0_opnd, 0);
        get_float80_from_stn(stn_opnd, stn);

        save_float80_to_stn(st0_opnd, stn);
        save_float80_to_stn(stn_opnd, 0);

    } else {
        gen_softfpu_helper2i((ADDR)helper_fxchg_ST0_STN, stn);
    }
    return true;
}

static bool translate_fxrstor_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    gen_softfpu_helper2m_ptr((ADDR)helper_fxrstor, mem_opnd);
    return true;
}

static bool translate_fxsave_softfpu(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    gen_softfpu_helper2m_ptr((ADDR)helper_fxsave, mem_opnd);
    return true;
}

static bool translate_fxtract_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fxtract);
    return true;
}

static bool translate_fyl2x_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fyl2x);
    return true;
}

static bool translate_fyl2xp1_softfpu(IR1_INST *pir1)
{
    gen_softfpu_helper1((ADDR)helper_fyl2xp1);
    return true;
}

#ifdef CONFIG_LATX_AVX_OPT
static bool translate_xgetbv_softfpu(IR1_INST *pir1)
{
    IR2_OPND ecx_opnd = ra_alloc_gpr(ecx_index);
    gen_softfpu_helper2m_ptr((ADDR)helper_xgetbv, ecx_opnd);
    IR2_OPND temp_low = ra_alloc_itemp();
    IR2_OPND temp_high = ra_alloc_itemp();
    la_bstrpick_d(temp_low, a0_ir2_opnd, 31, 0);
    la_bstrpick_d(temp_high, a0_ir2_opnd, 63, 32);
    la_st_d(temp_low, env_ir2_opnd, lsenv_offset_of_gpr(lsenv, R_EAX));
    la_st_d(temp_high, env_ir2_opnd, lsenv_offset_of_gpr(lsenv, R_EDX));
    return true;
}

static bool translate_xsetbv_softfpu(IR1_INST *pir1)
{
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND ecx_opnd = ra_alloc_gpr(ecx_index);
    IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
    IR2_OPND temp_rfbm = ra_alloc_itemp();

    la_bstrins_d(temp_rfbm, eax_opnd, 31, 0);
    la_bstrins_d(temp_rfbm, edx_opnd, 63, 32);
    gen_softfpu_helper3_ll((ADDR)helper_xsetbv, ecx_opnd, temp_rfbm);
    return true;
}

static bool translate_xsave_softfpu(IR1_INST *pir1)
{
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
    IR2_OPND temp_rfbm = ra_alloc_itemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    la_bstrins_d(temp_rfbm, eax_opnd, 31, 0);
    la_bstrins_d(temp_rfbm, edx_opnd, 63, 32);
    gen_softfpu_helper3_ll((ADDR)helper_xsave, mem_opnd, temp_rfbm);
    return true;
}

static bool translate_xsaveopt_softfpu(IR1_INST *pir1)
{
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
    IR2_OPND temp_rfbm = ra_alloc_itemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    la_bstrins_d(temp_rfbm, eax_opnd, 31, 0);
    la_bstrins_d(temp_rfbm, edx_opnd, 63, 32);
    gen_softfpu_helper3_ll((ADDR)helper_xsaveopt, mem_opnd, temp_rfbm);
    return true;
}

static bool translate_xrstor_softfpu(IR1_INST *pir1)
{
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
    IR2_OPND temp_rfbm = ra_alloc_itemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    la_bstrins_d(temp_rfbm, eax_opnd, 31, 0);
    la_bstrins_d(temp_rfbm, edx_opnd, 63, 32);
    gen_softfpu_helper3_ll((ADDR)helper_xrstor, mem_opnd, temp_rfbm);
    return true;
}
#endif

TRANS_FPU_WRAP_GEN(wait);
TRANS_FPU_WRAP_GEN(f2xm1);
TRANS_FPU_WRAP_GEN(fabs);
TRANS_FPU_WRAP_GEN(fadd);
TRANS_FPU_WRAP_GEN(faddp);
TRANS_FPU_WRAP_GEN(fbld);
TRANS_FPU_WRAP_GEN(fbstp);
TRANS_FPU_WRAP_GEN(fchs);
TRANS_FPU_WRAP_GEN(fcmovcc);
TRANS_FPU_WRAP_GEN(fcom);
TRANS_FPU_WRAP_GEN(fcomi);
TRANS_FPU_WRAP_GEN(fcomip);
TRANS_FPU_WRAP_GEN(fcomp);
TRANS_FPU_WRAP_GEN(fcompp);
TRANS_FPU_WRAP_GEN(fcos);
TRANS_FPU_WRAP_GEN(fdecstp);
TRANS_FPU_WRAP_GEN(fdiv);
TRANS_FPU_WRAP_GEN(fdivp);
TRANS_FPU_WRAP_GEN(fdivr);
TRANS_FPU_WRAP_GEN(fdivrp);
TRANS_FPU_WRAP_GEN(ffree);
TRANS_FPU_WRAP_GEN(ffreep);
TRANS_FPU_WRAP_GEN(fiadd);
TRANS_FPU_WRAP_GEN(ficom);
TRANS_FPU_WRAP_GEN(ficomp);
TRANS_FPU_WRAP_GEN(fidiv);
TRANS_FPU_WRAP_GEN(fidivr);
TRANS_FPU_WRAP_GEN(fild);
TRANS_FPU_WRAP_GEN(fimul);
TRANS_FPU_WRAP_GEN(fincstp);
TRANS_FPU_WRAP_GEN_NO_PROLOGUE(fist);
TRANS_FPU_WRAP_GEN_NO_PROLOGUE(fistp);
TRANS_FPU_WRAP_GEN_NO_PROLOGUE(fisttp);
TRANS_FPU_WRAP_GEN(fisub);
TRANS_FPU_WRAP_GEN(fisubr);
TRANS_FPU_WRAP_GEN(fld1);
TRANS_FPU_WRAP_GEN(fld);
TRANS_FPU_WRAP_GEN(fldcw);
TRANS_FPU_WRAP_GEN(fldenv);
TRANS_FPU_WRAP_GEN(fldl2e);
TRANS_FPU_WRAP_GEN(fldl2t);
TRANS_FPU_WRAP_GEN(fldlg2);
TRANS_FPU_WRAP_GEN(fldln2);
TRANS_FPU_WRAP_GEN(fldpi);
TRANS_FPU_WRAP_GEN(fldz);
TRANS_FPU_WRAP_GEN(fmul);
TRANS_FPU_WRAP_GEN(fmulp);
TRANS_FPU_WRAP_GEN(fnclex);
TRANS_FPU_WRAP_GEN(fninit);
TRANS_FPU_WRAP_GEN(fnop);
TRANS_FPU_WRAP_GEN(fnsave);
TRANS_FPU_WRAP_GEN_NO_PROLOGUE(fnstcw);
TRANS_FPU_WRAP_GEN(fnstenv);
TRANS_FPU_WRAP_GEN_NO_PROLOGUE(fnstsw);
TRANS_FPU_WRAP_GEN(fpatan);
TRANS_FPU_WRAP_GEN(fprem1);
TRANS_FPU_WRAP_GEN(fprem);
TRANS_FPU_WRAP_GEN(fptan);
TRANS_FPU_WRAP_GEN(frndint);
TRANS_FPU_WRAP_GEN(frstor);
TRANS_FPU_WRAP_GEN(fscale);
TRANS_FPU_WRAP_GEN(fsetpm);
TRANS_FPU_WRAP_GEN(fsin);
TRANS_FPU_WRAP_GEN(fsincos);
TRANS_FPU_WRAP_GEN(fsqrt);
TRANS_FPU_WRAP_GEN_NO_PROLOGUE(fst);
TRANS_FPU_WRAP_GEN_NO_PROLOGUE(fstp);
TRANS_FPU_WRAP_GEN(fsub);
TRANS_FPU_WRAP_GEN(fsubp);
TRANS_FPU_WRAP_GEN(fsubr);
TRANS_FPU_WRAP_GEN(fsubrp);
TRANS_FPU_WRAP_GEN(ftst);
TRANS_FPU_WRAP_GEN(fucom);
TRANS_FPU_WRAP_GEN(fucomi);
TRANS_FPU_WRAP_GEN(fucomip);
TRANS_FPU_WRAP_GEN(fucomp);
TRANS_FPU_WRAP_GEN(fucompp);
TRANS_FPU_WRAP_GEN(fxam);
TRANS_FPU_WRAP_GEN(fxch);
TRANS_FPU_WRAP_GEN(fxrstor);
TRANS_FPU_WRAP_GEN(fxsave);
TRANS_FPU_WRAP_GEN(fxtract);
TRANS_FPU_WRAP_GEN(fyl2x);
TRANS_FPU_WRAP_GEN(fyl2xp1);
#ifdef CONFIG_LATX_AVX_OPT
TRANS_FPU_WRAP_GEN_NO_PROLOGUE(xgetbv);
TRANS_FPU_WRAP_GEN(xsetbv);
TRANS_FPU_WRAP_GEN(xsave);
TRANS_FPU_WRAP_GEN(xsaveopt);
TRANS_FPU_WRAP_GEN(xrstor);
#endif
