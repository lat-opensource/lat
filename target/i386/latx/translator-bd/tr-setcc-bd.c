#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "flag-lbt-bd.h"
#include "translate-bd.h"

/**
 * @brief SETcc translation
 *
 * SETcc has:
 * - SETO    // OF = 1
 * - SETNO   // OF = 0
 * - SETB    // CF = 1
 * - SETAE   // CF = 0
 * - SETE    // ZF = 1
 * - SETNE   // ZF = 0
 * - SETBE   // CF = 1 | ZF = 1
 * - SETA    // CF = O & ZF = O
 * - SETS    // SF = 1
 * - SETNS   // SF = 0
 * - SETP    // PF = 1
 * - SETNP   // PF = 0
 * - SETL    // SF <> OF
 * - SETGE   // SF = OF
 * - SETLE   // ZF = 1 | SF <> OF
 * - SETG    // ZF = 0 & SF = OF
 *
 * @param pir1 input current inst
 * @return true if transalte success
 * @return false translate failure
 */
bool translate_setcc_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);

    IR2_OPND cond = ra_alloc_itemp();

    get_eflag_condition_bd(&cond, pir1);

    store_ireg_to_ir1_bd(cond, opnd0, false);

    ra_free_temp(cond);

    return true;
}

bool translate_tzcnt_bd(IR1_INST *pir1)
{
    IR2_OPND src_opnd =
        load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_ctz = ra_alloc_label();
    IR2_OPND count;
    bool count_opnd_is_temp = false;
    int opnd_size = ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0));
    /* if the reg is 64 bits or is zero-extend from the high bits */
    lsassert(ir1_opnd_is_gpr_bd(ir1_get_opnd_bd(pir1, 0)));
    if (opnd_size == 64) {
        count = ra_alloc_gpr(ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0)));
    } else {
        count = ra_alloc_itemp();
        count_opnd_is_temp = true;
    }

    if (ir1_need_calculate_any_flag_bd(pir1)) {
        IR2_OPND eflags = ra_alloc_itemp();
        la_ori(eflags, zero_ir2_opnd, 0x1);
        la_masknez(eflags, eflags, src_opnd);
        la_bstrins_w(eflags, src_opnd, 6, 6);
        la_x86mtflag(eflags, ZF_USEDEF_BIT | CF_USEDEF_BIT);
        ra_free_temp(eflags);
    }

    /* case 1: if reg/mem is zero, it means dest should be opnd_size*/
    la_bne(src_opnd, zero_ir2_opnd, label_ctz);
    li_wu(count, opnd_size);
    la_b(label_exit);

    /* case 2: reg/mem not zero */
    /* bit scan forward, looks like the `ctz.d` */
    la_label(label_ctz);
    la_ctz_d(count, src_opnd);

    la_label(label_exit);
    /* 3. store result */
    if (count_opnd_is_temp) {
        store_ireg_to_ir1_bd(count, ir1_get_opnd_bd(pir1, 0), false);
        ra_free_temp(count);
    }
    return true;
}

bool translate_bsf_bd(IR1_INST *pir1)
{
    IR2_OPND src_opnd =
        load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND count;
    bool count_opnd_is_temp = false;
    int opnd_size = ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0));
    /* if the reg is 64 bits or is zero-extend from the high bits */
    lsassert(ir1_opnd_is_gpr_bd(ir1_get_opnd_bd(pir1, 0)));
    if (opnd_size == 64) {
        count = ra_alloc_gpr(ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0)));
    } else {
        count = ra_alloc_itemp();
        count_opnd_is_temp = true;
    }
    generate_eflag_calculation_bd(src_opnd, src_opnd, src_opnd, pir1, true);
    /* case 1: if reg/mem is zero */
    la_beq(src_opnd, zero_ir2_opnd, label_exit);
    /* case 2: reg/mem not zero */
    /* bit scan forward, looks like the `ctz.d` */
    /* 1. count zero */
    la_ctz_d(count, src_opnd);
    /* 2. store result */
    if (count_opnd_is_temp) {
        store_ireg_to_ir1_bd(count, ir1_get_opnd_bd(pir1, 0), false);
        ra_free_temp(count);
    }

/* label_exit: */
    la_label(label_exit);
    return true;
}

bool translate_bsr_bd(IR1_INST *pir1)
{
    IR2_OPND src_opnd =
        load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1), ZERO_EXTENSION, false);

    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND count;
    bool count_opnd_is_temp = false;
    int opnd_size = ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0));
    /* if the reg is 64 bits or is zero-extend from the high bits */
    lsassert(ir1_opnd_is_gpr_bd(ir1_get_opnd_bd(pir1, 0)));
    if (opnd_size == 64) {
        count = ra_alloc_gpr(ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0)));
    } else {
        count = ra_alloc_itemp();
        count_opnd_is_temp = true;
    }

    generate_eflag_calculation_bd(src_opnd, src_opnd, src_opnd, pir1, true);
    /* case 1: if reg/mem is zero */
    la_beq(src_opnd, zero_ir2_opnd, label_exit);
    /* case 2: reg/mem not zero */
    /* bit scan forward, looks like the `CLZ.D` */
    /* 1. we can use `CLZ.D` */
    la_clz_d(count, src_opnd);
    /* 2. using `CLZ.D` we get the data is `reverse` */
    /*    we need make count <- 63 - count */
    la_addi_d(count, count, -63);
    la_sub_d(count, zero_ir2_opnd, count);
    /* 3. store result */
    if (count_opnd_is_temp) {
        store_ireg_to_ir1_bd(count, ir1_get_opnd_bd(pir1, 0), false);
        ra_free_temp(count);
    }
    /* 5. finish all steps, jump to exit */
/* label_exit: */
    la_label(label_exit);

    return true;
}

bool translate_lzcnt_bd(IR1_INST *pir1)
{
    IR2_OPND src_opnd =
        load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1), ZERO_EXTENSION, false);

    IR2_OPND temp = ra_alloc_itemp();
    IR2_OPND count;
    bool count_opnd_is_temp = false;
    int opnd_size = ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0));
    li_d(temp, 64 - opnd_size);
    lsassert(ir1_opnd_is_gpr_bd(ir1_get_opnd_bd(pir1, 0)));
    if (opnd_size == 64) {
        count = ra_alloc_gpr(ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(pir1, 0)));
    } else {
        count = ra_alloc_itemp();
        count_opnd_is_temp = true;
    }

    la_clz_d(count, src_opnd);
    la_sub_d(count, count, temp);
    if (count_opnd_is_temp) {
        store_ireg_to_ir1_bd(count, ir1_get_opnd_bd(pir1, 0), false);
        ra_free_temp(count);
    }

    IR2_OPND dest_opnd =
        load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 0), ZERO_EXTENSION, false);
    generate_eflag_calculation_bd(dest_opnd, dest_opnd, src_opnd, pir1, true);

    return true;
}
