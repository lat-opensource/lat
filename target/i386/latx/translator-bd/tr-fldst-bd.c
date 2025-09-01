#include <math.h>
#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "translate-bd.h"

/* TODO: long_double */
/* bool is_long_double_ir1_opnd(IR1_OPND_BD* opnd){ */
/*     if(!OPTIONS::long_double()) */
/*         return false; */
/*     if(opnd->size()==80 || opnd->size() == 128) */
/*         return true; */
/*     return false; */
/* } */

bool translate_fld_bd(IR1_INST *pir1)
{
    /* TODO: long_double */
    /* if(is_long_double_ir1_opnd(ir1_get_opnd_bd(pir1, 0))){ */
    /*     fprintf(stderr, "Long double for %s not implemented. translation */
    /*     failed.\n", __FUNCTION__); return false; */
    /* } */

    /* 1. the position to be overwritten is st(7) at this time */
    IR2_OPND dest_opnd = ra_alloc_st(7);

    /* 2. load the value */
    load_freg_from_ir1_2_bd(dest_opnd, ir1_get_opnd_bd(pir1, 1), IS_CONVERT);

    /* 3. adjust top */
    tr_fpu_push();

    return true;
}

bool translate_fldz_bd(IR1_INST *pir1)
{
    /* 1. the position to be overwritten is st(0) */
    tr_fpu_push();
    IR2_OPND dest_opnd = ra_alloc_st(0);

    /* 2. load the value */

    la_movgr2fr_d(dest_opnd, zero_ir2_opnd);
    la_ffint_d_w(dest_opnd, dest_opnd);

    return true;
}

bool translate_fld1_bd(IR1_INST *pir1)
{
    /* 1. the position to be overwritten is st(0) */
    tr_fpu_push();
    IR2_OPND dest_opnd = ra_alloc_st(0);

    /* 2. load the value */
    IR2_OPND itemp_1 = ra_alloc_itemp();
    la_ori(itemp_1, zero_ir2_opnd, 1);
    la_movgr2fr_d(dest_opnd, itemp_1);
    la_ffint_d_w(dest_opnd, dest_opnd);

    ra_free_temp(itemp_1);
    return true;
}

bool translate_fldl2e_bd(IR1_INST *pir1)
{
    /* 1. the position to be overwritten is st(0) */
    tr_fpu_push();
    IR2_OPND dest_opnd = ra_alloc_st(0);
    IR2_OPND itemp_l2e = ra_alloc_itemp();

    /* 2. load the value */
    /* l2e = 1 / log(2.0) = 0x3FF71547652B82FE */
    li_d(itemp_l2e, 0x3FF71547652B82FE);
    la_movgr2fr_d(dest_opnd, itemp_l2e);

    ra_free_temp(itemp_l2e);

    return true;
}

bool translate_fldl2t_bd(IR1_INST *pir1)
{
    /* 1. the position to be overwritten is st(0) */
    tr_fpu_push();
    IR2_OPND dest_opnd = ra_alloc_st(0);
    IR2_OPND itemp_l2t = ra_alloc_itemp();

    /* 2. load the value */
    /* l2t = 1 / log10(2.0) = 0x400A934F0979A371 */
    li_d(itemp_l2t, 0x400A934F0979A371);
    la_movgr2fr_d(dest_opnd, itemp_l2t);

    ra_free_temp(itemp_l2t);


    return true;
}

bool translate_fldlg2_bd(IR1_INST *pir1)
{
    /* 1. the position to be overwritten is st(0) */
    tr_fpu_push();
    IR2_OPND dest_opnd = ra_alloc_st(0);
    IR2_OPND itemp_lg2 = ra_alloc_itemp();

    /* 2. load the value */
    /* lg2 = log10(2.0) = 0x3FD34413509F79FF */
    li_d(itemp_lg2, 0x3FD34413509F79FF);
    la_movgr2fr_d(dest_opnd, itemp_lg2);

    ra_free_temp(itemp_lg2);

    return true;
}

bool translate_fldln2_bd(IR1_INST *pir1)
{
    /* 1. the position to be overwritten is st(0) */
    tr_fpu_push();
    IR2_OPND dest_opnd = ra_alloc_st(0);
    IR2_OPND itemp_ln2 = ra_alloc_itemp();

    /* 2. load the value */
    /* ln2 = log(2.0) = 0x3fe62e42fefa39ef */
    li_d(itemp_ln2, 0x3fe62e42fefa39ef);
    la_movgr2fr_d(dest_opnd, itemp_ln2);

    ra_free_temp(itemp_ln2);

    return true;
}

bool translate_fldpi_bd(IR1_INST *pir1)
{
    /* 1. the position to be overwritten is st(0) */
    tr_fpu_push();
    IR2_OPND dest_opnd = ra_alloc_st(0);
    IR2_OPND itemp_pi = ra_alloc_itemp();

    /* 2. load the value */
    /* pi = 0x400921FB54442D18 */
    li_d(itemp_pi, 0x400921FB54442D18);
    la_movgr2fr_d(dest_opnd, itemp_pi);

    ra_free_temp(itemp_pi);

    return true;
}

bool translate_fstp_bd(IR1_INST *pir1)
{
    /* TODO: long_double */
    /* if(is_long_double_ir1_opnd(ir1_get_opnd_bd(pir1, 0))){ */
    /*     fprintf(stderr, "Long double for %s not implemented. translation */
    /*     failed.\n", __FUNCTION__); return false; */
    /* } */

    /* 1. load value from st(0) */
    IR2_OPND src_opnd = ra_alloc_st(0);

    /* 2. write to target */
    store_freg_to_ir1_bd(src_opnd, ir1_get_opnd_bd(pir1, 0), false, true);

    /* 3. adjust top */
    tr_fpu_pop();

    return true;
}

bool translate_fst_bd(IR1_INST *pir1)
{
    /* if(is_long_double_ir1_opnd(ir1_get_opnd_bd(pir1, 0))){ */
    /*     fprintf(stderr, "Long double for %s not implemented. translation */
    /*     failed.\n", __FUNCTION__); return false; */
    /* } */

    /* 1. load value from st(0) */
    IR2_OPND src_opnd = ra_alloc_st(0);

    /* 2. write to target */
    store_freg_to_ir1_bd(src_opnd, ir1_get_opnd_bd(pir1, 0), false, true);

    return true;
}

bool translate_fild_bd(IR1_INST *pir1)
{
    tr_fpu_push();

    IR2_OPND dest_opnd = ra_alloc_st(0);

    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 1);
    load_freg_from_ir1_2_bd(dest_opnd, opnd0, IS_INTEGER);
    if (ir1_opnd_size_bd(opnd0) > 32) {
        la_ffint_d_l(dest_opnd, dest_opnd);
    } else {
        la_ffint_d_w(dest_opnd, dest_opnd);
    }

    return true;
}

bool translate_fist_bd(IR1_INST *pir1)
{
    /* 1. load rounding_mode from x86_control_word to mips_fcsr */
    IR2_OPND tmp_fcsr = ra_alloc_itemp();
    la_movfcsr2gr(tmp_fcsr, fcsr_ir2_opnd);

    lsassert((ir1_get_opnd_num_bd(pir1) == 2));

    IR2_OPND dest_opnd = ra_alloc_st(0);

    IR2_OPND dest_int = ra_alloc_itemp();

    /* first, we should make sure if dest_opnd is unordered? overflow? */
    /* underflow? */
    uint64 low_bound =0;
    uint64 high_bound = 0;
    switch (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0))) {
    case 16:
        low_bound = 0xc0dfffe000000000ull;
        high_bound = 0x40dfffe000000000ull;
        break;
    case 32:
        low_bound = 0xc1dfffffffe00000ull;
        high_bound = 0x41dfffffffe00000ull;
        break;
    case 64:
        low_bound = 0xc3dfffffffffffffull;
        high_bound = 0x43dfffffffffffffull;
        break;
    default:
        lsassertm(0, "Wrong opnd size : %d!\n", ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)));
    }

    IR2_OPND bounder_opnd = ra_alloc_itemp();
    li_d(bounder_opnd, high_bound); /* double for 0x7fff+0.499999 */
    IR2_OPND f_high_bounder_opnd = ra_alloc_ftemp();
    la_movgr2fr_d(f_high_bounder_opnd, bounder_opnd);

    li_d(bounder_opnd, low_bound); /* double for 0x8000+(-0.499999) */
    IR2_OPND f_low_bounder_opnd = ra_alloc_ftemp();
    la_movgr2fr_d(f_low_bounder_opnd, bounder_opnd);
    ra_free_temp(bounder_opnd);

    /*is unorder? */
    la_fcmp_cond_d(fcc0_ir2_opnd, dest_opnd, dest_opnd, FCMP_COND_CUN);
    IR2_OPND label_flow = ra_alloc_label();
    la_bcnez(fcc0_ir2_opnd, label_flow);

    /* is underflow or overflow? */
    if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) != 64) {

        la_fcmp_cond_d(fcc0_ir2_opnd, f_high_bounder_opnd, dest_opnd, FCMP_COND_CLE);
        la_bcnez(fcc0_ir2_opnd, label_flow);
        la_fcmp_cond_d(fcc0_ir2_opnd, dest_opnd, f_low_bounder_opnd, FCMP_COND_CLE);
        la_bcnez(fcc0_ir2_opnd, label_flow);
    } else {

        la_fcmp_cond_d(fcc0_ir2_opnd, f_high_bounder_opnd, dest_opnd, FCMP_COND_CLT);
        la_bcnez(fcc0_ir2_opnd, label_flow);
        la_fcmp_cond_d(fcc0_ir2_opnd, dest_opnd, f_low_bounder_opnd, FCMP_COND_CLT);
        la_bcnez(fcc0_ir2_opnd, label_flow);
    }
    ra_free_temp(f_low_bounder_opnd);
    ra_free_temp(f_high_bounder_opnd);
    /* not unorder or flow */
    IR2_OPND fp_opnd = ra_alloc_ftemp();
    la_ftint_l_d(fp_opnd, dest_opnd);
    la_movfr2gr_d(dest_int, fp_opnd);
    ra_free_temp(fp_opnd);
    IR2_OPND label_end = ra_alloc_label();
    la_b(label_end);

    /*overflow, mov ox8000 or 0x80000000 to dest*/
    la_label(label_flow);
    switch (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0))) {
    case 16:
        li_w(dest_int, 0x8000);
        break;
    case 32:
        li_w(dest_int, 0x80000000);
        break;
    case 64:
        li_d(dest_int, 0x8000000000000000ull);
        break;
    default:
        lsassertm(0, "Wrong opnd size : %d!\n", ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)));
    }

    la_label(label_end);
    store_ireg_to_ir1_bd(dest_int, ir1_get_opnd_bd(pir1, 0), false);
    ra_free_temp(dest_int);

    la_movgr2fcsr(fcsr_ir2_opnd, tmp_fcsr);
    ra_free_temp(tmp_fcsr);

    return true;
}

bool translate_fistp_bd(IR1_INST *pir1)
{
    translate_fist_bd(pir1);
    tr_fpu_pop();
    return true;
}

bool translate_fnstsw_bd(IR1_INST *pir1)
{
    lsassert(ir1_get_opnd_num_bd(pir1) == 1);

    IR2_OPND sw_value = ra_alloc_itemp();

   /*
    * NOTE: Refer to X86 Docs, there is m2bytes
    * condition ONLY, TCG hardcode write 16bit as well.
    * So change the opnd size to 16 bit to avoid data
    * overwtite.
    */
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    // int opnd_size = ir1_opnd_size_bd(opnd0);

    // if (opnd_size == 32) {
    //     opnd0->size = (16 >> 3);
    //  }

    update_sw_by_fcsr_bd(sw_value);

    /* 2. store the current value of status_word to dest_opnd */
    if (ir1_opnd_is_mem_bd(opnd0)) {
        int mem_imm;
        IR2_OPND mem_opnd = convert_mem_bd(opnd0, &mem_imm);
        la_st_h(sw_value, mem_opnd, mem_imm);
    } else {
        store_ireg_to_ir1_bd(sw_value, opnd0, false);
    }

    /* 3. free tmp */
    ra_free_temp(sw_value);

    return true;
}
