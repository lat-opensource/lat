#include "latx-options.h"
#include "error.h"
#include "qemu/cutils.h"
#include "reg-alloc.h"
#include "latx-debug.h"

#if defined(CONFIG_LATX_KZT)
int option_kzt = 0;
#endif

#ifdef CONFIG_LATX_AVX_OPT
int option_avx_cpuid = 0;
#endif

#ifdef CONFIG_LATX_FLAG_REDUCTION
int option_flag_reduction = 1;
#endif

int option_lative = 0;
#if defined(CONFIG_LATX_JRRA_STACK) && defined(CONFIG_LATX_LSFPU)

int option_jr_ra_stack = 1;
int option_jr_ra = 0;
#else
#if defined(CONFIG_LATX_JRRA) && defined(CONFIG_LATX_LSFPU)
/* when using jr_ra, lsfpu should be on */
int option_jr_ra_stack = 0;
int option_jr_ra = 0;
#else
int option_jr_ra = 0;
int option_jr_ra_stack = 0;
#endif
#endif


#ifdef CONFIG_LATX_TUNNEL_LIB
int option_tunnel_lib = 1;
#else
int option_tunnel_lib;
#endif

#ifdef CONFIG_LATX_INSTS_PATTERN
int option_insts_pattern = 1;
#endif

int close_latx_parallel;

uint64_t option_begin_trace_addr;
uint64_t option_end_trace_addr;

int option_enable_fcsr_exc;
int option_dump;
int option_dump_host;
int option_dump_ir1;
int option_dump_ir2;
int option_dump_profile;
int option_trace_tb;
int option_trace_ir1;
int option_check;
int option_em_debug;
int option_dump_all_tb;
int option_latx_disassemble_trace_cmp;
int option_debug_lative;
int option_aot;
int option_load_aot;
int option_aot_wine;
int option_debug_aot;
int option_imm_reg;
int option_imm_rip;
int option_imm_precache;
int option_imm_complex;
int option_debug_imm_reg;
uint64_t imm_skip_pc;
uint64_t debug_tb_pc;
uint64_t latx_trace_mem;
uint64_t latx_break_insn;
uint64_t latx_unlink_count;
uint32_t latx_unlink_cpu;
int option_softfpu;
int option_softfpu_fast;
int option_prlimit;
int option_fputag;
int option_save_xmm;
int option_enable_lasx;
int option_split_tb;
int option_anonym;
int option_mem_test;
int option_real_maps;
int option_monitor_shared_mem;
int option_shadow_file;
int option_smc_opt;
int option_set_rounding_opt;

unsigned long long counter_tb_exec;
unsigned long long counter_tb_tr;

unsigned long long counter_ir1_tr;
unsigned long long counter_mips_tr;

void options_init(void)
{
    option_debug_lative = 0;
    option_save_xmm = 0xff;
    option_dump_host = 0;
    option_dump_ir1 = 0;
    option_dump_ir2 = 0;
    option_dump = 0;
    option_trace_tb = 0;
    option_trace_ir1 = 0;
    option_check = 0;
    option_dump_all_tb = 0;
    option_latx_disassemble_trace_cmp = 0;
    option_enable_lasx = 1;

    counter_tb_exec = 0;
    counter_tb_tr = 0;

    counter_ir1_tr = 0;
    counter_mips_tr = 0;

#ifdef CONFIG_LATX_AVX_OPT
    option_avx_cpuid = 1;
#endif /*CONFIG_LATX_AVX_OPT*/

#ifdef CONFIG_LATX_AOT
    option_aot = 1;
    option_load_aot = 1;
    option_aot_wine = 0;
    option_debug_aot = 0;
#endif
#ifdef CONFIG_LATX_IMM_REG
    option_imm_reg = 0;
    option_imm_rip = 0;
    // complex:base+index*scale
    option_imm_complex = 0;
    option_imm_precache = 0;
    option_debug_imm_reg = 0;
    imm_skip_pc = 0;
#endif
#ifdef CONFIG_LATX_SPLIT_TB
    option_split_tb = 1;
#endif
    option_anonym = 0;
    option_mem_test = 0;
    option_real_maps = 0;
    option_monitor_shared_mem = 0;
#ifdef LOW_MEM_MODE_0
    option_shadow_file = 1;
#endif

#ifdef CONFIG_LATX_SMC_OPT
    option_smc_opt = 0;
#endif

#ifdef CONFIG_LATX_AVX_OPT
    option_set_rounding_opt = 0;
#else
    option_set_rounding_opt = 1;
#endif
}

#define OPTIONS_IMM_REG 0
#define OPTIONS_IMM_RIP 1
#define OPTIONS_IMM_COMPLEX 2
#define OPTIONS_IMM_PRECACHE 3

void options_parse_imm_reg(const char *bits)
{
    if (!bits) {
        return;
    }

    if (bits[OPTIONS_IMM_REG] == '1') {
        option_imm_reg = 1;
    } else if (bits[OPTIONS_IMM_REG] == '0') {
        option_imm_reg = 0;
    } else {
        lsassertm(0, "wrong options for imm_reg.");
    }

    if (bits[OPTIONS_IMM_RIP] == '1') {
        option_imm_rip = 1;
    } else if (bits[OPTIONS_IMM_RIP] == '0') {
        option_imm_rip = 0;
    } else {
        lsassertm(0, "wrong options for imm_reg_rip.");
    }

    if (bits[OPTIONS_IMM_COMPLEX] == '1') {
        option_imm_complex = 1;
    } else if (bits[OPTIONS_IMM_COMPLEX] == '0') {
        option_imm_complex = 0;
    } else {
        lsassertm(0, "wrong options for imm_reg_complex.");
    }

    if (bits[OPTIONS_IMM_PRECACHE] == '1') {
        option_imm_precache = 1;
    } else if (bits[OPTIONS_IMM_PRECACHE] == '0') {
        option_imm_precache = 0;
    } else {
        lsassertm(0, "wrong options for imm_reg_precache.");
    }
}

#define OPTIONS_DUMP_FUNC 0
#define OPTIONS_DUMP_IR1 1
#define OPTIONS_DUMP_IR2 2
#define OPTIONS_DUMP_HOST 3
#define OPTIONS_DUMP_PROFILE 4

void options_parse_dump(const char *bits)
{
    if (!bits) {
        return;
    }

    if (bits[OPTIONS_DUMP_PROFILE] == '1') {
        option_dump_profile = 1;
    } else if (bits[OPTIONS_DUMP_PROFILE] == '0') {
        option_dump_profile = 0;
    } else {
        lsassertm(0, "wrong options for dump profile.");
    }

    if (bits[OPTIONS_DUMP_IR1] == '1') {
        option_dump_ir1 = 1;
    } else if (bits[OPTIONS_DUMP_IR1] == '0') {
        option_dump_ir1 = 0;
    } else {
        lsassertm(0, "wrong options for dump ir1.");
    }

    if (bits[OPTIONS_DUMP_IR2] == '1') {
        option_dump_ir2 = 1;
    } else if (bits[OPTIONS_DUMP_IR2] == '0') {
        option_dump_ir2 = 0;
    } else {
        lsassertm(0, "wrong options for dump ir2.");
    }

    if (bits[OPTIONS_DUMP_HOST] == '1') {
        option_dump_host = 1;
    } else if (bits[OPTIONS_DUMP_HOST] == '0') {
        option_dump_host = 0;
    } else {
        lsassertm(0, "wrong options for dump host.");
    }

    if (bits[OPTIONS_DUMP_FUNC] == '1') {
        option_dump = 1;
    } else if (bits[OPTIONS_DUMP_FUNC] == '0') {
        option_dump = 0;
    } else {
        lsassertm(0, "wrong options for dump func.");
    }
}

void options_parse_opt(const char *arg)
{
    if (arg && !strcmp(arg, "tunnel-lib")) {
        option_tunnel_lib = true;
    }
}

void options_parse_show_tb(const char *pc)
{
    if (!pc) {
        return;
    }

    qemu_strtou64(pc, NULL, 0, &debug_tb_pc);
    printf("debug_tb_pc = 0x%lx\n", debug_tb_pc);
}

void options_parse_trace_mem(const char *pc)
{
    if (!pc) {
        return;
    }

    qemu_strtou64(pc, NULL, 0, &latx_trace_mem);
    printf("latx_trace_mem = 0x%lx\n", latx_trace_mem);
}

void options_parse_break_insn(const char *pc)
{
    if (!pc) {
        return;
    }

    qemu_strtou64(pc, NULL, 0, &latx_break_insn);
    printf("latx_break_insn = 0x%lx\n", latx_break_insn);
}

void options_parse_debug_lative(void)
{
    debug_tb_pc = 0x30000;
    latx_break_insn = 0x30000;
    option_debug_lative = 1;
}

void options_parse_latx_unlink(const char *arg)
{
    if (!arg) {
        return;
    }
    gchar **arr = NULL;
    arr = g_strsplit(arg, ",", 0);
    latx_unlink_count = atol(arr[0]);
    if (arr[1]) {
        latx_unlink_cpu = atoi(arr[1]);
    }
    printf("latx_unlink_count = %ld latx_unlink_cpu = %d\n",
            latx_unlink_count, latx_unlink_cpu);
    g_strfreev(arr);
}

#define OPTIONS_TRACE_TB 0
#define OPTIONS_TRACE_IR1 1

void options_parse_trace(const char *bits)
{
    if (!bits) {
        return;
    }

    if (bits[OPTIONS_TRACE_TB] == '1') {
        option_trace_tb = 1;
    } else if (bits[OPTIONS_TRACE_TB] == '0') {
        option_trace_tb = 0;
    } else {
        lsassertm(0, "wrong options for trace tb.");
    }

    if (bits[OPTIONS_TRACE_IR1] == '1') {
        option_trace_ir1 = 1;
    } else if (bits[OPTIONS_TRACE_IR1] == '0') {
        option_trace_ir1 = 0;
    } else {
        lsassertm(0, "wrong options for trace ir1 .");
    }
}

void options_parse_latx_disassemble_trace_cmp(const char *args)
{
    if (!args) {
        return;
    }
    char *findSpil = strstr(args, ":");
    if (!findSpil) {
        lsassertm(0, "Can't find \':\' from args.\n");
        return;
    }
    char strtmp[100] = {0};
    option_latx_disassemble_trace_cmp = 0;
    lsassert(findSpil - args <= 100);
    strncpy(strtmp, args, findSpil - args);
    if (!strcmp(strtmp, "lacapstone")) {
        option_latx_disassemble_trace_cmp |= OPT_V1LACAPSTONE;
    } else if (!strcmp(strtmp, "nextcapstone")) {
        option_latx_disassemble_trace_cmp |= OPT_V1NEXTCAPSTONE;
    } else if (!strcmp(strtmp, "laxed")) {
        option_latx_disassemble_trace_cmp |= OPT_V1LAXED;
    } else if (!strcmp(strtmp, "lazydis")) {
        option_latx_disassemble_trace_cmp |= OPT_V1LAZYDIS;
    } else {
        lsassertm(0, "V1 must be lacapstone, lazydis, "
            "nextcapstone or laxed, but this V1=%s\n", strtmp);
        return;
    }
    strncpy(strtmp, findSpil + 1, 99);
    if (!strcmp(strtmp, "lacapstone")) {
        option_latx_disassemble_trace_cmp |= OPT_V2LACAPSTONE;
    } else if (!strcmp(strtmp, "nextcapstone")) {
        option_latx_disassemble_trace_cmp |= OPT_V2NEXTCAPSTONE;
    } else if (!strcmp(strtmp, "laxed")) {
        option_latx_disassemble_trace_cmp |= OPT_V2LAXED;
    } else if (!strcmp(strtmp, "lazydis")) {
        option_latx_disassemble_trace_cmp |= OPT_V2LAZYDIS;
    } else if (strlen(strtmp) == 0) {
        /*Only want to choose la_disa_v1, No cmp.*/
    } else {
        lsassertm(0, "V2 must be lacapstone, lazydis, "
            "nextcapstone, laxed or NULL, but this V2=%s\n", strtmp);
    }
}
uint8 options_to_save(void)
{
    uint8 option_bitmap = 0;
    return option_bitmap;
}
