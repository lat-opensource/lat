#ifndef _LATX_OPTIONS_H_
#define _LATX_OPTIONS_H_

#include "latx-types.h"
#include "optimize-config.h"
#include "latx-disassemble-trace.h"
#include "latx-debug.h"

extern int option_em_debug;
#ifdef CONFIG_LATX_INSTS_PATTERN
extern int option_insts_pattern;
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
extern int option_check;
extern int option_enable_fcsr_exc;
extern int option_dump_all_tb;
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
static inline int latx_smc_inv_page(void) { return option_smc_opt == 0; }
static inline int latx_smc_inv_tb(void) { return option_smc_opt != 0; }
static inline int latx_smc_shmm(void) { return option_smc_opt & 0x2; }
static inline void latx_smc_shmm_disable(void) { option_smc_opt &= ~0x2; }
static inline int latx_smc_use_store_helper(void) { return option_smc_opt & 0x4; }

extern unsigned long long counter_tb_exec;
extern unsigned long long counter_tb_tr;

extern unsigned long long counter_ir1_tr;
extern unsigned long long counter_mips_tr;

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
#endif
