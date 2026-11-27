/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "lsenv.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "fpu/softfloat.h"
#include "translate.h"

/* Private host-stack frame; preserve temporaries as well as guest mappings. */
typedef struct LatxFcvtFrame {
    uint64_t gpr[32];
    uint64_t fpr[32][4];
    uint64_t fcsr;
    uint64_t fcc[8];
} LatxFcvtFrame;

void latx_fcvt_soft(void *opaque, unsigned int operation,
                    unsigned int dest, unsigned int src, unsigned int high)
{
    LatxFcvtFrame *frame = opaque;
    CPUX86State *env = (CPUX86State *)lsenv->cpu_state;
    float_status status = env->fp_status;
    static const int rounding[] = {
        float_round_nearest_even, float_round_to_zero,
        float_round_up, float_round_down,
    };
    floatx80 extended;
    unsigned int flags, exceptions = 0;

    set_float_rounding_mode(rounding[(frame->fcsr >> 8) & 3], &status);
    set_float_exception_flags(0, &status);
    if (operation == 0) {
        extended.low = frame->fpr[src][0];
        extended.high = frame->fpr[high][0];
        frame->fpr[dest][0] = floatx80_to_float64(extended, &status);
    } else {
        extended = float64_to_floatx80(frame->fpr[src][0], &status);
        frame->fpr[dest][0] = operation == 1 ? extended.low : extended.high;
    }
    flags = get_float_exception_flags(&status);
    if (flags & float_flag_inexact) { exceptions |= 1; }
    if (flags & float_flag_underflow) { exceptions |= 2; }
    if (flags & float_flag_overflow) { exceptions |= 4; }
    if (flags & float_flag_divbyzero) { exceptions |= 8; }
    if (flags & float_flag_invalid) { exceptions |= 16; }
    frame->fcsr = (frame->fcsr & ~(UINT64_C(31) << 24)) |
                  ((uint64_t)exceptions << 24) |
                  ((uint64_t)exceptions << 16);
}

static void latx_emit_fcvt_soft(unsigned int operation, IR2_OPND dest,
                                IR2_OPND src, IR2_OPND high)
{
    const int frame_size = (sizeof(LatxFcvtFrame) + 31) & ~31;
    IR2_OPND scratch = a0_ir2_opnd;

    /* LASX state needs 32-byte alignment even when the incoming ABI stack
     * is only 16-byte aligned. Save the original stack and scratch register.
     */
    la_addi_d(sp_ir2_opnd, sp_ir2_opnd, -frame_size - 64);
    la_st_d(scratch, sp_ir2_opnd, 0);
    la_mov64(scratch, sp_ir2_opnd);
    la_bstrins_d(sp_ir2_opnd, zero_ir2_opnd, 4, 0);
    la_st_d(scratch, sp_ir2_opnd, 3 * 8);
    la_ld_d(scratch, scratch, 0);
    for (int i = 1; i < 32; i++) {
        if (i != 3) {
            la_st_d(ir2_opnd_new(IR2_OPND_GPR, i), sp_ir2_opnd, i * 8);
        }
    }
    for (int i = 0; i < 32; i++) {
        IR2_OPND reg = ir2_opnd_new(IR2_OPND_FPR, i);
        if (option_enable_lasx) {
            la_xvst(reg, sp_ir2_opnd, 256 + i * 32);
        } else {
            la_vst(reg, sp_ir2_opnd, 256 + i * 32);
        }
    }
    la_movfcsr2gr(scratch, fcsr_ir2_opnd);
    la_st_d(scratch, sp_ir2_opnd, offsetof(LatxFcvtFrame, fcsr));
    for (int i = 0; i < 8; i++) {
        la_movcf2gr(scratch, ir2_opnd_new(IR2_OPND_CC, i));
        la_st_d(scratch, sp_ir2_opnd, offsetof(LatxFcvtFrame, fcc) + i * 8);
    }
    la_mov64(a0_ir2_opnd, sp_ir2_opnd);
    li_d(a1_ir2_opnd, operation);
    li_d(a2_ir2_opnd, ir2_opnd_base_reg_num(&dest));
    li_d(ir2_opnd_new(IR2_OPND_GPR, 7), ir2_opnd_base_reg_num(&src));
    li_d(ir2_opnd_new(IR2_OPND_GPR, 8), ir2_opnd_base_reg_num(&high));
    TranslationBlock *tb __attribute__((unused)) =
        (TranslationBlock *)lsenv->tr_data->curr_tb;
    IR2_OPND function = ir2_opnd_new(IR2_OPND_GPR, 12);
    aot_load_host_addr(function, (ADDR)latx_fcvt_soft,
                       LOAD_HELPER_FCVT_SOFT, 0);
    la_jirl(ra_ir2_opnd, function, 0);
    for (int i = 0; i < 8; i++) {
        la_ld_d(scratch, sp_ir2_opnd, offsetof(LatxFcvtFrame, fcc) + i * 8);
        la_movgr2cf(ir2_opnd_new(IR2_OPND_CC, i), scratch);
    }
    la_ld_d(scratch, sp_ir2_opnd, offsetof(LatxFcvtFrame, fcsr));
    la_movgr2fcsr(fcsr_ir2_opnd, scratch);
    for (int i = 0; i < 32; i++) {
        IR2_OPND reg = ir2_opnd_new(IR2_OPND_FPR, i);
        if (option_enable_lasx) {
            la_xvld(reg, sp_ir2_opnd, 256 + i * 32);
        } else {
            la_vld(reg, sp_ir2_opnd, 256 + i * 32);
        }
    }
    for (int i = 1; i < 32; i++) {
        if (i != 3) {
            la_ld_d(ir2_opnd_new(IR2_OPND_GPR, i), sp_ir2_opnd, i * 8);
        }
    }
    la_ld_d(sp_ir2_opnd, sp_ir2_opnd, 3 * 8);
    la_addi_d(sp_ir2_opnd, sp_ir2_opnd, frame_size + 64);
}

void latx_fcvt_d_ld(IR2_OPND dest, IR2_OPND low, IR2_OPND high)
{
    if (option_enable_lbt) {
        la_fcvt_d_ld(dest, low, high);
    } else {
        latx_emit_fcvt_soft(0, dest, low, high);
    }
}

void latx_fcvt_ld_d(IR2_OPND dest, IR2_OPND src)
{
    if (option_enable_lbt) {
        la_fcvt_ld_d(dest, src);
    } else {
        latx_emit_fcvt_soft(1, dest, src, src);
    }
}

void latx_fcvt_ud_d(IR2_OPND dest, IR2_OPND src)
{
    if (option_enable_lbt) {
        la_fcvt_ud_d(dest, src);
    } else {
        latx_emit_fcvt_soft(2, dest, src, src);
    }
}
