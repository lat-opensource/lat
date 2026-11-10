/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file tu.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief TU optimization header file
 */
#ifndef __TU_H_
#define __TU_H_
#include "reg-map.h"
#include "ir1.h"

void get_last_info(TranslationBlock *tb, IR1_INST *pir1);

#ifdef CONFIG_LATX_TU
void tu_prepare_restore_tb(TranslationBlock *copy,
                           struct separated_data *data,
                           const TranslationBlock *tb);

/*
 * The reason why MAX_TB_IN_CACHE > MAX_TB_IN_TU is
 * TBs in TU sometimes need to translate next guest_pc
 * for call-return optimazation.
 * Although the current implementation do not need them.
 */
#define MAX_TB_IN_TU 50
#define MAX_TB_IN_TS (500)
#define MAX_TB_IN_CACHE (MAX_TB_IN_TS + 10)
/* Fixed-size PC index; keep it a power of two and above MAX_TB_IN_CACHE. */
#define TU_MEMBER_INDEX_SIZE 2048
#if (TU_MEMBER_INDEX_SIZE & (TU_MEMBER_INDEX_SIZE - 1)) != 0
#error TU_MEMBER_INDEX_SIZE must be a power of two
#endif
#if MAX_TB_IN_CACHE >= TU_MEMBER_INDEX_SIZE
#error TU_MEMBER_INDEX_SIZE must exceed MAX_TB_IN_CACHE
#endif
/* Count only pages that contain TU members. */
#define MAX_PAGES_IN_TU 16
/* Keep sparse direct branches from expanding a TU across a whole segment. */
#define MAX_TU_PAGE_SPAN 64
#if MAX_TU_PAGE_SPAN < MAX_PAGES_IN_TU
#error MAX_TU_PAGE_SPAN must cover all dependency pages
#endif
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
    /* Open-addressed TB index; zero means empty, otherwise tb_list[] + 1. */
    uint16_t member_index[TU_MEMBER_INDEX_SIZE];

    /* Guest pages that contain at least one member of the current TU. */
    target_ulong pages[MAX_PAGES_IN_TU];
    uint32_t page_num;

    /* Inclusive address window; holes do not consume pages[]. */
    target_ulong first_page;
    target_ulong last_page;

} TUControl;

typedef enum TU_TB_START_TYPE {
    TU_TB_START_NONE = 0,
    TU_TB_START_JMP,
    TU_TB_START_NORMAL,
    TU_TB_START_ENTRY
} TU_TB_START_TYPE;

/*
 * Both blocks were decoded from the same protected guest bytes. Only reuse
 * the tail if the decoded boundaries and execution modes agree.
 */
static inline bool tu_shares_fallthrough_tail(const TranslationBlock *tb,
        const TranslationBlock *tail, int max_insns)
{
    target_ulong end = tail->s_data->next_pc;
    const IR1_INST *a = tb->s_data->ir1;
    const IR1_INST *b = tail->s_data->ir1;

    if (tb->flags != tail->flags || tb->cflags != tail->cflags ||
        ((tb->bool_flags ^ tail->bool_flags) & (IS_CODE64 | TBSMC_OPTED)) ||
        tb->pc >= tail->pc || tail->pc >= end ||
        tb->s_data->next_pc != end || tb->size != end - tb->pc ||
        tail->size != end - tail->pc ||
        tb->s_data->last_ir1_type != tail->s_data->last_ir1_type ||
        !tail->icount || tb->icount <= tail->icount ||
        tb->icount >= max_insns) {
        return false;
    }

    a += tb->icount - tail->icount;
    if (a->info->address != tail->pc) {
        return false;
    }
    for (unsigned int i = 0; i < tail->icount; i++) {
        if (a[i].info->address != b[i].info->address ||
            a[i].info->size != b[i].info->size ||
            a[i].info->id != b[i].info->id) {
            return false;
        }
    }
    return true;
}

extern __thread TUControl *tu_data;

void tu_enough_space(CPUState *cpu);
TranslationBlock *tu_member_lookup(target_ulong pc);
void tu_control_init(void);
void tu_control_destroy(void);
TranslationBlock* tb_create(CPUState *cpu, target_ulong pc,
        target_ulong cs_base, uint32_t flags, int cflags,
        int max_insns, uint16_t bool_flags, TU_TB_START_TYPE mode);
TranslationBlock *tu_create_fallthrough_prefix(CPUState *cpu, target_ulong pc,
        target_ulong cs_base, int max_insns, uint16_t bool_flags,
        const TranslationBlock *tail);
void tu_build_reset(void);
bool tu_page_range_available(target_ulong pc, uint32_t size);
bool tu_push_back(TranslationBlock *tb);
void tu_link_members(TranslationBlock **tb_list, uint32_t tb_num);
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
