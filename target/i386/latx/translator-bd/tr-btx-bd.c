#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "flag-lbt-bd.h"
#include "translate-bd.h"

static void get_mem_bitbase(IR2_OPND mem_op, int *mem_off,
                            IR1_OPND_BD *opnd0, IR1_OPND_BD *opnd1,
                            bool is_llsc)
{
    IR2_OPND tmp_mem_op;

    lsassertm(ir2_opnd_is_itemp(&mem_op), "%s:%d\n", __func__, __LINE__);
    if (is_llsc) {
        tmp_mem_op = convert_mem_to_itemp_bd(opnd0);
        *mem_off = 0;
    } else {
        tmp_mem_op = convert_mem_bd(opnd0, mem_off);
    }
    la_or(mem_op, tmp_mem_op, zero_ir2_opnd);
#ifdef CONFIG_LATX_IMM_REG
    imm_cache_free_temp_helper(tmp_mem_op);
#else
    ra_free_temp_auto(tmp_mem_op);
#endif 

    /*
     * If BitBase is a memory address, the BitOffset can range has different
     * ranges depending on the operand size.(see Intel SDM Vol.2A Table 3-2).
     *     | Operand Size | Immediate BitOffset | Register BitOffset |
     *     | ------------ | ------------------- | ------------------ |
     *     |      16      |      0 to 15        | − 2^15 to 2^15 − 1 |
     *     |      32      |      0 to 31        | − 2^31 to 2^31 − 1 |
     *     |      64      |      0 to 63        | − 2^63 to 2^63 − 1 |
     *
     * If BitOffset is a register, we recalculate BitBase and BitOffset.
     */
    if (ir1_opnd_is_gpr_bd(opnd1)) {
        IR2_OPND tmp = ra_alloc_itemp();
        IR2_OPND src1 = load_ireg_from_ir1_bd(opnd1, SIGN_EXTENSION, false);
        int opnd_size = ir1_opnd_size_bd(opnd0);
        int ctz_opnd_size = __builtin_ctz(opnd_size);
        int ctz_align_size = __builtin_ctz(opnd_size / 8);
        lsassertm((opnd_size == 16) || (opnd_size == 32) || (opnd_size == 64),
                  "%s opnd_size error!", __func__);
        la_srai_d(tmp, src1, ctz_opnd_size);
        la_alsl_d(mem_op, tmp, mem_op, ctz_align_size - 1);
        ra_free_temp(tmp);
    }
}

static void get_bitoffset(IR2_OPND bit_offset, IR1_OPND_BD *opnd0, IR1_OPND_BD *opnd1)
{
    int opnd_size, ctz_opnd_size;

    lsassertm(ir2_opnd_is_itemp(&bit_offset), "%s:%d\n", __func__, __LINE__);
    IR2_OPND src1 = load_ireg_from_ir1_bd(opnd1, SIGN_EXTENSION, false);
    opnd_size = ir1_opnd_size_bd(opnd0);
    ctz_opnd_size = __builtin_ctz(opnd_size);
    la_bstrpick_d(bit_offset, src1, ctz_opnd_size - 1, 0);
    if (ir2_opnd_is_itemp(&src1)) {
        ra_free_temp(src1);
    }
}

static bool calculate_btx(IR2_OPND src0, IR2_OPND bit_offset, IR1_OPCODE_BD opcode)
{
    IR2_OPND bit_opnd = ra_alloc_itemp();

    /* set bit_opnd */
    la_ori(bit_opnd, zero_ir2_opnd, 1);
    la_sll_d(bit_opnd, bit_opnd, bit_offset);

    switch (opcode) {
    case ND_INS_BT:
        ra_free_temp(bit_opnd);
        return false;
    case ND_INS_BTS:
        la_or(src0, src0, bit_opnd);
        break;
    case ND_INS_BTR:
        la_nor(bit_opnd, bit_opnd, zero_ir2_opnd);
        la_and(src0, src0, bit_opnd);
        break;
    case ND_INS_BTC:
        la_xor(src0, src0, bit_opnd);
        break;
    default:
        lsassertm(0, "Invalid opcode in translate_btx\n");
        break;
    }

    ra_free_temp(bit_opnd);
    return true;
}

bool translate_btx_llsc_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);

    IR2_OPND src0, sc_opnd, bit_offset, mem_opnd;
    IR2_OPND label_ll;
    int imm;
    bool write_back;

    label_ll = ra_alloc_label();

    /* alloc itemp */
    bit_offset = ra_alloc_itemp();
    mem_opnd = ra_alloc_itemp();
    src0 = ra_alloc_itemp();

    /* get bitoffset */
    get_bitoffset(bit_offset, opnd0, opnd1);

    /* get mem_opnd and recalculate BitBase and BitOffset to avoid sigbus
     * if the mem_opnd is not aligned:
     *     temp BitOffset = BitBase % 8 * 8 + BitOffset
     *     new BitOffset = temp BitOffset - 64
     *     temp BitBase = BitBase / 8
     *     new BitBase = temp BitOffset >= 64 ? temp BitBase + 8 : temp BitBase
     */
    IR2_OPND tmp = ra_alloc_itemp();
    get_mem_bitbase(mem_opnd, &imm, opnd0, opnd1, true);
    la_bstrpick_d(tmp, mem_opnd, 2, 0);
    la_beq(tmp, zero_ir2_opnd, label_ll);
    la_bstrins_d(mem_opnd, zero_ir2_opnd, 2, 0);
    la_slli_d(tmp, tmp, 3);
    la_add_w(bit_offset, tmp, bit_offset);
    la_srli_d(tmp, bit_offset, 6);
    la_beq(tmp, zero_ir2_opnd, label_ll);
    la_bstrpick_w(bit_offset, bit_offset, 5, 0);
    la_addi_d(mem_opnd, mem_opnd, 8);
    ra_free_temp(tmp);

    /* read src0 */
    la_label(label_ll);
    la_ll_d(src0, mem_opnd, imm);

    /* set eflag */
    generate_eflag_calculation_bd(src0, src0, bit_offset, pir1, true);

    /* calculate */
    IR1_OPCODE_BD opcode = ir1_opcode_bd(pir1);
    write_back = calculate_btx(src0, bit_offset, opcode);

    /* Write back */
    if (write_back) {
        sc_opnd = ra_alloc_itemp();
        la_or(sc_opnd, zero_ir2_opnd, src0);
        la_sc_d(sc_opnd, mem_opnd, imm);
        la_beq(sc_opnd, zero_ir2_opnd, label_ll);
        ra_free_temp(sc_opnd);
    }

    ra_free_temp(src0);
    ra_free_temp(bit_offset);
    ra_free_temp(mem_opnd);
    return true;
}

bool translate_btx_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock_bd(pir1);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

    /*
     * lock btx -> translate_lock_btx()
     */
#ifdef CONFIG_LATX_LLSC
    if (is_lock && ir1_opnd_is_mem_bd(opnd0)) {
        return translate_btx_llsc_bd(pir1);
    }
#endif
    IR2_OPND lat_lock_addr;

    /*
     * btx
     */
    IR2_OPND src0, bit_offset, mem_opnd;
    int imm, opnd0_size;
    bool write_back;

    opnd0_size = ir1_opnd_size_bd(opnd0);

    /* alloc itemp */
    bit_offset = ra_alloc_itemp();
    mem_opnd = ra_alloc_itemp();

    /* get bitoffset */
    get_bitoffset(bit_offset, opnd0, opnd1);

    /* read src0 */
    if (ir1_opnd_is_gpr_bd(opnd0)) {
        /* r16/r32/r64 */
        src0 = convert_gpr_opnd_bd(opnd0, UNKNOWN_EXTENSION);
    } else {
        src0 = ra_alloc_itemp();
        get_mem_bitbase(mem_opnd, &imm, opnd0, opnd1, false);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        if (opnd0_size == 64) {
            /* m64 */
            la_ld_d(src0, mem_opnd, imm);
        } else {
            /* m16/m32 */
            la_ld_w(src0, mem_opnd, imm);
        }
    }

    /* set eflag */
    generate_eflag_calculation_bd(src0, src0, bit_offset, pir1, true);

    /* calculate */
    IR1_OPCODE_BD opcode = ir1_opcode_bd(pir1);
    write_back = calculate_btx(src0, bit_offset, opcode);

    /* Write back */
    if (write_back) {
        if (ir1_opnd_is_gpr_bd(opnd0)) {
            /* r16/r32/r64 */
            store_ireg_to_ir1_bd(src0, opnd0, false);
        } else {
            if (opnd0_size == 64) {
                /* m64 */
                la_st_d(src0, mem_opnd, imm);
            } else {
                /* m16/m32 */
                la_st_w(src0, mem_opnd, imm);
            }
            if (is_lock) {
                tr_lat_spin_unlock(lat_lock_addr);
            }
        }
    }

    ra_free_temp_auto(src0);
    ra_free_temp(bit_offset);
    ra_free_temp(mem_opnd);
    return true;
}

bool translate_blsr_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest = load_ireg_from_ir1_bd(opnd0, UNKNOWN_EXTENSION, false);
    IR2_OPND src = load_ireg_from_ir1_bd(opnd1, UNKNOWN_EXTENSION, false);

    IR2_OPND temp = ra_alloc_itemp();
    IR2_OPND temp_and = ra_alloc_itemp();
    IR2_OPND src_temp = ra_alloc_itemp();
    if (ir1_opnd_is_same_reg_bd(opnd0, opnd1)) {
        la_or(src_temp, zero_ir2_opnd, src);
    }
    if (ir1_opnd_size_bd(opnd0) == 32) {
        la_addi_w(temp, src, -1);
        la_and(dest, temp, src);
        la_bstrpick_d(dest, dest, 31, 0);
    } else {
        la_addi_d(temp, src, -1);
        la_and(dest, temp, src);
    }

    if (ir1_opnd_is_same_reg_bd(opnd0, opnd1)) {
        generate_eflag_calculation_bd(dest, dest, src_temp, pir1, true);
    } else {
        generate_eflag_calculation_bd(dest, dest, src, pir1, true);
    }

    ra_free_temp(temp);
    ra_free_temp(temp_and);
    ra_free_temp(src_temp);

    return true;
}

bool translate_blsi_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND dest = load_ireg_from_ir1_bd(opnd0, UNKNOWN_EXTENSION, false);
    IR2_OPND src = load_ireg_from_ir1_bd(opnd1, UNKNOWN_EXTENSION, false);

    IR2_OPND temp = ra_alloc_itemp();
    IR2_OPND src_temp = ra_alloc_itemp();
    if (ir1_opnd_is_same_reg_bd(opnd0, opnd1)) {
        la_or(src_temp, zero_ir2_opnd, src);
    }
    if (ir1_opnd_size_bd(opnd0) == 32) {
        la_sub_w(temp, zero_ir2_opnd, src);
        la_and(dest, temp, src);
        la_bstrpick_d(dest, dest, 31, 0);
    } else {
        la_sub_d(temp, zero_ir2_opnd, src);
        la_and(dest, temp, src);
    }

    if (ir1_opnd_is_same_reg_bd(opnd0, opnd1)) {
        generate_eflag_calculation_bd(dest, dest, src_temp, pir1, true);
    } else {
        generate_eflag_calculation_bd(dest, dest, src, pir1, true);
    }

    ra_free_temp(temp);
    ra_free_temp(src_temp);

    return true;
}

bool translate_salc_bd(IR1_INST *pir1)
{
    IR2_OPND label_1 = ra_alloc_label();
    IR2_OPND label_2 = ra_alloc_label();
    IR2_OPND src_opnd =
        load_ireg_from_ir1_bd(&eax_ir1_opnd_bd, ZERO_EXTENSION, false);
    IR2_OPND flag_opnd = ra_alloc_itemp();
    la_x86mfflag(flag_opnd, CF_USEDEF_BIT);
    la_bne(flag_opnd, zero_ir2_opnd, label_1);
    la_bstrins_w(src_opnd, zero_ir2_opnd, 7, 0);
    la_b(label_2);
    la_label(label_1);
    la_ori(src_opnd, src_opnd, 0xff);
    la_label(label_2);
    store_ireg_to_ir1_bd(src_opnd, &eax_ir1_opnd_bd, false);
    return true;
}
