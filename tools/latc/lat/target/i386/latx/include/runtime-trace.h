/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ir1.h"

enum trace_save_regs_indx {
    la_a0_idx,
    la_a1_idx,
    la_a2_idx,
    la_a3_idx,
    la_a4_idx,
    la_a5_idx,
    la_a6_idx,
    la_a7_idx,

    la_t0_idx,
    la_t1_idx,
    la_t2_idx,
    la_t3_idx,
    la_t4_idx,
    la_t5_idx,
    la_t6_idx,
    la_t7_idx,
    la_t8_idx,

    la_s0_idx,
    la_s1_idx,
    la_s2_idx,
    la_s3_idx,
    la_s4_idx,
    la_s5_idx,
    la_s6_idx,
    la_s7_idx,
    la_s8_idx,

    la_xmm0_l_idx,
    la_xmm0_h_idx,
    la_xmm1_l_idx,
    la_xmm1_h_idx,
    la_xmm2_l_idx,
    la_xmm2_h_idx,
    la_xmm3_l_idx,
    la_xmm3_h_idx,
    la_xmm4_l_idx,
    la_xmm4_h_idx,
    la_xmm5_l_idx,
    la_xmm5_h_idx,
    la_xmm6_l_idx,
    la_xmm6_h_idx,
    la_xmm7_l_idx,
    la_xmm7_h_idx,
    la_mm0_idx,
    la_mm1_idx,
    la_mm2_idx,
    la_mm3_idx,
    la_mm4_idx,
    la_mm5_idx,
    la_mm6_idx,
    la_mm7_idx,
    la_eflag_idx,
    la_ins_idx,
    la_ins_size_idx,
    la_ins_buf_lw_idx,
    la_ins_buf_hi_idx,
    la_opnds_idx,
};

struct opnd_item {
    uint64_t opnd_type;
    uint64_t mem_addr;
    uint64_t mem_data[2];
};

#define CONTEXT_IN 0
#define CONTEXT_OUT 1

struct store_x86_infos {
    uint64_t eflag;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t eax;
    uint64_t ecx;
    uint64_t edx;
    uint64_t ebx;
    uint64_t esp;
    uint64_t ebp;
    uint64_t esi;
    uint64_t edi;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t eip;
    /* xmm reg */
    uint64_t xmm0[2];
    uint64_t xmm1[2];
    uint64_t xmm2[2];
    uint64_t xmm3[2];
    uint64_t xmm4[2];
    uint64_t xmm5[2];
    uint64_t xmm6[2];
    uint64_t xmm7[2];
    /* mmx reg, shared with x87 fpu */
    uint64_t mm0;
    uint64_t mm1;
    uint64_t mm2;
    uint64_t mm3;
    uint64_t mm4;
    uint64_t mm5;
    uint64_t mm6;
    uint64_t mm7;
    uint64_t ctx_type;
    uint64_t ins;
    uint64_t ins_size;
    uint64_t ins_buf[2];
    uint64_t opnd_size;
    struct opnd_item opnd_items[0];
};

void gen_ins_context_in_helper(IR1_INST *pir1);
void trace_session_begin(uint64_t ins_length, uint64_t reg_store_addr,
    uint64_t eip, uint64_t mem_access_count);
