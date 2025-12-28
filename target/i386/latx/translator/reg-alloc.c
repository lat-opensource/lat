/**
 * @file reg-alloc.c
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief Add some register allocation functions
 */
#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include <string.h>
#include "latx-options.h"
#include "translate.h"
#include "imm-cache.h"

/*
 * Init in global_register_init()
 */
IR2_OPND imm_zero_ir2_opnd;
IR2_OPND env_ir2_opnd;
IR2_OPND zero_ir2_opnd;
IR2_OPND ra_ir2_opnd;
IR2_OPND tp_ir2_opnd;
IR2_OPND sp_ir2_opnd;
IR2_OPND a0_ir2_opnd;
IR2_OPND a1_ir2_opnd;
IR2_OPND a2_ir2_opnd;
IR2_OPND a3_ir2_opnd;
IR2_OPND a4_ir2_opnd;
IR2_OPND a5_ir2_opnd;
IR2_OPND a6_ir2_opnd;
IR2_OPND a7_ir2_opnd;
IR2_OPND t0_ir2_opnd;
IR2_OPND t1_ir2_opnd;
IR2_OPND t2_ir2_opnd;
IR2_OPND t3_ir2_opnd;
IR2_OPND t4_ir2_opnd;
IR2_OPND t5_ir2_opnd;
IR2_OPND t6_ir2_opnd;
IR2_OPND t7_ir2_opnd;
IR2_OPND t8_ir2_opnd;
IR2_OPND r21_ir2_opnd;
IR2_OPND fp_ir2_opnd;
IR2_OPND s0_ir2_opnd;
IR2_OPND s1_ir2_opnd;
IR2_OPND s2_ir2_opnd;
IR2_OPND s3_ir2_opnd;
IR2_OPND s4_ir2_opnd;
IR2_OPND s5_ir2_opnd;
IR2_OPND s6_ir2_opnd;
IR2_OPND s7_ir2_opnd;
IR2_OPND s8_ir2_opnd;
IR2_OPND fcsr_ir2_opnd;
IR2_OPND fcsr1_ir2_opnd;
IR2_OPND fcsr2_ir2_opnd;
IR2_OPND fcsr3_ir2_opnd;
IR2_OPND fcc0_ir2_opnd;
IR2_OPND fcc1_ir2_opnd;
IR2_OPND fcc2_ir2_opnd;
IR2_OPND fcc3_ir2_opnd;
IR2_OPND fcc4_ir2_opnd;
IR2_OPND fcc5_ir2_opnd;
IR2_OPND fcc6_ir2_opnd;
IR2_OPND fcc7_ir2_opnd;

IR2_OPND scr0_ir2_opnd;
IR2_OPND scr1_ir2_opnd;
IR2_OPND scr2_ir2_opnd;
IR2_OPND scr3_ir2_opnd;

/*
 * LA opnd mapping table
 */
const uint64_t la_ir2_opnd_tab[] = {
    /* {reg_index, opnd addr} */
    REG_MAP_DEF(la_zero, (uint64_t)&zero_ir2_opnd),
    REG_MAP_DEF(la_ra, (uint64_t)&ra_ir2_opnd),
    REG_MAP_DEF(la_tp, (uint64_t)&tp_ir2_opnd),
    REG_MAP_DEF(la_sp, (uint64_t)&sp_ir2_opnd),
    REG_MAP_DEF(la_a0, (uint64_t)&a0_ir2_opnd),
    REG_MAP_DEF(la_a1, (uint64_t)&a1_ir2_opnd),
    REG_MAP_DEF(la_a2, (uint64_t)&a2_ir2_opnd),
    REG_MAP_DEF(la_a3, (uint64_t)&a3_ir2_opnd),
    REG_MAP_DEF(la_a4, (uint64_t)&a4_ir2_opnd),
    REG_MAP_DEF(la_a5, (uint64_t)&a5_ir2_opnd),
    REG_MAP_DEF(la_a6, (uint64_t)&a6_ir2_opnd),
    REG_MAP_DEF(la_a7, (uint64_t)&a7_ir2_opnd),
    REG_MAP_DEF(la_t0, (uint64_t)&t0_ir2_opnd),
    REG_MAP_DEF(la_t1, (uint64_t)&t1_ir2_opnd),
    REG_MAP_DEF(la_t2, (uint64_t)&t2_ir2_opnd),
    REG_MAP_DEF(la_t3, (uint64_t)&t3_ir2_opnd),
    REG_MAP_DEF(la_t4, (uint64_t)&t4_ir2_opnd),
    REG_MAP_DEF(la_t5, (uint64_t)&t5_ir2_opnd),
    REG_MAP_DEF(la_t6, (uint64_t)&t6_ir2_opnd),
    REG_MAP_DEF(la_t7, (uint64_t)&t7_ir2_opnd),
    REG_MAP_DEF(la_t8, (uint64_t)&t8_ir2_opnd),
    REG_MAP_DEF(la_r21, (uint64_t)&r21_ir2_opnd),
    REG_MAP_DEF(la_fp, (uint64_t)&fp_ir2_opnd),
    REG_MAP_DEF(la_s0, (uint64_t)&s0_ir2_opnd),
    REG_MAP_DEF(la_s1, (uint64_t)&s1_ir2_opnd),
    REG_MAP_DEF(la_s2, (uint64_t)&s2_ir2_opnd),
    REG_MAP_DEF(la_s3, (uint64_t)&s3_ir2_opnd),
    REG_MAP_DEF(la_s4, (uint64_t)&s4_ir2_opnd),
    REG_MAP_DEF(la_s5, (uint64_t)&s5_ir2_opnd),
    REG_MAP_DEF(la_s6, (uint64_t)&s6_ir2_opnd),
    REG_MAP_DEF(la_s7, (uint64_t)&s7_ir2_opnd),
    REG_MAP_DEF(la_s8, (uint64_t)&s8_ir2_opnd),
};

/*
 * Register mapping table
 */
const int reg_gpr_map[] = {
    REG_MAP_DEF(eax_index, la_s0),
    REG_MAP_DEF(ecx_index, la_s1),
    REG_MAP_DEF(edx_index, la_s2),
    REG_MAP_DEF(ebx_index, la_s3),
    REG_MAP_DEF(esp_index, la_s4),
    REG_MAP_DEF(ebp_index, la_s5),
    REG_MAP_DEF(esi_index, la_s6),
    REG_MAP_DEF(edi_index, la_s7),
#ifdef TARGET_X86_64
    REG_MAP_DEF(r8_index, la_a2),
    REG_MAP_DEF(r9_index, la_a3),
    REG_MAP_DEF(r10_index, la_a4),
    REG_MAP_DEF(r11_index, la_a5),
    REG_MAP_DEF(r12_index, la_t5),
    REG_MAP_DEF(r13_index, la_t6),
    REG_MAP_DEF(r14_index, la_t7),
    REG_MAP_DEF(r15_index, la_t8),
    REG_MAP_DEF(riz_index, la_zero),
#endif

};

const int reg_fpr_map[] = {
    REG_MAP_DEF(0, 0),
    REG_MAP_DEF(1, 1),
    REG_MAP_DEF(2, 2),
    REG_MAP_DEF(3, 3),
    REG_MAP_DEF(4, 4),
    REG_MAP_DEF(5, 5),
    REG_MAP_DEF(6, 6),
    REG_MAP_DEF(7, 7),
};
#define reg_mmx_map reg_fpr_map

const int reg_xmm_map[] = {
    REG_MAP_DEF(0, XMM0_MAPS),
    REG_MAP_DEF(1, XMM1_MAPS),
    REG_MAP_DEF(2, XMM2_MAPS),
    REG_MAP_DEF(3, XMM3_MAPS),
    REG_MAP_DEF(4, XMM4_MAPS),
    REG_MAP_DEF(5, XMM5_MAPS),
    REG_MAP_DEF(6, XMM6_MAPS),
    REG_MAP_DEF(7, XMM7_MAPS),
#ifdef TARGET_X86_64
    REG_MAP_DEF(8, XMM8_MAPS),
    REG_MAP_DEF(9, XMM9_MAPS),
    REG_MAP_DEF(10, XMM10_MAPS),
    REG_MAP_DEF(11, XMM11_MAPS),
    REG_MAP_DEF(12, XMM12_MAPS),
    REG_MAP_DEF(13, XMM13_MAPS),
    REG_MAP_DEF(14, XMM14_MAPS),
    REG_MAP_DEF(15, XMM15_MAPS),
#endif
};

const int reg_statics_map[] = {
    /* last exec tb */
    REG_MAP_DEF(S_UD0, la_fp),
    /* next x86 addr */
    REG_MAP_DEF(S_UD1, la_r21),
#ifndef TARGET_X86_64
    /* we don't use shadow stack */
    REG_MAP_DEF(S_UD2, NOT_DEF),
#endif
    REG_MAP_DEF(S_ENV, la_s8),
    REG_MAP_DEF(S_EFLAGS, la_a6),
};

const int reg_trace_map[] = {
    REG_MAP_DEF(TRACE, NOT_DEF),
};

const int reg_itemp_map[] = {
    REG_MAP_DEF(ITEMP0, la_t0),
    REG_MAP_DEF(ITEMP1, la_t1),
    REG_MAP_DEF(ITEMP2, la_t2),
    REG_MAP_DEF(ITEMP3, la_t3),
    REG_MAP_DEF(ITEMP4, la_t4),
#ifndef TARGET_X86_64
    REG_MAP_DEF(ITEMP5, la_t5),
    REG_MAP_DEF(ITEMP6, la_t6),
    REG_MAP_DEF(ITEMP7, la_t7),
    REG_MAP_DEF(ITEMP8, la_t8),
    REG_MAP_DEF(ITEMP9, la_ra),
    REG_MAP_DEF(ITEMP10, la_a7),
#else
    REG_MAP_DEF(ITEMP5, la_ra),
    REG_MAP_DEF(ITEMP6, la_a7),
#endif
};

const int reg_itemp_reverse_map[32] = {
    REG_MAP_DEF(0 ... 31, INVALID_TEMP),
    REG_MAP_DEF(la_t0, ITEMP0),
    REG_MAP_DEF(la_t1, ITEMP1),
    REG_MAP_DEF(la_t2, ITEMP2),
    REG_MAP_DEF(la_t3, ITEMP3),
    REG_MAP_DEF(la_t4, ITEMP4),
#ifndef TARGET_X86_64
    REG_MAP_DEF(la_t5, ITEMP5),
    REG_MAP_DEF(la_t6, ITEMP6),
    REG_MAP_DEF(la_t7, ITEMP7),
    REG_MAP_DEF(la_t8, ITEMP8),
    REG_MAP_DEF(la_ra, ITEMP9),
    REG_MAP_DEF(la_a7, ITEMP10),
#else
    REG_MAP_DEF(la_ra, ITEMP5),
    REG_MAP_DEF(la_a7, ITEMP6),
#endif
};

const int reg_ftemp_map[] = {
    REG_MAP_DEF(FTEMP0, 8),
    REG_MAP_DEF(FTEMP1, 9),
    REG_MAP_DEF(FTEMP2, 10),
    REG_MAP_DEF(FTEMP3, 11),
    REG_MAP_DEF(FTEMP4, 12),
    REG_MAP_DEF(FTEMP5, 13),
    REG_MAP_DEF(FTEMP6, 14),
    REG_MAP_DEF(FTEMP7, 15),
};

const int reg_ftemp_reverse_map[32] = {
    REG_MAP_DEF(0 ... 7, INVALID_TEMP),
    REG_MAP_DEF(8, FTEMP0),
    REG_MAP_DEF(9, FTEMP1),
    REG_MAP_DEF(10, FTEMP2),
    REG_MAP_DEF(11, FTEMP3),
    REG_MAP_DEF(12, FTEMP4),
    REG_MAP_DEF(13, FTEMP5),
    REG_MAP_DEF(14, FTEMP6),
    REG_MAP_DEF(15, FTEMP7),
    REG_MAP_DEF(16 ... 31, INVALID_TEMP),
};

const int reg_scr_map[] = {
    REG_MAP_DEF(0, 0),
    REG_MAP_DEF(1, 1),
    REG_MAP_DEF(2, 2),
    REG_MAP_DEF(3, 3),
};

/*
 * Base interface
 */
IR2_OPND get_la_ir2_opnd(int la_reg_num)
{
    return *(IR2_OPND *)la_ir2_opnd_tab[la_reg_num];
}

/* ra_alloc_gpr(int index) */
RA_ALLOC_FUNC(gpr, IR2_OPND_GPR)
/* ra_alloc_fpr(int index) */
RA_ALLOC_FUNC(fpr, IR2_OPND_FPR)
/* ra_alloc_mmx(int index) */
RA_ALLOC_FUNC(mmx, IR2_OPND_FPR)
/* ra_alloc_xmm(int index) */
RA_ALLOC_FUNC(xmm, IR2_OPND_FPR)
/* ra_alloc_statics */
RA_ALLOC_FUNC(statics, IR2_OPND_GPR)
/* ra_alloc_trace */
RA_ALLOC_FUNC(trace, IR2_OPND_GPR)
/* ra_alloc_src */
RA_ALLOC_FUNC(scr, IR2_OPND_SCR);
/* ra_alloc_itemp_num() */
RA_ALLOC_TEMP_NUM_FUNC(itemp)
/* ra_alloc_ftemp_num() */
RA_ALLOC_TEMP_NUM_FUNC(ftemp)

/*
 * Some encapsulation of base interface
 */
IR2_OPND ra_alloc_st(int st_num)
{
    return ra_alloc_fpr(st_num);
}

IR2_OPND ra_alloc_itemp(void)
{
    IR2_OPND ir2_opnd;
    int itemp_reg_num;

    itemp_reg_num = ra_alloc_itemp_num();
#ifdef CONFIG_LATX_IMM_REG
    if (option_imm_reg) {
        int itemp_num = reg_itemp_reverse_map[itemp_reg_num];
        if (imm_cache_is_imm_itemp(itemp_num)) {
            // need free
            free_imm_reg(itemp_num);
        }
    }
#endif
    ir2_opnd_build(&ir2_opnd, IR2_OPND_GPR, itemp_reg_num);

    return ir2_opnd;
}

IR2_OPND ra_alloc_ftemp(void)
{
    IR2_OPND ir2_opnd;
    ir2_opnd_build(&ir2_opnd, IR2_OPND_FPR, ra_alloc_ftemp_num());
    return ir2_opnd;
}

inline bool itemp_is_free(int itemp_num)
{
    int mask = 1 << itemp_num;
    return (lsenv->tr_data->itemp_status & mask) == 0;
}

IR2_OPND ra_alloc_imm_reg(int itemp_num)
{
    if (itemp_is_free(itemp_num)) {
        //itemp_num 3-6 / 6-9
        return ir2_opnd_new(IR2_OPND_GPR, reg_itemp_map[itemp_num]);
    }
    return ir2_opnd_new_none();
}

void free_imm_reg(int itemp_num)
{
    qemu_log_mask(LAT_IMM_REG, "[free_imm_reg]%d\n", itemp_num);
    imm_cache_free_itemp(lsenv->tr_data->imm_cache, itemp_num);
}

void free_imm_reg_all(void)
{
    imm_cache_free_all(lsenv->tr_data->imm_cache);
}

/**
 * this function is avoid free temp call after mem computed.
 * if the temp is imm_reg,should skip;
 */
void imm_cache_free_temp_helper(IR2_OPND dest_op)
{
    imm_log("[free_temp_helper]call\n");
    if (!option_imm_reg ||
        lsenv->tr_data->imm_cache->itemp_allocated == true) {
        imm_log("[free_temp_helper]free_temp_auto\n");
        ra_free_temp_auto(dest_op);
        return;
    }
    imm_log("[free_temp_helper]call but not free\n");
}

IR2_OPND ra_alloc_num_4095(void)
{
    IR2_OPND num = ra_alloc_itemp();
    la_ori(num, zero_ir2_opnd, 0xfff);
    return num;
}

inline __attribute__((__always_inline__)) void ra_free_num_4095(IR2_OPND opnd)
{
    ra_free_itemp(opnd._reg_num);
}

inline __attribute__((__always_inline__)) void ra_free_all(void)
{
    lsenv->tr_data->all_temp_status = 0;
}

/*
 * Future patches may remove it.
 */
IR2_OPND ra_alloc_mda(void)
{
    return ra_ir2_opnd;
}
