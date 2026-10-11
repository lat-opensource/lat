/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file reg-map.h
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief Header file for register mapping rules
 */
#ifndef _REG_MAP_H_
#define _REG_MAP_H_

#define REG_MAP_DEF(x86_index, la_index)        \
[x86_index] = la_index

#ifdef TARGET_X86_64
#define GPR_NUM         16
#define XMM_NUM         16
#else /* i386 temp reg numbers */
#define GPR_NUM         8
#define XMM_NUM         8
#endif

#define NOT_DEF         -1

/*
 * X86 GPR
 */
#define eax_index       0
#define ecx_index       1
#define edx_index       2
#define ebx_index       3
#define esp_index       4
#define ebp_index       5
#define esi_index       6
#define edi_index       7
#ifdef TARGET_X86_64
#define rax_index       eax_index
#define rcx_index       ecx_index
#define rdx_index       edx_index
#define rbx_index       ebx_index
#define rsp_index       esp_index
#define rbp_index       ebp_index
#define rsi_index       esi_index
#define rdi_index       edi_index
#define r8_index        8
#define r9_index        9
#define r10_index       10
#define r11_index       11
#define r12_index       12
#define r13_index       13
#define r14_index       14
#define r15_index       15
#define riz_index       16
#endif

/*
 * Segment registers
 */
#define es_index        0
#define cs_index        1
#define ss_index        2
#define ds_index        3
#define fs_index        4
#define gs_index        5

/*
 * Virtual registers (self-defined registers)
 */
#define STATIC_NUM      4
#define S_UD0           0
#define S_UD1           1
#define S_UD2           2
#define S_ENV           3
#define S_EFLAGS        4

/*
 * Trace registers (self-defined registers)
 */
#define TRACE           0

/*
 * temporary registers
 */
#define INVALID_TEMP    -1
#define ITEMP0          0
#define ITEMP1          1
#define ITEMP2          2
#define ITEMP3          3
#define ITEMP4          4
#define ITEMP5          5
#define ITEMP6          6
#define ITEMP7          7
#define ITEMP8          8
#define ITEMP9          9
#define ITEMP10         10
#define FTEMP0          0
#define FTEMP1          1
#define FTEMP2          2
#define FTEMP3          3
#define FTEMP4          4
#define FTEMP5          5
#define FTEMP6          6
#define FTEMP7          7


/*
 * xmm registers maps
 */
#define XMM0_MAPS       16
#define XMM1_MAPS       17
#define XMM2_MAPS       18
#define XMM3_MAPS       19
#define XMM4_MAPS       20
#define XMM5_MAPS       21
#define XMM6_MAPS       22
#define XMM7_MAPS       23
#define XMM8_MAPS       24
#define XMM9_MAPS       25
#define XMM10_MAPS      26
#define XMM11_MAPS      27
#define XMM12_MAPS      28
#define XMM13_MAPS      29
#define XMM14_MAPS      30
#define XMM15_MAPS      31

/*
 * LA registers
 */
#define la_zero    0
#define la_ra      1
#define la_tp      2
#define la_sp      3
#define la_a0      4
#define la_a1      5
#define la_a2      6
#define la_a3      7
#define la_a4      8
#define la_a5      9
#define la_a6      10
#define la_a7      11
#define la_t0      12
#define la_t1      13
#define la_t2      14
#define la_t3      15
#define la_t4      16
#define la_t5      17
#define la_t6      18
#define la_t7      19
#define la_t8      20
#define la_r21     21
#define la_fp      22
#define la_s0      23
#define la_s1      24
#define la_s2      25
#define la_s3      26
#define la_s4      27
#define la_s5      28
#define la_s6      29
#define la_s7      30
#define la_s8      31


extern const int reg_gpr_map[];
extern const int reg_fpr_map[];
extern const int reg_xmm_map[];
extern const int reg_statics_map[];
extern const int reg_itemp_map[];
extern const int reg_ftemp_map[];
extern const int reg_itemp_reverse_map[];
extern const int reg_ftemp_reverse_map[];

#endif
