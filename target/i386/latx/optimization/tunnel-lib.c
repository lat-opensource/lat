#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <lsenv.h>
#include <translate.h>
#include <include/exec/tb-lookup.h>
#include <latx-options.h>
#include <accel/tcg/internal.h>
#include <reg-alloc.h>
#include <x86.h>
#include <accel/tcg/internal.h>

#include <glibconfig.h>
#include <glib.h>
#include <stdlib.h>

#include <qemu.h>
#include <pthread.h>
#include <tunnel_lib.h>
#include <latx-options.h>
#include "qemu/cacheflush.h"

#include "aot.h"

#ifdef DEBUG_TUNNEL
#define tunnel_debug(...) do {printf("[pid %d] [cpu %d] [tunnel_debug] [%s]:",\
    getpid(), current_cpu->cpu_index, __func__);\
    printf(__VA_ARGS__);\
    printf("\n"); } while (0)
#define tunnel_trace(...) do {printf("[pid %d] [cpu %d] [tunnel_trace] [%s]:",\
    getpid(), current_cpu->cpu_index, __func__);\
    printf(__VA_ARGS__);\
    printf("\n"); } while (0)
#else
#define tunnel_debug(...)
#define tunnel_trace(...)
#endif

GHashTable *tunnel_method_hash;

#ifdef TARGET_X86_64

#ifdef TUNNEL_MATH
static void x64_arg_d(IR2_OPND *esp_ir2_opnd)
{
    la_fmov_d(ir2_opnd_new(IR2_OPND_FPR, 0),
    ir2_opnd_new(IR2_OPND_FPR, 16));
}

static void x64_arg_d_d(IR2_OPND *esp_ir2_opnd)
{
    la_fmov_d(ir2_opnd_new(IR2_OPND_FPR, 0),
    ir2_opnd_new(IR2_OPND_FPR, 16));
    la_fmov_d(ir2_opnd_new(IR2_OPND_FPR, 1),
    ir2_opnd_new(IR2_OPND_FPR, 18));
}

static void x64_arg_f(IR2_OPND *esp_ir2_opnd)
{
    la_fmov_d(ir2_opnd_new(IR2_OPND_FPR, 0),
    ir2_opnd_new(IR2_OPND_FPR, 16));
}

static void x64_fpr_ret_f()
{
    la_fmov_d(ir2_opnd_new(IR2_OPND_FPR, 16),
    ir2_opnd_new(IR2_OPND_FPR, 0));
}

static void x64_fpr_ret_d()
{
    la_fmov_d(ir2_opnd_new(IR2_OPND_FPR, 16),
    ir2_opnd_new(IR2_OPND_FPR, 0));
}
#endif

static void x64_arg_gpr_cmn(IR2_OPND *rsp_ir2_opnd)
{
    IR2_OPND a0_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a0);
    IR2_OPND a1_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a1);
    IR2_OPND a2_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a2);
    IR2_OPND rdi_ir2_opnd = ra_alloc_gpr(rdi_index);
    IR2_OPND rsi_ir2_opnd = ra_alloc_gpr(rsi_index);
    IR2_OPND rdx_ir2_opnd = ra_alloc_gpr(rdx_index);

    la_or(a0_ir2_opnd, rdi_ir2_opnd, zero_ir2_opnd);
    la_or(a1_ir2_opnd, rsi_ir2_opnd, zero_ir2_opnd);
    la_or(a2_ir2_opnd, rdx_ir2_opnd, zero_ir2_opnd);
}

#else

static void x86_arg_gpr_cmn(IR2_OPND *esp_ir2_opnd)
{
    IR2_OPND a0_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a0);
    IR2_OPND a1_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a1);
    IR2_OPND a2_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a2);
    IR2_OPND a3_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a3);
    IR2_OPND a4_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a4);
    IR2_OPND a5_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a5);
    IR2_OPND a6_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a6);

    la_bstrpick_d(*esp_ir2_opnd, *esp_ir2_opnd, 31, 0);
    la_ld_wu(a0_ir2_opnd, *esp_ir2_opnd, 0x04);
    la_ld_wu(a1_ir2_opnd, *esp_ir2_opnd, 0x08);
    la_ld_wu(a2_ir2_opnd, *esp_ir2_opnd, 0x0c);
    la_ld_wu(a3_ir2_opnd, *esp_ir2_opnd, 0x10);
    la_ld_wu(a4_ir2_opnd, *esp_ir2_opnd, 0x14);
    la_ld_wu(a5_ir2_opnd, *esp_ir2_opnd, 0x18);
    la_ld_wu(a6_ir2_opnd, *esp_ir2_opnd, 0x1c);
}

#ifdef TUNNEL_MATH
static void x86_arg_d_d(IR2_OPND *esp_ir2_opnd)
{
    la_bstrpick_d(*esp_ir2_opnd, *esp_ir2_opnd, 31, 0);
    la_fld_d(ir2_opnd_new(IR2_OPND_FPR, 0),
        *esp_ir2_opnd, 0x4);
    la_fld_d(ir2_opnd_new(IR2_OPND_FPR, 1),
        *esp_ir2_opnd, 0xc);
}

static void x86_arg_d_p_p(IR2_OPND *esp_ir2_opnd)
{
    IR2_OPND a0_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a0);
    IR2_OPND a1_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a1);

    la_bstrpick_d(*esp_ir2_opnd, *esp_ir2_opnd, 31, 0);
    la_fld_d(ir2_opnd_new(IR2_OPND_FPR, 0), *esp_ir2_opnd, 0x4);
    la_ld_w(a0_ir2_opnd, *esp_ir2_opnd, 0x0c);
    la_ld_w(a1_ir2_opnd, *esp_ir2_opnd, 0x10);
}

static void x86_arg_d_d_d(IR2_OPND *esp_ir2_opnd)
{
    la_bstrpick_d(*esp_ir2_opnd, *esp_ir2_opnd, 31, 0);
    la_fld_d(ir2_opnd_new(IR2_OPND_FPR, la_a0),
        *esp_ir2_opnd, 0x4);
    la_fld_d(ir2_opnd_new(IR2_OPND_FPR, la_a1),
        *esp_ir2_opnd, 0xc);
    la_fld_d(ra_alloc_st(2), *esp_ir2_opnd, 0x14);
}

static void x86_arg_d(IR2_OPND *esp_ir2_opnd)
{
    la_bstrpick_d(*esp_ir2_opnd, *esp_ir2_opnd, 31, 0);
    la_fld_d(ir2_opnd_new(IR2_OPND_FPR, 0), *esp_ir2_opnd, 0x4);
}

static void x86_arg_f(IR2_OPND *esp_ir2_opnd)
{
    la_bstrpick_d(*esp_ir2_opnd, *esp_ir2_opnd, 31, 0);
    la_fld_s(ra_alloc_st(0), *esp_ir2_opnd, 0x4);
}

static void x86_fpr_ret_d(void)
{
    la_fmov_d(ra_alloc_st(0), ir2_opnd_new(IR2_OPND_FPR, 0));
}

static void x86_fpr_ret_f(void)
{
    IR2_OPND st0_opnd = ra_alloc_st(0);
    la_fmov_d(st0_opnd, ir2_opnd_new(IR2_OPND_FPR, 0));
    la_fcvt_d_s(st0_opnd, st0_opnd);
}
#endif
#endif

static void gpr_ret_cmn(void)
{
    IR2_OPND a0_ir2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a0);
    IR2_OPND rax_ir2_opnd = ra_alloc_gpr(eax_index);
    la_or(rax_ir2_opnd, a0_ir2_opnd, zero_ir2_opnd);
}

#define x64_gpr_ret_cmn gpr_ret_cmn
#define x86_gpr_ret_cmn gpr_ret_cmn

#define x86_arg_p_p_i x86_arg_gpr_cmn

#ifdef TARGET_X86_64
#define ARCH(METHOD) x64_ ## METHOD
#else
#define ARCH(METHOD) x86_ ## METHOD
#endif

#define LIB_IFNA(METHOD) (char *)#METHOD, METHOD /* normal; */
#define LIB_WRAP(METHOD) (char *)#METHOD, latx_ ## METHOD /* latx wrap; */
#define LIB_ALOC(METHOD) (char *)#METHOD "@in", latx_mi_ ## METHOD /* malloc; */
#define LIB_FAST(METHOD) (char *)#METHOD, METHOD ## _fast /* inline assembly; */

static void *latx_memcpy(void *dest, const void *src, size_t n)
{
    return memmove(dest, src, n);
}

struct lib_method_item method_table[]  = {
    {LIB_IFNA(memcmp),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(strchr),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(memset),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    /* x64 use memmove as memcopy, so do we */
    {LIB_WRAP(memcpy),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(memchr),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(memmove),   { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(__mempcpy), { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(strncmp),   { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(strlen),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(strcmp),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(strcpy),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(strstr),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(strtok),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(strspn),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
#ifdef TUNNEL_MATH
    {LIB_IFNA(ceil),      { ARCH(arg_d),         ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(ceilf),     { ARCH(arg_f),         ARCH(fpr_ret_f),   NULL} },
    {LIB_IFNA(log10),     { ARCH(arg_d),         ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(floor),     { ARCH(arg_d),         ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(floorf),    { ARCH(arg_f),         ARCH(fpr_ret_f),   NULL} },
    {LIB_IFNA(exp),       { ARCH(arg_d),         ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(fmod),      { ARCH(arg_d_d),       ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(pow),       { ARCH(arg_d_d),       ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(sin),       { ARCH(arg_d),         ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(cos),       { ARCH(arg_d),         ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(atan),      { ARCH(arg_d),         ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(log),       { ARCH(arg_d),         ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(round),     { ARCH(arg_d),         ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(sincos),    { ARCH(arg_d_p_p),     NULL,              NULL} },
    {LIB_IFNA(fma),       { ARCH(arg_d_d_d),     ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(qemu_strtod),    { ARCH(arg_gpr_cmn),   ARCH(fpr_ret_d),   NULL} },
    {LIB_IFNA(qemu_strtof),    { ARCH(arg_gpr_cmn),   ARCH(fpr_ret_f),   NULL} },
    {LIB_IFNA(strrchr),   { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
    {LIB_IFNA(qemu_strtol),    { ARCH(arg_gpr_cmn),   ARCH(gpr_ret_cmn), NULL} },
#endif
};
const int method_table_size = sizeof(method_table) / sizeof(method_table[0]);

static void init_tunnel_table(void)
{
    tunnel_debug("init hash");
    tunnel_method_hash = g_hash_table_new(g_str_hash, g_str_equal);
    int table_item_num = sizeof(method_table) / sizeof(struct lib_method_item);
    for (int i = 0; i < table_item_num; i++) {
        tunnel_debug("add %s loongarch %p", method_table[i].method_name,
            method_table[i].loongarch_addr);
        g_hash_table_insert(tunnel_method_hash,
           (gpointer *)method_table[i].method_name, &method_table[i]);
    }
}

#ifdef DEBUG_TUNNEL
static inline void dump_cpu(
    target_ulong pc, target_ulong cs_base,
    uint32_t flags, uint32_t cflags)
{
    tunnel_debug("pc %lx cs_base %lx flags %x cflags %x",
        pc, cs_base, flags, cflags);
}
#endif

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

    la_addi_addrx(*esp_ir2_opnd,
        *esp_ir2_opnd, sizeof(target_ulong));
    la_store_addrx(nextip_ir2_opnd,
        env_ir2_opnd, lsenv_offset_of_eip(lsenv));
}

#define x86_gen_set_next_tb_code gen_set_next_tb_code
#define x64_gen_set_next_tb_code gen_set_next_tb_code

static void gen_set_last_tb_code(TranslationBlock *tb)
{
    IR2_OPND last_tb_ir2_opnd = LAST_TB_OPND;
    aot_load_host_addr(last_tb_ir2_opnd, (ADDR)tb, LOAD_TB_ADDR, 0);
    la_store_addrx(last_tb_ir2_opnd, env_ir2_opnd,
        lsenv_offset_of_last_executed_tb(lsenv));
}

#define x86_gen_set_last_tb_code gen_set_last_tb_code
#define x64_gen_set_last_tb_code gen_set_last_tb_code

static void save_reg(void)
{
#ifdef TARGET_X86_64
    if (CODEIS64) {
        /* save R12-R15 because they are itemp regs */
        for (int i = 17; i < 21; i++) {
            la_store_addrx(ir2_opnd_new(IR2_OPND_GPR, i),
               env_ir2_opnd, lsenv_offset_of_all_gpr(lsenv, i));
        }
    }
#endif
    la_store_addrx(ra_alloc_eflags(),
        env_ir2_opnd, lsenv_offset_of_eflags(lsenv));
}

#define x86_restore_reg restore_reg
#define x64_restore_reg restore_reg

static void restore_reg(void)
{
#ifdef TARGET_X86_64
    if (CODEIS64) {
        for (int i = 17; i < 21; i++) {
            la_load_addrx(ir2_opnd_new(IR2_OPND_GPR, i),
                env_ir2_opnd, lsenv_offset_of_all_gpr(lsenv, i));
        }
    }
#endif
    la_load_addrx(ra_alloc_eflags(),
        env_ir2_opnd, lsenv_offset_of_eflags(lsenv));
}

#define x86_save_reg save_reg
#define x64_save_reg save_reg

static void set_ret_location(TranslationBlock *tb, ADDR ret)
{
    IR2_OPND base = ra_alloc_data();
    IR2_OPND target = ra_alloc_data();
    la_data_li(base, (ADDR)tb->tc.ptr);
    la_data_li(target, ret);
    aot_la_append_ir2_jmp_far(target, base, B_NATIVE_JMP_GLUE2, 0);
}

struct cpu_state_info {
    uint32_t flags;
    uint32_t cflags;
    target_ulong cs_base;
    target_ulong current_pc;
};

static size_t gen_tunnel_glue(TranslationBlock *tb,
        struct lib_method_item *method_item)
{
    TRANSLATION_DATA *lat_ctx = lsenv->tr_data;
    IR2_OPND esp_ir2_opnd = ra_alloc_gpr(esp_index);
    /* start tb loongarch assemble create session */
    tr_init(tb);
#ifndef TARGET_X86_64
    la_bstrpick_d(esp_ir2_opnd, esp_ir2_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(esp_ir2_opnd, esp_ir2_opnd, 31, 0);
    }
#endif
    /* save context registor */
    ARCH(save_reg)();
    /* argument transform */
    if (method_item->trans.fill_argument) {
        method_item->trans.fill_argument(&esp_ir2_opnd);
    }
    int method_item_index = method_item - method_table;
    assert(method_item_index >= 0 && method_item_index < method_table_size);
    tr_gen_call_to_helper((ADDR)method_item->loongarch_addr,
                          LOAD_TUNNEL_ADDR_BEGIN + method_item_index);
    /* transform return-value */
    if (method_item->trans.fill_return) {
        method_item->trans.fill_return();
    }
    /* restore context registor */
    ARCH(restore_reg)();
    ARCH(gen_set_next_tb_code)(&esp_ir2_opnd);
    ARCH(gen_set_last_tb_code)(tb);
    /* tunnel glue return to indirect_jmp_glue*/
    set_ret_location(tb, indirect_jmp_glue);
    /* set_ret_location(tb, context_switch_native_to_bt); */
    label_dispose(tb, lat_ctx);
    int code_nr = tr_ir2_assemble((void *)tb->tc.ptr, lat_ctx->first_ir2) + 1;
    int code_size = code_nr * 4;
    /* tunnel tb doesn't has x86 code we */
    /* assume it's same as loongarch     */
    tb->icount = code_nr;
    /* finish tb loongarch assemble create session */
    tr_fini(false);
    tb->size = code_size;
    tb->tc.size = code_size;
    return code_size;
}

static void reset_tb(TranslationBlock *tb)
{
    qemu_spin_init(&tb->jmp_lock);
    tb->jmp_dest[0] = (uintptr_t)NULL;
    tb->jmp_dest[1] = (uintptr_t)NULL;
    tb->jmp_list_head = (uintptr_t)NULL;
    tb->jmp_list_next[0] = (uintptr_t)NULL;
    tb->jmp_list_next[1] = (uintptr_t)NULL;

    /* init top in and top out */
    tb->s_data->_top_out = -1;
    tb->s_data->_top_in = -1;
    tb->jmp_reset_offset[0] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_reset_offset[1] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_stub_reset_offset[0] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_stub_reset_offset[1] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_indirect = TB_JMP_RESET_OFFSET_INVALID;
#ifdef CONFIG_LATX_INSTS_PATTERN
    tb->eflags_target_arg[0] = TB_JMP_RESET_OFFSET_INVALID;
    tb->eflags_target_arg[1] = TB_JMP_RESET_OFFSET_INVALID;
    tb->eflags_target_arg[2] = TB_JMP_RESET_OFFSET_INVALID;
#endif
    tb->bool_flags = OPT_BCC | IS_TUNNEL_LIB;
    clear_signal_link_flag(tb, 0);
    clear_signal_link_flag(tb, 1);
    tb->first_jmp_align = TB_JMP_RESET_OFFSET_INVALID;
    tb_set_page_addr0(tb, -1);
    tb_set_page_addr1(tb, -1);
#ifdef CONFIG_LATX_JRRA
    tb->next_86_pc = 0;
    tb->return_target_ptr = NULL;
#endif
#ifdef CONFIG_LATX_AOT
    tb->s_data->rel_start = -1;
    tb->s_data->rel_end = -1;
#endif
}

static void init_tb_by_cpu(struct cpu_state_info *state_info,
    CPUState *cpu, TranslationBlock *tb)
{
    /* tb->cs_base = state_info->cs_base; */
    tb->flags = state_info->flags;
    tb->cflags = cpu->tcg_cflags;
}

static void set_tb_pc(TranslationBlock *tb,
    void *gen_code_buf, target_ulong target_pc)
{
    tb->tc.ptr = tcg_splitwx_to_rx(gen_code_buf);
    tb->pc = target_pc;
}

static void update_tcg_context_before_asm(TranslationBlock *tb)
{
    tcg_ctx->tb_jmp_reset_offset = tb->jmp_reset_offset;
    if (TCG_TARGET_HAS_direct_jump) {
        tcg_ctx->tb_jmp_insn_offset = tb->jmp_target_arg;
        tcg_ctx->tb_jmp_target_addr = NULL;
    }
}

static void update_tcg_context_after_asm(void *gen_code_buf,
    TranslationBlock *tb, size_t glue_code_size)
{
    uint32_t search_size = encode_search(tb,
        (void *)gen_code_buf + glue_code_size);

    /* increase code_gen_ptr with align */
    qatomic_set(&tcg_ctx->code_gen_ptr,
         (void *)ROUND_UP((uintptr_t)gen_code_buf +
             glue_code_size + search_size, CODE_GEN_ALIGN));
}

/* check tb whether exists, reuse old tb if exists */
static bool is_existed_tb(CPUState *cpu,
    TranslationBlock *tb, target_ulong target_pc, void *gen_code_buf)
{
    target_ulong virt_page2, phys_page2, phys_pc;
    phys_pc = get_page_addr_code(cpu->env_ptr, target_pc);
    assert(phys_pc != -1);
    virt_page2 = (target_pc + tb->size - 1) & TARGET_PAGE_MASK;
    phys_page2 = -1;
    if ((target_pc & TARGET_PAGE_MASK) != virt_page2) {
        phys_page2 = get_page_addr_code(cpu->env_ptr, virt_page2);
    }

    TranslationBlock *existing_tb =
        tb_link_page(tb, phys_pc, phys_page2);
    if (unlikely(existing_tb != tb)) {
        uintptr_t orig_aligned = (uintptr_t)gen_code_buf;
        orig_aligned -= ROUND_UP(sizeof(*tb), qemu_icache_linesize);
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)orig_aligned);
        tb_destroy(tb);
        tunnel_debug("tb_link_page fail");
        return true;
    }
    return false;
}

static void do_create_tb(
    struct lib_method_item *method_item,
    struct cpu_state_info *state_info,
    CPUState *cpu, target_ulong target_pc,
    TranslationBlock *tb)
{
    tb = tcg_tb_alloc(tcg_ctx);
    void *gen_code_buf = tcg_ctx->code_gen_ptr;
    reset_tb(tb);
    init_tb_by_cpu(state_info, cpu, tb);
    set_tb_pc(tb, gen_code_buf, target_pc);
    update_tcg_context_before_asm(tb);
    size_t glue_code_size = gen_tunnel_glue(tb, method_item);
    update_tcg_context_after_asm(gen_code_buf, tb, glue_code_size);
    flush_idcache_range(0, 0, 0);

    if (!is_existed_tb(cpu, tb, target_pc, gen_code_buf)) {
        tcg_tb_insert(tb);
    }
}

static void create_tunnel_tb(
    struct lib_method_item *method_item, target_ulong org)
{
    TranslationBlock *tb;
    struct cpu_state_info state_info;
    target_ulong target_pc = org;
    CPUState *cpu = env_cpu(lsenv->cpu_state);

    /* fill in cpu_state_info */
    state_info.cflags = cpu->tcg_cflags;
    cpu_get_tb_cpu_state(cpu->env_ptr,
        &state_info.current_pc,
        &state_info.cs_base,
        &state_info.flags);

    mmap_lock();

    /* check tb whether exists */
    tb = tb_lookup(cpu, target_pc,
        state_info.cs_base, state_info.flags, state_info.cflags);
    if (tb) {
        if (strstr(method_item->method_name, "@in")) {
            /* if lib method has been translated,
             * we will remove these code
             * we do this when tunnel inline l
             * ib-function like malloc  */
            tb_phys_invalidate(tb, tb_page_addr0(tb));
            tb = tb_lookup(cpu, target_pc,
                state_info.cs_base,
                state_info.flags,
                state_info.cflags);
            assert(!tb);
        } else {
            /* tb already exists, just return */
            mmap_unlock();
            return;
        }
    }
    /* now we assume tunnel doesn't exists
     * tb creation flow copy from translate tb */
    do_create_tb(method_item, &state_info, cpu, target_pc, tb);

    mmap_unlock();
}

void reg_priv_plt(abi_ulong method, abi_ulong plt_addr, abi_ulong org)
{
    if (!option_tunnel_lib) {
        return;
    }
    char *method_name = (char *)(unsigned long)method;
    if (!tunnel_method_hash) {
        init_tunnel_table();
    }
    struct lib_method_item *method_item;
    tunnel_debug("try to find %s", method_name);
    method_item = (struct lib_method_item *)
        g_hash_table_lookup(tunnel_method_hash, method_name);
    if (method_item) {
        create_tunnel_tb(method_item, org);
    }
}
