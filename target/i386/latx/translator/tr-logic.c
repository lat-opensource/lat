#include "common.h"
#include "reg-alloc.h"
#include "flag-lbt.h"
#include "latx-options.h"
#include "translate.h"
#include "hbr.h"

static bool translate_shrd_imm(IR1_INST *pir1);
static bool translate_shrd_cl(IR1_INST *pir1);
static bool translate_shld_cl(IR1_INST *pir1);
static bool translate_shld_imm(IR1_INST *pir1);

bool translate_xor(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_xor(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;
    bool opt_imm, eflags_calc;

    opnd0_size = ir1_opnd_size(opnd0);
    opt_imm = ir1_opnd_is_s2uimm12(opnd1);
    eflags_calc = ir1_need_calculate_any_flag(pir1);

    /* get src1 */
    if (opt_imm && !eflags_calc) {
        /*
         * If src1 is imm12 and eflags need not calculate,
         * src1 need not load into register.
         */
        src1 = zero_ir2_opnd;
    } else {
        src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    }

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
        if (opnd0_size >= 32) {
            dest = src0;
        } else {
            dest = ra_alloc_itemp();
        }
    } else {
        src0 = ra_alloc_itemp();
        dest = src0;
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /* set eflag and calculate */
    if (ir1_opnd_is_same_reg(opnd0, opnd1) && ir1_opnd_size(opnd0) >= 32) {
        la_mov64(dest, zero_ir2_opnd);
        generate_eflag_calculation(dest, dest, dest, pir1, true);
        return true;
    }

    generate_eflag_calculation(dest, src0, src1, pir1, true);
    if (opt_imm) {
        la_xori(dest, src0, ir1_opnd_s2uimm(opnd1));
    } else {
        la_xor(dest, src0, src1);
    }

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
#ifdef TARGET_X86_64
        /* r32 */
        if (!GHBR_ON(pir1) && CODEIS64 && opnd0_size == 32) {
            la_mov32_zx(dest, dest);
        } else
#endif
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

/**
 * @brief check imm if it is a maskable value
 * @details Maskable value is a type of value which has consecutive ONEs from
 * the beginning (end) to some position, and other position are ZEROs. For
 * example, 0x00ff (low maskable) and 0xff00 (high maskable) are maskable value,
 * but 0x00ff00 is not. This function can help you check if the value is
 * maskable, also get the type of the maskable value (high maskable/low
 * maskable) and the dividing position between 0 and 1.
 *
 * @param imm input imm
 * @param low if imm is a low maskable value
 * @param pos the mask position
 * @return true if it is a maskable value
 * @return false if not
 */
static inline bool imm_mask(int32_t imm, bool *low, uint32_t *pos)
{
    /* msb is 0 (0x00ff) */
    *low = imm > 0;
    /**
     * First:
     * 0xff00 => 0x00ff => 0x0100
     *        \ neg     \ +1
     * 0x00ff           => 0x0100
     *
     * Then:
     * Check 1 position.
     *
     * |<-clz->|<--ctz-->|
     * 0000 0001 0000 0000  <= 0x0100 (from 0xff00 or 0x00ff)
     * 0000 0111 0000 0000  <= 0x0700 (from 0xf900 or 0x06ff)
     * |<clz>| |<--ctz-->|
     *
     * if (clz + ctz + 1 == len):
     *   maskable vaule
     * else
     *   False
     */

    /* if 0xff00: ~imm + 1; then imm + 1; */
    imm = (*low ? imm : ~imm) + 1;
    /* now src_imm is 0b0..010..0 if in special mode */
    uint32_t t_pos = __builtin_ctz(imm);
    uint32_t l_pos = __builtin_clz(imm);
    if (t_pos + l_pos == sizeof(imm) * 8 - 1) {
       *pos = t_pos;
       return true;
    }
    *pos = 0;
    *low = false;
    return false;
}

bool translate_and(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_and(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;
    bool opt_imm, eflags_calc;

    opnd0_size = ir1_opnd_size(opnd0);
    opt_imm = ir1_opnd_is_s2uimm12(opnd1);
    eflags_calc = ir1_need_calculate_any_flag(pir1);

    /*
     * Find imm is special mode?
     * 0xff00 or 0x00ff
     */
    bool special_mask = false, low_mask = false;
    uint32_t mask_pos = 0;
    /* 8h will load to temp and store, do not need to check */
    if (ir1_opnd_is_imm(opnd1)) {
        special_mask = imm_mask(ir1_opnd_simm(opnd1), &low_mask, &mask_pos);
        opt_imm |= special_mask;
    }

    /* get src1 */
    if (opt_imm && !eflags_calc) {
        /*
         * If src1 is imm12 and eflags need not calculate,
         * src1 need not load into register.
         */
        src1 = zero_ir2_opnd;
    } else {
        src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    }

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
        /* special_mask can clear origin register directly */
        if (opnd0_size >= 32 || special_mask) {
            dest = src0;
        } else {
            dest = ra_alloc_itemp();
        }
    } else {
        src0 = ra_alloc_itemp();
        dest = src0;
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /* set eflag */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* calculate */
    if (special_mask) {
        lsassert(IR2_OPND_EQ(dest, src0));
        if (low_mask) {
            /* low mask, clear high bits */
            int _high_pos = (opnd0_size >= 32) ? 63 : (opnd0_size - 1);
            la_bstrins_d(dest, zero_ir2_opnd, _high_pos, mask_pos);
        } else if (mask_pos > 0) {
            /* high mask, clear low bits (pos cannot be 0 -> 0xff) */
            la_bstrins_d(dest, zero_ir2_opnd, mask_pos - 1, 0);
        }
    } else if (opt_imm) {
        la_andi(dest, src0, ir1_opnd_s2uimm(opnd1));
    } else {
        la_and(dest, src0, src1);
    }

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
#ifdef TARGET_X86_64
        /* r32, also low_mask will clear high bits */
        if (!GHBR_ON(pir1) && CODEIS64 && opnd0_size == 32 && !low_mask) {
            la_mov32_zx(dest, dest);
        } else
#endif
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

bool translate_test(IR1_INST *pir1)
{
    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0), UNKNOWN_EXTENSION, false);
    IR2_OPND src_opnd_1 =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 1), UNKNOWN_EXTENSION, false);

    generate_eflag_calculation(src_opnd_1, src_opnd_0, src_opnd_1, pir1, true);
    return true;
}

bool translate_or(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_or(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;
    bool opt_imm, eflags_calc;

    opnd0_size = ir1_opnd_size(opnd0);
    opt_imm = ir1_opnd_is_s2uimm12(opnd1);
    eflags_calc = ir1_need_calculate_any_flag(pir1);

    /* get src1 */
    if (opt_imm && !eflags_calc) {
        /*
         * If src1 is imm12 and eflags need not calculate,
         * src1 need not load into register.
         */
        src1 = zero_ir2_opnd;
    } else {
        src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    }

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
        if (opnd0_size >= 32) {
            dest = src0;
        } else {
            dest = ra_alloc_itemp();
        }
    } else {
        src0 = ra_alloc_itemp();
        dest = src0;
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /* set eflag */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* calculate */
    if (opt_imm) {
        la_ori(dest, src0, ir1_opnd_s2uimm(opnd1));
    } else {
        la_or(dest, src0, src1);
    }

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
#ifdef TARGET_X86_64
        /* r32 */
        if (!GHBR_ON(pir1) && CODEIS64 && opnd0_size == 32) {
            la_mov32_zx(dest, dest);
        } else
#endif
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

bool translate_not(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_not(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, dest;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;

    opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        src0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
        if (opnd0_size >= 32) {
            dest = src0;
        } else {
            dest = ra_alloc_itemp();
        }
    } else {
        src0 = ra_alloc_itemp();
        dest = src0;
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    /* calculate */
    la_nor(dest, zero_ir2_opnd, src0);
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif

    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
        if (is_lock) {
            tr_lat_spin_unlock(lat_lock_addr);
        }
    }

    return true;
}

/**
 * @brief translate inst shl
 * SHL inst has some patterns
 * - shl reg/mem, imm8/1
 * - shl reg/mem, CL
 *
 * We can see that it can have some optimizations:
 * - shl reg, imm8 < optimize
 * - shl mem, imm8 < optimize
 * - shl reg, CL   < optimize
 * - shl mem, CL   < default
 *
 * @param pir1
 * @return true
 * @return false
 */
bool translate_shl(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);

    int opnd_size = ir1_opnd_size(opnd1);
    bool dest_is_gpr = ir1_opnd_is_gpr(opnd1);
    bool directly = dest_is_gpr && (opnd_size >= 32);

    uint32 mask = (ir1_opnd_size(opnd1) == 64) ? 63 : 31;

    IR2_INST *(*shifti_inst)(IR2_OPND, IR2_OPND, int);
    IR2_INST *(*shift_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND src, dest;

    if (directly) {
        /* GPR32/GPR64 can broken the reg */
        dest = src = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
    } else {
        /*
         * shift left did not consider EXTENSION
         *   because high bits will be destroied
         */
        src = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
        dest = ra_alloc_itemp();
    }
    shift_inst = (opnd_size == 64) ? la_sll_d : la_sll_w;
    shifti_inst = (opnd_size == 64) ? la_slli_d : la_slli_w;

    if (ir1_opnd_is_imm(opnd2)) {
        /* Shift is imm8/1 case */
        uint8 shift = ir1_opnd_uimm(opnd2) & mask;

        if (shift) {
            /* we can use lbt.i but imm can only contain non-overflow case */
            bool can_use_imm = shift < opnd_size;
            generate_eflag_calculation(dest, src,
                                       ir2_opnd_new(IR2_OPND_IMM, shift), pir1,
                                       can_use_imm);
            shifti_inst(dest, src, shift);
        }
        /*
         * When we want to calc dest?
         *           GPR64/32   else
         * shift         Y       Y
         * noshift       Y       N
         */
        if (shift || directly) {
            store_ireg_to_ir1(dest, opnd1, false);
        }

    } else {
        IR2_OPND label_exit = ra_alloc_label();
        /* Shift is CL */
        IR2_OPND count =
            load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);

        if (ir1_need_calculate_any_flag(pir1)) {
            IR2_OPND masked_count = ra_alloc_itemp();

            /* 1. mask */
            la_andi(masked_count, count, mask);
            /* 2. check if zero */
            la_beq(masked_count, zero_ir2_opnd, label_exit);

            generate_eflag_calculation(dest, src, count, pir1, false);
        }

        /* 3. shift */
        shift_inst(dest, src, count);

        /*
         * For dest is:
         * - GPR64, dest == opnd1, no operate
         * - GRP32, dest == opnd1, but ZX not, clean high 32 bit
         * - else, do default
         * For count == zero
         * - GPR32, clean high 32 bit
         * - else, nothing
         */
        if (directly) {
            la_label(label_exit);
            store_ireg_to_ir1(dest, opnd1, false);
        } else {
            store_ireg_to_ir1(dest, opnd1, false);
            la_label(label_exit);
        }
    }

    return true;
}

/**
 * @brief translate inst shr
 * SHR inst has some patterns
 * - shr reg/mem, imm8/1
 * - shr reg/mem, CL
 *
 * We can see that it can have some optimizations:
 * - shr reg, imm8 < optimize
 * - shr mem, imm8 < optimize
 * - shr reg, CL   < optimize
 * - shr mem, CL   < default
 *
 * @param pir1
 * @return true
 * @return false
 */
bool translate_shr(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);

    int opnd_size = ir1_opnd_size(opnd1);
    bool dest_is_gpr = ir1_opnd_is_gpr(opnd1);
    bool directly = dest_is_gpr && (opnd_size >= 32);

    uint32 mask = (ir1_opnd_size(opnd1) == 64) ? 63 : 31;

    IR2_INST *(*shifti_inst)(IR2_OPND, IR2_OPND, int);
    IR2_INST *(*shift_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND src, dest;

    if (directly) {
        /* GPR32/GPR64 can broken the reg */
        dest = src = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
    } else {
        src = load_ireg_from_ir1(opnd1, ZERO_EXTENSION, false);
        dest = ra_alloc_itemp();
    }
    shift_inst = (opnd_size == 64) ? la_srl_d : la_srl_w;
    shifti_inst = (opnd_size == 64) ? la_srli_d : la_srli_w;

    if (ir1_opnd_is_imm(opnd2)) {
        /* Shift is imm8/1 case */
        uint8 shift = ir1_opnd_uimm(opnd2) & mask;

        if (shift) {
            /* we can use lbt.i but imm can only contain non-overflow case */
            bool can_use_imm = shift < opnd_size;
            generate_eflag_calculation(dest, src,
                                       ir2_opnd_new(IR2_OPND_IMM, shift), pir1,
                                       can_use_imm);
            shifti_inst(dest, src, shift);
        }
        /*
         * When we want to calc dest?
         *           GPR64/32   else
         * shift         Y       Y
         * noshift       Y       N
         */
        if (shift || directly) {
            store_ireg_to_ir1(dest, opnd1, false);
        }

    } else {
        IR2_OPND label_exit = ra_alloc_label();
        /* Shift is CL */
        IR2_OPND count =
            load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);

        if (ir1_need_calculate_any_flag(pir1)) {
            IR2_OPND masked_count = ra_alloc_itemp();

            /* 1. mask */
            la_andi(masked_count, count, mask);
            /* 2. check if zero */
            la_beq(masked_count, zero_ir2_opnd, label_exit);

            generate_eflag_calculation(dest, src, count, pir1, false);
        }

        /* 3. shift */
        shift_inst(dest, src, count);

        /*
         * For dest is:
         * - GPR64, dest == opnd1, no operate
         * - GRP32, dest == opnd1, but ZX not, clean high 32 bit
         * - else, do default
         * For count == zero
         * - GPR32, clean high 32 bit
         * - else, nothing
         */
        if (directly) {
            la_label(label_exit);
            store_ireg_to_ir1(dest, opnd1, false);
        } else {
            store_ireg_to_ir1(dest, opnd1, false);
            la_label(label_exit);
        }
    }

    return true;
}

bool translate_sal(IR1_INST *pir1)
{
    return translate_shl(pir1);
}

/**
 * @brief translate inst sar
 * SAR inst has some patterns
 * - sar reg/mem, imm8/1
 * - sar reg/mem, CL
 *
 * We can see that it can have some optimizations:
 * - sar reg, imm8 < optimize
 * - sar mem, imm8 < optimize
 * - sar reg, CL   < optimize
 * - sar mem, CL   < default
 *
 * @param pir1
 * @return true
 * @return false
 */
bool translate_sar(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 1);

    int opnd_size = ir1_opnd_size(opnd1);
    bool dest_is_gpr = ir1_opnd_is_gpr(opnd1);
    bool directly = dest_is_gpr && (opnd_size >= 32);

    uint32 mask = (ir1_opnd_size(opnd1) == 64) ? 63 : 31;

    IR2_INST *(*shifti_inst)(IR2_OPND, IR2_OPND, int);
    IR2_INST *(*shift_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND src, dest;

    if (directly) {
        /* GPR32/GPR64 can broken the reg */
        dest = src = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
    } else {
        src = load_ireg_from_ir1(opnd1, SIGN_EXTENSION, false);
        dest = ra_alloc_itemp();
    }
    shift_inst = (opnd_size == 64) ? la_sra_d : la_sra_w;
    shifti_inst = (opnd_size == 64) ? la_srai_d : la_srai_w;

    if (ir1_opnd_is_imm(opnd2)) {
        /* Shift is imm8/1 case */
        uint8 shift = ir1_opnd_uimm(opnd2) & mask;

        if (shift) {
            /* we can use lbt.i but imm can only contain non-overflow case */
            bool can_use_imm = shift < opnd_size;
            generate_eflag_calculation(dest, src,
                                       ir2_opnd_new(IR2_OPND_IMM, shift), pir1,
                                       can_use_imm);
            shifti_inst(dest, src, shift);
        }
        /*
         * When we want to calc dest?
         *           GPR64/32   else
         * shift         Y       Y
         * noshift       Y       N
         */
        if (shift || directly) {
            store_ireg_to_ir1(dest, opnd1, false);
        }

    } else {
        IR2_OPND label_exit = ra_alloc_label();
        /* Shift is CL */
        IR2_OPND count =
            load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);

        if (ir1_need_calculate_any_flag(pir1)) {
            IR2_OPND masked_count = ra_alloc_itemp();

            /* 1. mask */
            la_andi(masked_count, count, mask);
            /* 2. check if zero */
            la_beq(masked_count, zero_ir2_opnd, label_exit);

            generate_eflag_calculation(dest, src, count, pir1, false);
        }

        /* 3. shift */
        shift_inst(dest, src, count);

        /*
         * For dest is:
         * - GPR64, dest == opnd1, no operate
         * - GRP32, dest == opnd1, but ZX not, clean high 32 bit
         * - else, do default
         * For count == zero
         * - GPR32, clean high 32 bit
         * - else, nothing
         */
        if (directly) {
            la_label(label_exit);
            store_ireg_to_ir1(dest, opnd1, false);
        } else {
            store_ireg_to_ir1(dest, opnd1, false);
            la_label(label_exit);
        }
    }

    return true;
}

bool translate_rol(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int dest_size = ir1_opnd_size(opnd0);

    if (ir1_opnd_is_imm(opnd1)) {
        /* Shift is imm8/1 case */
        uint32 mask = (dest_size == 64) ? 63 : 31;
        uint8 shift = ir1_opnd_uimm(opnd1) & mask;

        if(shift) {
            IR2_OPND dest;
            if(ir1_opnd_is_gpr(opnd0) && !ir1_opnd_is_8h(opnd0)) {
                int gpr_num = ir1_opnd_base_reg_num(opnd0);
                dest =  ra_alloc_gpr(gpr_num);
            } else {
                dest = load_ireg_from_ir1(opnd0, ZERO_EXTENSION, false);
            }


            if (ir1_need_calculate_any_flag(pir1)) {
                if (dest_size == 8) {
                    la_x86rotli_b(dest, shift);
                } else if (dest_size == 16) {
                    la_x86rotli_h(dest, shift);
                } else if (dest_size == 32) {
                    la_x86rotli_w(dest, shift);
                } else if (dest_size == 64) {
                    la_x86rotli_d(dest, shift);
                }
            }
            IR2_OPND tmp_dest = ra_alloc_itemp();

            if (dest_size == 8) {
                la_rotri_b(tmp_dest, dest, dest_size - shift);
            } else if (dest_size == 16) {
                la_rotri_h(tmp_dest, dest, dest_size - shift);
            } else if (dest_size == 32) {
                la_rotri_w(tmp_dest, dest, dest_size - shift);
            } else if (dest_size == 64) {
                if(ir1_opnd_is_gpr(opnd0)) {
                    la_rotri_d(dest, dest, dest_size - shift);
                    return true;
                } else {
                    la_rotri_d(tmp_dest, dest, dest_size - shift);
                }
            }

            store_ireg_to_ir1(tmp_dest, opnd0, false);
            ra_free_temp(tmp_dest);
        } else {
#ifdef TARGET_X86_64
            /* In x64 when opnd_size==32, dest is zero-extended even if rotate 0 bit */
            if (!GHBR_ON(pir1) && ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32
            && ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))) {
                IR2_OPND gpr_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
                la_mov32_zx(gpr_opnd, gpr_opnd);
            }
#endif
        }

        return true;
    }
    /* get real count */
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND original_count;
    if (ir1_get_opnd_num(pir1) == 1) {
        original_count = ra_alloc_itemp();
        li_wu(original_count, 1);
    } else {
        original_count =
            load_ireg_from_ir1(ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);
    }

    IR2_OPND count = ra_alloc_itemp();
#ifndef TARGET_X86_64
    la_andi(count, original_count, 0x1f);
    la_beq(count, zero_ir2_opnd, label_exit);
#else
    if (CODEIS64) {
        if (ir1_opnd_size(opnd0) == 64) {
            la_andi(count, original_count, 0x3f);
        } else {
            la_andi(count, original_count, 0x1f);
        }
        if (!GHBR_ON(pir1) && ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32
        && ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))) {
            /* clean the target*/
            IR2_OPND gpr_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
            la_mov32_zx(gpr_opnd, gpr_opnd);
            la_beq(count, zero_ir2_opnd, label_exit);
        } else {
            la_beq(count, zero_ir2_opnd, label_exit);
        }
    } else {
        la_andi(count, original_count, 0x1f);
        la_beq(count, zero_ir2_opnd, label_exit);
    }
#endif

    IR2_OPND dest = load_ireg_from_ir1(opnd0, ZERO_EXTENSION, false);
    IR2_OPND tmp_dest = ra_alloc_itemp();

    if (ir1_need_calculate_any_flag(pir1)) {
        if (ir1_opnd_size(opnd0) == 8) {
            la_x86rotl_b(dest, original_count);
        } else if (ir1_opnd_size(opnd0) == 16) {
            la_x86rotl_h(dest, original_count);
        } else if (ir1_opnd_size(opnd0) == 32) {
            la_x86rotl_w(dest, original_count);
        } else if (ir1_opnd_size(opnd0) == 64) {
            la_x86rotl_d(dest, original_count);
        }
    }

    if (dest_size != 32 && dest_size != 64) {
        la_andi(count, original_count,
                                dest_size - 1);
    }
    if (ir2_opnd_is_itemp(&original_count)) {
        ra_free_temp(original_count);
    }

    IR2_OPND tmp = ra_alloc_itemp();
    li_wu(tmp, dest_size);
    la_sub_d(tmp, tmp, count);
    la_rotr_d(tmp_dest, dest, tmp);
#ifndef TARGET_X86_64
    la_srli_d(tmp, tmp_dest, 64 - dest_size);
    la_or(tmp_dest, tmp_dest, tmp);
    la_bstrins_d(tmp_dest, zero_ir2_opnd, 63, 32);
#else
    if (CODEIS64) {
        if (dest_size != 64) {
            la_srli_d(tmp, tmp_dest, 64 - dest_size);
            la_or(tmp_dest, tmp_dest, tmp);
        }
    } else {
        la_srli_d(tmp, tmp_dest, 64 - dest_size);
        la_or(tmp_dest, tmp_dest, tmp);
        la_bstrins_d(tmp_dest, zero_ir2_opnd, 63, 32);
    }
#endif
    store_ireg_to_ir1(tmp_dest, ir1_get_opnd(pir1, 0), false);
    la_label(label_exit);
    ra_free_temp(tmp);
    ra_free_temp(tmp_dest);
    return true;
}

bool translate_ror(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int dest_size = ir1_opnd_size(opnd0);

    if (ir1_opnd_is_imm(opnd1)) {
        /* Shift is imm8/1 case */
        uint32 mask = (dest_size == 64) ? 63 : 31;
        uint8 shift = ir1_opnd_uimm(opnd1) & mask;

        if(shift) {
            IR2_OPND dest;
            if(ir1_opnd_is_gpr(opnd0) && !ir1_opnd_is_8h(opnd0)) {
                int gpr_num = ir1_opnd_base_reg_num(opnd0);
                dest =  ra_alloc_gpr(gpr_num);
            } else {
                dest = load_ireg_from_ir1(opnd0, ZERO_EXTENSION, false);
            }


            if (ir1_need_calculate_any_flag(pir1)) {
                if (dest_size == 8) {
                    la_x86rotri_b(dest, shift);
                } else if (dest_size == 16) {
                    la_x86rotri_h(dest, shift);
                } else if (dest_size == 32) {
                    la_x86rotri_w(dest, shift);
                } else if (dest_size == 64) {
                    la_x86rotri_d(dest, shift);
                }
            }
            IR2_OPND tmp_dest = ra_alloc_itemp();

            if (dest_size == 8) {
                la_rotri_b(tmp_dest, dest, shift);
            } else if (dest_size == 16) {
                la_rotri_h(tmp_dest, dest, shift);
            } else if (dest_size == 32) {
                la_rotri_w(tmp_dest, dest, shift);
            } else if (dest_size == 64) {
                if(ir1_opnd_is_gpr(opnd0)) {
                    la_rotri_d(dest, dest, shift);
                    return true;
                } else {
                    la_rotri_d(tmp_dest, dest, shift);
                }
            }

            store_ireg_to_ir1(tmp_dest, opnd0, false);
            ra_free_temp(tmp_dest);
        } else {
#ifdef TARGET_X86_64
            /* In x64 when opnd_size==32, dest is zero-extended even if rotate 0 bit */
            if (!GHBR_ON(pir1) && ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32
            && ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))) {
                IR2_OPND gpr_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
                la_mov32_zx(gpr_opnd, gpr_opnd);
           }
#endif
        }
        return true;
    }

    /* get real count */
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND original_count;
    if (ir1_get_opnd_num(pir1) == 1) {
        original_count = ra_alloc_itemp();
        li_wu(original_count, 1);
    } else {
        original_count =
            load_ireg_from_ir1(ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);
    }

    IR2_OPND count = ra_alloc_itemp();
#ifndef TARGET_X86_64
    la_andi(count, original_count, 0x1f);
    la_beq(count, zero_ir2_opnd, label_exit);
#else
    if (CODEIS64) {
        if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 64) {
            la_andi(count, original_count, 0x3f);
        } else {
            la_andi(count, original_count, 0x1f);
        }
        if (!GHBR_ON(pir1) && ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32
        && ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))) {
            /* clean the target*/
            IR2_OPND gpr_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
            la_mov32_zx(gpr_opnd, gpr_opnd);
            la_beq(count, zero_ir2_opnd, label_exit);
        } else {
            la_beq(count, zero_ir2_opnd, label_exit);
        }
    } else {
        la_andi(count, original_count, 0x1f);
        la_beq(count, zero_ir2_opnd, label_exit);
    }
#endif
    IR2_OPND dest = load_ireg_from_ir1(opnd0, ZERO_EXTENSION, false);
    IR2_OPND tmp_dest = ra_alloc_itemp();
    IR2_OPND tmp = ra_alloc_itemp();

    if (ir1_need_calculate_any_flag(pir1)) {
        if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 8) {
            la_x86rotr_b(dest, original_count);
        } else if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 16) {
            la_x86rotr_h(dest, original_count);
        } else if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32) {
            la_x86rotr_w(dest, original_count);
        } else if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 64) {
            la_x86rotr_d(dest, original_count);
        }
    }

    if (dest_size != 32 && dest_size != 64) {
        la_andi(count, original_count, dest_size - 1);
    }
    if (ir2_opnd_is_itemp(&original_count)) {
        ra_free_temp(original_count);
    }

    la_rotr_d(tmp_dest, dest, count);
#ifndef TARGET_X86_64
    la_srli_d(tmp, tmp_dest, 64 - dest_size);
    la_or(tmp_dest, tmp_dest, tmp);
    la_bstrins_d(tmp_dest, zero_ir2_opnd, 63, 32);
#else
    if(CODEIS64) {
        if (dest_size != 64) {
            la_srli_d(tmp, tmp_dest, 64 - dest_size);
            la_or(tmp_dest, tmp_dest, tmp);
        }
    } else {
        la_srli_d(tmp, tmp_dest, 64 - dest_size);
        la_or(tmp_dest, tmp_dest, tmp);
        la_bstrins_d(tmp_dest, zero_ir2_opnd, 63, 32);
    }
#endif
    store_ireg_to_ir1(tmp_dest, ir1_get_opnd(pir1, 0), false);
    la_label(label_exit);
    ra_free_temp(tmp);
    ra_free_temp(tmp_dest);
    return true;
}

bool translate_rorx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND dest = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND src = load_ireg_from_ir1(ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND imm = load_ireg_from_ir1(ir1_get_opnd(pir1, 2), ZERO_EXTENSION, false);
    int dest_size = ir1_opnd_size(opnd0);
    IR2_OPND count = ra_alloc_itemp();
    IR2_OPND tmp = ra_alloc_itemp();
    if (dest_size == 32) {
        la_andi(count, imm, 0x1f);
        la_rotr_w(tmp, src, count);
        la_bstrpick_d(dest, tmp, 31, 0);
    } else if (dest_size == 64) {
        la_andi(count, imm, 0x3f);
        la_rotr_d(dest, src, count);
    } else {
        lsassert(0);
    }
    return true;
}

bool translate_rcl(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    int dest_size = ir1_opnd_size(opnd0);

    /* get real count */
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND original_count;
    if (ir1_get_opnd_num(pir1) == 1) {
        original_count = ra_alloc_itemp();
        li_wu(original_count, 1);
    } else {
        original_count =
            load_ireg_from_ir1(ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);
    }

    IR2_OPND count = ra_alloc_itemp();
#ifndef TARGET_X86_64
    la_andi(count, original_count, 0x1f);
    la_beq(count, zero_ir2_opnd, label_exit);
#else
    if (CODEIS64) {
        int32 mask = (dest_size == 64) ? 63 : 31;
        la_andi(count, original_count, mask);
        if (!GHBR_ON(pir1) && dest_size == 32 && ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))) {
            /* clean the target*/
            IR2_OPND gpr_opnd =
                ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
            la_mov32_zx(gpr_opnd, gpr_opnd);
            la_beq(count, zero_ir2_opnd, label_exit);
        } else {
            la_beq(count, zero_ir2_opnd, label_exit);
        }
    } else {
        la_andi(count, original_count, 0x1f);
        la_beq(count, zero_ir2_opnd, label_exit);
    }
#endif
    if (ir2_opnd_is_itemp(&original_count)) {
        ra_free_temp(original_count);
    }

    if (dest_size < 32) {
        IR2_OPND tmp_imm = ra_alloc_itemp();
        la_addi_d(tmp_imm, zero_ir2_opnd,
                          dest_size + 1);
        la_mod_du(count, count, tmp_imm);
        la_beq(count, zero_ir2_opnd, label_exit);
        ra_free_temp(tmp_imm);
    }

    IR2_OPND dest = load_ireg_from_ir1(opnd0, ZERO_EXTENSION, false);
#ifndef TARGET_X86_64
    IR2_OPND tmp_dest = ra_alloc_itemp();

    get_eflag_condition(&tmp_dest, pir1);
    la_slli_d(tmp_dest, tmp_dest, dest_size);
    la_bstrins_d(tmp_dest, dest, dest_size - 1, 0);

    IR2_OPND tmp = ra_alloc_itemp();
    li_wu(tmp, dest_size + 1);
    la_sub_d(tmp, tmp, count);
    la_rotr_d(tmp, tmp_dest, tmp);
    la_srli_d(tmp_dest, tmp, 63 - dest_size);
    la_or(tmp_dest, tmp_dest, tmp);
    la_bstrins_d(tmp_dest, zero_ir2_opnd, 63, 32);
    generate_eflag_calculation(tmp_dest, dest, count, pir1, true);
    store_ireg_to_ir1(tmp_dest, ir1_get_opnd(pir1, 0), false);
    ra_free_temp(tmp);
    ra_free_temp(tmp_dest);
#else
    if (CODEIS64) {
        IR2_OPND cf = ra_alloc_itemp();
        get_eflag_condition(&cf, pir1);
        if (dest_size <= 32) {
            la_slli_d(cf, cf, dest_size);
        } else {
            IR2_OPND cf_move = ra_alloc_itemp();
            /* 64 bits, cf will put the right bit now */
            /* cf_move <- count - 1 */
            la_addi_w(cf_move, count, -1);
            /* cf <- cf << cf_move */
            la_sll_d(cf, cf, cf_move);
            ra_free_temp(cf_move);
        }

        IR2_OPND tmp_dest;
        IR2_OPND high_dest = ra_alloc_itemp();
        IR2_OPND label_finish = ra_alloc_label();
        /* but if size = 64, no more bits for the cf */
        if (dest_size != 64) {
            tmp_dest = ra_alloc_itemp();
            /* for 32bits etc. we can put the cf at the upper bit */
            la_or(tmp_dest, dest, cf);
            la_sll_d(high_dest, tmp_dest, count);
        } else {
            la_sll_d(high_dest, dest, count);
            /* attach the cf at the high_dest */
            la_or(high_dest, high_dest, cf);
            la_addi_w(cf, count, -1);
            la_beq(cf, zero_ir2_opnd, label_finish);
            tmp_dest = dest;
        }
        ra_free_temp(cf);

        la_addi_d(count, count, -1 - dest_size);
        la_sub_d(count, zero_ir2_opnd, count);

        IR2_OPND low_dest = ra_alloc_itemp();
        la_srl_d(low_dest, tmp_dest, count);
        la_sub_d(count, zero_ir2_opnd, count);
        la_addi_d(count, count, 1 + dest_size);

        if (dest_size != 64) {
            ra_free_temp(tmp_dest);
        }

        la_or(high_dest, high_dest, low_dest);
    /* label_finish: */
        la_label(label_finish);
        generate_eflag_calculation(high_dest, dest, count, pir1, true);
        /* we can't move the dest directly */
        /* because eflag_calculation need the resource data */
        store_ireg_to_ir1(high_dest, ir1_get_opnd(pir1, 0), false);
        ra_free_temp(high_dest);
        ra_free_temp(low_dest);
        ra_free_temp(count);

    } else {
        IR2_OPND tmp_dest = ra_alloc_itemp();

        get_eflag_condition(&tmp_dest, pir1);
        la_slli_d(tmp_dest, tmp_dest, dest_size);
        la_bstrins_d(tmp_dest, dest, dest_size - 1, 0);

        IR2_OPND tmp = ra_alloc_itemp();
        li_wu(tmp, dest_size + 1);
        la_sub_d(tmp, tmp, count);
        la_rotr_d(tmp, tmp_dest, tmp);
        la_srli_d(tmp_dest, tmp, 63 - dest_size);
        la_or(tmp_dest, tmp_dest, tmp);
        la_bstrins_d(tmp_dest, zero_ir2_opnd, 63, 32);
        generate_eflag_calculation(tmp_dest, dest, count, pir1, true);
        store_ireg_to_ir1(tmp_dest, ir1_get_opnd(pir1, 0), false);
        ra_free_temp(tmp);
        ra_free_temp(tmp_dest);
    }
#endif
/* label_exit: */
    la_label(label_exit);
    return true;
}

bool translate_rcr(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    int dest_size = ir1_opnd_size(opnd0);

    /* get real count */
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND original_count;
    if (ir1_get_opnd_num(pir1) == 1) {
        original_count = ra_alloc_itemp();
        li_wu(original_count, 1);
    } else {
        original_count =
            load_ireg_from_ir1(ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);
    }

    IR2_OPND count = ra_alloc_itemp();
#ifndef TARGET_X86_64
    la_andi(count, original_count, 0x1f);
    la_beq(count, zero_ir2_opnd, label_exit);
#else
    if (CODEIS64) {
        int32 mask = (dest_size == 64) ? 63 : 31;
        la_andi(count, original_count, mask);
        if (!GHBR_ON(pir1) && dest_size == 32 && ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))) {
            /* clean the target*/
            IR2_OPND gpr_opnd =
                ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
            la_mov32_zx(gpr_opnd, gpr_opnd);
            la_beq(count, zero_ir2_opnd, label_exit);
        } else {
            la_beq(count, zero_ir2_opnd, label_exit);
        }
    } else {
        la_andi(count, original_count, 0x1f);
        la_beq(count, zero_ir2_opnd, label_exit);
    }
#endif

    if (ir1_opnd_size(opnd0) < 32) {
        IR2_OPND tmp_imm = ra_alloc_itemp();
        la_addi_d(tmp_imm, zero_ir2_opnd,
                          dest_size + 1);
        la_mod_du(count, count, tmp_imm);
        la_beq(count, zero_ir2_opnd, label_exit);
        ra_free_temp(tmp_imm);
    }

    IR2_OPND dest = load_ireg_from_ir1(opnd0, ZERO_EXTENSION, false);
#ifndef TARGET_X86_64
    IR2_OPND tmp_dest = ra_alloc_itemp();

    get_eflag_condition(&tmp_dest, pir1);
    la_bstrins_d(tmp_dest, dest, dest_size, 1);

    IR2_OPND tmp = ra_alloc_itemp();
    la_rotr_d(tmp, tmp_dest, count);
    la_srli_d(tmp_dest, tmp, 63 - dest_size);
    la_or(tmp_dest, tmp_dest, tmp);

    la_bstrpick_d(tmp_dest, tmp_dest, dest_size, 1);
    generate_eflag_calculation(tmp_dest, dest, count, pir1, true);
    store_ireg_to_ir1(tmp_dest, ir1_get_opnd(pir1, 0), false);
    ra_free_temp(tmp);
    ra_free_temp(tmp_dest);
#else
    if (CODEIS64) {
        IR2_OPND cf = ra_alloc_itemp();
        get_eflag_condition(&cf, pir1);
        if (dest_size <= 32) {
            la_slli_d(cf, cf, dest_size);
        } else {
            IR2_OPND cf_move = ra_alloc_itemp();
            /* 64 bits, cf will put the right bit now */
            /* cf_move <- size - count */
            la_addi_d(cf_move, zero_ir2_opnd, 64);
            la_sub_w(cf_move, cf_move, count);
            /* cf <- cf << cf_move */
            la_sll_d(cf, cf, cf_move);
            ra_free_temp(cf_move);
        }

        IR2_OPND tmp_dest;
        IR2_OPND low_dest = ra_alloc_itemp();
        IR2_OPND label_finish = ra_alloc_label();

        /* but if size = 64, no more bits for the cf */
        if (dest_size != 64) {
            tmp_dest = ra_alloc_itemp();
            /* for 32bits etc. we can put the cf at the upper bit */
            la_or(tmp_dest, dest, cf);
            la_srl_d(low_dest, tmp_dest, count);
        } else {
            la_srl_d(low_dest, dest, count);
            /* if count = 1 */
            /* attach the cf at the low_dest */
            la_or(low_dest, low_dest, cf);
            la_addi_w(cf, count, -1);
            la_beq(cf, zero_ir2_opnd, label_finish);
            tmp_dest = dest;
        }
        ra_free_temp(cf);

        /* attention: dsllv $reg, $reg(=64) is not work */
        la_addi_d(count, count, -1 - dest_size);
        la_sub_d(count, zero_ir2_opnd, count);
    
        IR2_OPND high_dest = ra_alloc_itemp();
        la_sll_d(high_dest, tmp_dest, count);
        la_sub_d(count, zero_ir2_opnd, count);
        la_addi_d(count, count, 1 + dest_size);
    
        if (dest_size != 64) {
            ra_free_temp(tmp_dest);
        }

        la_or(low_dest, high_dest, low_dest);
        ra_free_temp(high_dest);
    /* label_finish: */
        la_label(label_finish);
        generate_eflag_calculation(low_dest, dest, count, pir1, true);
        ra_free_temp(count);
        store_ireg_to_ir1(low_dest, ir1_get_opnd(pir1, 0), false);
        ra_free_temp(low_dest);
    } else {
        IR2_OPND tmp_dest = ra_alloc_itemp();

        get_eflag_condition(&tmp_dest, pir1);
        la_bstrins_d(tmp_dest, dest, dest_size, 1);

        IR2_OPND tmp = ra_alloc_itemp();
        la_rotr_d(tmp, tmp_dest, count);
        la_srli_d(tmp_dest, tmp, 63 - dest_size);
        la_or(tmp_dest, tmp_dest, tmp);

        la_bstrpick_d(tmp_dest, tmp_dest, dest_size, 1);
        generate_eflag_calculation(tmp_dest, dest, count, pir1, true);
        store_ireg_to_ir1(tmp_dest, ir1_get_opnd(pir1, 0), false);
        ra_free_temp(tmp);
        ra_free_temp(tmp_dest);
    }
#endif
/* label_exit: */
    la_label(label_exit);
    return true;
}

static bool translate_shrd_cl(IR1_INST *pir1)
{
    IR2_OPND count_opnd =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 2, ZERO_EXTENSION, false);

    IR2_OPND count = ra_alloc_itemp();
    int opnd0_size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    int mask = (opnd0_size == 64) ? 0x3f : 0x1f;
    la_andi(count, count_opnd, mask);
    if (ir2_opnd_is_itemp(&count_opnd)) {
        ra_free_temp(count_opnd);
    }
    IR2_OPND label_exit = ra_alloc_label();
    /* if reg size is 32 and dest is not 32 zero extend */
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0)) &&
        opnd0_size == 32) {
        /* clean the target */
        IR2_OPND dest_opnd =
                ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
        la_mov32_zx(dest_opnd, dest_opnd);
    }
#endif
    la_beq(count, zero_ir2_opnd, label_exit);

    IR2_OPND left_count = ra_alloc_itemp();
    li_w(left_count, mask + 1);
    la_sub_w(left_count, left_count, count);

    IR2_OPND dest_opnd = ra_alloc_itemp();
    IR2_OPND src_opnd = ra_alloc_itemp();
    /* use temp register to avoid clobber if src == dest */
    load_ireg_from_ir1_2(dest_opnd, ir1_get_opnd(pir1, 0), ZERO_EXTENSION, false);
    load_ireg_from_ir1_2(src_opnd, ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);
    /* TODO: 32 bit case in X86_64 */
    if (opnd0_size == 16) {
        la_bstrins_d(dest_opnd, src_opnd, 31, 16);
        la_bstrins_d(src_opnd, dest_opnd, 15, 0);
    }
    IR2_OPND low_dest = ra_alloc_itemp();
    la_srl_d(low_dest, dest_opnd, count);
    la_sll_d(src_opnd, src_opnd, left_count);
    ra_free_temp(left_count);

    IR2_OPND final_dest = ra_alloc_itemp();
    la_or(final_dest, src_opnd, low_dest);
    ra_free_temp(low_dest);
    ra_free_temp(src_opnd);

    generate_eflag_calculation(final_dest, dest_opnd, count, pir1, true);
    ra_free_temp(dest_opnd);
    ra_free_temp(count);

    store_ireg_to_ir1(final_dest, ir1_get_opnd(pir1, 0), false);

    ra_free_temp(final_dest);
    la_label(label_exit);

    return true;
}

static bool translate_shrd_imm(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_imm(ir1_get_opnd(pir1, 0) + 2));
    int opnd0_size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    int32 mask = (opnd0_size == 64) ? 0x3f : 0x1f;
    int count = ir1_opnd_simm(ir1_get_opnd(pir1, 2)) & mask;

    if (count == 0) {
#ifdef TARGET_X86_64
        if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))
            && opnd0_size == 32) {
            /* clean high 32 bits */
            IR2_OPND dest_opnd =
                ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
            la_mov32_zx(dest_opnd, dest_opnd);
        }
#endif
        return true;
    }
    int left_count = mask + 1 - count;

    IR2_OPND src_opnd = ra_alloc_itemp();
    IR2_OPND dest_opnd = ra_alloc_itemp();
    /* use temp register to avoid clobber if src == dest */
    load_ireg_from_ir1_2(dest_opnd, ir1_get_opnd(pir1, 0), ZERO_EXTENSION, false);
    load_ireg_from_ir1_2(src_opnd, ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);
    /* TODO: 32 bit case in dt_X86_64 */
    if (opnd0_size == 16) {
        la_bstrins_d(dest_opnd, src_opnd, 31, 16);
        la_bstrins_d(src_opnd, dest_opnd, 15, 0);
    }
    /* shift right firest */
    IR2_OPND low_dest = ra_alloc_itemp();
    IR2_OPND high_dest = ra_alloc_itemp();
    la_srli_d(low_dest, dest_opnd, count);
    la_slli_d(high_dest, src_opnd, left_count);
    ra_free_temp(src_opnd);

    IR2_OPND final_dest = ra_alloc_itemp();
    la_or(final_dest, high_dest, low_dest);
    ra_free_temp(low_dest);
    ra_free_temp(high_dest);

    IR2_OPND count_opnd = ir2_opnd_new(IR2_OPND_IMM, (int16)count);
    generate_eflag_calculation(final_dest, dest_opnd, count_opnd, pir1, true);

    store_ireg_to_ir1(final_dest, ir1_get_opnd(pir1, 0), false);
    ra_free_temp(final_dest);

    return true;
}

bool translate_shrd(IR1_INST *pir1)
{
    if (ir1_opnd_is_imm(ir1_get_opnd(pir1, 0) + 2))
        return translate_shrd_imm(pir1);
    else
        return translate_shrd_cl(pir1);

    return true;
}

static bool translate_shld_cl(IR1_INST *pir1)
{
    IR2_OPND count_opnd =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 2, ZERO_EXTENSION, false);

    IR2_OPND count = ra_alloc_itemp();
    int opnd0_size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    int mask = (opnd0_size == 64) ? 63 : 31;
    la_andi(count, count_opnd, mask);
    if (ir2_opnd_is_itemp(&count_opnd)) {
        ra_free_temp(count_opnd);
    }
    IR2_OPND label_exit = ra_alloc_label();
    /* if reg size is 32 and dest is not 32 zero extend */
#ifdef TARGET_X86_64
    if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0)) &&
        opnd0_size == 32) {
        /* clean the target */
        IR2_OPND dest_opnd =
                ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
        la_mov32_zx(dest_opnd, dest_opnd);
    }
#endif
    la_beq(count, zero_ir2_opnd, label_exit);

    IR2_OPND left_count = ra_alloc_itemp();
    li_w(left_count, mask + 1);
    la_sub_w(left_count, left_count, count);

    IR2_OPND src_opnd = ra_alloc_itemp();
    IR2_OPND dest_opnd = ra_alloc_itemp();
    /* use temp register to avoid clobber if src == dest */
    load_ireg_from_ir1_2(dest_opnd, ir1_get_opnd(pir1, 0), ZERO_EXTENSION, false);
    load_ireg_from_ir1_2(src_opnd, ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);

    /* TODO: 32 bit case in dt_X86_64 */
    if (opnd0_size == 16) {
        la_bstrins_d(src_opnd, src_opnd, 31, 16);
        la_bstrins_d(src_opnd, dest_opnd, 15, 0);
    }

    IR2_OPND high_dest = ra_alloc_itemp();
    la_sll_d(high_dest, dest_opnd, count);

    IR2_OPND low_dest = ra_alloc_itemp();
    la_srl_d(low_dest, src_opnd, left_count);
    ra_free_temp(left_count);

    IR2_OPND final_dest = ra_alloc_itemp();
    la_or(final_dest, high_dest, low_dest);
    ra_free_temp(low_dest);
    ra_free_temp(high_dest);
    if (opnd0_size == 16) {
        la_bstrins_d(dest_opnd, dest_opnd, 31, 16);
        la_bstrpick_d(src_opnd, src_opnd, 31, 16);
        la_bstrins_d(dest_opnd, src_opnd, 15, 0);
    }
    ra_free_temp(src_opnd);

    generate_eflag_calculation(final_dest, dest_opnd, count, pir1, true);
    ra_free_temp(count);

    store_ireg_to_ir1(final_dest, ir1_get_opnd(pir1, 0), false);

    ra_free_temp(final_dest);
    la_label(label_exit);

    return true;
}

static bool translate_shld_imm(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_imm(ir1_get_opnd(pir1, 0) + 2));
    int opnd0_size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    int32 mask = (opnd0_size == 64) ? 0x3f : 0x1f;
    int count = ir1_opnd_simm(ir1_get_opnd(pir1, 2)) & mask;
    if (count == 0) {
#ifdef TARGET_X86_64
        if (!GHBR_ON(pir1) && CODEIS64 && ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))
            && opnd0_size == 32) {
            /* clean high 32 bits */
            IR2_OPND dest_opnd =
                ra_alloc_gpr(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0)));
            la_mov32_zx(dest_opnd, dest_opnd);
        }
#endif
        return true;
    }
    int left_count = mask + 1 - count;

    IR2_OPND src_opnd = ra_alloc_itemp();
    IR2_OPND dest_opnd = ra_alloc_itemp();

    /* use temp register to avoid clobber if src == dest */
    load_ireg_from_ir1_2(dest_opnd, ir1_get_opnd(pir1, 0), ZERO_EXTENSION, false);
    load_ireg_from_ir1_2(src_opnd, ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);

    /* TODO: 32 bit case in dt_X86_64 */
    if (opnd0_size == 16) {
        la_bstrins_d(src_opnd, src_opnd, 31, 16);
        la_bstrins_d(src_opnd, dest_opnd, 15, 0);
    }

    IR2_OPND high_dest = ra_alloc_itemp();
    IR2_OPND low_dest = ra_alloc_itemp();
    la_slli_d(high_dest, dest_opnd, count);
    la_srli_d(low_dest, src_opnd, left_count);

    IR2_OPND final_dest = ra_alloc_itemp();
    la_or(final_dest, high_dest, low_dest);
    ra_free_temp(low_dest);
    ra_free_temp(high_dest);

    if (opnd0_size == 16) {
        la_bstrins_d(dest_opnd, dest_opnd, 31, 16);
        la_bstrpick_d(src_opnd, src_opnd, 31, 16);
        la_bstrins_d(dest_opnd, src_opnd, 15, 0);
    }
    ra_free_temp(src_opnd);

    IR2_OPND count_opnd = ir2_opnd_new(IR2_OPND_IMM, (int16)count);
    generate_eflag_calculation(final_dest, dest_opnd, count_opnd, pir1, true);

    store_ireg_to_ir1(final_dest, ir1_get_opnd(pir1, 0), false);
    ra_free_temp(final_dest);
    ra_free_temp(dest_opnd);

    return true;
}

bool translate_shld(IR1_INST *pir1)
{
    if (ir1_opnd_is_imm(ir1_get_opnd(pir1, 0) + 2))
        return translate_shld_imm(pir1);
    else
        return translate_shld_cl(pir1);

    return true;
}

bool translate_bswap(IR1_INST *pir1)
{
    IR2_OPND bswap_opnd;
    IR1_OPND *ir1_opnd = ir1_get_opnd(pir1, 0);
    int opnd_size = ir1_opnd_size(ir1_opnd);

    if(opnd_size == 16){
        ir1_opnd->size = (32 >> 3);
        switch (ir1_opnd->reg) {
        case dt_X86_REG_AX:
            ir1_opnd->reg = dt_X86_REG_EAX;
            break;
        case dt_X86_REG_BX:
            ir1_opnd->reg = dt_X86_REG_EBX;
            break;
        case dt_X86_REG_CX:
            ir1_opnd->reg = dt_X86_REG_ECX;
            break;
        case dt_X86_REG_DX:
            ir1_opnd->reg = dt_X86_REG_EDX;
            break;
        case dt_X86_REG_SP:
            ir1_opnd->reg = dt_X86_REG_ESP;
            break;
        case dt_X86_REG_BP:
            ir1_opnd->reg = dt_X86_REG_EBP;
            break;
        case dt_X86_REG_SI:
            ir1_opnd->reg = dt_X86_REG_ESI;
            break;
        case dt_X86_REG_DI:
            ir1_opnd->reg = dt_X86_REG_EDI;
            break;
        case dt_X86_REG_R8W:
            ir1_opnd->reg = dt_X86_REG_R8D;
            break;
        case dt_X86_REG_R9W:
            ir1_opnd->reg = dt_X86_REG_R9D;
            break;
        case dt_X86_REG_R10W:
            ir1_opnd->reg = dt_X86_REG_R10D;
            break;
        case dt_X86_REG_R11W:
            ir1_opnd->reg = dt_X86_REG_R11D;
            break;
        case dt_X86_REG_R12W:
            ir1_opnd->reg = dt_X86_REG_R12D;
            break;
        case dt_X86_REG_R13W:
            ir1_opnd->reg = dt_X86_REG_R13D;
            break;
        case dt_X86_REG_R14W:
            ir1_opnd->reg = dt_X86_REG_R14D;
            break;
        case dt_X86_REG_R15W:
            ir1_opnd->reg = dt_X86_REG_R15D;
            break;
        default:
            lsassert(0);
            break;
        }
        bswap_opnd =
            load_ireg_from_ir1(ir1_opnd, UNKNOWN_EXTENSION, false);
        la_bstrins_w(bswap_opnd, zero_ir2_opnd, 15, 0);
    } else if (opnd_size == 32) {
        bswap_opnd = load_ireg_from_ir1(ir1_opnd, UNKNOWN_EXTENSION, false);

        la_revb_2w(bswap_opnd, bswap_opnd);
    } else {
        lsassert(opnd_size == 64);
        bswap_opnd = load_ireg_from_ir1(ir1_opnd, UNKNOWN_EXTENSION, false);

        la_revb_d(bswap_opnd, bswap_opnd);
    }
   /*
    * FIXME: high 32bit sign extension may corrupt, add.w 0 to resolve
    */
    //ir2_opnd_set_em(&bswap_opnd, UNKNOWN_EXTENSION, 32);

    store_ireg_to_ir1(bswap_opnd, ir1_get_opnd(pir1, 0), false);

    return true;
}

/**
 * @brief translate inst shrx
 * SHR inst has some patterns
 * - shrx r32a, r/m32, r32b
 * - shrx r64a, r/m64, r64b
 *
 * @param pir1
 * @return true
 * @return false
 */
bool translate_shrx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);
    IR2_OPND src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    IR2_OPND src2 = load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);

    int opnd_size = ir1_opnd_size(opnd1);
    IR2_INST *(*shift_inst)(IR2_OPND, IR2_OPND, IR2_OPND);


    shift_inst = (opnd_size == 64) ? la_srl_d : la_srl_w;

    shift_inst(dest, src1, src2);
    if (opnd_size == 32) {
        la_bstrins_d(dest, zero_ir2_opnd, 63, 32);
    }

    return true;
}

/**
 * @brief translate inst shlx
 * SHR inst has some patterns
 * - shlx r32a, r/m32, r32b
 * - shlx r64a, r/m64, r64b
 *
 * @param pir1
 * @return true
 * @return false
 */
bool translate_shlx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);
    IR2_OPND src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    IR2_OPND src2 = load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);

    int opnd_size = ir1_opnd_size(opnd1);
    IR2_INST *(*shift_inst)(IR2_OPND, IR2_OPND, IR2_OPND);

    shift_inst = (opnd_size == 64) ? la_sll_d : la_sll_w;

    shift_inst(dest, src1, src2);
    if (opnd_size == 32) {
        la_bstrins_d(dest, zero_ir2_opnd, 63, 32);
    }

    return true;
}

/**
 * @brief translate inst sarx
 * SHR inst has some patterns
 * - sarx r32a, r/m32, r32b
 * - sarx r64a, r/m64, r64b
 *
 * @param pir1
 * @return true
 * @return false
 */
bool translate_sarx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    IR2_OPND dest = load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);
    IR2_OPND src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    IR2_OPND src2 = load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);

    int opnd_size = ir1_opnd_size(opnd1);
    IR2_INST *(*shift_inst)(IR2_OPND, IR2_OPND, IR2_OPND);

    shift_inst = (opnd_size == 64) ? la_sra_d : la_sra_w;

    shift_inst(dest, src1, src2);
    if (opnd_size == 32) {
        la_bstrins_d(dest, zero_ir2_opnd, 63, 32);
    }

    return true;
}
