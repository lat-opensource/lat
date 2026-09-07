/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "translate.h"

/*
 * Map LoongArch FCSR0 sticky Flags V/Z/O/U/I to the common x86
 * exception-status layout IE/ZE/OE/UE/PE.  Bit 1 (DE) remains clear
 * because FCSR0 has no directly corresponding flag.
 *
 * This only emits register operations.  It neither accesses guest
 * shadow state nor clears FCSR0.
 */
static void fcsr_flags_to_x86_exceptions(IR2_OPND dest, IR2_OPND fcsr)
{
    /*
     * After bit reversal and shifting:
     * dest[4:0] = I/U/O/Z/V.
     */
    la_bitrev_w(dest, fcsr);
    la_srli_d(dest, dest, 31 - FCSR_OFF_FLAGS_V);

    /* Preserve V as x86 IE at bit 0.  fcsr is scratch from here on. */
    la_bstrpick_d(fcsr, dest, 0, 0);

    /*
     * I/U/O/Z move from dest[4:1] to x86 PE/UE/OE/ZE at bits 5:2.
     * Bit 1, x86 DE, deliberately remains clear.
     */
    la_bstrpick_d(dest, dest, 4, 1);
    la_slli_d(dest, dest, 2);
    la_or(dest, dest, fcsr);
}

/* Merge the native FCSR sticky flags into an x86 flag-bearing value. */
static void merge_fcsr_flags(IR2_OPND value)
{
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND x86_exceptions = ra_alloc_itemp();

    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    fcsr_flags_to_x86_exceptions(x86_exceptions, fcsr);
    la_or(value, value, x86_exceptions);

    ra_free_temp(x86_exceptions);
    ra_free_temp(fcsr);
}

/* Clear only the native sticky exception flags, preserving FCSR controls. */
static void clear_native_fcsr_flags(IR2_OPND fcsr)
{

    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrins_w(fcsr, zero_ir2_opnd,
                 FCSR_OFF_FLAGS_V, FCSR_OFF_FLAGS_I);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);

}

/* Merge native FCSR flags into the SSE/MXCSR shadow. */
static void submit_sse_flags_from_fcsr(IR2_OPND mxcsr_opnd)
{
    int mxcsr_offset = lsenv_offset_of_mxcsr(lsenv);

    lsassert(mxcsr_offset <= 0x7ff);
    la_ld_wu(mxcsr_opnd, env_ir2_opnd, mxcsr_offset);
    merge_fcsr_flags(mxcsr_opnd);
    la_st_w(mxcsr_opnd, env_ir2_opnd, mxcsr_offset);
}

/*
 * Access the LATX-private FCSR state byte with the shortest available
 * addressing form.  The field is appended to CPUX86State and may therefore
 * be outside the signed 12-bit immediate range on some layouts.
 */
void tr_load_fcsr_flags_state(IR2_OPND state)
{
    int state_offset = lsenv_offset_of_fcsr_flags_state(lsenv);

    if (!si12_overflow(state_offset)) {
        la_ld_bu(state, env_ir2_opnd, state_offset);
    } else {
        /* The destination is dead before the load, so reuse it for offset. */
        li_d(state, state_offset);
        la_ldx_bu(state, env_ir2_opnd, state);
    }
}

void tr_store_fcsr_flags_state(IR2_OPND state)
{
    int state_offset = lsenv_offset_of_fcsr_flags_state(lsenv);

    if (!si12_overflow(state_offset)) {
        la_st_b(state, env_ir2_opnd, state_offset);
    } else {
        /* Store needs both the value and an independent offset register. */
        IR2_OPND state_offset_opnd = ra_alloc_itemp();

        li_d(state_offset_opnd, state_offset);
        la_stx_b(state, env_ir2_opnd, state_offset_opnd);
        ra_free_temp(state_offset_opnd);
    }
}

static void update_fcsr_flag(IR2_OPND status_word, IR2_OPND fcsr)
{
    IR2_OPND temp = ra_alloc_itemp();

    /* convert x86 to LA */
    la_bitrev_w(temp, status_word);
    la_bstrpick_w(temp, temp, 30, 26);
    /* set fcsr  */
    la_bstrins_w(fcsr, temp, FCSR_OFF_FLAGS_Z, FCSR_OFF_FLAGS_I);
    la_bstrins_w(fcsr, status_word,
                            FCSR_OFF_FLAGS_V, FCSR_OFF_FLAGS_V);

    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);

    ra_free_temp(temp);
}

static void update_fcsr_enable(IR2_OPND control_word, IR2_OPND fcsr)
{
    IR2_OPND temp = ra_alloc_itemp();

    if (option_enable_fcsr_exc) {
        /* reset enables */
        la_bstrins_w(fcsr, zero_ir2_opnd,
                     FCSR_OFF_EN_V, FCSR_OFF_EN_I);
        /* IM */
        la_bstrpick_w(temp, control_word,
                      X87_CR_OFF_IM, X87_CR_OFF_IM);
        la_xori(temp, zero_ir2_opnd, 1);
        la_bstrins_w(fcsr, temp,
                     FCSR_OFF_EN_V, FCSR_OFF_EN_V);
        /* ZM */
        la_bstrpick_w(temp, control_word,
                      X87_CR_OFF_ZM, X87_CR_OFF_ZM);
        la_xori(temp, zero_ir2_opnd, 1);
        la_bstrins_w(fcsr, temp,
                     FCSR_OFF_EN_Z, FCSR_OFF_EN_Z);
        /* OM */
        la_bstrpick_w(temp, control_word,
                      X87_CR_OFF_OM, X87_CR_OFF_OM);
        la_xori(temp, zero_ir2_opnd, 1);
        la_bstrins_w(fcsr, temp,
                     FCSR_OFF_EN_O, FCSR_OFF_EN_O);
        /* UM */
        la_bstrpick_w(temp, control_word,
                      X87_CR_OFF_UM, X87_CR_OFF_UM);
        la_xori(temp, zero_ir2_opnd, 1);
        la_bstrins_w(fcsr, temp,
                     FCSR_OFF_EN_U, FCSR_OFF_EN_U);
        /* PM */
        la_bstrpick_w(temp, control_word,
                      X87_CR_OFF_PM, X87_CR_OFF_PM);
        la_andi(temp, zero_ir2_opnd, 0x0);
        la_bstrins_w(fcsr, temp,
                     FCSR_OFF_EN_I, FCSR_OFF_EN_I);

    } else {
        /* convert x86 to LA */
        la_bitrev_w(temp, control_word);
        la_bstrpick_w(temp, temp, 30, 26);
        la_bstrins_w(temp, control_word, FCSR_OFF_EN_V, FCSR_OFF_EN_V);
        la_xori(temp, temp, 0x1f);
        /* set fcsr  */
        la_bstrins_w(fcsr, temp, FCSR_OFF_EN_V, FCSR_OFF_EN_I);
    }

    ra_free_temp(temp);

}

void update_fcsr_rm(IR2_OPND control_word, IR2_OPND fcsr)
{
    /* reset rounding mode */
    la_bstrins_w(fcsr, zero_ir2_opnd,
                            FCSR_OFF_RM + 1, FCSR_OFF_RM);
    /* load rounding mode in x86 control register */
    la_bstrpick_w(control_word, control_word,
                            X87_CR_OFF_RC + 1, X87_CR_OFF_RC);
    /*
     *turn x86 rm to LA rm
     *          x86      LA
     *
     * RN       00       00
     * RD       01       11
     * RU       10       10
     * RZ       11       01
     *
     */
    IR2_OPND temp_cw = ra_alloc_itemp();
    la_andi(temp_cw, control_word, 1);
    IR2_OPND label = ra_alloc_label();
    la_beq(temp_cw, zero_ir2_opnd, label);
    la_xori(control_word, control_word, 2);
    la_label(label);
    /* set rounding mode in LA FCSR */
    la_bstrins_w(fcsr, control_word,
                            FCSR_OFF_RM + 1, FCSR_OFF_RM);

    ra_free_temp(temp_cw);
}

void update_fcsr_by_sw(IR2_OPND sw)
{
    IR2_OPND old_fcsr = ra_alloc_itemp();
    la_movfcsr2gr(old_fcsr, fcsr_ir2_opnd);
    update_fcsr_flag(sw, old_fcsr);
    ra_free_temp(old_fcsr);
}

void update_fcsr_by_cw(IR2_OPND cw)
{
    IR2_OPND fcsr = ra_alloc_itemp();
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    update_fcsr_enable(cw, fcsr);
    update_fcsr_rm(cw, fcsr);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
    ra_free_temp(fcsr);
}

/* Merge native FCSR flags into the x87 status shadow. */
void submit_x87_flags_from_fcsr(IR2_OPND sw_opnd)
{
    int status_offset = lsenv_offset_of_status_word(lsenv);
    lsassert(status_offset <= 0x7ff);
    la_ld_h(sw_opnd, env_ir2_opnd, status_offset);

    merge_fcsr_flags(sw_opnd);

    la_st_h(sw_opnd, env_ir2_opnd, status_offset);
}

/*
 * Prepare the shared native FCSR for the selected x87 or SSE/AVX domain.
 *
 * A domain transition may:
 *   - submit pending native Flags from the previous dirty owner;
 *   - clear native sticky Flags before entering the new domain;
 *   - load the target domain's rounding mode into FCSR.RM;
 *   - mark the target owner dirty for the next native producer.
 *
 * The runtime state byte remains authoritative across TBs.  The TB-local
 * domain cache suppresses redundant preparation for consecutive instructions
 * in the same domain.  Dirty is established before the native producer so an
 * asynchronous exit cannot observe a producer result while the runtime state
 * still names the previous domain.
 */
void prepare_fcsr_for_domain(FcsrFlagsDomain domain)
{
    TRANSLATION_DATA *tr_data = lsenv->tr_data;
    IR2_OPND state;
    IR2_OPND state_cmp;
    IR2_OPND same_owner;
    IR2_OPND no_submit;
    int target_owner;
    int opposite_dirty;

    lsassert(domain == FCSR_FLAGS_DOMAIN_X87 ||
             domain == FCSR_FLAGS_DOMAIN_SSE);

    if (tr_data->fcsr_flags_domain == domain) {
        return;
    }

    target_owner = domain == FCSR_FLAGS_DOMAIN_X87
                 ? FCSR_FLAGS_STATE_X87 : FCSR_FLAGS_STATE_SSE;
    opposite_dirty = domain == FCSR_FLAGS_DOMAIN_X87
                   ? FCSR_FLAGS_STATE_SSE_DIRTY
                   : FCSR_FLAGS_STATE_X87_DIRTY;

    state = ra_alloc_itemp();
    state_cmp = ra_alloc_itemp();
    same_owner = ra_alloc_label();
    no_submit = ra_alloc_label();

    tr_load_fcsr_flags_state(state);

    /* Same-domain clean/dirty state keeps the existing sticky FCSR flags. */
    la_andi(state_cmp, state, FCSR_FLAGS_OWNER_MASK);
    la_xori(state_cmp, state_cmp, target_owner);
    la_beqz(state_cmp, same_owner);

    /* Submit pending flags only when the previous domain is dirty. */
    la_xori(state_cmp, state, opposite_dirty);
    la_bnez(state_cmp, no_submit);
    if (domain == FCSR_FLAGS_DOMAIN_X87) {
        submit_sse_flags_from_fcsr(state_cmp);
    } else {
        submit_x87_flags_from_fcsr(state_cmp);
    }
    la_label(no_submit);

    /* NONE and every cross-domain transition start from clean native flags. */
    clear_native_fcsr_flags(state_cmp);

    la_label(same_owner);

    if (domain == FCSR_FLAGS_DOMAIN_SSE) {
        la_ld_wu(state, env_ir2_opnd, lsenv_offset_of_mxcsr(lsenv));
        la_bstrpick_w(state, state, 14, 13);
    } else {
        la_ld_h(state, env_ir2_opnd, lsenv_offset_of_control_word(lsenv));
        la_bstrpick_w(state, state, X87_CR_OFF_RC + 1, X87_CR_OFF_RC);
    }
    /*
    * x86 RC → LoongArch RM:
    *
    * 00 RN → 00
    * 01 RD → 11
    * 10 RU → 10
    * 11 RZ → 01
    * state initially holds x86 RC.
    * Convert x86 RC encoding to LoongArch FCSR RM encoding:
    *
    *   00 -> 00, 01 -> 11, 10 -> 10, 11 -> 01
    */
    IR2_OPND no_toggle = ra_alloc_label();

    la_andi(state_cmp, state, 1);
    la_beqz(state_cmp, no_toggle);
    la_xori(state, state, 2);

    la_label(no_toggle);

    la_movfcsr2gr(state_cmp, fcsr_ir2_opnd);
    la_bstrins_w(state_cmp, state, FCSR_OFF_RM + 1, FCSR_OFF_RM);
    la_movgr2fcsr(fcsr_ir2_opnd, state_cmp);

    li_wu(state, target_owner | FCSR_FLAGS_DIRTY);
    tr_store_fcsr_flags_state(state);

    ra_free_temp(state_cmp);
    ra_free_temp(state);

    tr_data->fcsr_flags_domain = domain;
}

/*
 * Prepare an x87 status word for an architectural read.
 *
 * The status shadow is the default source.  Native FCSR flags are merged
 * only when x87 owns the dirty flags; in that case the shadow becomes the
 * clean x87 owner after the merge.  This helper does not refresh TOP and
 * does not clear native FCSR flags.
 */
void prepare_x87_status(IR2_OPND sw_value)
{
    IR2_OPND fcsr_state = ra_alloc_itemp();
    IR2_OPND no_x87_dirty = ra_alloc_label();
    int status_offset = lsenv_offset_of_status_word(lsenv);

    lsassert(status_offset <= 0x7ff);

    /* Non-dirty paths use the existing x87 status shadow. */
    la_ld_h(sw_value, env_ir2_opnd, status_offset);

    tr_load_fcsr_flags_state(fcsr_state);
    la_xori(fcsr_state,
            fcsr_state,
            FCSR_FLAGS_STATE_X87_DIRTY);
    la_bnez(fcsr_state, no_x87_dirty);

    /* Only X87_DIRTY may contribute native FCSR flags to x87 status. */
    submit_x87_flags_from_fcsr(sw_value);

    li_wu(fcsr_state, FCSR_FLAGS_STATE_X87);
    tr_store_fcsr_flags_state(fcsr_state);

    la_label(no_x87_dirty);

    ra_free_temp(fcsr_state);
    lsenv->tr_data->fcsr_flags_domain = FCSR_FLAGS_DOMAIN_UNKNOWN;
}

/*
 * Prepare MXCSR for an architectural read.
 *
 * The MXCSR shadow is the default source.  Native FCSR flags are merged
 * only when SSE owns the dirty flags; after the merge the shadow becomes
 * the clean SSE owner.  This helper does not clear native FCSR flags.
 */
void prepare_sse_mxcsr(IR2_OPND mxcsr_value)
{
    IR2_OPND fcsr_state = ra_alloc_itemp();
    IR2_OPND output = ra_alloc_label();
    int mxcsr_offset = lsenv_offset_of_mxcsr(lsenv);

    lsassert(mxcsr_offset <= 0x7ff);

    la_ld_wu(mxcsr_value,
             env_ir2_opnd,
             mxcsr_offset);

    tr_load_fcsr_flags_state(fcsr_state);
    la_xori(fcsr_state,
            fcsr_state,
            FCSR_FLAGS_STATE_SSE_DIRTY);
    la_bnez(fcsr_state, output);

    /* Only SSE_DIRTY may contribute native FCSR flags to MXCSR. */
    merge_fcsr_flags(mxcsr_value);

    la_st_w(mxcsr_value,
            env_ir2_opnd,
            mxcsr_offset);
    li_wu(fcsr_state, FCSR_FLAGS_STATE_SSE);
    tr_store_fcsr_flags_state(fcsr_state);

    la_label(output);

    ra_free_temp(fcsr_state);
    lsenv->tr_data->fcsr_flags_domain = FCSR_FLAGS_DOMAIN_UNKNOWN;
}

void refresh_sw_top(IR2_OPND sw_opnd)
{
    IR2_OPND top = ra_alloc_itemp();

    /* update top */
    if(!option_softfpu) {
        la_x86mftop(top);
    } else {
        la_ld_wu(top, env_ir2_opnd, lsenv_offset_of_top(lsenv));
    }
    la_bstrins_d(sw_opnd, top, 13, 11);
    la_st_h(sw_opnd, env_ir2_opnd, lsenv_offset_of_status_word(lsenv));
    ra_free_temp(top);
}

bool translate_fnstcw(IR1_INST *pir1)
{
    /* 1. load the value of fpu control word */
    IR2_OPND cw_opnd = ra_alloc_itemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    int offset = lsenv_offset_of_control_word(lsenv);
    lsassert(offset <= 0x7ff);
    la_ld_h(cw_opnd, env_ir2_opnd, offset);

    /* 2. store the control word to the dest_opnd */

    if (ir1_opnd_is_mem(opnd0)) {
        int mem_imm;
        IR2_OPND mem_opnd = convert_mem(opnd0, &mem_imm);
        la_st_h(cw_opnd, mem_opnd, mem_imm);
    } else {
        store_ireg_to_ir1(cw_opnd, opnd0, false);
    }

    ra_free_temp(cw_opnd);
    return true;
}

bool translate_fldcw(IR1_INST *pir1)
{

    /* 1. load new control word from the source opnd(mem) */
    IR2_OPND new_cw = load_ireg_from_ir1(ir1_get_opnd(pir1, 0), UNKNOWN_EXTENSION, false);

    /* 2. store the value into the env->fpu_control_word(reg to reg)*/
    la_st_h(new_cw, env_ir2_opnd, lsenv_offset_of_control_word(lsenv));

    update_fcsr_by_cw(new_cw);
    //tr_gen_call_to_helper1((ADDR)update_fp_status, 1);

    lsenv->tr_data->fcsr_flags_domain = FCSR_FLAGS_DOMAIN_UNKNOWN;
    return true;
}

bool translate_stmxcsr(IR1_INST *pir1)
{
    IR2_OPND mxcsr_opnd = ra_alloc_itemp();

    prepare_sse_mxcsr(mxcsr_opnd);

    store_ireg_to_ir1(mxcsr_opnd, ir1_get_opnd(pir1, 0), false);

    ra_free_temp(mxcsr_opnd);
    return true;
}

bool translate_ldmxcsr(IR1_INST *pir1)
{
    /* 1. load new mxcsr value from the source opnd */
    IR2_OPND new_mxcsr =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0), UNKNOWN_EXTENSION, false);
    int mxcsr_offset = lsenv_offset_of_mxcsr(lsenv);


    IR2_OPND sw = ra_alloc_itemp();
    prepare_x87_status(sw);
    ra_free_temp(sw);

    IR2_OPND fcsr = ra_alloc_itemp();
    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrins_w(fcsr, zero_ir2_opnd,
             FCSR_OFF_FLAGS_V, FCSR_OFF_FLAGS_I);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr);

    /* 2. store the value into the env->mxcsr */
    lsassert(mxcsr_offset <= 0x7ff);
    la_st_w(new_mxcsr, env_ir2_opnd, mxcsr_offset);
    IR2_OPND fcsr_state = ra_alloc_itemp();
    li_wu(fcsr_state, FCSR_FLAGS_STATE_NONE);
    tr_store_fcsr_flags_state(fcsr_state);

    ra_free_temp(fcsr_state);
    ra_free_temp(fcsr);
    tr_gen_call_to_helper1((ADDR)update_mxcsr_status, 1,
                           LOAD_HELPER_UPDATE_MXCSR_STATUS);
    return true;
}

/* FIXME: for now JUST use 3 (bcnez + b) to implement this inst,
 * maybe it will be optimized later.
 */
static bool translate_fcomi_internal(IR1_INST *pir1, bool unordered)
{
    bool is_zpc_def =
        ir1_is_zf_def(pir1) || ir1_is_pf_def(pir1) || ir1_is_cf_def(pir1);
    bool is_osa_def =
        ir1_is_of_def(pir1) || ir1_is_sf_def(pir1) || ir1_is_af_def(pir1);
    if (is_zpc_def || is_osa_def) {
        /* calculate OF, SF and AF */
        IR2_OPND eflags_temp;
        if (is_osa_def || is_zpc_def) {
            eflags_temp = ra_alloc_itemp();
            la_x86mtflag(zero_ir2_opnd, 0x3f);
        }

        /* calculate ZF, PF, CF */
        if (is_zpc_def) {
            IR2_OPND st0 = ra_alloc_st(0);
            uint8_t index = 0;
            switch (ir1_get_opnd_num(pir1)) {
            case 1:
                index = 0;
                break;
            case 2:
                index = 1;
                break;
            default:
                lsassert(0);
            }
            IR2_OPND sti = load_freg_from_ir1_1(ir1_get_opnd(pir1, index),
                false, true);

            la_ori(eflags_temp, zero_ir2_opnd, 0xfff);

            IR2_OPND label_for_exit = ra_alloc_label();
            IR2_OPND label_for_not_unordered = ra_alloc_label();
            IR2_OPND label_for_sti_cle_st0 = ra_alloc_label();
            IR2_OPND label_for_sti_ceq_st0 = ra_alloc_label();

            /* First: if unordered, set ZF/PF/CF and exit, else jmp to label_for_not_unordered */
            if(unordered)
                la_fcmp_cond_d(fcc0_ir2_opnd, sti, st0, FCMP_COND_CUN);
            else
                la_fcmp_cond_d(fcc0_ir2_opnd, sti, st0, FCMP_COND_SUN);
            la_bceqz(fcc0_ir2_opnd, label_for_not_unordered);
            la_x86mtflag(eflags_temp, 0xb);
            la_b(label_for_exit);

            /* LABEL: label_for_not_unordered, if(st0>=sti) then jmp to label_for_sti_cle_st0 */
            la_label(label_for_not_unordered);
            la_fcmp_cond_d(fcc0_ir2_opnd, sti, st0, FCMP_COND_CLE);
            la_bcnez(fcc0_ir2_opnd, label_for_sti_cle_st0);

            /* else if (st0<st1), set CF and exit*/
            la_x86mtflag(eflags_temp, 0x1);
            la_b(label_for_exit);

            /* LABEL: label_for_sti_cle_st0 if(st0 == sti), set fcc0 then jmp to label label_for_sti_ceq_st0*/
            la_label(label_for_sti_cle_st0);
            la_fcmp_cond_d(fcc0_ir2_opnd, sti, st0, FCMP_COND_CEQ);
            la_bcnez(fcc0_ir2_opnd, label_for_sti_ceq_st0);

            /* else if (st0 > sti), ZF, PF and CF are all clear, just exit directly */
            la_b(label_for_exit);

            /* LABEL: label_for_sti_ceq_st0, set ZF if (st0 == sti) and exit*/
            la_label(label_for_sti_ceq_st0);
            la_x86mtflag(eflags_temp, 0x8);

            la_label(label_for_exit);
            ra_free_temp(eflags_temp);
        }
    }
    return true;
}

bool translate_fucomi(IR1_INST *pir1)
{
    translate_fcomi_internal(pir1, true);
    return true;
}

bool translate_fucomip(IR1_INST *pir1)
{
    translate_fcomi_internal(pir1, true);
    tr_fpu_pop();
    return true;
}

bool translate_fcomi(IR1_INST *pir1)
{
    translate_fcomi_internal(pir1, false);
    return true;
}

bool translate_fcomip(IR1_INST *pir1)
{
    translate_fcomi_internal(pir1, false);
    tr_fpu_pop();

    return true;
}

static bool translate_fcom_internal(IR1_INST *pir1, bool unordered)
{
#define C0_BIT (8)
#define C1_BIT (9)
#define C2_BIT (10)
#define C3_BIT (14)
    // int opnd_num = pir1->opnd_num;
    int opnd_num = ir1_get_opnd_num(pir1);

    IR2_OPND src;
    if (opnd_num == 0)
        src = ra_alloc_st(1);
    else {
            uint8_t index = 0;
            switch (ir1_get_opnd_num(pir1)) {
            case 1:
                index = 0;
                break;
            case 2:
                index = 1;
                break;
            default:
                lsassert(0);
            }
      src = load_freg_from_ir1_1(ir1_get_opnd(pir1, index), false, true);
    }
    IR2_OPND st0 = ra_alloc_st(0);
    IR2_OPND sw_opnd = ra_alloc_itemp();
    IR2_OPND n4095_opnd = ra_alloc_num_4095();

    /* load status_word */
    int offset = lsenv_offset_of_status_word(lsenv);
    lsassert(offset <= 0x7ff);
    la_ld_h(sw_opnd, env_ir2_opnd, offset);

    /* clear status_word c0 c1 c2 c3 */
    la_bstrins_d(sw_opnd, zero_ir2_opnd, C0_BIT, C0_BIT);
    la_bstrins_d(sw_opnd, zero_ir2_opnd, C1_BIT, C1_BIT);
    la_bstrins_d(sw_opnd, zero_ir2_opnd, C2_BIT, C2_BIT);
    la_bstrins_d(sw_opnd, zero_ir2_opnd, C3_BIT, C3_BIT);

    IR2_OPND label_for_exit = ra_alloc_label();
    IR2_OPND label_for_not_unordered = ra_alloc_label();
    IR2_OPND label_for_src_cle_st0 = ra_alloc_label();
    IR2_OPND label_for_src_ceq_st0 = ra_alloc_label();

    /* First: if unordered, set C0/C2/C3 and exit, else jmp to label_for_not_unordered */
    if(unordered)
        la_fcmp_cond_d(fcc0_ir2_opnd, src, st0, FCMP_COND_CUN);
    else
        la_fcmp_cond_d(fcc0_ir2_opnd, src, st0, FCMP_COND_SUN);
    la_bceqz(fcc0_ir2_opnd, label_for_not_unordered);
    la_bstrins_d(sw_opnd, n4095_opnd, C0_BIT, C0_BIT);
    la_bstrins_d(sw_opnd, n4095_opnd, C2_BIT, C2_BIT);
    la_bstrins_d(sw_opnd, n4095_opnd, C3_BIT, C3_BIT);
    la_b(label_for_exit);

    /* LABEL: label_for_not_unordered, if(st0>=src) then jmp to label_for_src_cle_st0 */
    la_label(label_for_not_unordered);
    la_fcmp_cond_d(fcc0_ir2_opnd, src, st0, FCMP_COND_CLE);
    la_bcnez(fcc0_ir2_opnd, label_for_src_cle_st0);

    /* else if (st0<st1), set CF and exit*/
    la_bstrins_d(sw_opnd, n4095_opnd, C0_BIT, C0_BIT);
    la_b(label_for_exit);

    /* LABEL: label_for_src_cle_st0 if(st0 == src), set fcc0 then jmp to label label_for_src_ceq_st0*/
    la_label(label_for_src_cle_st0);
    la_fcmp_cond_d(fcc0_ir2_opnd, src, st0, FCMP_COND_CEQ);
    la_bcnez(fcc0_ir2_opnd, label_for_src_ceq_st0);

    /* else if (st0 > sti), ZF, PF and CF are all clear, just exit directly */
    la_b(label_for_exit);

    /* LABEL: label_for_src_ceq_st0, set ZF if (st0 == src) and exit*/
    la_label(label_for_src_ceq_st0);
    la_bstrins_d(sw_opnd, n4095_opnd, C3_BIT, C3_BIT);

    la_label(label_for_exit);

    la_st_h(sw_opnd, env_ir2_opnd, offset);
    ra_free_num_4095(n4095_opnd);
    ra_free_temp(sw_opnd);

    return true;
}

bool translate_fcom(IR1_INST *pir1)
{
    translate_fcom_internal(pir1, false);

    return true;
}

bool translate_fcomp(IR1_INST *pir1)
{
    translate_fcom_internal(pir1, false);
    tr_fpu_pop();

    return true;
}

bool translate_fucom(IR1_INST *pir1)
{
    translate_fcom_internal(pir1, true);
    return true;
}

bool translate_fucomp(IR1_INST *pir1)
{
    translate_fcom_internal(pir1, true);
    tr_fpu_pop();

    return true;
}

bool translate_fcompp(IR1_INST *pir1)
{
    translate_fcom_internal(pir1, false);
    tr_fpu_pop();
    tr_fpu_pop();

    return true;
}

bool translate_fucompp(IR1_INST *pir1)
{
    translate_fcom_internal(pir1, true);
    tr_fpu_pop();
    tr_fpu_pop();

    return true;
}

bool translate_ficom(IR1_INST *pir1)
{
#ifdef CONFIG_LATX_TU
    return false;
#endif
    assert(0 && "translate_ficom need to implemented correctly");
    IR2_OPND st0_opnd = ra_alloc_st(0);
    IR2_OPND mint_opnd = ra_alloc_ftemp();
    IR2_OPND tmp_opnd = ra_alloc_itemp();
    load_freg_from_ir1_2(mint_opnd, ir1_get_opnd(pir1, 0), 0);
    la_ffint_d_l(mint_opnd, mint_opnd);
    IR2_OPND sw_opnd = ra_alloc_itemp();

    /* load status_word */
    int offset = lsenv_offset_of_status_word(lsenv);

    lsassert(offset <= 0x7ff);
    la_ld_hu(sw_opnd, env_ir2_opnd, offset);

    /* clear status_word c0 c2 c3 */

    la_lu12i_w(tmp_opnd, 0xb);
    la_ori(tmp_opnd, tmp_opnd, 0xaff);
    la_and(sw_opnd, sw_opnd, tmp_opnd);

    IR2_OPND label_un = ra_alloc_label();
    IR2_OPND label_eq = ra_alloc_label();
    IR2_OPND label_lt = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    /* check is unordered */
    la_fcmp_cond_d(fcc0_ir2_opnd, st0_opnd, mint_opnd, FCMP_COND_CUN);
    la_bcnez(fcc0_ir2_opnd, label_un);
    /* check is equal */
    la_fcmp_cond_d(fcc0_ir2_opnd, st0_opnd, mint_opnd, FCMP_COND_CEQ);
    la_bcnez(fcc0_ir2_opnd, label_eq);
    /* check is less than */
    la_fcmp_cond_d(fcc0_ir2_opnd, st0_opnd, mint_opnd, FCMP_COND_CLT);
    la_bcnez(fcc0_ir2_opnd, label_lt);

    la_b(label_exit);
    /* lt: */
    la_label(label_lt);
    la_ori(sw_opnd, sw_opnd, 0x100);
    la_b(label_exit);
    /* eq: */
    la_label(label_eq);

    la_lu12i_w(tmp_opnd, 0x4);
    la_or(sw_opnd, sw_opnd, tmp_opnd);

    la_b(label_exit);
    /* un: */
    la_label(label_un);

    la_lu12i_w(tmp_opnd, 0x4);
    la_ori(tmp_opnd, tmp_opnd, 0x500);
    la_or(sw_opnd, sw_opnd, tmp_opnd);

    /* exit: */
    la_label(label_exit);
    /* append_ir2_opnd2i(mips_sh, sw_opnd, mem_opnd); */
    ra_free_temp(sw_opnd);
    ra_free_temp(mint_opnd);
    ra_free_temp(tmp_opnd);

    return true;
}

bool translate_ficomp(IR1_INST *pir1)
{
    translate_ficom(pir1);
    tr_fpu_pop();
    return true;
}

bool translate_ud2(IR1_INST *pir1)
{
    la_dbar(0);
    return translate_invalid(pir1);
}

/**
 * Load tag word(16-bit) to env, there are 4 cases for each st(i):
 * 00: Valid
 * 01: Zero
 * 10: Special: invalid (NaN, unsupported), infinity, or denormal
 * 11: Empty
 *
 * There is a corresponding (uint8_t)fptags[i] for each sti, for now we
 * can only support 2 cases:
 * (uint8_t)fptags[i] = 0x0: Valid
 * (uint8_t)fptags[i] = 0x1: Empty
 */
void tr_fpu_load_tag_to_env(IR2_OPND fpu_tag)
{
    IR2_OPND temp_1 = ra_alloc_itemp();
    IR2_OPND temp_2 = ra_alloc_itemp();
    IR2_OPND temp_3 = ra_alloc_itemp();

    /**
     * Every 2 bits for each st(i) in fpu_tag:
     * 00 ==> 0&0 = 0 ==> Valid
     * 01 ==> 0&1 = 0 ==> Valid
     * 10 ==> 1&0 = 0 ==> Valid
     * 11 ==> 1&1 = 1 ==> Empty
     */
    la_srli_d(temp_1, fpu_tag, 1);
    la_and(temp_1, fpu_tag, temp_1);
    for (int i = 0; i < 8; ++i) {
        la_bstrpick_d(temp_2, temp_1, i * 2, i * 2);
        la_bstrins_d(temp_3, temp_2, (i * 8 + 7), (i * 8));
    }

    /* store FPU tag to env */
    int tag_offset = lsenv_offset_of_tag_word(lsenv);
    lsassert(tag_offset <= 0x7ff);
    la_st_d(temp_3, env_ir2_opnd, tag_offset);

    ra_free_temp(temp_1);
    ra_free_temp(temp_2);
    ra_free_temp(temp_3);
}

void tr_fpu_store_tag_to_mem(IR2_OPND mem_opnd, int mem_imm)
{
    IR2_OPND fpu_tag = ra_alloc_itemp();
    IR2_OPND temp_1 = ra_alloc_itemp();
    IR2_OPND temp_2 = ra_alloc_itemp();

    int tag_offset = lsenv_offset_of_tag_word(lsenv);
    lsassert(tag_offset <= 0x7ff);
    la_ld_d(fpu_tag, env_ir2_opnd, tag_offset);
    la_slli_d(temp_1, fpu_tag, 1);
    la_or(fpu_tag, fpu_tag, temp_1);

    for (int i = 0; i < 8; i++) {
        la_bstrpick_d(temp_1, fpu_tag, (8 * i + 1), (8 * i));
        la_bstrins_d(temp_2, temp_1, (2 * i + 1), (2 * i));
    }
#ifndef TARGET_X86_64
    /*in 32 situation, mem_opnd must to clear hight 32bit*/
    la_bstrpick_d(mem_opnd, mem_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(mem_opnd, mem_opnd, 31, 0);
    }
#endif
    la_st_h(temp_2, mem_opnd, mem_imm);

    ra_free_temp(temp_1);
    ra_free_temp(temp_2);
    ra_free_temp(fpu_tag);
}

/**
 * Mark the ST(i)'s corresponding tag field in env as empty by top and tag word
 */
bool translate_ffree(IR1_INST *pir1)
{
    IR2_OPND fpu_tag = ra_alloc_itemp();
    IR2_OPND top_opnd = ra_alloc_itemp();
    IR2_OPND temp1 = ra_alloc_itemp();
    IR2_OPND temp2 = ra_alloc_itemp();
    IR1_OPND *sti_reg = ir1_get_opnd(pir1, 0);

    /* get TOP */
    la_x86mftop(top_opnd);

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

    return true;
}
bool translate_ffreep(IR1_INST *pir1)
{
    translate_ffree(pir1);
    tr_fpu_pop();
    return true;
}

bool translate_fldenv(IR1_INST *pir1)
{
    IR2_OPND value = ra_alloc_itemp();

    /* mem_opnd is not supported in ir2 assemble */
    /* convert mem_opnd to ireg_opnd */

    IR1_OPND* opnd1 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd1);
    ir2_set_opnd_type(&mem_opnd, IR2_OPND_GPR);

    /* store float control register at env memory */
    la_ld_h(value, mem_opnd, 0);
    int control_offset = lsenv_offset_of_control_word(lsenv);
    lsassert(control_offset <= 0x7ff);
    la_st_h(value, env_ir2_opnd, control_offset);
    update_fcsr_by_cw(value);

    /* store float status register at env memory */
    la_ld_h(value, mem_opnd, 4);
    int status_offset = lsenv_offset_of_status_word(lsenv);
    lsassert(status_offset <= 0x7ff);
    la_st_h(value, env_ir2_opnd, status_offset);
    update_fcsr_by_sw(value);

    /* get top value */
    la_srli_w(value, value, 11);
    la_andi(value, value, 7);

    /* set fpstt */
    int top_offset = lsenv_offset_of_top(lsenv);
    lsassert(top_offset <= 0x7ff);
    la_st_h(value, env_ir2_opnd, top_offset);
    tr_load_top_from_env();

    //tr_gen_call_to_helper1((ADDR)update_fp_status, 1);

    /* load FPU tag word from memory to env*/
    la_ld_h(value, mem_opnd, 8);
    tr_fpu_load_tag_to_env(value);
    /* dispose tag word */
    ra_free_temp(value);
    lsenv->tr_data->fcsr_flags_domain = FCSR_FLAGS_DOMAIN_UNKNOWN;
    return true;
}

bool translate_fnstenv(IR1_INST *pir1)
{
    IR2_OPND cw_value = ra_alloc_itemp();
    IR2_OPND fcsr_value = ra_alloc_itemp();
    IR2_OPND temp = ra_alloc_itemp();
    IR2_OPND sw_value = ra_alloc_itemp();
    li_wu(temp, 0xffff0000ULL);

    /* mem_opnd is not supported in ir2 assemble */
    /* convert mem_opnd to ireg_opnd */
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd1);
    ir2_set_opnd_type(&mem_opnd, IR2_OPND_GPR);

    int control_offset = lsenv_offset_of_control_word(lsenv);
    lsassert(control_offset <= 0x7ff);
    la_ld_h(cw_value, env_ir2_opnd, control_offset);

    la_or(cw_value, temp, cw_value);
    la_st_w(cw_value, mem_opnd, 0);

    /* Mask all floating-point exceptions */
    la_ori(cw_value, cw_value, X87_CR_EXCP_MASK);
    la_st_h(cw_value, env_ir2_opnd, control_offset);
    ra_free_temp(cw_value);
    /* Disable FCSR VZOUI accordingly */
    la_movfcsr2gr(fcsr_value , fcsr_ir2_opnd);
    la_bstrins_w(fcsr_value, zero_ir2_opnd,
                            FCSR_OFF_EN_V, FCSR_OFF_EN_I);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_value);
    ra_free_temp(fcsr_value);

    prepare_x87_status(sw_value);
    refresh_sw_top(sw_value);
    la_or(sw_value, temp, sw_value);
    la_st_w(sw_value, mem_opnd, 4);

    /* store FPU tag word to memory */
    tr_fpu_store_tag_to_mem(mem_opnd, 8);

    la_st_w(temp, mem_opnd, 24);

    ra_free_temp(temp);
    ra_free_temp(sw_value);
    return true;
}

static float float_max = FLT_MAX;
static float float_min = FLT_MIN;
static float float_1e32 = 1.0e32f;
static float float_2 = 2.0f;
static float float_3 = 3.0f;

bool translate_wait(IR1_INST *pir1)
{
    IR2_OPND temp = ra_alloc_itemp();
    IR2_OPND sw_opnd = ra_alloc_itemp();
    IR2_OPND ftemp1_opnd = ra_alloc_ftemp();
    IR2_OPND ftemp2_opnd = ra_alloc_ftemp();
    IR2_OPND value_addr_opnd = ra_alloc_itemp();

    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_O_exit = ra_alloc_label();
    IR2_OPND label_U_exit = ra_alloc_label();
    la_ld_h(temp, env_ir2_opnd,
                            lsenv_offset_of_control_word(lsenv));
    la_ld_h(sw_opnd, env_ir2_opnd,
                            lsenv_offset_of_status_word(lsenv));
    la_andn(sw_opnd, sw_opnd, temp);
    la_andi(sw_opnd, sw_opnd, 0x3d); /* PUOZ_I */

    la_beq(sw_opnd, zero_ir2_opnd, label_exit);
    /* raise umasked float exceptions */
    /* Invalid-arithmetic-operand exception */
    /* TODO */
    /* Divide-By-Zero Exception */
    /* TODO */
    /* Numeric Overflow Exception */
    la_bstrpick_w(temp, sw_opnd,
                            X87_SR_OFF_OE, X87_SR_OFF_OE);
    la_beq(temp, zero_ir2_opnd, label_O_exit);
    li_d(value_addr_opnd, (uint64_t)&float_max);
    la_fld_s(ftemp1_opnd, value_addr_opnd, 0);
    li_d(value_addr_opnd, (uint64_t)&float_1e32);
    la_fld_s(ftemp2_opnd, value_addr_opnd, 0);
    la_fadd_s(ftemp1_opnd, ftemp1_opnd, ftemp2_opnd);
    la_label(label_O_exit);

    /* Numeric Underflow Exception */
    la_bstrpick_w(temp, sw_opnd,
                            X87_SR_OFF_UE, X87_SR_OFF_UE);
    la_beq(temp, zero_ir2_opnd, label_U_exit);
    li_d(value_addr_opnd, (uint64_t)&float_min);
    la_fld_s(ftemp1_opnd, value_addr_opnd, 0);
    li_d(value_addr_opnd, (uint64_t)&float_3);
    la_fld_s(ftemp2_opnd, value_addr_opnd, 0);
    la_fdiv_s(ftemp1_opnd, ftemp1_opnd, ftemp2_opnd);
    la_label(label_U_exit);

    /* Inexact-Result (Precision) Exception */
    la_bstrpick_w(temp, sw_opnd,
                            X87_SR_OFF_PE, X87_SR_OFF_PE);
    la_beq(temp, zero_ir2_opnd, label_exit);
    li_d(value_addr_opnd, (uint64_t)&float_2);
    la_fld_s(ftemp1_opnd, value_addr_opnd, 0);
    li_d(value_addr_opnd, (uint64_t)&float_3);
    la_fld_s(ftemp2_opnd, value_addr_opnd, 0);
    la_fdiv_s(ftemp1_opnd, ftemp1_opnd, ftemp2_opnd);

    la_label(label_exit);

    ra_free_temp(value_addr_opnd);
    ra_free_temp(temp);
    ra_free_temp(sw_opnd);
    ra_free_temp(ftemp1_opnd);
    ra_free_temp(ftemp2_opnd);
    return true;
}

bool translate_fnclex(IR1_INST *pir1) {
    /* get status word and load in sw_value */
    IR2_OPND sw_value = ra_alloc_itemp();
    IR2_OPND fcsr0 = ra_alloc_itemp();
    prepare_sse_mxcsr(fcsr0);
    int offset = lsenv_offset_of_status_word(lsenv);
    la_ld_hu(sw_value, env_ir2_opnd, offset);
    la_bstrins_d(sw_value, zero_ir2_opnd, X87_SR_OFF_ES, X87_SR_OFF_IE);
    la_bstrins_d(sw_value, zero_ir2_opnd, X87_SR_OFF_B, X87_SR_OFF_B);
    clear_native_fcsr_flags(fcsr0);
    la_st_h(sw_value, env_ir2_opnd,
                        lsenv_offset_of_status_word(lsenv));
    ra_free_temp(sw_value);
    ra_free_temp(fcsr0);

    IR2_OPND fcsr_state = ra_alloc_itemp();
    li_wu(fcsr_state, FCSR_FLAGS_STATE_NONE);
    tr_store_fcsr_flags_state(fcsr_state);
    ra_free_temp(fcsr_state);
    lsenv->tr_data->fcsr_flags_domain = FCSR_FLAGS_DOMAIN_UNKNOWN;
    return true;
}

bool translate_fninit(IR1_INST *pir1) {
    IR2_OPND temp = ra_alloc_itemp();
    IR2_OPND fcsr0 = ra_alloc_itemp();
    IR2_OPND fcsr_state = ra_alloc_itemp();
    int offset;

    prepare_sse_mxcsr(temp);
    /* clear status and set control*/
    offset = lsenv_offset_of_status_word(lsenv);
    lsassert(offset <= 0x7ff);
    li_wu(temp, 0x37f0000ULL);
    la_st_w(temp, env_ir2_opnd, offset);

    /* clear top */
    la_x86mttop(0);
    la_x86settm();

    /* set fptags */
    offset = lsenv_offset_of_tag_word(lsenv);
    lsassert(offset <= 0x7ff);
    li_d(temp, 0x101010101010101ULL);
    la_st_d(temp, env_ir2_opnd, offset);

    la_movfcsr2gr(fcsr0, fcsr_ir2_opnd);
    /* VZOUI disable */
    li_wu(temp, FCSR_ENABLE_CLEAR);
    la_and(fcsr0, fcsr0, temp);
    /* RM = 00 */
    li_wu(temp, FCSR_RM_CLEAR);
    la_and(fcsr0, fcsr0, temp);
    /* clear native sticky flags FCSR[20:16] */
    la_bstrins_w(fcsr0, zero_ir2_opnd,
             FCSR_OFF_FLAGS_V, FCSR_OFF_FLAGS_I);
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr0);
    li_wu(fcsr_state, FCSR_FLAGS_STATE_NONE);
    tr_store_fcsr_flags_state(fcsr_state);

    ra_free_temp(temp);
    ra_free_temp(fcsr0);
    ra_free_temp(fcsr_state);
    lsenv->tr_data->fcsr_flags_domain = FCSR_FLAGS_DOMAIN_UNKNOWN;
    return true;
}
