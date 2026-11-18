/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "cpu.h"
#include "tcg/helper-tcg.h"
#include "exec/cpu_ldst.h"
#include "latx-options.h"
#include "translate.h"
#include "wow64-x86-state.h"

static uint32_t segment_flags(bool code)
{
    uint32_t flags = DESC_G_MASK | DESC_B_MASK | DESC_P_MASK | DESC_S_MASK |
                     DESC_DPL_MASK | DESC_A_MASK;

    return flags | (code ? DESC_CS_MASK | DESC_R_MASK : DESC_W_MASK);
}

static void set_segment_descriptor(CPUX86State *env, uint16_t selector,
                                   uint32_t base)
{
    uint32_t *descriptor;
    uint32_t offset = selector & ~7u;

    if (!selector || (selector & 4) || offset + 7 > env->gdt.limit) {
        return;
    }
    descriptor = g2h_untagged(env->gdt.base + offset);
    stl_le_p(descriptor, (base << 16) | 0xffff);
    stl_le_p(descriptor + 1, ((base >> 16) & 0xff) | (base & 0xff000000) |
                           0xf0000 | segment_flags(false));
}

void lat_wow64_context_to_cpu(CPUX86State *env,
                                       const LatWow64Context *context)
{
    uint8_t fxsave[512] QEMU_ALIGNED(16);
    unsigned int i;

    memset(env->regs, 0, sizeof(env->regs));
    for (i = 0; i < LAT_WOW64_REGISTER_COUNT; ++i) {
        env->regs[i] = context->gpr[i];
    }
    env->eip = context->eip;
    env->efer &= ~(MSR_EFER_LMA | MSR_EFER_LME);
    env->hflags &= ~(HF_LMA_MASK | HF_CS64_MASK);
    cpu_x86_load_seg_cache(env, R_CS, context->cs, 0, UINT32_MAX,
                           segment_flags(true));
    cpu_x86_load_seg_cache(env, R_SS, context->ss, 0, UINT32_MAX,
                           segment_flags(false));
    cpu_x86_load_seg_cache(env, R_DS, context->ds, 0, UINT32_MAX,
                           segment_flags(false));
    cpu_x86_load_seg_cache(env, R_ES, context->es, 0, UINT32_MAX,
                           segment_flags(false));
    cpu_x86_load_seg_cache(env, R_FS, context->fs, context->fs_base,
                           UINT32_MAX, segment_flags(false));
    cpu_x86_load_seg_cache(env, R_GS, context->gs, context->gs_base,
                           UINT32_MAX, segment_flags(false));
    set_segment_descriptor(env, context->fs, context->fs_base);
    set_segment_descriptor(env, context->gs, context->gs_base);
    cpu_load_eflags(env, context->eflags, UINT32_MAX);
    env->dr[0] = context->dr0;
    env->dr[1] = context->dr1;
    env->dr[2] = context->dr2;
    env->dr[3] = context->dr3;
    env->dr[6] = context->dr6;
    env->dr[7] = context->dr7 | DR7_FIXED_1;
    memcpy(fxsave, context->fxsave, sizeof(fxsave));
    cpu_x86_fxrstor(env, h2g(fxsave));
    env->fpop = lduw_le_p(fxsave + 6);
    env->fpip = ldl_le_p(fxsave + 8);
    env->fpdp = ldl_le_p(fxsave + 16);
    cpu_x86_sync_latx_fcsr(env);
    if (latx_softfpu_sse_sync_enabled()) {
        /* SoftFloat owns x87 flags. Native FCSR carries only SSE flags. */
        env->fcsr &= ~(0x1fu << 16);
    }
    cpu_x86_sync_latx_fpu_mode(env);
    if (!option_softfpu && env->mode_fpu) {
        convert_fpregs_x80_to_64();
    }
}

static uint32_t lat_wow64_fcsr_exceptions(uint32_t fcsr)
{
    uint32_t flags = (fcsr >> 16) & 0x1f;

    /* LoongArch I/U/O/Z/V become x86 PE/UE/OE/ZE/IE. */
    return ((flags & 1) << 5) | ((flags & 2) << 3) |
           ((flags & 4) << 1) | ((flags & 8) >> 1) |
           ((flags & 16) >> 4);
}

void lat_wow64_context_from_cpu(LatWow64Context *context,
                                         CPUX86State *env, int trap,
                                         uint32_t float_status)
{
    uint8_t fxsave[512] QEMU_ALIGNED(16) = { 0 };
    unsigned int i;

    for (i = 0; i < LAT_WOW64_REGISTER_COUNT; ++i) {
        context->gpr[i] = env->regs[i];
    }
    context->eip = env->eip;
    context->eflags = cpu_compute_eflags(env);
    context->cs = env->segs[R_CS].selector;
    context->ss = env->segs[R_SS].selector;
    context->ds = env->segs[R_DS].selector;
    context->es = env->segs[R_ES].selector;
    context->fs = env->segs[R_FS].selector;
    context->gs = env->segs[R_GS].selector;
    context->fs_base = env->segs[R_FS].base;
    context->gs_base = env->segs[R_GS].base;
    context->dr0 = env->dr[0];
    context->dr1 = env->dr[1];
    context->dr2 = env->dr[2];
    context->dr3 = env->dr[3];
    context->dr6 = env->dr[6];
    context->dr7 = env->dr[7];
    uint32_t fcsr = env->fcsr;
    if (trap == EXCP10_COPR || trap == 19) {
        /* An unmasked fault can set Cause without setting sticky Flags. */
        fcsr |= (fcsr >> 8) & (0x1fu << 16);
    }
    if (trap == 19) {
        env->mxcsr |= lat_wow64_fcsr_exceptions(fcsr) | float_status;
    } else if (!option_softfpu) {
        env->fpus |= lat_wow64_fcsr_exceptions(fcsr) | float_status;
        if (float_status) {
            env->fpus |= 0x8080; /* x87 ES and B. */
            env->fpip = env->eip;
        }
    } else if (latx_softfpu_sse_sync_enabled()) {
        env->mxcsr |= lat_wow64_fcsr_exceptions(env->fcsr);
    }
    /* Native x87 registers contain doubles; the public context contains
     * architectural 80-bit values.  MMX keeps its raw integer payload. */
    if (!option_softfpu && env->mode_fpu) {
        convert_fpregs_64_to_x80();
    }
    cpu_x86_canonicalize_latx_mmx_state(env);
    cpu_x86_fxsave(env, h2g(fxsave));
    stw_le_p(fxsave + 6, env->fpop);
    stl_le_p(fxsave + 8, env->fpip);
    stl_le_p(fxsave + 16, env->fpdp);
    memcpy(context->fxsave, fxsave, sizeof(fxsave));
}

/* The restored guest instruction distinguishes x87 (#MF) from SSE (#XM). */
int lat_wow64_float_trap(CPUX86State *env, int trap)
{
    if (trap != EXCP10_COPR) {
        return trap;
    }
    mmap_lock();
    for (unsigned int i = 0; i < 15; ++i) {
        uint32_t pc = env->eip + i;
        uint8_t opcode;

        if (!(page_get_flags(pc) & PAGE_READ)) {
            break;
        }
        opcode = ldub_p(g2h_untagged(pc));
        switch (opcode) {
        case 0x26: case 0x2e: case 0x36: case 0x3e: case 0x64: case 0x65:
        case 0x66: case 0x67: case 0xf0: case 0xf2: case 0xf3:
            continue;
        case 0x0f: case 0xc4: case 0xc5:
            trap = 19; /* SIMD floating-point exception. */
            break;
        default:
            break;
        }
        break;
    }
    mmap_unlock();
    return trap;
}
