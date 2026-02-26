#include "config-host.h"
#include "common.h"
#include "diStorm/distorm.h"
#include "ir1.h"
#include "ir2.h"
#include "lsenv.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "ir1-optimization.h"
#include "profile.h"
#include "trace.h"
#include "translate.h"
#include "latx-config.h"
#include "syscall-tunnel.h"

#ifdef CONFIG_LATX_TU
#include "tu.h"
void target_disasm(struct TranslationBlock *tb, int max_insns)
{
    /* max_insns = 100; */
    counter_tb_tr += 1;

    trace_xtm_tr_tb((void *)tb, (void *)tb->tc.ptr,
                    (void *)(unsigned long long)tb->pc);

    if (option_dump) {
        qemu_log("=====================================\n");
        qemu_log("|| TB translation : %14p ||\n", tb);
        qemu_log("=====================================\n");
    }

    /* target => IR1
     * IR1 stored in lsenv->tr_data
     */
#ifdef CONFIG_LATX_PROFILER
    TCGProfile *prof = &tcg_ctx->prof;
    int64_t ti = profile_getclock();
#endif

    ADDRX pc = tb->pc;
    /* get ir1 instructions */
    tb->s_data->ir1 = get_ir1_list(tb, pc, max_insns);

    lsenv->tr_data->curr_ir1_inst = NULL;
#if defined(CONFIG_LATX_FLAG_REDUCTION) && \
    defined(CONFIG_LATX_FLAG_REDUCTION_EXTEND)
    tb->_tb_type = get_etb_type(ir1_list + ir1_num - 1);

    if (option_flag_reduction) {
        tb_add_succ(etb, 2);
        tb->flags |= SUCC_IS_SET_MASK;
    }
#endif

#ifdef CONFIG_LATX_DEBUG
    counter_ir1_tr += tb->icount;
#endif

    /* tr_disasm(tb, max_insns); */

#ifdef CONFIG_LATX_PROFILER
    qatomic_set(&prof->tr_disasm_time,
                prof->tr_disasm_time + profile_getclock() - ti);
#endif

    /* ir1_optimization(tb); */
}
#endif

int target_latx_host(CPUArchState *env, struct TranslationBlock *tb,
                     int max_insns)
{
    counter_tb_tr += 1;

    trace_xtm_tr_tb((void *)tb, (void *)tb->tc.ptr,
                    (void *)(unsigned long long)tb->pc);

    if (option_dump) {
        qemu_log("=====================================\n");
        qemu_log("|| TB translation : %14p ||\n", tb);
        qemu_log("=====================================\n");
    }

    /* target => IR1
     * IR1 stored in lsenv->tr_data
     */
#ifdef CONFIG_LATX_PROFILER
    TCGProfile *prof = &tcg_ctx->prof;
    int64_t ti = profile_getclock();
#endif
    tr_disasm(tb, max_insns);
    /* return code_size and skip translate */
    if (!tb->icount && tb->pc < reserved_va) {
        return 0;
    }

#ifdef CONFIG_LATX_PROFILER
    qatomic_set(&prof->tr_disasm_time,
                prof->tr_disasm_time + profile_getclock() - ti);
#endif

    ir1_optimization(tb);

    /* IR1 => IR2 => host
     * IR2 stored in lsenv->tr_data
     * host write into TB
     */
    return tr_translate_tb(tb);
}

#ifdef CONFIG_LATX_DEBUG
#include "debug.h"
void trace_tb_execution(struct TranslationBlock *tb)
{
    lsassert(tb != NULL);

    if (!option_trace_tb && !option_trace_ir1) {
        return;
    }

    fprintf(stderr, "[trace] ========================\n");
    fprintf(stderr, "[trace] TB to execute\n");

    if (option_trace_tb) {
        fprintf(stderr, "[trace] ========================\n");
        fprintf(stderr, "[trace] TB      = %-18p , TB's address\n", (void *)tb);
        fprintf(stderr, "[trace] Counter = %-18lld , TB's execution\n",
                counter_tb_exec);
        fprintf(stderr, "[trace] Counter = %-18lld , TB's translation\n",
                counter_tb_tr);
        fprintf(stderr, "[trace] Counter = %-18lld , IR1 translated\n",
                counter_ir1_tr);
        fprintf(stderr, "[trace] Counter = %-18lld , MIPS generated\n",
                counter_mips_tr);
        fprintf(stderr, "[trace] PC      = %-18p , target's virtual address\n",
                (void *)(unsigned long)tb->pc);
        fprintf(stderr, "[trace] csbase  = %-18p , target's CS segment base\n",
                (void *)0);
        fprintf(stderr, "[trace] size    = %-18ld , TB's target code size\n",
                (unsigned long)tb->size);
        fprintf(stderr, "[trace] tc.ptr  = %-18p , TB's host code size\n",
                (void *)tb->tc.ptr);
        fprintf(stderr, "[trace] tc.size = %-18ld , TB's host code size\n",
                (unsigned long)tb->tc.size);
        fprintf(stderr, "[trace] host nr = %-18ld , TB's host code number\n",
                (unsigned long)tb->tc.size / 4);
    }


    IR1_INST *ir1_list = tb_ir1_inst(tb, 0);
    IR1_INST *pir1 = NULL;
    int ir1_nr = tb->icount;

    int i = 0;

    if (option_trace_ir1) {
        fprintf(stderr, "[trace] ========================\n");
        fprintf(stderr, "[trace] ir1_nr  = %-18ld , TB's IR1 code size\n",
                (unsigned long)ir1_nr);
        for (i = 0; i < ir1_nr; ++i) {
            pir1 = ir1_list + i;
            fprintf(stderr, "[trace] ");
            ir1_dump(pir1);
            fprintf(stderr, "\n");
        }
    }

    fprintf(stderr, "[trace] ========================\n");
}
#endif

#if defined(CONFIG_LATX_KZT)
__attribute__((unused))
#include "qemu.h"

static void kzt_helper1(ADDR func)
{
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)func);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);
}

void kzt_native_to_wrapper(void);
void kzt_wrapper_to_native(void);
static void generate_ld_callback_context(void *code_buf, void (*kzt_tb_callback)(CPUX86State *))
{
    kzt_native_to_wrapper();
    kzt_helper1((ADDR)kzt_tb_callback);
    kzt_wrapper_to_native();
    la_nop();//for resolve tb;
    la_nop();//for resolve tb;
    la_nop();//for long jmp
    la_nop();//for long jmp
}
/*
 * ld_callback
 */
int target_latx_ld_callback(void *code_buf_addr, void (*kzt_tb_callback)(CPUX86State *))
{
    int code_nr = 0;
    TRANSLATION_DATA *lat_ctx = lsenv->tr_data;

    if (option_dump)
        qemu_log("[LATX] ld_callback_context = %p\n",
                 (void *)code_buf_addr);

    tr_init(NULL);
    generate_ld_callback_context(code_buf_addr, kzt_tb_callback);
    label_dispose(NULL, lat_ctx);
    code_nr = tr_ir2_assemble((void *)code_buf_addr,
                              lat_ctx->first_ir2);
    tr_fini(false);
    return code_nr;
}
#endif

void latx_interpret_glue_init(void)
{
    interpret_glue = mmap(NULL, qemu_host_page_size,
                            PROT_EXEC | PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (interpret_glue == MAP_FAILED) {
        fprintf(stderr, "[LATX-LOG] alloc interpret_glue error!\n");
    }
}

/*
 * prologue <=> bt to native
 */
int target_latx_prologue(void *code_buf_addr)
{
    int code_nr = 0;
    TRANSLATION_DATA *lat_ctx = lsenv->tr_data;

    lsassert(context_switch_bt_to_native == 0);
    context_switch_bt_to_native = (ADDR)code_buf_addr;

    if (option_dump)
        qemu_log("[LATX] context_switch_bt_to_native = %p\n",
                 (void *)context_switch_bt_to_native);

    tr_init(NULL);
    generate_context_switch_bt_to_native(code_buf_addr);
    label_dispose(NULL, lat_ctx);
    code_nr = tr_ir2_assemble((void *)context_switch_bt_to_native,
                              lat_ctx->first_ir2);
    tr_fini(false);

    return code_nr;
}

/*
 * epilogue <=> native to bt
 */
int target_latx_epilogue(void *code_buf_addr)
{
    int code_nr = 0;
    TRANSLATION_DATA *lat_ctx = lsenv->tr_data;

    lsassert(context_switch_native_to_bt == 0);
    context_switch_native_to_bt_ret_0 = (ADDR)code_buf_addr;
    /*
     *  0:  save eip to env     ret_0
     *  4:  mov  a0, zero
     *  8:  b    cs
     * 12:  ori  a0, a0, 0x3    ret_id_3
     * 16:  b    cs
     * 20:  ori  a0, a0, 0x1    ret_id_1
     * 24:  b    cs
     * 28:  ori  a0, a0, 0x0    ret_id_0
     * 32:  context switch      native_to_bt
     */
    context_switch_native_to_bt_ret_id_3 = (ADDR)code_buf_addr + 12;
    context_switch_native_to_bt_ret_id_1 = (ADDR)code_buf_addr + 20;
    context_switch_native_to_bt_ret_id_0 = (ADDR)code_buf_addr + 28;
    context_switch_native_to_bt = (ADDR)code_buf_addr + 32;

    if (option_dump)
        qemu_log("[LATX] context_switch_native_to_bt = %p\n",
                 (void *)context_switch_native_to_bt);

    tr_init(NULL);
    generate_context_switch_native_to_bt();
    label_dispose(NULL, lat_ctx);
    code_nr = tr_ir2_assemble((void *)context_switch_native_to_bt_ret_0,
                              lat_ctx->first_ir2);
    tr_fini(false);

    return code_nr;
}

#ifdef CONFIG_LATX_DEBUG
void latx_before_exec_trace_tb(CPUArchState *env, struct TranslationBlock *tb)
{
    if (option_trace_tb)
        fprintf(stderr,
                "[LATX] before executing TB {PC = %p, code at %p}.\n",
                (void *)(unsigned long)tb->pc, (void *)tb->tc.ptr);
    counter_tb_exec += 1;
}

void latx_after_exec_trace_tb(CPUArchState *env, struct TranslationBlock *tb)
{
    if (option_trace_tb)
        fprintf(stderr,
                "[LATX] after  executing TB {PC = %p, code at %p}.\n",
                (void *)(unsigned long)tb->pc, (void *)tb->tc.ptr);
}

#endif

/*
 * native rotate fpu by
 */
int target_latx_fpu_rotate(void *code_buf_addr)
{
    return generate_native_rotate_fpu_by(code_buf_addr);
}

static void global_register_init(void)
{
    env_ir2_opnd = INIT_RA(IR2_OPND_GPR, reg_statics_map[S_ENV]);

    imm_zero_ir2_opnd = INIT_IMM(0);
    /* All LA reg */
    zero_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_zero);
    ra_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_ra);
    tp_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_tp);
    sp_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_sp);
    a0_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_a0);
    a1_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_a1);
    a2_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_a2);
    a3_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_a3);
    a4_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_a4);
    a5_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_a5);
    a6_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_a6);
    a7_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_a7);
    t0_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_t0);
    t1_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_t1);
    t2_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_t2);
    t3_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_t3);
    t4_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_t4);
    t5_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_t5);
    t6_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_t6);
    t7_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_t7);
    t8_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_t8);
    r21_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_r21);
    fp_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_fp);
    s0_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_s0);
    s1_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_s1);
    s2_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_s2);
    s3_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_s3);
    s4_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_s4);
    s5_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_s5);
    s6_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_s6);
    s7_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_s7);
    s8_ir2_opnd = INIT_RA(IR2_OPND_GPR, la_s8);
    fcsr_ir2_opnd = INIT_RA(IR2_OPND_FCSR, 0);
    fcsr1_ir2_opnd = INIT_RA(IR2_OPND_FCSR, 1);
    fcsr2_ir2_opnd = INIT_RA(IR2_OPND_FCSR, 2);
    fcsr3_ir2_opnd = INIT_RA(IR2_OPND_FCSR, 3);
    fcc0_ir2_opnd = INIT_RA(IR2_OPND_CC, 0);
    fcc1_ir2_opnd = INIT_RA(IR2_OPND_CC, 1);
    fcc2_ir2_opnd = INIT_RA(IR2_OPND_CC, 2);
    fcc3_ir2_opnd = INIT_RA(IR2_OPND_CC, 3);
    fcc4_ir2_opnd = INIT_RA(IR2_OPND_CC, 4);
    fcc5_ir2_opnd = INIT_RA(IR2_OPND_CC, 5);
    fcc6_ir2_opnd = INIT_RA(IR2_OPND_CC, 6);
    fcc7_ir2_opnd = INIT_RA(IR2_OPND_CC, 7);

    scr0_ir2_opnd = INIT_RA(IR2_OPND_SCR, 0);
    scr1_ir2_opnd = INIT_RA(IR2_OPND_SCR, 1);
    scr2_ir2_opnd = INIT_RA(IR2_OPND_SCR, 2);
    scr3_ir2_opnd = INIT_RA(IR2_OPND_SCR, 3);
}

static GHashTable *ht_pc_thunk;

void ht_pc_thunk_insert(uint32_t thunk_addr, int reg_index)
{
    g_hash_table_insert(ht_pc_thunk, GUINT_TO_POINTER(thunk_addr),
                        GINT_TO_POINTER(reg_index + 1));
}

/*
 * -1 means miss
 */
int ht_pc_thunk_lookup(uint32_t thunk_addr)
{
    int reg_index;
    reg_index = GPOINTER_TO_INT(g_hash_table_lookup(ht_pc_thunk,
                                GUINT_TO_POINTER(thunk_addr)));
    if (reg_index) {
        return reg_index - 1;
    } else {
        return -1;
    }
}

static int remove_thunk_entry(gpointer key, gpointer value, gpointer user_data)
{
        uint32_t thunk_addr = GPOINTER_TO_UINT(key);
        unsigned long *data = user_data;
        uint32_t start = *data >> 32;
        uint32_t end = *data & 0xffffffff;

        if (thunk_addr >= start && thunk_addr <end) {
            return TRUE;
        }
        return FALSE;
}

void ht_pc_thunk_invalidate(uint32_t start, uint32_t end)
{
    unsigned long data = ((unsigned long)start << 32) | end;
    g_hash_table_foreach_remove(ht_pc_thunk, remove_thunk_entry, &data);
}


static void ht_pc_thunk_init(void)
{
    if (ht_pc_thunk) {
        return;
    }
    ht_pc_thunk = g_hash_table_new(NULL, NULL);
}

static void __attribute__((__constructor__)) latx_init(void)
{
    context_switch_bt_to_native = 0;
    context_switch_native_to_bt_ret_0 = 0;
    context_switch_native_to_bt = 0;
    /* context_switch_is_init = 0; */
    native_rotate_fpu_by = 0;

    options_init();
    //xtm_capstone_init();
    global_register_init();
    ht_pc_thunk_init();
}

static __thread ENV lsenv_real;
static __thread TRANSLATION_DATA tr_data_real;

/* global lsenv defined here */
__thread ENV *lsenv;

#ifdef CONFIG_LATX_FAST_JMPCACHE
void latx_fast_jmp_cache_add(CPUState *cs, int h, struct TranslationBlock *tb)
{
    X86CPU *cpu = X86_CPU(cs);
    CPUX86State *env = &cpu->env;
    FastTB *fast_jmp_cache = (FastTB *)env->tb_jmp_cache_ptr;
    qatomic_set(&fast_jmp_cache[h].ptr, tb->tc.ptr);
    qatomic_set(&fast_jmp_cache[h].pc, tb->pc);
}

void latx_fast_jmp_cache_clear(CPUState *cs, int h)
{
    X86CPU *cpu = X86_CPU(cs);
    CPUX86State *env = &cpu->env;
    FastTB *fast_jmp_cache = (FastTB *)env->tb_jmp_cache_ptr;
    qatomic_set(&fast_jmp_cache[h].pc, 0);
}

void latx_fast_jmp_cache_clear_all(CPUState *cs)
{
    X86CPU *cpu = X86_CPU(cs);
    CPUX86State *env = &cpu->env;
    memset(env->tb_jmp_cache_ptr, 0, sizeof(struct FastTB) * TB_JMP_CACHE_SIZE);
}

bool latx_fast_jmp_cache_init(void *env)
{
    FastTB *fast_jmp_cache;
    CPUX86State *x86env = env;

    fast_jmp_cache = calloc(TB_JMP_CACHE_SIZE, sizeof(struct FastTB));
    if (!fast_jmp_cache) {
        return false;
    }
    x86env->tb_jmp_cache_ptr = fast_jmp_cache;

    return true;
}
#endif

#ifdef CONFIG_LATX_DEBUG
__thread char *func_stack[FUNC_DEPTH];

void latx_guest_stack_init(CPUArchState *env)
{
    int i;
    if (env->call_func == NULL) {
        env->call_func = func_stack;
    }
    env->func_index = 0;
    env->last_func_index = -1;
    for (i = 0; i < FUNC_DEPTH; i++) {
        func_stack[i] = NULL;
    }
}
#endif
void latx_dt_init(void)
{
#ifdef CONFIG_LATX_DEBUG
    disassemble_trace_init(TARGET_ABI_BITS, option_latx_disassemble_trace_cmp);
#else
#ifdef CONFIG_LATX_CAPSTONE_GIT
    gitcapstone_init(TARGET_ABI_BITS);
#else
    lacapstone_init(TARGET_ABI_BITS);
#endif
#endif
}

void latx_init_fpu_regs(CPUArchState *env)
{
    if (option_enable_lasx) {
        /* vldi $vr0,0 */
        asm volatile (
            ".word (0x77e00000 | (0 << 5) | (0)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (1)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (2)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (3)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (4)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (5)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (6)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (7)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (8)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (9)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (10)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (11)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (12)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (13)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (14)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (15)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (16)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (17)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (18)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (19)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (20)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (21)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (22)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (23)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (24)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (25)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (26)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (27)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (28)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (29)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (30)) \r\n"
            ".word (0x77e00000 | (0 << 5) | (31)) \r\n"
            : : :
        );
    } else {
        /* vldi $vr0,0 */
        asm volatile (
            ".word (0x73e00000 | (0 << 5) | (0)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (1)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (2)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (3)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (4)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (5)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (6)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (7)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (8)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (9)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (10)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (11)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (12)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (13)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (14)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (15)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (16)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (17)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (18)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (19)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (20)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (21)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (22)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (23)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (24)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (25)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (26)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (27)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (28)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (29)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (30)) \r\n"
            ".word (0x73e00000 | (0 << 5) | (31)) \r\n"
            : : :
        );
    }
}

void latx_lsenv_init(CPUArchState *env)
{
    lsenv = &lsenv_real;
    lsenv->cpu_state = env;
    lsenv->tr_data = &tr_data_real;
#ifdef CONFIG_LATX_TU
    tu_control_init();
#endif

    if (option_dump) {
        qemu_log("[LATX] env init : %p\n", lsenv->cpu_state);
    }
    env->mode_fpu = 1;
    env->fpu_clobber = false;
    env->insn_save[0] = 0;
    env->insn_save[1] = 0;
#ifdef CONFIG_LATX
#ifdef TARGET_X86_64
    env->sys.codemode = LATX_DT_X64;
#else
    env->sys.codemode = LATX_DT_X86;
#endif
#endif
#ifdef CONFIG_LATX_DEBUG
    env->tb_exec_count = 0;
    env->last_store_insn = 0;
    lsenv->current_ir1 = 0;
    latx_guest_stack_init(env);
#endif
#ifdef CONFIG_LATX_SYSCALL_TUNNEL
    lsenv->syscall_optimize_confirm = (void *)syscall_optimize_confirm;
#endif
}
