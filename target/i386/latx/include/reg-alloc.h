/**
 * @file reg-alloc.h
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief Header file for register allocation functions
 */
#ifndef _REG_ALLOC_H_
#define _REG_ALLOC_H_

#include "ir2.h"
#include "reg-map.h"
#include "lsenv.h"
#include "imm-cache.h"

extern IR2_OPND imm_zero_ir2_opnd;
extern IR2_OPND env_ir2_opnd;
extern IR2_OPND zero_ir2_opnd;
extern IR2_OPND ra_ir2_opnd;
extern IR2_OPND tp_ir2_opnd;
extern IR2_OPND sp_ir2_opnd;
extern IR2_OPND a0_ir2_opnd;
extern IR2_OPND a1_ir2_opnd;
extern IR2_OPND a2_ir2_opnd;
extern IR2_OPND a3_ir2_opnd;
extern IR2_OPND a4_ir2_opnd;
extern IR2_OPND a5_ir2_opnd;
extern IR2_OPND a6_ir2_opnd;
extern IR2_OPND a7_ir2_opnd;
extern IR2_OPND t0_ir2_opnd;
extern IR2_OPND t1_ir2_opnd;
extern IR2_OPND t2_ir2_opnd;
extern IR2_OPND t3_ir2_opnd;
extern IR2_OPND t4_ir2_opnd;
extern IR2_OPND t5_ir2_opnd;
extern IR2_OPND t6_ir2_opnd;
extern IR2_OPND t7_ir2_opnd;
extern IR2_OPND t8_ir2_opnd;
extern IR2_OPND r21_ir2_opnd;
extern IR2_OPND fp_ir2_opnd;
extern IR2_OPND s0_ir2_opnd;
extern IR2_OPND s1_ir2_opnd;
extern IR2_OPND s2_ir2_opnd;
extern IR2_OPND s3_ir2_opnd;
extern IR2_OPND s4_ir2_opnd;
extern IR2_OPND s5_ir2_opnd;
extern IR2_OPND s6_ir2_opnd;
extern IR2_OPND s7_ir2_opnd;
extern IR2_OPND s8_ir2_opnd;
extern IR2_OPND fcsr_ir2_opnd;
extern IR2_OPND fcsr1_ir2_opnd;
extern IR2_OPND fcsr2_ir2_opnd;
extern IR2_OPND fcsr3_ir2_opnd;
extern IR2_OPND fcc0_ir2_opnd;
extern IR2_OPND fcc1_ir2_opnd;
extern IR2_OPND fcc2_ir2_opnd;
extern IR2_OPND fcc3_ir2_opnd;
extern IR2_OPND fcc4_ir2_opnd;
extern IR2_OPND fcc5_ir2_opnd;
extern IR2_OPND fcc6_ir2_opnd;
extern IR2_OPND fcc7_ir2_opnd;
extern IR2_OPND scr0_ir2_opnd;
extern IR2_OPND scr1_ir2_opnd;
extern IR2_OPND scr2_ir2_opnd;
extern IR2_OPND scr3_ir2_opnd;

#define V0_RENAME_OPND  a7_ir2_opnd
#define LAST_TB_OPND    ra_ir2_opnd

#define ra_free_temp(opnd)                                              \
    do {                                                                \
        if (ir2_opnd_is_itemp(&opnd)) {                                 \
            ra_free_itemp(opnd._reg_num);                               \
        } else if (ir2_opnd_is_ftemp(&opnd)) {                          \
            ra_free_ftemp(opnd._reg_num);                               \
        } else {                                                        \
            lsassertm(0, "attempt to free a non-temp register");        \
        }                                                               \
    } while (0)

#define ra_free_temp_auto(opnd)                                         \
    do {                                                                \
        if (ir2_opnd_is_itemp(&opnd)) {                                 \
            ra_free_itemp(opnd._reg_num);                               \
        } else if (ir2_opnd_is_ftemp(&opnd)) {                          \
            ra_free_ftemp(opnd._reg_num);                               \
        }                                                               \
    } while (0)

/*
 * Template functions
 */
#define RA_ALLOC_FUNC_DEF(name) IR2_OPND ra_alloc_##name(int)
#define RA_ALLOC_FUNC(name, type)                                       \
IR2_OPND ra_alloc_##name(int index)                                     \
{                                                                       \
    lsassert(index >= 0 &&                                              \
             index < (sizeof(reg_##name##_map) / sizeof(int)));         \
    IR2_OPND opnd;                                                      \
    opnd._type = type;                                                  \
    opnd._reg_num = reg_##name##_map[index];                            \
    return opnd;                                                        \
}
/*
 * Calculate the number of used temp reg
 */
#define RA_ALLOC_TEMP_NUM_FUNC(name)                                    \
static int ra_alloc_##name##_num(void)                                  \
{                                                                       \
    int name##_num, cto_num;                                            \
    asm("cto.w %0, %1\n\t"                                              \
    : "=r"(cto_num)                                                     \
    : "r"(lsenv->tr_data->name##_status));                              \
    lsassertm(cto_num < name##_status_num,                              \
              "\n%s:%d alloc " #name " failed, cto_num %d\n",           \
              __func__, __LINE__, cto_num);                             \
    name##_num = reg_##name##_map[cto_num];                             \
    BITS_SET(lsenv->tr_data->name##_status, 1 << cto_num);              \
    return name##_num;                                                  \
}

#define RA_FREE_CODE(name, phy_id)                                      \
    do {                                                                \
        int virt_id = reg_##name##_reverse_map[phy_id];                 \
        lsassertm(virt_id >= 0 &&                                       \
                ((lsenv->tr_data->name##_status) & (1 << virt_id)),     \
                "\n%s:%d free " #name " failed, phy_id %d virt_id %d "  \
                "lsenv->tr_data->"#name"_status 0x%x\n",                \
                 __func__, __LINE__, phy_id, virt_id,                   \
                lsenv->tr_data->name##_status);                         \
        lsenv->tr_data->name##_status &= ~(1 << virt_id);               \
    } while (0)

#define INIT_RA(type, phyno)                                            \
(IR2_OPND){._type = type, ._reg_num = phyno}
#define INIT_IMM(imm)                                                   \
    (IR2_OPND){._type = IR2_OPND_IMM, ._imm32 = imm}

/*
 * Function declarations
 */
IR2_OPND get_la_ir2_opnd(int);
/* ra_alloc_gpr */
RA_ALLOC_FUNC_DEF(gpr);
/* ra_alloc_fpr */
RA_ALLOC_FUNC_DEF(fpr);
/* ra_alloc_mmx */
RA_ALLOC_FUNC_DEF(mmx);
/* ra_alloc_xmm */
RA_ALLOC_FUNC_DEF(xmm);
/* ra_alloc_statics */
RA_ALLOC_FUNC_DEF(statics);
/* ra_alloc_trace */
RA_ALLOC_FUNC_DEF(trace);
/* ra_alloc_src */
RA_ALLOC_FUNC_DEF(scr);
/* allocate and free temp register */
IR2_OPND ra_alloc_itemp(void);
IR2_OPND ra_alloc_ftemp(void);
IR2_OPND ra_alloc_imm_reg(int itemp_num);
bool ld_imm_to_imm_reg(uint64);
#define ra_free_itemp(phy_id)   RA_FREE_CODE(itemp, phy_id)
#define ra_free_ftemp(phy_id)   RA_FREE_CODE(ftemp, phy_id)
void ra_free_all(void);
bool itemp_is_free(int itemp_num);
IR2_OPND ra_alloc_num_4095(void);
void ra_free_num_4095(IR2_OPND);
void free_imm_reg(int itemp_num);
void free_imm_reg_all(void);
void imm_cache_free_temp_helper(IR2_OPND dest_op);

/*
 * Alias
 */
#define ra_alloc_static0() ra_alloc_statics(S_UD0)
#define ra_alloc_dbt_arg2() ra_alloc_statics(S_UD1)
#define ra_alloc_ss() ra_alloc_statics(S_UD2)
#define ra_alloc_env() ra_alloc_statics(S_ENV)
#define ra_alloc_eflags() ra_alloc_statics(S_EFLAGS)
#define ra_alloc_itemp_internal ra_alloc_itemp
#define ra_alloc_ftemp_internal ra_alloc_ftemp

/* allocate x86 register */
IR2_OPND ra_alloc_st(int);
IR2_OPND ra_alloc_mda(void);

#endif
