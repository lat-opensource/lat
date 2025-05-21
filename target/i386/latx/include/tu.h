/**
 * @file tu.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief TU optimization header file
 */
#ifndef __TU_H_
#define __TU_H_
#include <gmodule.h>
#include "reg-map.h"
#include "ir1.h"

void get_last_info(TranslationBlock *tb, IR1_INST *pir1);

#ifdef CONFIG_LATX_TU
/*
 * The reason why MAX_TB_IN_CACHE > MAX_TB_IN_TU is
 * TBs in TU sometimes need to translate next guest_pc
 * for call-return optimazation.
 * Although the current implementation do not need them.
 */
#define MAX_TB_IN_TU 50
#define MAX_TB_IN_TS (500)
#define MAX_TB_IN_CACHE (MAX_TB_IN_TS + 10)
#define MAX_TB_SIZE 5120
#define MAX_IR1_IN_TU (TARGET_PAGE_SIZE * 4)
/* Max TU gen code size. */
#define MAX_TU_SIZE (MAX_IR1_IN_TU * 16)

#define BCC_OPCODE    0xFC000000
#define BFCC_OPCODE   0xFC000100

#define BCEQZ_OPCODE  0x48000000
#define BCNEZ_OPCODE  0x48000100
#define BEQZ_OPCODE   0x40000000
#define BNEZ_OPCODE   0x44000000

#define BEQ_OPCODE    0x58000000
#define BNE_OPCODE    0x5C000000
#define BLT_OPCODE    0x60000000
#define BGE_OPCODE    0x64000000
#define BLTU_OPCODE   0x68000000
#define BGEU_OPCODE   0x6C000000

#define B_OPCODE      0x50000000
#define BL_OPCODE     0x54000000

#define OFF16_MASK    0xFC0003FF
#define BITS16_MASK   0xFFFF
#define OFF16_SHIFT   10
#define OFF16_MAX     32767
#define OFF16_MIN     -32768
#define BCC_OFF16_MAX (1 << 17) - 1 - 256 * 4
#define BCC_OFF16_MIN -(1 << 17) + 256 * 4

#define OFF20_MASK     0xFC0003E0
#define OFF20_HI_BITS  0x1f
#define OFF20_HI_SHIFT 16

#define OFF20_MAX     0x7ffff
#define OFF20_MIN     -0x80000

#define MAX_OFFS      0x00020000

/* TUControl manages the translation of TU */
typedef struct TUControl {
    /* the numbers of TB in current TU */
    uint32_t tb_num;
    /* all ir1 number in current TU, which is used for calculate ir1 offset */
    uint32_t ir1_num_in_tu;
    /* all TB pointer in current TU  */
    TranslationBlock *tb_list[MAX_TB_IN_CACHE];
    /* Search GPC to TB in current TU  */
    GTree *tree;

} TUControl;

typedef enum TU_TB_START_TYPE {
    TU_TB_START_NONE = 0,
    TU_TB_START_JMP,
    TU_TB_START_NORMAL,
    TU_TB_START_ENTRY
} TU_TB_START_TYPE;

extern __thread TUControl *tu_data;

void tu_enough_space(CPUState *cpu);
void tu_trees_reset(void);
TranslationBlock *tu_tree_lookup(target_ulong pc);
void tu_control_init(void);
TranslationBlock* tb_create(CPUState *cpu, target_ulong pc,
        target_ulong cs_base, uint32_t flags, int cflags,
        int max_insns, bool is_first_tb, TU_TB_START_TYPE mode);
void tu_push_back(TranslationBlock *tb);
TranslationBlock *tu_gen_code(CPUState *cpu, target_ulong pc,
                              target_ulong cs_base, uint32_t flags,
                              int cflags);
void translate_tu(uint32 tb_num_in_tu, TranslationBlock **tb_list);
void solve_tb_overlap(uint32 tb_num_in_tu,
		TranslationBlock **tb_list, int max_insns);
void tu_ir1_optimization(TranslationBlock **tb_list, int tb_num_in_tu);

int translate_tb_in_tu(struct TranslationBlock *tb);
void tu_reset_tb(TranslationBlock *tb);
int tu_relocat_target_branch(TranslationBlock * tb);
void tu_relocat_next_branch(TranslationBlock * tb);
void bcc_ins_recover(TranslationBlock *tb);
uint bcc_ins_convert(uint convert_insn);
#ifdef CONFIG_LATX_DEBUG
void print_ir1(TranslationBlock* tb);
void print_tu_tb(TranslationBlock *tb);
#endif

bool judge_tu_eflag_gen(void *tb_in_tu);
void tu_jcc_nop_gen(TranslationBlock *tb);
#endif
#endif
