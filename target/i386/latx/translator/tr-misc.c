#include "reg-alloc.h"
#include "latx-options.h"
#include "latx-config.h"
#include "lsenv.h"
#include "flag-lbt.h"
#include "translate.h"
#include "syscall-tunnel.h"
#include "profile.h"
#include "tu.h"
#include "hbr.h"
#include "jrra-translator.h"

#if defined(CONFIG_LATX_KZT)
#include "wrapper.h"
#include "debug.h"
#include "bridge_private.h"
#include "exec/tb-lookup.h"
#include "elfloader.h"

struct cpu_state_info {
    uint32_t flags;
    uint32_t cflags;
    target_ulong cs_base;
    target_ulong current_pc;
};
#endif

bool translate_nop(IR1_INST *pir1)
{
    if (option_anonym) {
        IR2_OPND eflags_opnd = ra_alloc_eflags();
        IR2_OPND eflags_temp_opnd = ra_alloc_itemp();
        IR2_OPND tmp_opnd = ra_alloc_itemp();
        IR2_OPND eip_opnd = ra_alloc_dbt_arg2();
        IR2_OPND label_TF = ra_alloc_label();

        la_andi(eflags_temp_opnd, eflags_opnd, TF_MASK);
        la_beqz(eflags_temp_opnd, label_TF);
        li_d(eip_opnd, ir1_addr(pir1));
        la_store_addrx(eip_opnd, env_ir2_opnd,
                        lsenv_offset_of_eip(lsenv));
        tr_save_registers_to_env(0xff, 0xff, option_save_xmm, options_to_save());
        aot_load_host_addr(tmp_opnd, (ADDR)helper_raise_trapop,
            LOAD_HELPER_RAISE_TRAPOP, 0);
        la_jirl(zero_ir2_opnd, tmp_opnd, 0);
        ra_free_temp(tmp_opnd);
        la_label(label_TF);
    }

    la_andi(zero_ir2_opnd, zero_ir2_opnd, 0);

    return true;
}
bool translate_endbr32(IR1_INST *pir1) { return true; }
bool translate_endbr64(IR1_INST *pir1) { return true; }

#if defined(CONFIG_LATX_KZT)
static uint8_t Peek8(uintptr_t addr, uintptr_t offset)
{
    return *(uint8_t*)(addr+offset);
}

static void gen_set_next_tb_code(IR2_OPND *esp_ir2_opnd)
{
    IR2_OPND nextip_ir2_opnd = ra_alloc_dbt_arg2();

#ifndef TARGET_X86_64
    la_bstrpick_d(*esp_ir2_opnd, *esp_ir2_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(*esp_ir2_opnd, *esp_ir2_opnd, 31, 0);
    }
#endif
    la_load_addrx(nextip_ir2_opnd, *esp_ir2_opnd, 0);

    la_addi_addrx(*esp_ir2_opnd, *esp_ir2_opnd, sizeof(target_ulong));
    la_store_addrx(nextip_ir2_opnd, env_ir2_opnd, lsenv_offset_of_eip(lsenv));
}

static void wrapper_gpr_trans(ADDR addr)
{
    IR2_OPND a0_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a0);
    li_d(a0_ir2_opnd, addr);
}

static void tr_generate_exit_tb_for_bridge(void)
{
    TranslationBlock *tb = lsenv->tr_data->curr_tb;
    IR2_OPND base = ra_alloc_data();
    IR2_OPND target = ra_alloc_data();
    la_data_li(base, (ADDR)tb->tc.ptr);
    if (!qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN)) {
        la_data_li(target, indirect_jmp_glue);
        aot_la_append_ir2_jmp_far(target, base, B_NATIVE_JMP_GLUE2, 0);
    } else {
        la_data_li(target, context_switch_native_to_bt_ret_0);
        aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE, 0);
    }
}
static void mmm_free(void *mem) {
    printf_log(LOG_DEBUG, "debug la free %p\n", mem);
    free(mem);
}
static void* mmm_realloc(void *mem, size_t len) {
    printf_log(LOG_DEBUG, "debug realloc %p\n", mem);
    return realloc(mem, len);
}

#include "box64context.h"
extern void my___libc_free(void* m);
extern void my_cfree(void* m);
extern void my_free(void* m);
extern void my___free(void* m);
extern void my_realloc(void* m, void *old, uintptr_t len);
extern void* x86free;
extern void* x86realloc;
static void kzt_helper_ptr(ADDR func, IR2_OPND ptr)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, ptr);
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
}

static void kzt_helper_pFpL(ADDR func, IR2_OPND ptr, IR2_OPND L)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, ptr);
    la_mov64(a1_ir2_opnd, L);
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
    //for kzt_wrapper_to_native for return.
    la_st_d(a0_ir2_opnd, env_ir2_opnd, lsenv_offset_of_gpr(lsenv, R_EAX));
}

void kzt_native_to_wrapper(void);
void kzt_native_to_wrapper(void)
{
    tr_save_registers_to_env(0xff, 0xff, option_save_xmm, options_to_save());
    tr_save_x64_8_registers_to_env(0xff, option_save_xmm);
    /* restore dbt FCSR (#31) */
    IR2_OPND fcsr_value_opnd = ra_alloc_itemp();
    /* save fcsr for native */
    la_movfcsr2gr(fcsr_value_opnd, fcsr_ir2_opnd);
    la_st_w(fcsr_value_opnd, env_ir2_opnd,
                          lsenv_offset_of_fcsr(lsenv));

}
void kzt_wrapper_to_native(void);
void kzt_wrapper_to_native(void)
{
    /* save dbt FCSR */
    IR2_OPND fcsr_value_opnd = ra_alloc_itemp();

    la_ld_w(fcsr_value_opnd, env_ir2_opnd,
                          lsenv_offset_of_fcsr(lsenv));
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_value_opnd);
    ra_free_temp(fcsr_value_opnd);

    /* load x86 registers from env. top, eflags, and ss */
    tr_load_registers_from_env(0xff, 0xff, option_save_xmm, options_to_save());
    tr_load_x64_8_registers_from_env(0xff, option_save_xmm);
    /* load &HASH_JMP_CACHE[0] */
    IR2_OPND jmp_cache_addr = ra_alloc_static0();
    la_ld_d(jmp_cache_addr, env_ir2_opnd, lsenv_offset_of_tb_jmp_cache_ptr(lsenv));
}

static void do_translate_realloc_brick_tb(void)
{
    uintptr_t realloc_pc = (uint64_t)&x86realloc;
    IR2_OPND reserved_va_opnd = ra_alloc_itemp();
    IR2_OPND gpr_rdi_opnd = ra_alloc_gpr(edi_index);
    IR2_OPND back_to_x86realloc_opnd = ra_alloc_label();
    IR2_OPND esp_ir2_opnd = ra_alloc_gpr(esp_index);
    lsassert(realloc_pc);
    li_d(reserved_va_opnd, (ADDR)reserved_va);
    la_bltu(gpr_rdi_opnd, reserved_va_opnd, back_to_x86realloc_opnd);
    kzt_native_to_wrapper();
    kzt_helper_pFpL((ADDR)mmm_realloc, gpr_rdi_opnd, ra_alloc_gpr(esi_index));
    kzt_wrapper_to_native();
    gen_set_next_tb_code(&esp_ir2_opnd);
    tr_generate_exit_tb_for_bridge();
    la_label(back_to_x86realloc_opnd);
    IR2_OPND eip_opnd = ra_alloc_dbt_arg2();
    li_d(eip_opnd, (ADDR)realloc_pc);
    la_ld_d(eip_opnd,eip_opnd, 0);
    lsassert(lsenv_offset_of_eip(lsenv) >= -2048 &&
            lsenv_offset_of_eip(lsenv) <= 2047);
    la_store_addrx(eip_opnd, env_ir2_opnd,
                            lsenv_offset_of_eip(lsenv));
    tr_generate_exit_tb_for_bridge();
}

static void do_translate_free_brick_tb(void)
{
    uintptr_t free_pc = (uint64_t)&x86free;
    IR2_OPND reserved_va_opnd = ra_alloc_itemp();
    IR2_OPND gpr_rdi_opnd = ra_alloc_gpr(edi_index);
    IR2_OPND back_to_x86free_opnd = ra_alloc_label();
    IR2_OPND esp_ir2_opnd = ra_alloc_gpr(esp_index);
    lsassert(free_pc);
    li_d(reserved_va_opnd, (ADDR)reserved_va);
    la_bltu(gpr_rdi_opnd, reserved_va_opnd, back_to_x86free_opnd);
    kzt_native_to_wrapper();
    kzt_helper_ptr((ADDR)mmm_free, gpr_rdi_opnd);
    kzt_wrapper_to_native();
    gen_set_next_tb_code(&esp_ir2_opnd);
    tr_generate_exit_tb_for_bridge();
    la_label(back_to_x86free_opnd);
    IR2_OPND eip_opnd = ra_alloc_dbt_arg2();
    li_d(eip_opnd, (ADDR)free_pc);
    la_ld_d(eip_opnd,eip_opnd, 0);
    lsassert(lsenv_offset_of_eip(lsenv) >= -2048 &&
            lsenv_offset_of_eip(lsenv) <= 2047);
    la_store_addrx(eip_opnd, env_ir2_opnd,
                            lsenv_offset_of_eip(lsenv));
    tr_generate_exit_tb_for_bridge();
}

static void do_translate_brick_tb(onebridge_t *bridge, struct cpu_state_info *state_info, CPUState *cpu, target_ulong tb_pc, TranslationBlock *tb)
{
    tb = lsenv->tr_data->curr_tb;
    IR2_OPND esp_ir2_opnd = ra_alloc_gpr(esp_index);
    if (bridge->f == (uintptr_t)my_free ||bridge->f == (uintptr_t)my___libc_free ||bridge->f == (uintptr_t)my___free ||bridge->f == (uintptr_t)my_cfree ) {
        do_translate_free_brick_tb();
        return;
    } else if (bridge->f == (uintptr_t)my_realloc) {
        do_translate_realloc_brick_tb();
        return;
    }
    kzt_native_to_wrapper();
    wrapper_gpr_trans((ADDR)bridge->f);

    tr_set_running_of_cs(false);
    li_d(ra_ir2_opnd, (ADDR)bridge->w);
    la_jirl(ra_ir2_opnd, ra_ir2_opnd, 0);
    tr_set_running_of_cs(true);

    kzt_wrapper_to_native();
    gen_set_next_tb_code(&esp_ir2_opnd);

    tr_generate_exit_tb_for_bridge();
}
void do_translate_tbbridge(ADDR func_pc, ADDR wrapper, TranslationBlock *tb);
void do_translate_tbbridge(ADDR func_pc, ADDR wrapper, TranslationBlock *tb)
{
    IR2_OPND esp_ir2_opnd = ra_alloc_gpr(esp_index);
    kzt_native_to_wrapper();
    wrapper_gpr_trans((ADDR)func_pc);

    tr_set_running_of_cs(false);
    li_d(ra_ir2_opnd, (ADDR)wrapper);
    la_jirl(ra_ir2_opnd, ra_ir2_opnd, 0);
    tr_set_running_of_cs(true);

    kzt_wrapper_to_native();
    gen_set_next_tb_code(&esp_ir2_opnd);

    tr_generate_exit_tb_for_bridge();
}
#endif

bool translate_int_3(IR1_INST *pir1)
{
#if defined(CONFIG_LATX_KZT)
    if (CODEIS64) {
        struct cpu_state_info state_info;
        CPUState *cpu = env_cpu(lsenv->cpu_state);
        /* fill in cpu_state_info */
        state_info.cflags = cpu->tcg_cflags;
        cpu_get_tb_cpu_state(cpu->env_ptr, &state_info.current_pc,
                             &state_info.cs_base, &state_info.flags);
        if(option_kzt && Peek8(state_info.current_pc + 1, 0) == 'S' && Peek8(state_info.current_pc + 1, 1) == 'C')
        {
            TranslationBlock *tb = NULL;
            mmap_lock();
            onebridge_t *bridge= (onebridge_t*)state_info.current_pc;
            do_translate_brick_tb(bridge, &state_info, cpu, state_info.current_pc, tb);
            mmap_unlock();
        } else {
            la_break(0x5);
        }
    } else {
        la_break(0x5);
    }
    return true;
#else
    la_break(0x5);
    return true;
#endif
}

bool translate_in(IR1_INST *pir1)
{
    /* raise GPF */
    IR2_OPND tmp_opnd = ra_alloc_itemp();
    IR2_OPND eip_opnd = ra_alloc_dbt_arg2();
    li_d(eip_opnd, ir1_addr(pir1));
    la_store_addrx(eip_opnd, env_ir2_opnd,
                    lsenv_offset_of_eip(lsenv));
    tr_save_registers_to_env(0xff, 0xff, option_save_xmm, options_to_save());
    aot_load_host_addr(tmp_opnd, (ADDR)helper_raise_gpf,
        LOAD_HELPER_RAISE_GPF, 0);
    la_jirl(zero_ir2_opnd, tmp_opnd, 0);
    ra_free_temp(tmp_opnd);
    return true;
}

bool translate_out(IR1_INST *pir1)
{
    /* raise GPF */
    IR2_OPND tmp_opnd = ra_alloc_itemp();
    IR2_OPND eip_opnd = ra_alloc_dbt_arg2();
    li_d(eip_opnd, ir1_addr(pir1));
    la_store_addrx(eip_opnd, env_ir2_opnd,
                    lsenv_offset_of_eip(lsenv));
    tr_save_registers_to_env(0xff, 0xff, option_save_xmm, options_to_save());
    aot_load_host_addr(tmp_opnd, (ADDR)helper_raise_gpf,
        LOAD_HELPER_RAISE_GPF, 0);
    la_jirl(zero_ir2_opnd, tmp_opnd, 0);
    ra_free_temp(tmp_opnd);
    return true;
}

bool translate_into(IR1_INST *pir1)
{
    /* raise INTO , enabel when OF = 1 */
    IR2_OPND flag_opnd = ra_alloc_itemp();
    IR2_OPND label_z = ra_alloc_label();
    la_x86mfflag(flag_opnd, OF_USEDEF_BIT);
    la_beq(flag_opnd, zero_ir2_opnd, label_z);

    IR2_OPND tmp_opnd = ra_alloc_itemp();
    IR2_OPND eip_opnd = ra_alloc_dbt_arg2();
    li_d(eip_opnd, ir1_addr(pir1));
    la_store_addrx(eip_opnd, env_ir2_opnd,
                    lsenv_offset_of_eip(lsenv));
    tr_save_registers_to_env(0xff, 0xff, option_save_xmm, options_to_save());
    aot_load_host_addr(tmp_opnd, (ADDR)helper_raise_into,
        LOAD_HELPER_RAISE_INTO, 0);
    la_jirl(zero_ir2_opnd, tmp_opnd, 0);
    ra_free_temp(tmp_opnd);

    la_label(label_z);
    ra_free_temp(flag_opnd);
    return true;
}

bool translate_bound(IR1_INST *pir1)
{
    /* raise BOUND , disenable when a<=b<=c */
    int opnd_size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    IR2_OPND high = ra_alloc_itemp();
    IR2_OPND src = load_ireg_from_ir1(ir1_get_opnd(pir1, 0), SIGN_EXTENSION, false);
    IR2_OPND bound_low = load_ireg_from_ir1(ir1_get_opnd(pir1, 1), SIGN_EXTENSION, false);
    if (opnd_size == 16) {
        la_srai_w(high, bound_low, 16);
        la_slli_w(bound_low, bound_low, 16);
        la_srai_w(bound_low, bound_low, 16);
    } else {
        la_srai_d(high, bound_low, 32);
        la_bstrpick_w(bound_low, bound_low, 31, 0);
    }
    IR2_OPND label_oh = ra_alloc_label();
    IR2_OPND label_ol = ra_alloc_label();
    la_blt(high, src, label_oh);
    la_bge(src, bound_low, label_ol);
    ra_free_temp(high);

    la_label(label_oh);
    IR2_OPND tmp_opnd = ra_alloc_itemp();
    IR2_OPND eip_opnd = ra_alloc_dbt_arg2();
    li_d(eip_opnd, ir1_addr(pir1));
    la_store_addrx(eip_opnd, env_ir2_opnd,
                    lsenv_offset_of_eip(lsenv));
    tr_save_registers_to_env(0xff, 0xff, option_save_xmm, options_to_save());
    aot_load_host_addr(tmp_opnd, (ADDR)helper_raise_bound,
        LOAD_HELPER_RAISE_BOUND, 0);
    la_jirl(zero_ir2_opnd, tmp_opnd, 0);
    ra_free_temp(tmp_opnd);

    la_label(label_ol);
    return true;
}

bool translate_prefetch(IR1_INST *pir1) { return true; }
bool translate_prefetchnta(IR1_INST *pir1) { return true; }
// bool translate_prefetcht0(IR1_INST *pir1) { return true; }
bool translate_prefetcht1(IR1_INST *pir1) { return true; }
bool translate_prefetcht2(IR1_INST *pir1) { return true; }

bool translate_lfence(IR1_INST *pir1)
{
    la_dbar(0);
    return true;
}

bool translate_mfence(IR1_INST *pir1)
{
    la_dbar(0);
    return true;
}

bool translate_sfence(IR1_INST *pir1)
{
    la_dbar(0);
    return true;
}

bool translate_clflush(IR1_INST *pir1)
{
    la_ibar(0);
    return true;
}

bool translate_clflushopt(IR1_INST *pir1)
{
    la_ibar(0);
    return true;
}

bool translate_ret_without_ss_opt(IR1_INST *pir1);

static inline void set_CPUX86State_error_code(ENV *lsenv, int error_code)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    cpu->error_code = error_code;
}

bool translate_ins(IR1_INST *pir1) {
#ifdef CONFIG_LATX_DEBUG
    printf("FIXME: the instruction INSD is used, but LATX do not support it.\n");
#endif
#ifdef CONFIG_LATX_TU
	return false;
#endif
    return true;
}

static inline void set_CPUX86State_exception_is_int(ENV *lsenv, int exception_is_int)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    cpu->exception_is_int = exception_is_int;
}

static inline void set_CPUState_can_do_io(ENV *lsenv, int can_do_io)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    CPUState *cs = env_cpu(cpu);
    cs->can_do_io = can_do_io;
}

static void siglongjmp_cpu_jmp_env(void)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;

    /* siglongjmp will skip the execution of latx_after_exec_tb
     * which is expected to reset top_bias/top
     */
    TranslationBlock *last_tb __attribute__((unused)) =
        (TranslationBlock *)lsenv_get_last_executed_tb(lsenv);
#ifdef CONFIG_LATX_DEBUG
    latx_after_exec_trace_tb(cpu, last_tb);
#endif

    CPUState *cpu_state = env_cpu(cpu);
    siglongjmp(cpu_state->jmp_env, 1);
}

/* Instead save intno in helper_raise_int, we save intno in translate_int. */
void helper_raise_int(void)
{
    set_CPUX86State_error_code(lsenv, 0);
    set_CPUX86State_exception_is_int(lsenv, 1);
    set_CPUState_can_do_io(lsenv, 1);
    siglongjmp_cpu_jmp_env();
}

void helper_raise_gpf(void)
{
    CPUX86State *env = (CPUX86State *)lsenv->cpu_state;
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP0D_GPF;
    env->error_code = 0;
    env->exception_is_int = 0;
    env->exception_next_eip = env->eip;
    cpu_loop_exit(cs);
}

void helper_raise_illop(void)
{
    CPUX86State *env = (CPUX86State *)lsenv->cpu_state;
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP06_ILLOP;
    env->error_code = 0;
    env->exception_is_int = 0;
    env->exception_next_eip = env->eip;
    cpu_loop_exit(cs);
}

void helper_raise_trapop(void)
{
    CPUX86State *env = (CPUX86State *)lsenv->cpu_state;
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP01_DB;
    env->error_code = 0;
    env->exception_is_int = 0;
    env->exception_next_eip = env->eip;
    cpu_loop_exit(cs);
}

bool translate_invalid(IR1_INST *pir1)
{
    IR2_OPND tmp_opnd = ra_alloc_itemp();
    IR2_OPND eip_opnd = ra_alloc_dbt_arg2();
    li_d(eip_opnd, ir1_addr(pir1));
    la_store_addrx(eip_opnd, env_ir2_opnd,
                    lsenv_offset_of_eip(lsenv));
    tr_save_registers_to_env(0xff, 0xff, option_save_xmm, options_to_save());
    aot_load_host_addr(tmp_opnd, (ADDR)helper_raise_illop,
        LOAD_HELPER_RAISE_ILLOP, 0);
    la_jirl(zero_ir2_opnd, tmp_opnd, 0);
    ra_free_temp(tmp_opnd);
    return true;
}

void helper_raise_into(void)
{
    CPUX86State *env = (CPUX86State *)lsenv->cpu_state;
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP04_INTO;
    env->error_code = 0;
    env->exception_is_int = 0;
    env->exception_next_eip = env->eip;
    cpu_loop_exit(cs);
}

void helper_raise_bound(void)
{
    CPUX86State *env = (CPUX86State *)lsenv->cpu_state;
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP05_BOUND;
    env->error_code = 0;
    env->exception_is_int = 0;
    env->exception_next_eip = env->eip;
    cpu_loop_exit(cs);
}

#ifdef TARGET_X86_64
#if defined(CONFIG_USER_ONLY)
void helper_raise_syscall(void)
{
    /* the exception_index and exception_next_eip will set */
    /* in the `translate_syscall` */
    /* cs->exception_index = EXCP_SYSCALL; */
    /* env->exception_next_eip = env->eip + next_eip_addend; */
    set_CPUX86State_exception_is_int(lsenv, 0);
    set_CPUState_can_do_io(lsenv, 1);
    siglongjmp_cpu_jmp_env();
}
#else
void helper_raise_syscall(void)
{
    lsassertm(0, "`Syscall` is not support in system mode now!");
}
#endif /* CONFIG_USER_ONLY */
#endif /* TARGET_X86_64 */

/*
 * esp should not change before ra is correctly stored in stack
 * in case that the store may trigger a SEGV
 */
bool translate_call(IR1_INST *pir1)
{
    /* call will not consider the address size override */
#ifdef TARGET_X86_64
    if (CODEIS64) {
        pir1->info->x86.addr_size = 64 >> 3;
    } else {
        pir1->info->x86.addr_size = 32 >> 3;
    }
#else
    pir1->info->x86.addr_size = 32 >> 3;
#endif
    if(ir1_is_indirect_call(pir1)){
        return translate_callin(pir1);
    }
    /* 1. adjust esp */
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    int opnd_size = ir1_get_opnd_size(pir1) >> 3;

    /* 2. save return address onto stack */
    IR2_OPND return_addr_opnd = ra_alloc_itemp();
    IR2_OPND x86_addr_opnd = ra_alloc_itemp();

    target_ulong call_offset __attribute__((unused)) =
            aot_get_call_offset(ir1_addr_next(pir1));
    aot_load_guest_addr(x86_addr_opnd, ir1_addr_next(pir1),
                        LOAD_CALL_TARGET, call_offset);

    jrra_emit_call_metadata(lsenv->tr_data->curr_tb, ir1_addr_next(pir1),
                           JRRA_CALL_DIRECT);

#ifndef TARGET_X86_64
    la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
    }
#endif
    /* if opnd_size = 2, use `ST_H` */
    if (opnd_size == 8) {
        la_st_d(x86_addr_opnd, esp_opnd, -opnd_size);
    } else if (opnd_size == 4) {
        la_st_w(x86_addr_opnd, esp_opnd, -opnd_size);
    } else {
        la_st_h(x86_addr_opnd, esp_opnd, -opnd_size);
    }
    la_addi_addrx(esp_opnd, esp_opnd, -opnd_size);

    /* QEMU exit_tb & goto_tb */
    tr_generate_exit_tb(pir1, 0);

    ra_free_temp(return_addr_opnd);
    return true;
}

bool translate_callnext(IR1_INST *pir1)
{
    /* 1. load next_instr_addr to tmp_reg */
    IR2_OPND next_addr = ra_alloc_itemp();

    target_ulong call_offset __attribute__((unused)) =
            aot_get_call_offset(ir1_addr_next(pir1));
    aot_load_guest_addr(next_addr, ir1_addr_next(pir1),
                        LOAD_CALL_TARGET, call_offset);
    /* 2. store next_addr to x86_stack */
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    int opnd_size = ir1_get_opnd_size(pir1) >> 3;

#ifndef TARGET_X86_64
    la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
    }
#endif
    /* if opnd_size = 16, use `ST_H` */
    if (opnd_size == 8) {
        la_st_d(next_addr, esp_opnd, -opnd_size);
    } else if (opnd_size == 4) {
        la_st_w(next_addr, esp_opnd, -opnd_size);
    } else {
        la_st_h(next_addr, esp_opnd, -opnd_size);
    }
    la_addi_addrx(esp_opnd, esp_opnd, -opnd_size);
    ra_free_temp(next_addr);

    return true;
}

bool translate_callthunk(IR1_INST *pir1)
{
    int opnd0_gpr_num = ht_pc_thunk_lookup(ir1_target_addr(pir1));
    IR2_OPND dest  = ra_alloc_gpr(opnd0_gpr_num);

    target_ulong call_offset __attribute__((unused)) =
            aot_get_call_offset(ir1_addr_next(pir1));
    aot_load_guest_addr(dest, ir1_addr_next(pir1),
                        LOAD_CALL_TARGET, call_offset);
    return true;
}


bool translate_callin(IR1_INST *pir1)
{
    /* 1. set successor x86 address */
    IR2_OPND succ_x86_addr_opnd = ra_alloc_dbt_arg2();
    load_ireg_from_ir1_2(succ_x86_addr_opnd, ir1_get_opnd(pir1, 0), ZERO_EXTENSION,
                         false);

    /*
     * 2. adjust esp
     * NOTE: There is a corner case during wine kernel32 virtual unit test.
     * If esp updated at the begin of the insn, once stw trigger the segv,
     * The esp will be conflict and gcc stack check will be failed.
     * Solutuion is same as TCG did, update esp to temp reg, once store
'    * insn is successful, esp will be updated.
     */
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    int opnd_size = ir1_get_opnd_size(pir1) >> 3;

    /* 3. save return address onto stack */
    ra_alloc_itemp();
    IR2_OPND return_addr_opnd = ra_alloc_itemp();
    target_ulong call_offset __attribute__((unused)) =
            aot_get_call_offset(ir1_addr_next(pir1));
    aot_load_guest_addr(return_addr_opnd, ir1_addr_next(pir1),
                        LOAD_CALL_TARGET, call_offset);

    jrra_emit_call_metadata(lsenv->tr_data->curr_tb, ir1_addr_next(pir1),
                           JRRA_CALL_INDIRECT);

#ifndef TARGET_X86_64
    la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
    }
#endif
    /* if opnd_size = 16, use `ST_H` */
    if (opnd_size == 8) {
        la_st_d(return_addr_opnd, esp_opnd, -opnd_size);
    } else if (opnd_size == 4) {
        la_st_w(return_addr_opnd, esp_opnd, -opnd_size);
    } else {
        la_st_h(return_addr_opnd, esp_opnd, -opnd_size);
    }
    la_addi_addrx(esp_opnd, esp_opnd, -opnd_size);
    ra_free_temp(return_addr_opnd);

    /* 5. indirect linkage */
    /* env->tr_data->curr_tb->generate_tb_linkage_indirect(); */
    tr_generate_exit_tb(pir1, 0);
    return true;
}

bool translate_retf(IR1_INST *pir1)
{
    /* 1. load ret_addr into $25 */
    IR1_OPND seg_opnd;
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    IR2_OPND cs_opnd = ra_alloc_itemp();
    IR2_OPND return_addr_opnd = ra_alloc_dbt_arg2();
    int opnd_num = ir1_opnd_num(pir1);

    ir1_opnd_build_reg(&seg_opnd, 16, dt_X86_REG_CS);
#ifndef TARGET_X86_64
    la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
    /* pop rip */
    la_ld_wu(return_addr_opnd, esp_opnd, 0);
    /* pop cs */
    la_ld_w(cs_opnd, esp_opnd, 4);
    /* 2. adjust esp */
    la_addi_addrx(esp_opnd, esp_opnd, 8);
#else
    if (CODEIS64) {
        /* pop rip */
        la_ld_wu(return_addr_opnd, esp_opnd, 0);
        /* pop cs */
        la_ld_w(cs_opnd, esp_opnd, 4);
        /* 2. adjust esp */
        la_addi_addrx(esp_opnd, esp_opnd, 8);
    } else {
        la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
        /* pop rip */
        la_ld_wu(return_addr_opnd, esp_opnd, 0);
        /* pop cs */
        la_ld_w(cs_opnd, esp_opnd, 4);
        /* 2. adjust esp */
        la_addi_addrx(esp_opnd, esp_opnd, 8);
    }
#endif

    store_ireg_to_ir1_seg(cs_opnd, &seg_opnd);

    if (opnd_num) {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        IR2_OPND imm_opnd = load_ireg_from_ir1(opnd0, ZERO_EXTENSION, false);
        la_add_d(esp_opnd, esp_opnd, imm_opnd);
    }

    tr_generate_exit_tb(pir1, 0);
    ra_free_temp(cs_opnd);

    return true;
}


bool translate_iret(IR1_INST *pir1)
{
    /* 1. load ret_addr into $25 */
    IR1_OPND seg_opnd;
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    IR2_OPND eflags_opnd = ra_alloc_eflags();
    IR2_OPND cs_opnd = ra_alloc_itemp();
    IR2_OPND return_addr_opnd = ra_alloc_dbt_arg2();

#ifndef TARGET_X86_64
    la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
    }
#endif

    la_ld_wu(return_addr_opnd, esp_opnd, 0);
    la_ld_w(cs_opnd, esp_opnd, 4);
    la_ld_w(eflags_opnd, esp_opnd, 8);
    ir1_opnd_build_reg(&seg_opnd, 16, dt_X86_REG_CS);
    store_ireg_to_ir1_seg(cs_opnd, &seg_opnd);
    ra_free_temp(cs_opnd);

    la_x86mtflag(eflags_opnd, 0x3f);
    /* 2. adjust esp */
    la_addi_addrx(esp_opnd, esp_opnd, 12);

    tr_generate_exit_tb(pir1, 0);

    return true;
}

bool translate_iretq(IR1_INST *pir1)
{
    /* 1. load ret_addr into $25 */
    IR1_OPND seg_opnd;
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    IR2_OPND eflags_opnd = ra_alloc_eflags();
    IR2_OPND esp_tmp = ra_alloc_itemp();
    IR2_OPND tmp_opnd = ra_alloc_itemp();
    IR2_OPND return_addr_opnd = ra_alloc_dbt_arg2();

    la_ld_d(return_addr_opnd, esp_opnd, 0);
    /* cs */
    la_ld_d(tmp_opnd, esp_opnd, 8);
    ir1_opnd_build_reg(&seg_opnd, 16, dt_X86_REG_CS);
    store_ireg_to_ir1_seg(tmp_opnd, &seg_opnd);
    /* eflags */
    la_ld_d(eflags_opnd, esp_opnd, 16);
    la_x86mtflag(eflags_opnd, 0x3f);
    /* esp */
    la_ld_d(esp_tmp, esp_opnd, 24);
    /* ss */
    la_ld_d(tmp_opnd, esp_opnd, 32);
    ir1_opnd_build_reg(&seg_opnd, 16, dt_X86_REG_SS);
    store_ireg_to_ir1_seg(tmp_opnd, &seg_opnd);
    ra_free_temp(tmp_opnd);

    la_or(esp_opnd, esp_tmp, zero_ir2_opnd);
    ra_free_temp(esp_tmp);

    tr_generate_exit_tb(pir1, 0);

    return true;
}

bool translate_ret_without_ss_opt(IR1_INST *pir1)
{
    /* 1. load ret_addr into $25 */
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    IR2_OPND return_addr_opnd = ra_alloc_dbt_arg2();
#ifndef TARGET_X86_64
    load_ireg_from_ir1_2(return_addr_opnd, &esp_mem32_ir1_opnd,
                         UNKNOWN_EXTENSION, false);
    la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
#else
    if (CODEIS64) {
        load_ireg_from_ir1_2(return_addr_opnd, &rsp_mem64_ir1_opnd,
                         UNKNOWN_EXTENSION, false);
    } else {
        load_ireg_from_ir1_2(return_addr_opnd, &esp_mem32_ir1_opnd,
                             UNKNOWN_EXTENSION, false);
        la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
    }
#endif
    /* 2. adjust esp */
    int opnd_size = ir1_get_opnd_size(pir1) >> 3;
    /* printf("opnd_size %d\n",opnd_size); */
    if (pir1 != NULL && ir1_opnd_num(pir1) &&
        ir1_opnd_type(ir1_get_opnd(pir1, 0)) == dt_X86_OP_IMM) {
        int imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 0)) + opnd_size;
        if (imm > sextract64(imm, 0, 12)) {
            IR2_OPND imm_opnd = ra_alloc_itemp();
            li_d(imm_opnd, imm);
            la_add(esp_opnd, esp_opnd, imm_opnd);
        } else {
            la_addi_addrx(esp_opnd, esp_opnd, imm);
        }
    } else {
        la_addi_addrx(esp_opnd, esp_opnd, opnd_size);
    }
    if (jrra_translate_ret(lsenv->tr_data->curr_tb, return_addr_opnd, pir1)) {
        return true;
    }
    tr_generate_exit_tb(pir1, 0);

    return true;
}

bool translate_ret(IR1_INST *pir1)
{
    return translate_ret_without_ss_opt(pir1);
}
bool translate_ljmp(IR1_INST *pir1)
{
    ADDR local_fs_index;
    IR1_OPND* opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND tmp_opnd = ra_alloc_itemp();
    IR1_OPND seg_opnd;
    uint32_t old_size = 0;
    #ifdef TARGET_X86_64
    if (CODEIS64 == LATX_DT_X64 &&
        (!(pir1->info->x86.rex & 0x4))) {//rex.w be set.size is 64bit.
        opnd0->size = 6;
    }
    #endif
    local_fs_index = ir1_opnd_size(ir1_get_opnd(pir1, 0)) - 16;
    opnd0->size -= 16>>3;
    old_size = opnd0->size;
    /* 1. set successor x86 address */
    IR2_OPND succ_x86_addr_opnd = ra_alloc_dbt_arg2();
    load_ireg_from_ir1_2(succ_x86_addr_opnd, ir1_get_opnd(pir1, 0), ZERO_EXTENSION,
                         false);
    opnd0->size = 16 >>3;
    opnd0->mem.disp += local_fs_index >> 3;
    load_ireg_from_ir1_2(tmp_opnd, ir1_get_opnd(pir1, 0), ZERO_EXTENSION,
                         false);
    ir1_opnd_build_reg(&seg_opnd, 16, dt_X86_REG_CS);
    store_ireg_to_ir1_seg(tmp_opnd, &seg_opnd);
    ra_free_temp(tmp_opnd);
    /*recover opend0*/
    opnd0->size = old_size;
    opnd0->mem.disp -= local_fs_index >> 3;
    tr_generate_exit_tb(pir1, 1);
    return true;
}

bool translate_jmp(IR1_INST *pir1)
{
    if (pir1->info->x86.opcode[0] == 0xff &&
        (pir1->info->x86.modrm & 0x38) == 0x28) {
        return translate_ljmp(pir1);
    }
    if (ir1_is_indirect_jmp(pir1)) {
        return translate_jmpin(pir1);
    }

#ifdef CONFIG_LATX_TU
    TranslationBlock *tb = lsenv->tr_data->curr_tb;
    if (tb->s_data->next_tb[1] &&
        tb->tu_jmp[TU_TB_INDEX_TARGET] != TB_JMP_RESET_OFFSET_INVALID) {
        /* la_code_align(2, 0x03400000); */
        IR2_OPND target_label_opnd = ra_alloc_label();
        la_label(target_label_opnd);
        la_b(imm_zero_ir2_opnd);
#ifdef CONFIG_LATX_LARGE_CC
        tb->tu_jmp[TU_TB_INDEX_TARGET] = target_label_opnd._label_id;
        la_nop();
#endif
        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);
        /* return true; */
    } else {
        tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    }
#endif
    /* if (env->tr_data->static_translation) */
    /*     env->tr_data->curr_tb->sbt_generate_linkage_jump_target(); */
    /* else */
    /*     env->tr_data->curr_tb->generate_tb_linkage(0); */

    tr_generate_exit_tb(pir1, 1);
    return true;
}

bool translate_jmpin(IR1_INST *pir1)
{
    /* 1. set successor x86 address */
    IR2_OPND succ_x86_addr_opnd = ra_alloc_dbt_arg2();
    load_ireg_from_ir1_2(succ_x86_addr_opnd, ir1_get_opnd(pir1, 0), ZERO_EXTENSION,
                         false);

    /* 2. indirect linkage */
    /* env->tr_data->curr_tb->generate_tb_linkage_indirect(); */
    tr_generate_exit_tb(pir1, 1);
    return true;
}


/*
 * The following pseudo code shows the formal definition of the ENTER instruction
 * [  1]PUSH EBP;
 * [  2]FRAME_PTR <- ESP;
 * [  3]IF LEVEL > 0 THEN
 * [3-1]    DO (LEVEL − 1) times
 * [3-2]            EBP ← EBP − 4;
 * [3-3]            PUSH Pointer(EBP); (* doubleword pointed to by EBP *)
 * [---]    OD;
 * [3-4]    PUSH FRAME_PTR;
 * [---]FI;
 * [  4]EBP ← FRAME_PTR;
 * [  5]ESP ← ESP − STORAGE;
*/
bool translate_enter(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassert(ir1_opnd_is_imm(opnd0) && ir1_opnd_is_imm(opnd1));
    IR2_OPND alloc_size_opnd = load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);
	int nesting_level = ir1_opnd_uimm(opnd1) & 31;

    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    int push_size = ir1_get_opnd_size(pir1);
    int esp_decrement = push_size >> 3;
    /**
     * [  1]PUSH EBP;
     * push value onto stack
     */
    IR1_OPND mem_ir1_opnd;

#ifndef TARGET_X86_64
    ir1_opnd_build_mem(&mem_ir1_opnd, esp_decrement << 3,
        dt_X86_REG_ESP, -esp_decrement);
#else
    if (CODEIS64) {
        ir1_opnd_build_mem(&mem_ir1_opnd, esp_decrement << 3,
            dt_X86_REG_RSP, -esp_decrement);
    } else {
        ir1_opnd_build_mem(&mem_ir1_opnd, esp_decrement << 3,
            dt_X86_REG_ESP, -esp_decrement);
    }
#endif
    IR2_OPND rbp_opnd = ra_alloc_gpr(ebp_index);
    IR2_OPND frame_temp_opnd = ra_alloc_itemp();
    IR2_OPND rbp_temp_opnd = ra_alloc_itemp();
    store_ireg_to_ir1(rbp_opnd, &mem_ir1_opnd, false);

    la_addi_addrx(esp_opnd, esp_opnd, -esp_decrement);

    if (nesting_level != 0) {
        /* [  2]FRAME_PTR <- ESP; */
        la_mov64(frame_temp_opnd, esp_opnd);
        int i;

        /**
         * [3-1]    DO (LEVEL − 1) times
         * Copy level-1 pointers from the previous frame.
         */
        for (i = 1; i < nesting_level; ++i) {
            /*[3-2] EBP ← EBP − 4;*/
            la_addi_addrx(rbp_opnd, rbp_opnd, -esp_decrement);
            /*[3-3] PUSH Pointer(EBP);*/
            la_load_addrx(rbp_temp_opnd, rbp_opnd, 0);
            store_ireg_to_ir1(rbp_temp_opnd, &mem_ir1_opnd, false);
            la_addi_addrx(esp_opnd, esp_opnd, -esp_decrement);
		}

        /**
         * [3-4] PUSH FRAME_PTR;
         *  Push the current FrameTemp as the last level.  */
        store_ireg_to_ir1(frame_temp_opnd, &mem_ir1_opnd, false);
        la_addi_addrx(esp_opnd, esp_opnd, -esp_decrement);
        /* [  4]EBP ← FRAME_PTR; */
        la_mov64(rbp_opnd, frame_temp_opnd);
	} else {
        /**
         * [4]EBP ← FRAME_PTR <- [2]FRAME_PTR <- ESP;
         * EBP <- ESP
         * */
        la_mov64(rbp_opnd, esp_opnd);
    }
    /* [  5]ESP ← ESP − STORAGE; */
    la_sub_d(esp_opnd, esp_opnd, alloc_size_opnd);

    return true;
}

/*
 * Intel64 Architecture Programmer’s Manual Volume 2:
 * LEAVE:
 * Reverse the action of the previous ENTER instruction.
 * 1. EBP -> ESP
 * 2. pop EBP (old ebp on stack -> ebp)
 *
 * NOTE:
 * leavew is specifically used for 16-bit operations.
 * when popping ebp, we cannot directly pop the 32-bit value to ebp
 * we should pop low 16-bit value on stack to low 16-bit value of ebp
 * e.g:
 * before pop:
 * esp: 0x48001892
 * ebp: 0x48001892
 * 0x48001892: 0x0ee01938   0x080b0f06
 *
 * after pop:
 * esp: 0x48001890
 * ebp: 0x48001938 (should not be 0x0ee01938)
 * 0x48001890: 0x080b0f06
 */
bool translate_leave(IR1_INST *pir1)
{
    IR2_OPND rsp_opnd = ra_alloc_gpr(esp_index);
    IR2_OPND rbp_opnd = ra_alloc_gpr(ebp_index);

#ifndef TARGET_X86_64
    la_bstrpick_d(rsp_opnd, rbp_opnd, 31, 0);
#else
    if (CODEIS64) {
        la_or(rsp_opnd, rbp_opnd, zero_ir2_opnd);
    } else {
        la_bstrpick_d(rsp_opnd, rbp_opnd, 31, 0);
    }
#endif

    /*
     * NOTE: if previous insn as below
     * add        ebp, 0x5c
     * Once result of ebp is sign-extension, that will cause segv.
     * To avoid this, invoke esp instead of ebp because ebp is equal to
     * esp with zero extension.
     * This change is win-win because there is no insn increasement.
     */
    int opnd_size = ir1_get_opnd_size(pir1) >> 3;
    if (opnd_size == 2) {
        IR2_OPND ebp_temp_opnd = ra_alloc_itemp();
        la_ld_hu(ebp_temp_opnd, rsp_opnd, 0);
        la_bstrins_w(rbp_opnd, ebp_temp_opnd, 16, 0);
    } else {
        la_load_addrx(rbp_opnd, rsp_opnd, 0);
    }
    la_addi_addrx(rsp_opnd, rsp_opnd, opnd_size);

    return true;
}

#ifdef CONFIG_LATX_SYSCALL_TUNNEL
bool translate_int_syscall(IR1_INST *pir1)
{
    IR2_OPND syscall_optimize_arr = ra_alloc_itemp();
    IR2_OPND syscall_specified = ra_alloc_itemp();
    IR2_OPND label_traditionanl = ra_alloc_label();
    IR2_OPND label_finish = ra_alloc_label();
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    TranslationBlock *tb = lsenv->tr_data->curr_tb;

    /* 1. load the syscall-judgement-array's address to itmp1, it is stroed    */
    /*    in lsenv and initialized in latx_lsenv_init;                         */
    /* 2. store eax which represents syscall number to itmp2;                  */
    /* 3. add itmp2 to itmp1, so we get the array member corresponding to      */
    /*    the syscall;                                                         */
    /* 4. if itmp2 equals to 0, goes optimized way, otherwise traditional way. */
    aot_load_host_addr(syscall_optimize_arr,
                        (ADDR)lsenv->syscall_optimize_confirm,
                        LOAD_SYSCALL_OPTIMIZE_CONFIRM, 0);
    la_add_addr(syscall_optimize_arr,
                                syscall_optimize_arr, eax_opnd);
    la_ld_b(syscall_specified,
                                syscall_optimize_arr, 0x0);
    la_beq(syscall_specified,
                                zero_ir2_opnd, label_traditionanl);

    /* ==================================================================== */
    /* ======================optimized syscall process===================== */
    /* ==================================================================== */
    /* 1. build syscall num, bit31~bit16 in a7 are the ARCH specified mask, */
    /*    and the syscall num of I386 is stored in EAX(s0).                 */
    /*    LA     : 0x0000 0000                                              */
    /*    MIPSN64: 0x0001 0001                                              */
    /*    I386   : 0x0010 0010                                              */
    /*    X86-64 : 0x0011 0011                                              */
    li_wu(a7_ir2_opnd, 0x100000);
    la_or(a7_ir2_opnd, a7_ir2_opnd, ra_alloc_gpr(eax_index));

    /* 2. build LA registers which pass parameters within syscall:  */
    /*    a0 <- ebx(s3), a1 <- ecx(s1), a2 <- edx(s2),              */
    /*    a3 <- esi(s6), a4 <- edi(s7), a5 <- ebp(s5), a6 <-> ?     */
    la_or(a0_ir2_opnd, zero_ir2_opnd, ra_alloc_gpr(ebx_index));
    la_or(a1_ir2_opnd, zero_ir2_opnd, ra_alloc_gpr(ecx_index));
    la_or(a2_ir2_opnd, zero_ir2_opnd, ra_alloc_gpr(edx_index));
    la_or(a3_ir2_opnd, zero_ir2_opnd, ra_alloc_gpr(esi_index));
    la_or(a4_ir2_opnd, zero_ir2_opnd, ra_alloc_gpr(edi_index));
    la_or(a5_ir2_opnd, zero_ir2_opnd, ra_alloc_gpr(ebp_index));

    /* 3. translate int to syscall(LA), and its' code operand is 0x0 by default */
    la_syscall(ir2_opnd_new(IR2_OPND_IMM, 0x0));

    /* 4. save the return value to EAX(s0) */
    la_or(s0_ir2_opnd, zero_ir2_opnd, a0_ir2_opnd);

    /* 5. finish */
    la_b(label_finish);

    /* ================================================================= */
    /* ===================traditional syscall process=================== */
    /* ================================================================= */
    la_label(label_traditionanl);
    /* 1. store gpr as they will be used in do_syscall */
    tr_save_registers_to_env(0xff, 0, 0, options_to_save());

    /* 2. store intno to CPUState */
    IR2_OPND intno = ra_alloc_itemp();
    li_guest_addr(intno, ir1_get_opnd(pir1, 0)->imm);
    la_st_w(intno, env_ir2_opnd,
                      lsenv_offset_exception_index(lsenv));
    ra_free_temp(intno);

    /* 3. store next pc to CPUX86State */
    IR2_OPND next_pc = ra_alloc_itemp();
    target_ulong call_offset __attribute__((unused)) =
            aot_get_call_offset(ir1_addr_next(pir1));
    aot_load_guest_addr(next_pc, ir1_addr_next(pir1),
                        LOAD_CALL_TARGET, call_offset);

    la_st_d(next_pc, env_ir2_opnd,
            lsenv_offset_exception_next_eip(lsenv));
    ra_free_temp(next_pc);

    /* 4. store curr_tb to last_executed_tb */
    IR2_OPND tb_opnd = ra_alloc_itemp();
    aot_load_host_addr(tb_opnd, (ADDR)tb, LOAD_TB_ADDR, 0);
    la_store_addr(tb_opnd, env_ir2_opnd,
                      lsenv_offset_of_last_executed_tb(lsenv));
    ra_free_temp(tb_opnd);

    /* 5. call helper function */
    IR2_OPND helper_addr_opnd = ra_alloc_dbt_arg2();
    aot_load_host_addr(helper_addr_opnd, (ADDR)helper_raise_int,
                        LOAD_HELPER_RAISE_INT, 0);

    la_jirl(ra_ir2_opnd, helper_addr_opnd, 0);
    /* 6. load registers from env */
    tr_load_registers_from_env(0x1, 0, 0, options_to_save());

    la_label(label_finish);
    ra_free_temp(syscall_optimize_arr);
    ra_free_temp(syscall_specified);
    return true;
}
#endif

bool translate_int(IR1_INST *pir1)
{
#ifdef TARGET_X86_64
    IR2_OPND codemode_value_opnd = ra_alloc_itemp();
    IR2_OPND not_64 = ra_alloc_label();
    la_ld_d(codemode_value_opnd, env_ir2_opnd, offsetof(CPUX86State, sys.codemode));
    la_beq(codemode_value_opnd, zero_ir2_opnd, not_64);
    ra_free_temp(codemode_value_opnd);
    tr_save_x64_8_registers_to_env(0xff, option_save_xmm);
    la_label(not_64);
#endif
    tr_save_fcsr_to_env();
#ifdef CONFIG_LATX_SYSCALL_TUNNEL
    if (ir1_get_opnd(pir1, 0)->imm == 0x80) {
        return translate_int_syscall(pir1);
    }
#endif
    tr_save_registers_to_env(0xff, 0xff, 0xff, options_to_save());

    /* * store intno to CPUState */
    IR2_OPND intno = ra_alloc_itemp();
    li_guest_addr(intno, ir1_get_opnd(pir1, 0)->imm);
    la_st_w(intno, env_ir2_opnd,
                      lsenv_offset_exception_index(lsenv));
    ra_free_temp(intno);

    /* * store next pc to CPUX86State */
    IR2_OPND next_pc = ra_alloc_itemp();
    target_ulong call_offset __attribute__((unused)) =
            aot_get_call_offset(ir1_addr_next(pir1));
    aot_load_guest_addr(next_pc, ir1_addr_next(pir1),
                        LOAD_CALL_TARGET, call_offset);

    la_store_addrx(next_pc, env_ir2_opnd,
                      lsenv_offset_exception_next_eip(lsenv));
    ra_free_temp(next_pc);

    /* * call helper function */
    IR2_OPND helper_addr_opnd = ra_alloc_dbt_arg2();
    aot_load_host_addr(helper_addr_opnd, (ADDR)helper_raise_int,
                        LOAD_HELPER_RAISE_INT, 0);

    la_jirl(ra_ir2_opnd, helper_addr_opnd, 0);
    /* * load registers from env */
    tr_load_registers_from_env(0x1, 0, 0, options_to_save());
    return true;
}

#ifndef TARGET_X86_64
bool translate_syscall(IR1_INST *pir1)
{
    return false;
}
#else
bool translate_syscall(IR1_INST *pir1)
{
    tr_save_fcsr_to_env();
    tr_save_registers_to_env(0xff, 0xff, 0xff, options_to_save());
    tr_save_x64_8_registers_to_env(0xff, 0xff);

    /* store exception index(EXCP_SYSCALL) to CPUState */
    IR2_OPND exception_index = ra_alloc_itemp();
    li_guest_addr(exception_index, EXCP_SYSCALL);
    la_st_w(exception_index, env_ir2_opnd,
                      lsenv_offset_exception_index(lsenv));

    /* store next pc to CPUX86State */
    IR2_OPND next_pc = ra_alloc_itemp();
    /* * li_guest_addr(next_pc, pir1->_inst_length + pir1->_addr); */
    target_ulong call_offset __attribute__((unused)) =
            aot_get_call_offset(ir1_addr_next(pir1));
    aot_load_guest_addr(next_pc, ir1_addr_next(pir1),
                        LOAD_CALL_TARGET, call_offset);

    la_store_addrx(next_pc, env_ir2_opnd,
                      lsenv_offset_exception_next_eip(lsenv));
    ra_free_temp(next_pc);

    /* call helper function */
    IR2_OPND helper_addr_opnd = ra_alloc_dbt_arg2();
    aot_load_host_addr(helper_addr_opnd, (ADDR)helper_raise_syscall,
                        LOAD_HELPER_RAISE_SYSCALL, 0);
    la_jirl(ra_ir2_opnd, helper_addr_opnd, 0);

    /* load registers from env */
    tr_load_registers_from_env(0x1, 0, 0, options_to_save());
    tr_load_x64_8_registers_from_env(0xf0, 0);

    return true;
}
#endif

bool translate_hlt(IR1_INST *pir1)
{
    la_andi(zero_ir2_opnd, zero_ir2_opnd, 0);
    return true;
}

bool translate_rdtsc(IR1_INST *pir1)
{
    IR2_OPND ir2_eax = ra_alloc_gpr(eax_index);
    IR2_OPND ir2_edx = ra_alloc_gpr(edx_index);
    la_rdtime_d(ir2_eax, zero_ir2_opnd);
    la_bstrpick_d(ir2_edx, ir2_eax, 63, 32);
    la_bstrpick_d(ir2_eax, ir2_eax, 31, 0);

    return true;
}

bool translate_rdtscp(IR1_INST *pir1)
{
    IR2_OPND ir2_eax = ra_alloc_gpr(eax_index);
    IR2_OPND ir2_ecx = ra_alloc_gpr(ecx_index);
    IR2_OPND ir2_edx = ra_alloc_gpr(edx_index);
    la_rdtime_d(ir2_eax, ir2_ecx);
    la_bstrpick_d(ir2_edx, ir2_eax, 63, 32);
    la_bstrpick_d(ir2_eax, ir2_eax, 31, 0);
    la_bstrpick_d(ir2_ecx, ir2_ecx, 31, 0);

    return true;
}

bool translate_cpuid(IR1_INST *pir1)
{
    /* 1. store registers to env */
    tr_save_registers_to_env(EAX_USEDEF_BIT | ECX_USEDEF_BIT, 0, 0, options_to_save());
#ifdef TARGET_X86_64
    /* R9/R12/R13/R14/R10/R11 need save */
     tr_save_x64_8_registers_to_env(GPR_USEDEF_TO_SAVE >> 8, 0);
#endif
    /* 2. call helper */
    IR2_OPND helper_addr_opnd = ra_alloc_dbt_arg2();
    IR2_OPND a0_opnd = a0_ir2_opnd;

    TranslationBlock *tb __attribute__((unused)) = lsenv->tr_data->curr_tb;
    aot_load_host_addr(helper_addr_opnd, (ADDR)helper_cpuid,
                        LOAD_HELPER_CPUID, 0);
    la_mov64(a0_opnd, env_ir2_opnd);
    la_jirl(ra_ir2_opnd,
        helper_addr_opnd, 0);

    /* 3. load registers from env */
    tr_load_registers_from_env(
        EAX_USEDEF_BIT | ECX_USEDEF_BIT | EDX_USEDEF_BIT | EBX_USEDEF_BIT, 0,
        0, options_to_save());
#ifdef TARGET_X86_64
    /* R9/R12/R13/R14 need load */
    tr_load_x64_8_registers_from_env(GPR_USEDEF_TO_SAVE >> 8, 0);
#endif
    return true;
}

bool translate_cwd(IR1_INST *pir1)
{
    IR2_OPND ax = ra_alloc_gpr(eax_index);
    IR2_OPND dx = ra_alloc_itemp();
    la_ext_w_h(dx, ax);
    la_srai_w(dx, dx, 16);
    store_ireg_to_ir1(dx, &dx_ir1_opnd, false);
    ra_free_temp(dx);
    return true;
}

bool translate_cdq(IR1_INST *pir1)
{
    IR2_OPND eax = ra_alloc_gpr(eax_index);
    IR2_OPND edx = ra_alloc_itemp();
    la_srai_w(edx, eax, 31);
    store_ireg_to_ir1(edx, &edx_ir1_opnd, false);
    ra_free_temp(edx);
    return true;
}

#ifndef TARGET_X86_64
bool translate_cqo(IR1_INST *pir1)
{
    return false;
}
#else
bool translate_cqo(IR1_INST *pir1)
{
    IR2_OPND rax = ra_alloc_gpr(rax_index);
    IR2_OPND rdx = ra_alloc_gpr(rdx_index);
    la_srai_d(rdx, rax, 63);
    return true;
}
#endif

bool translate_sahf(IR1_INST *pir1)
{
    IR2_OPND ah = ra_alloc_itemp();
    load_ireg_from_ir1_2(ah, &ah_ir1_opnd, ZERO_EXTENSION, false);
    if (ir1_need_calculate_any_flag(pir1)) {
        la_x86mtflag(ah, SF_USEDEF_BIT | ZF_USEDEF_BIT |
                    AF_USEDEF_BIT | PF_USEDEF_BIT | CF_USEDEF_BIT);
    }
    ra_free_temp(ah);
    return true;
}

bool translate_lahf(IR1_INST *pir1)
{
    IR2_OPND ah = ra_alloc_itemp();
    la_x86mfflag(ah, 0x1f);
    la_ori(ah, ah, 0x2);
    store_ireg_to_ir1(ah, &ah_ir1_opnd, false);
    ra_free_temp(ah);
    return true;
}

bool translate_loopnz(IR1_INST *pir1)
{
    IR2_OPND ir2_xcx = ra_alloc_gpr(ir1_opnd_base_reg_num(&ecx_ir1_opnd));

    if (ir1_addr_size(pir1) == 64) {
        la_addi_d(ir2_xcx, ir2_xcx, -1);
    } else if (ir1_addr_size(pir1) == 32) {
        la_addi_w(ir2_xcx, ir2_xcx, -1);
        la_bstrpick_d(ir2_xcx, ir2_xcx, 31, 0);
    } else {
        IR2_OPND tmp_opnd = ra_alloc_itemp();
        la_addi_d(tmp_opnd, ir2_xcx, -1);
        la_bstrins_w(ir2_xcx, tmp_opnd, 16, 0);
    }
    IR2_OPND target_label = ra_alloc_label();

    IR2_OPND temp_zf = ra_alloc_itemp();

    get_eflag_condition(&temp_zf, pir1);

    //By now, if zf =0, then temp_zf =0 else temp_zf = 0xFFFFFFFFFFFFFFFF
    la_nor(temp_zf, temp_zf, zero_ir2_opnd);
    la_and(temp_zf, temp_zf, ir2_xcx);
#ifdef CONFIG_LATX_TU
    IR2_OPND translated_label_opnd = ra_alloc_label();
    TranslationBlock *tb = lsenv->tr_data->curr_tb;

    if (tb->s_data->next_tb[TU_TB_INDEX_NEXT] && tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {
        IR2_OPND target_label2 = ra_alloc_label();
        la_label(target_label2);
        la_bne(temp_zf, zero_ir2_opnd, target_label2);
#ifdef CONFIG_LATX_LARGE_CC
        tb->tu_jmp[TU_TB_INDEX_TARGET] = target_label2._label_id;
        /* la_code_align(2, 0x03400000); */
        /* la_nop(); */
        /* la_nop(); */
        tu_jcc_nop_gen(tb);
#endif
        /* tb->jmp_target_arg[0] = target_label2._label_id; */

        /* next_tb[TU_TB_INDEX_NEXT] already be translated */
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }
        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);
        /* return true; */
    } else {
        tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    }

#endif
    la_bne(temp_zf, zero_ir2_opnd, target_label);
    ra_free_temp(temp_zf);
    /* env->tr_data->curr_tb->generate_tb_linkage(0); */
    tr_generate_exit_tb(pir1, 0);
    la_label(target_label);
    /* env->tr_data->curr_tb->generate_tb_linkage(1); */
    tr_generate_exit_tb(pir1, 1);
    return true;
}

bool translate_loopz(IR1_INST *pir1)
{
    IR2_OPND ir2_xcx = ra_alloc_gpr(ir1_opnd_base_reg_num(&ecx_ir1_opnd));

    if (ir1_addr_size(pir1) == 64) {
        la_addi_d(ir2_xcx, ir2_xcx, -1);
    } else if (ir1_addr_size(pir1) == 32) {
        la_addi_w(ir2_xcx, ir2_xcx, -1);
        la_bstrpick_d(ir2_xcx, ir2_xcx, 31, 0);
    } else {
        IR2_OPND tmp_opnd = ra_alloc_itemp();
        la_addi_d(tmp_opnd, ir2_xcx, -1);
        la_bstrins_w(ir2_xcx, tmp_opnd, 16, 0);
    }
    IR2_OPND target_label = ra_alloc_label();

    IR2_OPND temp_zf = ra_alloc_itemp();

    get_eflag_condition(&temp_zf, pir1);

    //By now, if zf =0, then temp_zf =0 else temp_zf = 0xFFFFFFFFFFFFFFFF
    la_and(temp_zf, temp_zf, ir2_xcx);
#ifdef CONFIG_LATX_TU
    IR2_OPND translated_label_opnd = ra_alloc_label();
    TranslationBlock *tb = lsenv->tr_data->curr_tb;

    if (tb->s_data->next_tb[TU_TB_INDEX_NEXT] && tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {
        IR2_OPND target_label2 = ra_alloc_label();
        la_label(target_label2);
        la_bne(temp_zf, zero_ir2_opnd, target_label2);
#ifdef CONFIG_LATX_LARGE_CC
        tb->tu_jmp[TU_TB_INDEX_TARGET] = target_label2._label_id;
        /* la_code_align(2, 0x03400000); */
        /* la_nop(); */
        /* la_nop(); */
        tu_jcc_nop_gen(tb);
#endif
        /* tb->jmp_target_arg[0] = target_label2._label_id; */

        /* next_tb[TU_TB_INDEX_NEXT] already be translated */
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }
        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);
        /* return true; */
    } else {
        tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    }

#endif
    la_bne(temp_zf, zero_ir2_opnd, target_label);
    ra_free_temp(temp_zf);
    /* env->tr_data->curr_tb->generate_tb_linkage(0); */
    tr_generate_exit_tb(pir1, 0);
    la_label(target_label);
    /* env->tr_data->curr_tb->generate_tb_linkage(1); */
    tr_generate_exit_tb(pir1, 1);
    return true;
}

bool translate_loop(IR1_INST *pir1)
{
    IR2_OPND ir2_xcx = ra_alloc_gpr(ir1_opnd_base_reg_num(&ecx_ir1_opnd));

    if (ir1_addr_size(pir1) == 64) {
        la_addi_d(ir2_xcx, ir2_xcx, -1);
    } else if (ir1_addr_size(pir1) == 32) {
        la_addi_w(ir2_xcx, ir2_xcx, -1);
        la_bstrpick_d(ir2_xcx, ir2_xcx, 31, 0);
    } else {
        IR2_OPND tmp_opnd = ra_alloc_itemp();
        la_addi_d(tmp_opnd, ir2_xcx, -1);
        la_bstrins_w(ir2_xcx, tmp_opnd, 16, 0);
    }
    IR2_OPND target_label = ra_alloc_label();
#ifdef CONFIG_LATX_TU
    IR2_OPND translated_label_opnd = ra_alloc_label();
    TranslationBlock *tb = lsenv->tr_data->curr_tb;

    if (tb->s_data->next_tb[TU_TB_INDEX_NEXT] && tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {

        IR2_OPND target_label2 = ra_alloc_label();
        la_label(target_label2);
        la_bne(ir2_xcx, zero_ir2_opnd, target_label2);
#ifdef CONFIG_LATX_LARGE_CC
        tb->tu_jmp[TU_TB_INDEX_TARGET] = target_label2._label_id;
        /* la_code_align(2, 0x03400000); */
        /* la_nop(); */
        /* la_nop(); */
        tu_jcc_nop_gen(tb);
#endif
        /* tb->jmp_target_arg[0] = target_label2._label_id; */

        /* next_tb[TU_TB_INDEX_NEXT] already be translated */
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }
        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);
        /* return true; */
    } else {
        tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    }

#endif
    la_bne(ir2_xcx, zero_ir2_opnd, target_label);
    /* env->tr_data->curr_tb->generate_tb_linkage(0); */
    tr_generate_exit_tb(pir1, 0);
    la_label(target_label);
    /* env->tr_data->curr_tb->generate_tb_linkage(1); */
    tr_generate_exit_tb(pir1, 1);
    return true;
}

bool translate_cmc(IR1_INST *pir1)
{
    if (ir1_need_calculate_any_flag(pir1)) {
        IR2_OPND cf = ra_alloc_itemp();
        la_x86mfflag(cf, CF_USEDEF_BIT);
        la_xori(cf, cf, CF_USEDEF_BIT);
        la_x86mtflag(cf, CF_USEDEF_BIT);
        ra_free_temp(cf);
    }
    return true;
}

bool translate_cbw(IR1_INST *pir1)
{
    IR2_OPND temp = ra_alloc_itemp();
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    la_ext_w_b(temp, eax_opnd);
    la_bstrins_d(eax_opnd, temp, 15, 0);
    return true;
}

bool translate_cwde(IR1_INST *pir1)
{
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    la_ext_w_h(eax_opnd, eax_opnd);
#ifdef TARGET_X86_64
    /* x64 need clean the high 32 bits */
    if (!GHBR_ON(pir1) && CODEIS64) {
        la_mov32_zx(eax_opnd, eax_opnd);
    }
#endif
    return true;
}

#ifndef TARGET_X86_64
bool translate_cdqe(IR1_INST *pir1)
{
    return false;
}
#else
bool translate_cdqe(IR1_INST *pir1)
{
    IR2_OPND rax_opnd = ra_alloc_gpr(rax_index);
    /*elimiate the extension mode*/
    /* if (!ir2_opnd_is_sx(&rax_opnd, 32)) {
           la_slli_w(rax_opnd, rax_opnd, 0);
       }
     */
    la_slli_w(rax_opnd, rax_opnd, 0);
    return true;
}
#endif

bool translate_fnsave(IR1_INST *pir1)
{
#ifdef CONFIG_LATX_DEBUG
    printf("FIXME:fnsave/fsave didn't verify in 100, please investigate this insn first if you are in trouble\n");
#endif

    IR2_OPND mode_fpu = ra_alloc_itemp();
    IR2_OPND label_mmx = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    /* store x87 state information */
    IR2_OPND value = ra_alloc_itemp();
    IR2_OPND temp = ra_alloc_itemp();
    li_wu(temp, 0xffff0000ULL);

    /* mem_opnd is not supported in ir2 assemble */
    /* convert mem_opnd to ireg_opnd */

    IR1_OPND* opnd1 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd1);

    la_ld_h(value, env_ir2_opnd,
                      lsenv_offset_of_control_word(lsenv)); /* control_word */
    la_or(value, temp, value);
    la_st_w(value, mem_opnd, 0);

    update_sw_by_fcsr(value);
    la_or(value, temp, value);
    la_st_w(value, mem_opnd, 4);

    if (option_fputag) {
        /* store FPU tag word from env to memory */
        tr_fpu_store_tag_to_mem(mem_opnd, 8);
    } else {
        la_ld_h(value, env_ir2_opnd,
                          lsenv_offset_of_tag_word(lsenv)); /* tag_word */

        la_or(value, temp, value);
        /* dispose tag word */
        IR2_OPND temp_1 = ra_alloc_itemp();
        li_wu(temp_1, 0xffff0000ULL);
        la_and(value, value, temp_1);
        ra_free_temp(temp_1);
        la_st_w(value, mem_opnd, 8);
        ra_free_temp(value);

        la_st_w(temp, mem_opnd, 24);

    }

    /* check current mode(mmx/fpu) */
    la_ld_wu(mode_fpu, env_ir2_opnd, lsenv_offset_of_mode_fpu(lsenv));
    la_beq(mode_fpu, zero_ir2_opnd, label_mmx);

    /* store fpu data registers stack */
    int i;
    for (i = 0; i <= 7; i++) {
        IR2_OPND st = ra_alloc_st(i);
        store_64_bit_freg_to_ir1_80_bit_mem(st, mem_opnd, 28 + 10 * i);
    }
    la_b(label_exit);

    la_label(label_mmx);
    /* store mmx data registers stack */
    for (i = 0; i <= 7; i++) {
        IR2_OPND st = ra_alloc_st(i);
        la_movfr2gr_d(temp, st);
        la_st_d(temp, mem_opnd, 28 + 10 * i);
    }

    la_label(label_exit);
    /* reset SR and CR */
    translate_fninit(pir1);
    ra_free_temp(temp);
    ra_free_temp(mode_fpu);
    return true;
}

bool translate_frstor(IR1_INST *pir1)
{
    /*
    * Loads the FPU state (operating environment and register stack)
    * from the memory area specified with the source
    * operand. This state data is typically written to the specified
    * memory location by a previous FSAVE/FNSAVE instruction.
    * FPUControlWord ← SRC[FPUControlWord];
    * FPUStatusWord ← SRC[FPUStatusWord];
    * FPUTagWord ← SRC[FPUTagWord];
    * FPUDataPointer ← SRC[FPUDataPointer];
    * FPUInstructionPointer ← SRC[FPUInstructionPointer];
    * FPULastInstructionOpcode ← SRC[FPULastInstructionOpcode];
    * ST(0) ← SRC[ST(0)];
    * ST(1) ← SRC[ST(1)];
    * ST(2) ← SRC[ST(2)];
    * ST(3) ← SRC[ST(3)];
    * ST(4) ← SRC[ST(4)];
    * ST(5) ← SRC[ST(5)];
    * ST(6) ← SRC[ST(6)];
    * ST(7) ← SRC[ST(7)];
     */
#ifdef CONFIG_LATX_DEBUG
    printf("FIXME:frstor didn't verify in 100, please investigate this insn first if you are in trouble\n");
#endif
    IR2_OPND mode_fpu = ra_alloc_itemp();
    IR2_OPND label_mmx = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND value = ra_alloc_itemp();
    IR2_OPND temp = ra_alloc_itemp();
    li_wu(temp, 0xffff0000ULL);

    /* mem_opnd is not supported in ir2 assemble */
    /* convert mem_opnd to ireg_opnd */

    IR1_OPND* opnd1 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd1);
    ir2_set_opnd_type(&mem_opnd, IR2_OPND_GPR);

    la_ld_w(value, mem_opnd, 0);
    la_mov32_zx(value, value);
    la_st_h(value, env_ir2_opnd,
        lsenv_offset_of_control_word(lsenv)); /* control_word */

    la_ld_w(value, mem_opnd, 4);
    la_mov32_zx(value, value);
    la_st_h(value, env_ir2_opnd,
                      lsenv_offset_of_status_word(lsenv)); /* status_word */

    /* tag word */
    if (option_fputag) {
        /* load FPU tag word from memory to env*/
        la_ld_h(value, mem_opnd, 8);
        tr_fpu_load_tag_to_env(value);

    } else {
        la_ld_d(value, mem_opnd, 8);
        la_st_d(value, env_ir2_opnd,
                          lsenv_offset_of_tag_word(lsenv));
    }
    ra_free_temp(value);

    /* check current mode(mmx/fpu) */
    la_ld_wu(mode_fpu, env_ir2_opnd, lsenv_offset_of_mode_fpu(lsenv));
    la_beq(mode_fpu, zero_ir2_opnd, label_mmx);
    int i;
    for (i = 0; i <= 7; i++) {
        IR2_OPND st = ra_alloc_st(i);
        load_64_bit_freg_from_ir1_80_bit_mem(
            st, mem_opnd, 28 + 10 * i);
    }
    la_b(label_exit);

    la_label(label_mmx);
    for (i = 0; i <= 7; i++) {
        IR2_OPND st = ra_alloc_st(i);
        la_ld_d(temp, mem_opnd, 28 + 10 * i);
        la_movgr2fr_d(st, temp);
    }

    la_label(label_exit);
    ra_free_temp(temp);
    ra_free_temp(mode_fpu);
    return true;
}

bool translate_prefetcht0(IR1_INST *pir1)
{
    int offset;
    IR2_OPND mem_opnd = convert_mem(ir1_get_opnd(pir1, 0), &offset);
    /* if the value of zero_imm_opnd is 0, it is load.
     * if the value of zero_imm_opnd is 8, it is store.
     * if the value of zero_imm_opnd is others, it is invalid.
     */
    int imm = 0;

    la_preld(imm, mem_opnd, offset);
    return true;
}

bool translate_prefetchw(IR1_INST *pir1)
{
    int offset;
    IR2_OPND mem_opnd = convert_mem(ir1_get_opnd(pir1, 0), &offset);
    /*
     * if the value of zero_imm_opnd is 0, it is load.
     * if the value of zero_imm_opnd is 8, it is store.
     * if the value of zero_imm_opnd is others, it is invalid.
     */
    int imm = 8;

    la_preld(imm, mem_opnd, offset);
    return true;
}

/*  Set the x87 FPU tag word to empty: x87FPUTagWord ← FFFFH */
bool translate_emms(IR1_INST *pir1)
{
    IR2_OPND temp = ra_alloc_itemp();

    int tag_offset = lsenv_offset_of_tag_word(lsenv);
    lsassert(tag_offset <= 0x7ff);

    /**
     * Every 2 bits of FPTAGS is simulated as 8 bits in lsenv, and there
     * are only 2 states:
     * 00H: valid
     * 01H: invalid
     */
    li_d(temp, 0x101010101010101ULL);
    la_st_d(temp, env_ir2_opnd, tag_offset);

    /* transfer to fpu mode */
    transfer_to_fpu_mode();

    ra_free_temp(temp);
    return true;
}

bool translate_xlat(IR1_INST *pir1)
{
    /* TODO: ds override, also address size */
    IR2_OPND value_opnd = ra_alloc_itemp();
    IR2_OPND al_opnd =
        load_ireg_from_ir1(&al_ir1_opnd, ZERO_EXTENSION, false);
    IR2_OPND ebx_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(&ebx_ir1_opnd));
#ifndef TARGET_X86_64
    clear_h32(&ebx_opnd);
#else
    if (CODEIS64) {
        int addr_size = ir1_addr_size(pir1);
        IR2_OPND ebx_tmp = ra_alloc_itemp();
        if (addr_size == 32) {
            la_mov32_zx(ebx_tmp, ebx_opnd);
            ebx_opnd = ebx_tmp;
        } else if (addr_size == 16) {
            la_bstrpick_d(ebx_tmp, ebx_opnd, 15, 0);
            ebx_opnd = ebx_tmp;
        }
        /*
         * attention!! This place free ebx_tmp is only safe when
         * follow did not alloc itemp again.
         */
        ra_free_temp(ebx_tmp);
    } else {
        clear_h32(&ebx_opnd);
    }
#endif
    la_ldx_b(value_opnd, ebx_opnd, al_opnd);
    store_ireg_to_ir1(value_opnd, &al_ir1_opnd, false);
    return true;
}

bool translate_crc32(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    uint32 opnd0_size = ir1_opnd_size(opnd0);
    uint32 opnd1_size = ir1_opnd_size(opnd1);

    IR2_OPND src0 = load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);
    IR2_OPND src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    IR2_OPND dest = ra_alloc_itemp();

    if (opnd1_size == 8) {
        la_crcc_w_b_w(dest, src1, src0);
    } else if(opnd1_size == 16) {
        la_crcc_w_h_w(dest, src1, src0);
    } else if(opnd1_size == 32) {
        la_crcc_w_w_w(dest, src1, src0);
    } else if(opnd0_size == 64) {
        la_crcc_w_d_w(dest, src1, src0);
    }

    la_bstrpick_d(src0, dest, 31, 0);
    /* X86 manual define high bits should be save,
     * but test result show those bits be cleaned.
     *
     * if (opnd0_size == 32) {
     *     la_bstrins_d(src0, dest, 31, 0);
     * } else if(opnd0_size == 64) {
     *     la_bstrpick_d(src0, dest, 31, 0);
     * }
     */

    return true;
}
