#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "flag-lbt-bd.h"
#include "translate-bd.h"

//0000000000000040 <helper_pext>:
//  40:   0015000e    move    $r14,$r0
//  44:   400034a0    beqz    $r5,52(0x34) # 78 <.L7>
//  48:   0015000d    move    $r13,$r0
//  4c:   0015000e    move    $r14,$r0
//
//0000000000000050 <.L9>:
//  50:   00002cac    ctz.d   $r12,$r5
//  54:   0040818c    slli.w  $r12,$r12,0x0
//  58:   0019308c    srl.d   $r12,$r4,$r12
//  5c:   02fffcaf    addi.d  $r15,$r5,-1(0xfff)
//  60:   0340058c    andi    $r12,$r12,0x1
//  64:   0018b58c    sll.d   $r12,$r12,$r13
//  68:   0014bca5    and $r5,$r5,$r15
//  6c:   001531ce    or  $r14,$r14,$r12
//  70:   028005ad    addi.w  $r13,$r13,1(0x1)
//  74:   47ffdcbf    bnez    $r5,-36(0x7fffdc) # 50 <.L9>
//
//0000000000000078 <.L7>:
//  78:   001501c4    move    $r4,$r14
//  7c:   4c000020    jirl    $r0,$r1,0

bool translate_pext_bd(IR1_INST *pir1)
{
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_ctz = ra_alloc_label();
    IR2_OPND src_opnd =
        load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND mask_opnd = ra_alloc_itemp();
    load_ireg_from_ir1_2_bd(mask_opnd, ir1_get_opnd_bd(pir1, 2), ZERO_EXTENSION, false);
    IR2_OPND temp1_opnd = ra_alloc_itemp();
    IR2_OPND temp2_opnd = ra_alloc_itemp();
    IR2_OPND temp3_opnd = ra_alloc_itemp(); //shift_bit
    /* if the reg is 64 bits or is zero-extend from the high bits */
    lsassert(ir1_opnd_is_gpr_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND result_opnd = ra_alloc_itemp();

    la_mov64(temp3_opnd, zero_ir2_opnd);
    la_mov64(result_opnd, zero_ir2_opnd);
    la_beq(mask_opnd, zero_ir2_opnd, label_exit);

    la_label(label_ctz);
    if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 32) {
        la_ctz_w(temp1_opnd, mask_opnd);
        la_srl_w(temp1_opnd, src_opnd, temp1_opnd);
        la_addi_w(temp2_opnd, mask_opnd, -1);
        la_andi(temp1_opnd, temp1_opnd, 0x1);
        la_sll_w(temp1_opnd, temp1_opnd, temp3_opnd);
        la_and(mask_opnd, mask_opnd, temp2_opnd);
        la_or(result_opnd, result_opnd, temp1_opnd);
        la_addi_w(temp3_opnd, temp3_opnd, 1);
    } else {
        la_ctz_d(temp1_opnd, mask_opnd);
        la_srl_d(temp1_opnd, src_opnd, temp1_opnd);
        la_addi_d(temp2_opnd, mask_opnd, -1);
        la_andi(temp1_opnd, temp1_opnd, 0x1);
        la_sll_d(temp1_opnd, temp1_opnd, temp3_opnd);
        la_and(mask_opnd, mask_opnd, temp2_opnd);
        la_or(result_opnd, result_opnd, temp1_opnd);
        la_addi_d(temp3_opnd, temp3_opnd, 1);
    }
    la_bne(mask_opnd, zero_ir2_opnd, label_ctz);

    la_label(label_exit);
    /* 3. store result */
    store_ireg_to_ir1_bd(result_opnd, ir1_get_opnd_bd(pir1, 0), false);
    ra_free_temp(result_opnd);
    return true;
}

bool translate_pdep_bd(IR1_INST *pir1)
{
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_ctz = ra_alloc_label();
    IR2_OPND src_opnd =
        load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND mask_opnd = ra_alloc_itemp();
    load_ireg_from_ir1_2_bd(mask_opnd, ir1_get_opnd_bd(pir1, 2), ZERO_EXTENSION, false);
    IR2_OPND temp1_opnd = ra_alloc_itemp();
    IR2_OPND temp2_opnd = ra_alloc_itemp();
    IR2_OPND temp3_opnd = ra_alloc_itemp(); //shift_bit
    IR2_OPND temp4_opnd = ra_alloc_itemp(); 
    /* if the reg is 64 bits or is zero-extend from the high bits */
    lsassert(ir1_opnd_is_gpr_bd(ir1_get_opnd_bd(pir1, 0)));
    IR2_OPND result_opnd = ra_alloc_itemp();

    la_mov64(temp3_opnd, zero_ir2_opnd);
    la_mov64(result_opnd, zero_ir2_opnd);
    la_beq(mask_opnd, zero_ir2_opnd, label_exit);

    la_label(label_ctz);
    if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 32) {
        la_ctz_w(temp1_opnd, mask_opnd);
        la_srl_w(temp4_opnd, src_opnd, temp3_opnd);
        la_addi_w(temp2_opnd, mask_opnd, -1);
        la_andi(temp4_opnd, temp4_opnd, 0x1);
        la_sll_w(temp4_opnd, temp4_opnd, temp1_opnd);
        la_and(mask_opnd, mask_opnd, temp2_opnd);
        la_or(result_opnd, result_opnd, temp4_opnd);
        la_addi_w(temp3_opnd, temp3_opnd, 1);
    } else {
        la_ctz_d(temp1_opnd, mask_opnd);
        la_srl_d(temp4_opnd, src_opnd, temp3_opnd);
        la_addi_d(temp2_opnd, mask_opnd, -1);
        la_andi(temp4_opnd, temp4_opnd, 0x1);
        la_sll_d(temp4_opnd, temp4_opnd, temp1_opnd);
        la_and(mask_opnd, mask_opnd, temp2_opnd);
        la_or(result_opnd, result_opnd, temp4_opnd);
        la_addi_d(temp3_opnd, temp3_opnd, 1);
    }
    la_bne(mask_opnd, zero_ir2_opnd, label_ctz);

    la_label(label_exit);
    /* 3. store result */
    store_ireg_to_ir1_bd(result_opnd, ir1_get_opnd_bd(pir1, 0), false);
    ra_free_temp(result_opnd);
    return true;
}

bool translate_bextr_bd(IR1_INST *pir1)
{
    IR2_OPND src_opnd = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND ctrl_opnd = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 2), ZERO_EXTENSION, false);
    IR2_OPND start_opnd = ra_alloc_itemp();
    IR2_OPND len_opnd = ra_alloc_itemp();
    IR2_OPND slt_opnd = ra_alloc_itemp();
    IR2_OPND temp1_opnd = ra_alloc_itemp();
    IR2_OPND temp2_opnd = ra_alloc_itemp();
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_zf0 = ra_alloc_label();
    IR2_OPND label_zf1 = ra_alloc_label();
    int bound = (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 32) ? 32: 64;

    la_bstrpick_d(start_opnd, ctrl_opnd, 7, 0);
    la_bstrpick_d(len_opnd, ctrl_opnd, 15, 8);

    la_srl_d(temp1_opnd, src_opnd, start_opnd);
    la_sltui(slt_opnd, start_opnd, bound); 
    ra_free_temp(start_opnd);
    la_maskeqz(temp1_opnd, temp1_opnd, slt_opnd);

    li_d(temp2_opnd, -1);
    la_sll_d(temp2_opnd, temp2_opnd, len_opnd);
    la_sltui(slt_opnd, len_opnd, bound); 
    ra_free_temp(len_opnd);
    la_maskeqz(temp2_opnd, temp2_opnd, slt_opnd);

    la_andn(temp2_opnd, temp1_opnd, temp2_opnd);
    ra_free_temp(temp1_opnd);

    /* 2. set eflags */
    la_beq(temp2_opnd, zero_ir2_opnd, label_zf1);
    /* set zf = 0 */
    la_label(label_zf0);
    la_x86mtflag(zero_ir2_opnd, 0x8);
    la_b(label_exit);
    /* set zf = 1 */
    la_label(label_zf1);
    IR2_OPND n4095_opnd = ra_alloc_num_4095();
    la_x86mtflag(n4095_opnd, 0x8);
    ra_free_num_4095(n4095_opnd);
    la_label(label_exit);
    /* set cf = 0, of = 0 */
    la_x86mtflag(zero_ir2_opnd, 0x21);
    /* 3. store result */
    store_ireg_to_ir1_bd(temp2_opnd, ir1_get_opnd_bd(pir1, 0), false);
    return true;
}

bool translate_blsmsk_bd(IR1_INST *pir1)
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
        la_xor(dest, temp, src);
        la_bstrpick_d(dest, dest, 31, 0);
    } else {
        la_addi_d(temp, src, -1);
        la_xor(dest, temp, src);
    }

    if (ir1_opnd_is_same_reg_bd(opnd0, opnd1)) {
        generate_eflag_calculation_bd(dest, temp, src_temp, pir1, true);
    } else {
        generate_eflag_calculation_bd(dest, temp, src, pir1, true);
    }

    ra_free_temp(temp);
    ra_free_temp(temp_and);
    ra_free_temp(src_temp);

    return true;
}

bool translate_bzhi_bd(IR1_INST *pir1)
{
    IR2_OPND src_opnd = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND ctrl_opnd = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 2), ZERO_EXTENSION, false);
    IR2_OPND n_opnd = ra_alloc_itemp();
    IR2_OPND slt_opnd = ra_alloc_itemp();
    IR2_OPND temp1_opnd = ra_alloc_itemp();
    IR2_OPND label_exit = ra_alloc_label();

    int opnd0_size = ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0));
    int bound = (opnd0_size == 32) ? 32: 64;

    la_bstrpick_d(n_opnd, ctrl_opnd, 7, 0);

    li_d(temp1_opnd, -1);
    la_sll_d(temp1_opnd, temp1_opnd, n_opnd);
    la_sltui(slt_opnd, n_opnd, bound); 
    la_maskeqz(temp1_opnd, temp1_opnd, slt_opnd);

    la_andn(temp1_opnd, src_opnd, temp1_opnd);

    /* 2. set eflags */
    generate_eflag_calculation_bd(temp1_opnd, temp1_opnd, temp1_opnd, pir1, true);

    /* IF (N > OperandSize - 1) CF ← 1 */
    IR2_OPND temp2_opnd = ra_alloc_itemp();
    li_d(temp2_opnd, opnd0_size);
    la_addi_d(temp2_opnd, temp2_opnd, -1);
    la_bge(temp2_opnd, n_opnd, label_exit);
    /* set cf = 1 */
    IR2_OPND n4095_opnd = ra_alloc_num_4095();
    la_x86mtflag(n4095_opnd, CF_USEDEF_BIT);
    ra_free_num_4095(n4095_opnd);

    /* 3. store result */
    la_label(label_exit);
    store_ireg_to_ir1_bd(temp1_opnd, ir1_get_opnd_bd(pir1, 0), false);
    return true;
}
