/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __COMPLEX_T__H_
#define __COMPLEX_T__H_

typedef struct complex_s { double r; double i;} complex_t;
typedef struct complexf_s { float r; float i;} complexf_t;

#endif //__COMPLEX_T__H_

#define COMPLEX_IMPL
#ifdef COMPLEX_IMPL
#ifndef __COMPLEX_T_IMPL_H_
#define __COMPLEX_T_IMPL_H_
static inline complexf_t to_complexf(int i) {
    complexf_t ret;
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    ret.r = cpu->xmm_regs->ZMM_S(i);
    ret.i = cpu->xmm_regs->ZMM_S(i+1);
    return ret;
}
static inline complex_t to_complex(int i) {
    complex_t ret;
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    ret.r = cpu->xmm_regs->ZMM_D(i);
    ret.i = cpu->xmm_regs->ZMM_D(i+1);
    return ret;
}
static inline void from_complexf(complexf_t v) {
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    cpu->xmm_regs->ZMM_S(0)=v.r;
    cpu->xmm_regs->ZMM_S(1)=v.i;
}
static inline void from_complex(complex_t v) {
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    cpu->xmm_regs->ZMM_D(0)=v.r;
    cpu->xmm_regs->ZMM_D(1)=v.i;
}

static inline void fpush(CPUX86State *env)
{
    env->fpstt = (env->fpstt - 1) & 7;
    env->fptags[env->fpstt] = 0; /* validate stack entry */
}

static inline void fpop(CPUX86State *env)
{
    env->fptags[env->fpstt] = 1; /* invalidate stack entry */
    env->fpstt = (env->fpstt + 1) & 7;
}

#endif // __COMPLEX_T_IMPL_H_
#endif  // COMPLEX_IMPL
