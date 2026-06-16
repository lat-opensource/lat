/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef _LATX_OPTIONS_H_
#define _LATX_OPTIONS_H_

#include "latx-types.h"
#include "optimize-config.h"
#include "latx-disassemble-trace.h"
#include "latx-debug.h"

#ifdef CONFIG_LATX_INSTS_PATTERN
extern int option_instptn;
#endif
#ifdef CONFIG_LATX_FLAG_REDUCTION
extern int option_flag_reduction;
#endif
#ifdef CONFIG_LATX_TU
extern int option_tu_link;
#endif

#ifdef CONFIG_LATX_AVX_OPT
extern int option_avx_cpuid;
#endif /* CONFIG_LATX_AVX_OPT */

extern int close_latx_parallel;
extern int option_dump;
extern int option_dump_host;
extern int option_dump_ir1;
extern int option_dump_ir2;
extern int option_dump_profile;
extern int option_trace_tb;
extern int option_trace_ir1;
extern int option_enable_fcsr_exc;
extern int option_latx_disassemble_trace_cmp;
extern int option_jr_ra;
#define SMC_ILL_INST 0x1
/* ld.w      $a1,$zero,0 */
#define READ_ILL_INST 0x28800005
/* st.w      $a1,$zero,0 */
#define WRITE_ILL_INST 0x29800005
/* st.w      $a1,$zero,1 */
#define SHADOW_PAGE_INST 0x29800405
extern int option_lative;
extern int option_jr_ra_stack;
extern int option_tunnel_lib;
extern uint64_t option_end_trace_addr;
extern uint64_t option_begin_trace_addr;
extern int option_aot;
extern int option_aot_wine;
extern int option_load_aot;
extern int option_debug_aot;
extern char ** latx_aot_wine_pefiles_cache;

extern uint64_t debug_tb_pc;
extern uint64_t latx_trace_mem;
extern uint64_t latx_break_insn;
extern uint64_t latx_unlink_count;
extern uint32_t latx_unlink_cpu;
extern int option_softfpu;
extern int option_softfpu_fast;
extern int option_prlimit;
extern int option_fputag;
extern int option_save_xmm;
extern int option_enable_lasx;
extern int option_vpaes;
extern int option_split_tb;
extern int option_anonym;
extern int option_imm_reg;
extern int option_imm_precache;
extern int option_imm_rip;
extern int option_imm_complex;
extern int option_debug_imm_reg;
extern uint64_t imm_skip_pc;
extern int option_mem_test;
extern int option_real_maps;
extern int option_monitor_shared_mem;
extern int option_shadow_file;
extern int option_smc_opt;
extern int option_set_rounding_opt;
extern int option_cvt_opt;
extern int option_fast_atomic;
static inline int latx_smc_default(void) { return 0x2 | 0x4; }
static inline int latx_smc_inv_page(void) { return option_smc_opt == 0; }
static inline int latx_smc_inv_tb(void) { return option_smc_opt != 0; }
static inline int latx_smc_shmm(void) { return option_smc_opt & 0x2; }
static inline void latx_smc_shmm_disable(void) { option_smc_opt &= ~0x2; }
static inline int latx_smc_use_store_helper(void) { return option_smc_opt & 0x4; }

extern unsigned long long counter_tb_exec;
extern unsigned long long counter_tb_tr;

extern unsigned long long counter_ir1_tr;
extern unsigned long long counter_mips_tr;

#ifdef CONFIG_LATX
#define ENVSUP_LATX \
    ENVFUN(LATX_OPTIMIZE, handle_arg_optimize) \
    ENVFUN(LATX_VPAES, handle_arg_latx_vpaes) \
    ENVFUN(LATX_SMC, handle_arg_latx_smc) \
    ENVFUN(LATX_CLOSE_PARALLEL, handle_arg_latx_parallel) \
    ENVFUN(LATX_SOFTFPU, handle_arg_latx_softfpu) \
    ENVFUN(LATX_SOFTFPU_FAST, handle_arg_latx_softfpu_fast) \
    ENVFUN(LATX_PRLIMIT, handle_arg_latx_prlimit) \
    ENVFUN(LATX_ROUNDING_OPT, handle_arg_latx_rounding) \
    ENVFUN(LATX_CVT_OPT, handle_arg_latx_cvt_opt) \
    ENVFUN(LATX_FPUTAG, handle_arg_latx_fputag) \
    ENVFUN(SAVE_XMM, handle_arg_save_xmm) \
    ENVFUN(LATX_JRRA, handle_arg_latx_jrra) \
    ENVFUN(LATX_IMM_REG, handle_arg_latx_imm_reg) \
    ENVFUN(LATX_MT, handle_arg_latx_mem_test) \
    ENVFUN(LATX_REAL_MAPS, handle_arg_latx_real_maps) \
    ENVFUN(LATX_MONITOR_SHARED_MEM, handle_arg_latx_monitor_shared_mem) \
    ENVFUN(LATX_ANONYM, handle_arg_latx_anonym) \
    ENVFUN(LATX_MMAP_START, handle_arg_mmap_start) \
    ENVFUN(LATX_MMAP_FIXED, handle_arg_mmap_fixed) \
    ENVFUN(LATX_UNIMP_DUMP, handle_arg_latx_unimp_dump)
#else
#define ENVSUP_LATX
#endif

#if defined(CONFIG_LATX) && defined(CONFIG_LATX_AVX_OPT)
#define ENVSUP_AVX \
    ENVFUN(LATX_AVX_CPUID, handle_arg_latx_avx_cpuid)
#else
#define ENVSUP_AVX
#endif

#if defined(CONFIG_LATX) && defined(CONFIG_LATX_KZT)
#define ENVSUP_KZT \
    ENVFUN(LATX_KZT, handle_arg_latx_kzt)
#else
#define ENVSUP_KZT
#endif

#if defined(CONFIG_LATX) && defined(CONFIG_LATX_AOT)
#define ENVSUP_AOT \
    ENVFUN(LATX_AOT, handle_arg_latx_aot) \
    ENVFUN(LAT_AOT_FILE_SIZE, handle_arg_lat_aot_file_size) \
    ENVFUN(LAT_AOT_LEFT_FILE_SIZE, handle_arg_lat_aot_left_file_size) \
    ENVFUN(LATX_AOT_WINE_PEFILES_CACHE, handle_arg_latx_aot_wine_pefiles_cache)
#else
#define ENVSUP_AOT
#endif

#if defined(CONFIG_LATX_DEBUG) || defined(CONFIG_DEBUG_TCG)
#define ENVSUP_DEBUG \
    ENVFUN(LAT_GDB, handle_arg_gdb) \
    ENVFUN(LAT_STACK_SIZE, handle_arg_stack_size) \
    ENVFUN(LAT_CPU, handle_arg_cpu) \
    ENVFUN(LAT_SET_ENV, handle_arg_set_env) \
    ENVFUN(LAT_UNSET_ENV, handle_arg_unset_env) \
    ENVFUN(LAT_ARGV0, handle_arg_argv0) \
    ENVFUN(LAT_UNAME, handle_arg_uname) \
    ENVFUN(LAT_GUEST_BASE, handle_arg_guest_base) \
    ENVFUN(LAT_RESERVED_VA, handle_arg_reserved_va) \
    ENVFUN(LAT_LOG, handle_arg_log) \
    ENVFUN(LAT_IMM_SKIP_PC, handle_arg_imm_skip_pc) \
    ENVFUN(LAT_DFILTER, handle_arg_dfilter) \
    ENVFUN(LAT_LOG_FILENAME, handle_arg_log_filename) \
    ENVFUN(LAT_PAGESIZE, handle_arg_pagesize) \
    ENVFUN(LAT_SINGLESTEP, handle_arg_singlestep) \
    ENVFUN(LAT_STRACE, handle_arg_strace) \
    ENVFUN(LAT_STRACE_ERROR, handle_arg_strace_error) \
    ENVFUN(LAT_RAND_SEED, handle_arg_seed) \
    ENVFUN(LAT_TRACE, handle_arg_trace) \
    ENVFUN(LAT_LD_PREFIX, handle_arg_ld_prefix) \
    ENVFUN(LAT_VERSION, handle_arg_version)
#else
#define ENVSUP_DEBUG
#endif

#if (defined(CONFIG_LATX_DEBUG) || defined(CONFIG_DEBUG_TCG)) && defined(CONFIG_LATX)
#define ENVSUP_LATX_DEBUG \
    ENVFUN(LATX_BEGIN_TRACE, handle_range_trace_begin) \
    ENVFUN(LATX_END_TRACE, handle_range_trace_end) \
    ENVFUN(LATX_DUMP, handle_arg_latx_print) \
    ENVFUN(LATX_SHOW_TB, handle_arg_latx_show_tb) \
    ENVFUN(LATX_TRACE_MEM, handle_arg_latx_trace_mem) \
    ENVFUN(LATX_BREAK_INSN, handle_arg_latx_break_insn) \
    ENVFUN(LATX_DEBUG_LATIVE, handle_arg_debug_lative) \
    ENVFUN(LATX_ENABLE_FCSR_EXC, handle_arg_enable_fcsr_exc) \
    ENVFUN(LATX_UNLINK, handle_arg_latx_unlink) \
    ENVFUN(LATX_DISASSEMBLE_TRACE_CMP, handle_arg_latx_disassemble_trace_cmp)
#else
#define ENVSUP_LATX_DEBUG
#endif

#if (defined(CONFIG_LATX_DEBUG) || defined(CONFIG_DEBUG_TCG)) && defined(CONFIG_PLUGIN)
#define ENVSUP_PLUGIN \
    ENVFUN(LAT_PLUGIN, handle_arg_plugin)
#else
#define ENVSUP_PLUGIN
#endif

#define ENVS \
    ENVSUP_LATX \
    ENVSUP_AVX \
    ENVSUP_KZT \
    ENVSUP_AOT \
    ENVSUP_DEBUG \
    ENVSUP_LATX_DEBUG \
    ENVSUP_PLUGIN

void options_init(void);
void options_parse_opt(const char *opt);
void options_parse_imm_reg(const char *bits);
void options_parse_dump(const char *bits);
void options_parse_show_tb(const char *pc);
void options_parse_trace_mem(const char *pc);
void options_parse_debug_lative(void);
void options_parse_break_insn(const char *pc);
void options_parse_latx_unlink(const char *arg);
void options_parse_trace(const char *bits);
uint8 options_to_save(void);
void options_parse_latx_disassemble_trace_cmp(const char *args);
void conf_init(char **target_argv);
char* guest_program(char **argv);
void load_conf_file(const char *file, char* program);
void find_option(const char *name, const char *val);
int option_line_init(char *line, char **name, char **value);
char* trim(char *s);
#endif
