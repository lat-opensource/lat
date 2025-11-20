#ifndef _LSENV_H_
#define _LSENV_H_

#include "env.h"
#include "qemu-def.h"

extern __thread ENV *lsenv;
#define CODEIS64 (((CPUX86State *)lsenv->cpu_state)->sys.codemode)
//#define CODEIS64 (assert(((CPUX86State *)lsenv->cpu_state)->sys.codemode),1)
#define LATX_DT_X86    0
#define LATX_DT_X64    1

/* func to access ENV's attributes */
static inline int lsenv_offset_of_mips_regs(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->mips_regs[i]) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_gpr(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->regs[i]) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_all_gpr(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->mips_regs[i]) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_eflags(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->eflags) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_fcsr(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->fcsr) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_ibtc_table(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->ibtc_table_p) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_tb_jmp_cache_ptr(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->tb_jmp_cache_ptr) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_eip(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->eip) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_top(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->fpstt) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_imm_value(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->imm_cached_value[i]) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_cpu_index(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    CPUState *cpu_state = env_cpu(cpu);
    return (int)((ADDR)(&cpu_state->cpu_index) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_cpu_running(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    CPUState *cpu_state = env_cpu(cpu);
    return (int)((ADDR)(&cpu_state->running) - (ADDR)lsenv->cpu_state);
}

/* FPU top */
static inline int lsenv_get_top(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return cpu->fpstt;
}

static inline void lsenv_set_top(ENV *lsenv, int new_fpstt)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    cpu->fpstt = new_fpstt;
}

static inline FPReg lsenv_get_fpregs(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return cpu->fpregs[i];
}

static inline void lsenv_set_fpregs(ENV *lsenv, int i, FPReg new_value)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    cpu->fpregs[i] = new_value;
}

static inline int lsenv_offset_of_status_word(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->fpus) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_control_word(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->fpuc) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_get_fpu_control_word(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return cpu->fpuc;
}

static inline int lsenv_offset_of_tag_word(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->fptags[0]) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_mode_fpu(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->mode_fpu) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_fpr(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR) & (cpu->fpregs[i].d) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_mmx(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->fpregs[i]) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_mxcsr(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->mxcsr) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_seg_base(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->segs[i].base) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_seg_selector(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->segs[i].selector) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_seg_limit(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->segs[i].limit) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_seg_flags(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->segs[i].flags) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_ldt_base(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->ldt.base) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_gdt_base(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->gdt.base) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_gdt_limit(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->gdt.limit) - (ADDR)lsenv->cpu_state);
}

/* func to access ENV's interrupt/exception attributes */
static inline int lsenv_offset_exception_index(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    CPUState *cpu_state = env_cpu(cpu);
    return (int)((ADDR)(&cpu_state->exception_index) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_exception_next_eip(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->exception_next_eip) - (ADDR)lsenv->cpu_state);
}

#ifdef CONFIG_LATX_DEBUG
static inline int lsenv_offset_last_store_insn(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->last_store_insn) - (ADDR)lsenv->cpu_state);
}
#endif

static inline int lsenv_offset_of_fp_status(ENV *lsenv)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->fp_status) - (ADDR)lsenv->cpu_state);
}

/* virtual registers */
static inline int lsenv_offset_of_vreg(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->vregs[i]) - (ADDR)lsenv->cpu_state);
}

static inline int lsenv_offset_of_last_executed_tb(ENV *lsenv)
{
    return lsenv_offset_of_vreg(lsenv, 1);
}

static inline int lsenv_offset_of_next_eip(ENV *lsenv)
{
    return lsenv_offset_of_vreg(lsenv, 2);
}

static inline int lsenv_offset_of_top_bias(ENV *lsenv)
{
    return lsenv_offset_of_vreg(lsenv, 3);
}

static inline int lsenv_offset_of_ss(ENV *lsenv)
{
    return lsenv_offset_of_vreg(lsenv, 4);
}

static inline int lsenv_offset_of_xmm(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (int)((ADDR)(&cpu->xmm_regs[i]) - (ADDR)lsenv->cpu_state);
}

static inline ADDR lsenv_get_vreg(ENV *lsenv, int i)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    return (ADDR)cpu->vregs[i];
}

static inline void lsenv_set_vreg(ENV *lsenv, int i, ADDR val)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    cpu->vregs[i] = (uint64_t)val;
}

static inline ADDR lsenv_get_last_executed_tb(ENV *lsenv)
{
    return (ADDR)lsenv_get_vreg(lsenv, 1);
}

static inline void lsenv_set_last_executed_tb(ENV *lsenv, ADDR tb)
{
    lsenv_set_vreg(lsenv, 1, tb);
}

static inline ADDRX lsenv_get_next_eip(ENV *lsenv)
{
    return (ADDRX)lsenv_get_vreg(lsenv, 2);
}

static inline void lsenv_set_next_eip(ENV *lsenv, ADDRX eip)
{
    lsenv_set_vreg(lsenv, 2, eip);
}

static inline int lsenv_get_top_bias(ENV *lsenv)
{
    return (int)lsenv_get_vreg(lsenv, 3);
}

static inline void lsenv_set_top_bias(ENV *lsenv, int top_bias)
{
    lsenv_set_vreg(lsenv, 3, top_bias);
}

static inline int lsenv_offset_of_tr_data(ENV *lsenv)
{
    return (ADDR)(&lsenv->tr_data) - (ADDR)lsenv;
}

#endif
