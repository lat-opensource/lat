#ifndef _ENV_H_
#define _ENV_H_

#include "ir1.h"
#include "ir2.h"
#include "imm-cache.h"

#define itemp_status_num \
    (sizeof(reg_itemp_map) / sizeof(int))
#define ftemp_status_num \
    (sizeof(reg_ftemp_map) / sizeof(int))

typedef struct TRANSLATION_DATA {
    // EXTENSION_MODE
    // ireg_em[IR2_ITEMP_MAX]; /* extension mode of the 32 integer registers */
    // int8 ireg_eb[IR2_ITEMP_MAX]; /* bits number where the extension starts */

    void *curr_tb; /* from QEMU */

    /* ir1 */
    IR1_INST *curr_ir1_inst;
    int curr_ir1_count;
    /* uint8       ir1_dump_threshold[MAX_IR1_NUM_PER_TB]; */

    /* ir2 */
    IR2_INST *ir2_inst_array;
    int ir2_inst_num_max;
    int ir2_inst_num_current;
    int real_ir2_inst_num;

    /* the list of ir2 */
    IR2_INST *first_ir2;
    IR2_INST *last_ir2;

    /* label number */
    int label_num;
    /* data number, used for storage pseudo inst data  */
    int data_num;

    /* temp reg */
    union {
        uint32_t all_temp_status;
        struct {
            uint16_t itemp_status;
            uint16_t ftemp_status;
        };
    };
#ifdef CONFIG_LATX_INSTS_PATTERN
    uint16_t ptn_itemp_status;
    IR2_OPND ptn_itemp0;
    IR2_OPND ptn_itemp1;
#endif

    int curr_top;               /* top value (changes when translating) */

    /* TODO : support static translation */
    uint8 curr_ir1_skipped_eflags; /* these eflag calculation can be skipped */
                                   /* (because of flag pattern, etc) */

    /* manage the immediate number cache */
    IMM_CACHE *imm_cache;
} TRANSLATION_DATA;

typedef struct ENV {
    void *cpu_state;            /* from QEMU,CPUArchState */
    TRANSLATION_DATA *tr_data;  /* from LATX */
#ifdef CONFIG_LATX_DEBUG
    uint64_t current_ir1;
#endif
#ifdef CONFIG_LATX_SYSCALL_TUNNEL
    void *syscall_optimize_confirm;
#endif
} ENV;

/*
 * *ATTANTION:*
 * When you want to use this macro, think twice.
 * Some ir2 instructions may be eliminated in ir2-optimization.
 * So I recommend you confirm if your code is *position-relative*.
 */
#define CURRENT_INST_COUNTER(env) (env->tr_data->real_ir2_inst_num)

/* eflags mask */
#define CF_BIT (1 << 0)
#define PF_BIT (1 << 2)
#define AF_BIT (1 << 4)
#define ZF_BIT (1 << 6)
#define SF_BIT (1 << 7)
#define OF_BIT (1 << 11)
#define DF_BIT (1 << 10)

#define CF_BIT_INDEX 0
#define PF_BIT_INDEX 2
#define AF_BIT_INDEX 4
#define ZF_BIT_INDEX 6
#define SF_BIT_INDEX 7
#define OF_BIT_INDEX 11
#define DF_BIT_INDEX 10

/* fcsr */
#define FCSR_OFF_EN_I           0
#define FCSR_OFF_EN_U           1
#define FCSR_OFF_EN_O           2
#define FCSR_OFF_EN_Z           3
#define FCSR_OFF_EN_V           4
#define FCSR_OFF_FLAGS_I        16
#define FCSR_OFF_FLAGS_U        17
#define FCSR_OFF_FLAGS_O        18
#define FCSR_OFF_FLAGS_Z        19
#define FCSR_OFF_FLAGS_V        20
#define FCSR_OFF_CAUSE_I        24
#define FCSR_OFF_CAUSE_U        25
#define FCSR_OFF_CAUSE_O        26
#define FCSR_OFF_CAUSE_Z        27
#define FCSR_OFF_CAUSE_V        28
#define FCSR_ENABLE_SET         0x1f
#define FCSR_ENABLE_CLEAR       (~0x1f)
/* RM */
#define FCSR_OFF_RM             8
#define FCSR_RM_CLEAR           (~(0x3 << FCSR_OFF_RM))
#define FCSR_RM_RD              0x3
#define FCSR_RM_RU              0x2
#define FCSR_RM_RZ              0x1

/* x87 FPU Status Register */
#define X87_SR_OFF_IE           0
#define X87_SR_OFF_DE           1
#define X87_SR_OFF_ZE           2
#define X87_SR_OFF_OE           3
#define X87_SR_OFF_UE           4
#define X87_SR_OFF_PE           5
#define X87_SR_OFF_SF           6
#define X87_SR_OFF_ES           7
#define X87_SR_OFF_C0           8
#define X87_SR_OFF_C1           9
#define X87_SR_OFF_C2           10
#define X87_SR_OFF_C3           14
#define X87_SR_OFF_B            15

/* x87 FPU Control Register */
#define X87_CR_OFF_IM           0
#define X87_CR_OFF_DM           1
#define X87_CR_OFF_ZM           2
#define X87_CR_OFF_OM           3
#define X87_CR_OFF_UM           4
#define X87_CR_OFF_PM           5
#define X87_CR_OFF_PC           8
#define X87_CR_OFF_X            12
/* Rounding Control */
#define X87_CR_OFF_RC           10
#define X87_CR_RC_RN            0x0
#define X87_CR_RC_RD            0x1
#define X87_CR_RC_RU            0x2
#define X87_CR_RC_RZ            0x3
/* Exception Masks */
#define X87_CR_EXCP_MASK        0x3f
#endif
