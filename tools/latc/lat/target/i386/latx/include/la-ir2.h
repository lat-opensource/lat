/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __LA_IR2_H_
#define __LA_IR2_H_

//0-byte 1-half 2-word 3-double
#define VLDI_IMM_TYPE0(mode, imm) ((mode&0x3)<<10|(imm&0x3ff))
//vseti
#define VLDI_IMM_TYPE1(mode, imm) (1<<12|(mode&0x3f)<<8|(imm&0xff))
#define VEXTRINS_IMM_4_0(n, m) ((n&0xf)<<4|(m&0xf))
#define XVPERMI_Q_4_0(n, m) ((n & 0xf) << 4 | (m & 0xf))
#define XVPERMI_D_2_2_2_2(a, b, c, d) ((a & 0x3) << 6 | \
            (b & 0x3) << 4 | (c & 0x3) << 2 | (d & 0x3))

/* IR2_JCC; */
typedef enum {
    COND_A,		//!CF && !ZF
    COND_AE,	//!CF
    COND_B,     // CF
    COND_BE,    // CF || ZF
    COND_E,     // ZF
    COND_NE,    //!ZF
    COND_G,     //!ZF && (SF == OF)
    COND_GE,    // SF == OF
    COND_L,     // SF != OF
    COND_LE,    // ZF && (SF != OF)
    COND_S,     // SF
    COND_NS,    //!SF
    COND_O,     // OF
    COND_NO,    //!OF
    COND_PE,    // PF
    COND_PO,    //!PF
} IR2_JCC;

/* IR2_FCMP_COND; */
typedef enum {
    FCMP_COND_CAF, //0
    FCMP_COND_SAF,
    FCMP_COND_CLT,
    FCMP_COND_SLT,
    FCMP_COND_CEQ, //4
    FCMP_COND_SEQ,
    FCMP_COND_CLE,
    FCMP_COND_SLE,
    FCMP_COND_CUN, //8
    FCMP_COND_SUN,
    FCMP_COND_CULT,
    FCMP_COND_SULT,
    FCMP_COND_CUEQ, //c
    FCMP_COND_SUEQ,
    FCMP_COND_CULE,
    FCMP_COND_SULE,
    FCMP_COND_CNE, //10
    FCMP_COND_SNE,
    FCMP_COND_UND1,
    FCMP_COND_UND2,
    FCMP_COND_COR, //14
    FCMP_COND_SOR,
    FCMP_COND_UND3,
    FCMP_COND_UND4,
    FCMP_COND_CUNE, //18
    FCMP_COND_SUNE,
} IR2_FCMP_COND;

/**
 * fcmp conditon mmapings from x86 to la
 * NOTE:
 * LA has no condition of nle and nlt, they are replaced by sult
 * and sule. Remeber the order of dest and src operand needs to be swapped.
 */
#define X86_FCMP_COND_EQ FCMP_COND_CEQ
#define X86_FCMP_COND_LT FCMP_COND_SLT
#define X86_FCMP_COND_LE FCMP_COND_SLE
#define X86_FCMP_COND_NEQ FCMP_COND_CUNE
#define X86_FCMP_COND_NLT FCMP_COND_SULE
#define X86_FCMP_COND_NLE FCMP_COND_SULT
#define X86_FCMP_COND_ORD FCMP_COND_COR
#define X86_FCMP_COND_UNORD FCMP_COND_CUN
#define X86_FCMP_COND_EQ_UQ FCMP_COND_CUEQ
#define X86_FCMP_COND_NGE FCMP_COND_SULT
#define X86_FCMP_COND_NGT FCMP_COND_SULE
#define X86_FCMP_COND_FALSE FCMP_COND_CAF
#define X86_FCMP_COND_NEQ_OQ FCMP_COND_CNE
#define X86_FCMP_COND_GE FCMP_COND_SLE
#define X86_FCMP_COND_GT FCMP_COND_SLT
#define X86_FCMP_COND_TRUE FCMP_COND_CAF
#define X86_FCMP_COND_EQ_OS FCMP_COND_SEQ
#define X86_FCMP_COND_LT_OQ FCMP_COND_CLT
#define X86_FCMP_COND_LE_OQ FCMP_COND_CLE
#define X86_FCMP_COND_UNORD_S FCMP_COND_SUN
#define X86_FCMP_COND_NEQ_US FCMP_COND_SUNE
#define X86_FCMP_COND_NLT_UQ FCMP_COND_CULE
#define X86_FCMP_COND_NLE_UQ FCMP_COND_CULT
#define X86_FCMP_COND_ORD_S FCMP_COND_SOR
#define X86_FCMP_COND_EQ_US FCMP_COND_SUEQ
#define X86_FCMP_COND_NGE_UQ FCMP_COND_CULT
#define X86_FCMP_COND_NGT_UQ FCMP_COND_CULE
#define X86_FCMP_COND_FALSE_OS FCMP_COND_SAF
#define X86_FCMP_COND_NEQ_OS FCMP_COND_SNE
#define X86_FCMP_COND_GE_OQ FCMP_COND_CLE
#define X86_FCMP_COND_GT_OQ FCMP_COND_CLT
#define X86_FCMP_COND_TRUE_US FCMP_COND_SAF

/* IR2_OPCODE; */
typedef enum {
#include "ir2-opcode.h"
} IR2_OPCODE;

/* IR2_OPND_TYPE */
typedef enum {
    IR2_OPND_NONE = 80,
    IR2_OPND_GPR,
    IR2_OPND_SCR,
    IR2_OPND_FPR,
    IR2_OPND_FCSR,      /* immediate used in cfc1/ctc1 */
    IR2_OPND_CC,        /* condition code, FCC field in FCSR */
    IR2_OPND_IMM,       /* immediate */
    IR2_OPND_LABEL,
    IR2_OPND_MEM,       /* middle type. not used as backend */
    IR2_OPND_PSEUDO,    /* always used at pseudo inst */
    IR2_OPND_DATA,      /* hint pseudo inst data (if data > 32 bits) */
} IR2_OPND_TYPE;

/* IR2_OPND */
typedef struct IR2_OPND {
    IR2_OPND_TYPE _type;
    union {
        uint32 _val; /* used for backend */
        union { /* used for IR2 data */
            int32 _reg_num;  /* GPR/FPR/MEM.reg */
            uint32 _imm32;   /* IMM */
            int32 _label_id; /* LABEL */
        };
    };
} IR2_OPND;
#endif
