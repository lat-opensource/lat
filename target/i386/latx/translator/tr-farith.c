#include <math.h>
#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "translate.h"

bool translate_fadd(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
	if(pir1->info->x86.opcode[0] == 0xde) {
        translate_faddp(pir1);
        return true;
	}
    lsassert(opnd_num == 1 || opnd_num == 2);
    if (opnd_num == 1) {
        IR2_OPND st0_opnd = ra_alloc_st(0);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        la_fadd_d(st0_opnd, st0_opnd, src1_opnd);
    } else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fadd_d(dest_opnd, dest_opnd, src1_opnd);
    }

    return true;
}


bool translate_faddp(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    //lsassert(opnd_num == 0 || opnd_num == 2); //capstone may has only one opnd 0xde0xc1

    if (opnd_num == 0) {
        IR2_OPND st0 = ra_alloc_st(0);
        IR2_OPND st1 = ra_alloc_st(1);
        la_fadd_d(st1, st0, st1);
    }
    else if(opnd_num == 1){
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND st0 = ra_alloc_st(0);
        la_fadd_d(dest_opnd, dest_opnd, st0);
    }
    else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fadd_d(dest_opnd, dest_opnd, src1_opnd);
    }
    tr_fpu_pop();

    return true;
}

bool translate_fiadd(IR1_INST *pir1)
{
    uint8_t index = 0;
    switch (ir1_get_opnd_num(pir1)) {
    case 1:
        index = 0;
        break;
    case 2:
        index = 1;
        break;
    default:
        lsassert(0);
    }
    IR2_OPND st0 = ra_alloc_st(0);
    IR2_OPND t_freg = ra_alloc_ftemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    load_freg_from_ir1_2(t_freg, opnd0, 0);
    if (ir1_opnd_size(opnd0) > 32) {
        la_ffint_d_l(t_freg, t_freg);
    } else {
        la_ffint_d_w(t_freg, t_freg);
    }

    la_fadd_d(st0, t_freg, st0);
    ra_free_temp(t_freg);
    return true;
}

bool translate_fsub(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    if (opnd_num == 1) {
        IR2_OPND st0_opnd = ra_alloc_st(0);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        la_fsub_d(st0_opnd, st0_opnd, src1_opnd);
    } else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fsub_d(dest_opnd, dest_opnd, src1_opnd);
    }

    return true;
}

bool translate_fisub(IR1_INST *pir1)
{
    uint8_t index = 0;
    switch (ir1_get_opnd_num(pir1)) {
    case 1:
        index = 0;
        break;
    case 2:
        index = 1;
        break;
    default:
        lsassert(0);
    }
    IR2_OPND st0 = ra_alloc_st(0);
    IR2_OPND t_freg = ra_alloc_ftemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    load_freg_from_ir1_2(t_freg, opnd0, 0);
    if (ir1_opnd_size(opnd0) > 32) {
        la_ffint_d_l(t_freg, t_freg);
    } else {
        la_ffint_d_w(t_freg, t_freg);
    }

    la_fsub_d(st0, st0, t_freg);
    ra_free_temp(t_freg);
    return true;
}

bool translate_fisubr(IR1_INST *pir1)
{
    uint8_t index = 0;
    switch (ir1_get_opnd_num(pir1)) {
    case 1:
        index = 0;
        break;
    case 2:
        index = 1;
        break;
    default:
        lsassert(0);
    }
    IR2_OPND st0 = ra_alloc_st(0);
    IR2_OPND t_freg = ra_alloc_ftemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    load_freg_from_ir1_2(t_freg, opnd0, 0);
    if (ir1_opnd_size(opnd0) > 32) {
        la_ffint_d_l(t_freg, t_freg);
    } else {
        la_ffint_d_w(t_freg, t_freg);
    }

    la_fsub_d(st0, t_freg, st0);
    ra_free_temp(t_freg);
    return true;
}

bool translate_fsubr(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    if (opnd_num == 1) {
        IR2_OPND st0_opnd = ra_alloc_st(0);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        la_fsub_d(st0_opnd, src1_opnd, st0_opnd);
    } else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fsub_d(dest_opnd, src1_opnd, dest_opnd);
    }

    return true;
}

bool translate_fsubrp(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    //lsassert(opnd_num == 0 || opnd_num == 2);

    if (opnd_num == 0) {
        IR2_OPND st0 = ra_alloc_st(0);
        IR2_OPND st1 = ra_alloc_st(1);
        la_fsub_d(st1, st0, st1);
    }
    else if(opnd_num == 1){
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND st0 = ra_alloc_st(0);
        la_fsub_d(dest_opnd, st0, dest_opnd);
    }
    else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fsub_d(dest_opnd, src1_opnd, dest_opnd);
    }
    tr_fpu_pop();
    return true;
}

bool translate_fsubp(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
//    lsassert(opnd_num == 0 || opnd_num == 2);

    if (opnd_num == 0) {
        IR2_OPND st0 = ra_alloc_st(0);
        IR2_OPND st1 = ra_alloc_st(1);
        la_fsub_d(st1, st1, st0);
    }
    else if(opnd_num == 1){
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND st0 = ra_alloc_st(0);
        la_fsub_d(dest_opnd, dest_opnd, st0);
    }
    else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fsub_d(dest_opnd, dest_opnd, src1_opnd);
    }
    tr_fpu_pop();
    return true;
}

bool translate_fmul(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    if (opnd_num == 1) {
        IR2_OPND st0_opnd = ra_alloc_st(0);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        la_fmul_d(st0_opnd, st0_opnd, src1_opnd);
    } else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fmul_d(dest_opnd, dest_opnd, src1_opnd);
    }

    return true;
}

bool translate_fimul(IR1_INST *pir1)
{
    uint8_t index = 0;
    switch (ir1_get_opnd_num(pir1)) {
    case 1:
        index = 0;
        break;
    case 2:
        index = 1;
        break;
    default:
        lsassert(0);
    }
    IR2_OPND st0 = ra_alloc_st(0);
    IR2_OPND t_freg = ra_alloc_ftemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    load_freg_from_ir1_2(t_freg, opnd0, 0);
    if (ir1_opnd_size(opnd0) > 32) {
        la_ffint_d_l(t_freg, t_freg);
    } else {
        la_ffint_d_w(t_freg, t_freg);
    }

    la_fmul_d(st0, t_freg, st0);
    ra_free_temp(t_freg);
    return true;
}

bool translate_fmulp(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    //lsassert(opnd_num == 0 || opnd_num == 2);

    if (opnd_num == 0) {
        IR2_OPND st0 = ra_alloc_st(0);
        IR2_OPND st1 = ra_alloc_st(1);
        la_fmul_d(st1, st0, st1);
    }
    else if(opnd_num == 1){
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND st0 = ra_alloc_st(0);
        la_fmul_d(dest_opnd, dest_opnd, st0);
    }
    else {
        IR2_OPND src0_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fmul_d(src0_opnd, src0_opnd, src1_opnd);
    }
    tr_fpu_pop();

    return true;
}

bool translate_fdiv(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    if (opnd_num == 1) {
        IR2_OPND st0_opnd = ra_alloc_st(0);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        la_fdiv_d(st0_opnd, st0_opnd, src1_opnd);
    } else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fdiv_d(dest_opnd, dest_opnd, src1_opnd);
    }

    /* get status word and load in sw_value */
    IR2_OPND sw_value = ra_alloc_itemp();
    IR2_OPND fcsr0 = ra_alloc_itemp();
    IR2_OPND temp1 = ra_alloc_itemp();
    int offset = lsenv_offset_of_status_word(lsenv);
    assert(offset < 0x7ff);
    la_ld_hu(sw_value, env_ir2_opnd, offset);
    la_movfcsr2gr(fcsr0, fcsr_ir2_opnd);
    /* divide-by-zero exception */
    IR2_OPND label_z = ra_alloc_label();
    la_srli_w(temp1, fcsr0, FCSR_OFF_CAUSE_Z);
    la_andi(temp1, temp1, 0x1);
    la_beq(temp1, zero_ir2_opnd, label_z);
    la_ori(temp1, zero_ir2_opnd, 0x1 << X87_SR_OFF_ZE);
    la_or(sw_value, temp1, sw_value);
    la_label(label_z);
    /*  invalid-arithmetic-operand exception */
    IR2_OPND label_i = ra_alloc_label();
    la_srli_w(temp1, fcsr0, FCSR_OFF_CAUSE_V);
    la_andi(temp1, temp1, 0x1);
    la_beq(temp1, zero_ir2_opnd, label_i);
    la_ori(temp1, zero_ir2_opnd, 0x1 << X87_SR_OFF_IE);
    la_or(sw_value, temp1, sw_value);
    la_label(label_i);
    la_st_h(sw_value, env_ir2_opnd,
                        lsenv_offset_of_status_word(lsenv));

    return true;
}
bool translate_fdivr(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    lsassert(opnd_num == 1 || opnd_num == 2);

    if (opnd_num == 1) {
        IR2_OPND st0_opnd = ra_alloc_st(0);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        la_fdiv_d(st0_opnd, src1_opnd, st0_opnd);
    } else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fdiv_d(dest_opnd, src1_opnd, dest_opnd);
    }

    /* get status word and load in sw_value */
    IR2_OPND sw_value = ra_alloc_itemp();
    IR2_OPND fcsr0 = ra_alloc_itemp();
    IR2_OPND temp1 = ra_alloc_itemp();
    int offset = lsenv_offset_of_status_word(lsenv);
    assert(offset < 0x7ff);
    la_ld_hu(sw_value, env_ir2_opnd, offset);
    la_movfcsr2gr(fcsr0, fcsr_ir2_opnd);
    /* divide-by-zero exception */
    IR2_OPND label_z = ra_alloc_label();
    la_srli_w(temp1, fcsr0, FCSR_OFF_CAUSE_Z);
    la_andi(temp1, temp1, 0x1);
    la_beq(temp1, zero_ir2_opnd, label_z);
    la_ori(temp1, zero_ir2_opnd, 0x1 << X87_SR_OFF_ZE);
    la_or(sw_value, temp1, sw_value);
    la_label(label_z);
    /*  invalid-arithmetic-operand exception */
    IR2_OPND label_i = ra_alloc_label();
    la_srli_w(temp1, fcsr0, FCSR_OFF_CAUSE_V);
    la_andi(temp1, temp1, 0x1);
    la_beq(temp1, zero_ir2_opnd, label_i);
    la_ori(temp1, zero_ir2_opnd, 0x1 << X87_SR_OFF_IE);
    la_or(sw_value, temp1, sw_value);
    la_label(label_i);
    la_st_h(sw_value, env_ir2_opnd,
                        lsenv_offset_of_status_word(lsenv));

    return true;
}

bool translate_fidiv(IR1_INST *pir1)
{
    uint8_t index = 0;
    switch (ir1_get_opnd_num(pir1)) {
    case 1:
        index = 0;
        break;
    case 2:
        index = 1;
        break;
    default:
        lsassert(0);
    }
    IR2_OPND st0 = ra_alloc_st(0);
    IR2_OPND t_freg = ra_alloc_ftemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    load_freg_from_ir1_2(t_freg, opnd0, 0);
    if (ir1_opnd_size(opnd0) > 32) {
        la_ffint_d_l(t_freg, t_freg);
    } else {
        la_ffint_d_w(t_freg, t_freg);
    }

    la_fdiv_d(st0, st0, t_freg);
    ra_free_temp(t_freg);
    return true;
}
bool translate_fidivr(IR1_INST *pir1)
{
    uint8_t index = 0;
    switch (ir1_get_opnd_num(pir1)) {
    case 1:
        index = 0;
        break;
    case 2:
        index = 1;
        break;
    default:
        lsassert(0);
    }
    IR2_OPND st0 = ra_alloc_st(0);
    IR2_OPND t_freg = ra_alloc_ftemp();
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, index);
    load_freg_from_ir1_2(t_freg, opnd0, 0);
    if (ir1_opnd_size(opnd0) > 32) {
        la_ffint_d_l(t_freg, t_freg);
    } else {
        la_ffint_d_w(t_freg, t_freg);
    }

    la_fdiv_d(st0, t_freg, st0);
    ra_free_temp(t_freg);
    return true;
}

bool translate_fdivrp(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    //lsassert(opnd_num == 0 || opnd_num == 2);

    if (opnd_num == 0) {
        IR2_OPND st0 = ra_alloc_st(0);
        IR2_OPND st1 = ra_alloc_st(1);
        la_fdiv_d(st1, st0, st1);
    }
    else if(opnd_num == 1){
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND st0 = ra_alloc_st(0);
        la_fdiv_d(dest_opnd, st0, dest_opnd);
    }
    else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fdiv_d(dest_opnd, src1_opnd, dest_opnd);
    }
    tr_fpu_pop();

    /* get status word and load in sw_value */
    IR2_OPND sw_value = ra_alloc_itemp();
    IR2_OPND fcsr0 = ra_alloc_itemp();
    IR2_OPND temp1 = ra_alloc_itemp();
    int offset = lsenv_offset_of_status_word(lsenv);
    assert(offset < 0x7ff);
    la_ld_hu(sw_value, env_ir2_opnd, offset);
    la_movfcsr2gr(fcsr0, fcsr_ir2_opnd);
    /* divide-by-zero exception */
    IR2_OPND label_z = ra_alloc_label();
    la_srli_w(temp1, fcsr0, FCSR_OFF_CAUSE_Z);
    la_andi(temp1, temp1, 0x1);
    la_beq(temp1, zero_ir2_opnd, label_z);
    la_ori(temp1, zero_ir2_opnd, 0x1 << X87_SR_OFF_ZE);
    la_or(sw_value, temp1, sw_value);
    la_label(label_z);
    /*  invalid-arithmetic-operand exception */
    IR2_OPND label_i = ra_alloc_label();
    la_srli_w(temp1, fcsr0, FCSR_OFF_CAUSE_V);
    la_andi(temp1, temp1, 0x1);
    la_beq(temp1, zero_ir2_opnd, label_i);
    la_ori(temp1, zero_ir2_opnd, 0x1 << X87_SR_OFF_IE);
    la_or(sw_value, temp1, sw_value);
    la_label(label_i);
    la_st_h(sw_value, env_ir2_opnd,
                        lsenv_offset_of_status_word(lsenv));

    return true;
}
bool translate_fdivp(IR1_INST *pir1)
{
    int opnd_num = ir1_opnd_num(pir1);
    //lsassert(opnd_num == 0 || opnd_num == 2);

    if (opnd_num == 0) {
        IR2_OPND st0 = ra_alloc_st(0);
        IR2_OPND st1 = ra_alloc_st(1);
        la_fdiv_d(st1, st1, st0);
    }
    else if(opnd_num == 1){
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND st0 = ra_alloc_st(0);
        la_fdiv_d(dest_opnd, dest_opnd, st0);
    }
    else {
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, true);
        IR2_OPND src1_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, true);
        la_fdiv_d(dest_opnd, dest_opnd, src1_opnd);
    }
    tr_fpu_pop();

    /* get status word and load in sw_value */
    IR2_OPND sw_value = ra_alloc_itemp();
    IR2_OPND fcsr0 = ra_alloc_itemp();
    IR2_OPND temp1 = ra_alloc_itemp();
    int offset = lsenv_offset_of_status_word(lsenv);
    assert(offset < 0x7ff);
    la_ld_hu(sw_value, env_ir2_opnd, offset);
    la_movfcsr2gr(fcsr0, fcsr_ir2_opnd);
    /* divide-by-zero exception */
    IR2_OPND label_z = ra_alloc_label();
    la_srli_w(temp1, fcsr0, FCSR_OFF_CAUSE_Z);
    la_andi(temp1, temp1, 0x1);
    la_beq(temp1, zero_ir2_opnd, label_z);
    la_ori(temp1, zero_ir2_opnd, 0x1 << X87_SR_OFF_ZE);
    la_or(sw_value, temp1, sw_value);
    la_label(label_z);
    /*  invalid-arithmetic-operand exception */
    IR2_OPND label_i = ra_alloc_label();
    la_srli_w(temp1, fcsr0, FCSR_OFF_CAUSE_V);
    la_andi(temp1, temp1, 0x1);
    la_beq(temp1, zero_ir2_opnd, label_i);
    la_ori(temp1, zero_ir2_opnd, 0x1 << X87_SR_OFF_IE);
    la_or(sw_value, temp1, sw_value);
    la_label(label_i);
    la_st_h(sw_value, env_ir2_opnd,
                        lsenv_offset_of_status_word(lsenv));

    return true;
}

bool translate_fnop(IR1_INST *pir1)
{
    /* append_ir2_opnd0(mips_nop); */
    return true;
}

bool translate_fsqrt(IR1_INST *pir1)
{
    IR2_OPND st0_opnd = ra_alloc_st(0);
    la_fsqrt_d(st0_opnd, st0_opnd);

    return true;
}

bool translate_fabs(IR1_INST *pir1)
{
    IR2_OPND st0_opnd = ra_alloc_st(0);
    la_fabs_d(st0_opnd, st0_opnd);

    return true;
}

bool translate_fchs(IR1_INST *pir1)
{
    IR2_OPND st0_opnd = ra_alloc_st(0);
    la_fneg_d(st0_opnd, st0_opnd);

    return true;
}

bool translate_fdecstp(IR1_INST *pir1)
{
    tr_fpu_dec();

    /* decrements the top-of-stack pointer */
    IR2_OPND value_status = ra_alloc_itemp();
    IR2_OPND value_status_temp = ra_alloc_itemp();

    la_ld_h(value_status, env_ir2_opnd,
                      lsenv_offset_of_status_word(lsenv)); /* status_word */
    la_x86mftop(value_status_temp);
    la_bstrins_d(value_status, zero_ir2_opnd, 13, 11);
    la_slli_w(value_status_temp, value_status_temp, 11);
    la_or(value_status, value_status, value_status_temp);
    /* set C1 flag to 0 */
    la_bstrins_d(value_status, zero_ir2_opnd, 9, 9);
    la_st_h(value_status, env_ir2_opnd,
                      lsenv_offset_of_status_word(lsenv)); /* status_word */
    return true;
}

bool translate_fincstp(IR1_INST *pir1)
{
    tr_fpu_inc();

    /* increments the top-of-stack pointer */
    IR2_OPND value_status = ra_alloc_itemp();
    IR2_OPND value_status_temp = ra_alloc_itemp();
    la_ld_h(value_status, env_ir2_opnd,
                      lsenv_offset_of_status_word(lsenv)); /* status_word */
    la_x86mftop(value_status_temp);
    la_bstrins_d(value_status, zero_ir2_opnd, 13, 11);
    la_slli_w(value_status_temp, value_status_temp, 11);
    la_or(value_status, value_status, value_status_temp);

    /* set C1 flag to 0 */
    la_bstrins_d(value_status, zero_ir2_opnd, 9, 9);
    la_st_h(value_status, env_ir2_opnd,
                      lsenv_offset_of_status_word(lsenv)); /* status_word */
    return true;
}

bool translate_fsin(IR1_INST *pir1)
{
#ifdef USE_FSIN_HELPER
    tr_gen_call_to_helper1((ADDR)helper_fsin, 1, LOAD_HELPER_FSIN);
#else
    const int X86_C2_BITS = 10;
     /*
     * For LA $fa 0 is defined as argument and return value reg.
     * To avoid reg destroied, move argument setting after storing
     * regs to env.
      */
    IR2_OPND param_0 = ir2_opnd_new(IR2_OPND_FPR, 0);
    IR2_OPND param_8 = ra_alloc_ftemp();
    IR2_OPND st0_opnd = ra_alloc_st(0);
    la_fmov_d(param_8, st0_opnd);

    /* save regs before call helper func */
    tr_save_registers_to_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                             options_to_save());
#ifdef TARGET_X86_64
    tr_save_x64_8_registers_to_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif

    la_fmov_d(param_0, param_8);

    /* Call the function */
    tr_gen_call_to_helper((ADDR)sin, LOAD_HOST_SIN);

    /* f0 is mapped to x86 reg now, it will be destroyed by the following registers load,
     * save it
     */
    IR2_OPND ret_opnd, ret_value;
    ret_opnd = ir2_opnd_new(IR2_OPND_FPR, 0);
    ret_value = ra_alloc_ftemp();
    la_fmov_d(ret_value, ret_opnd);

    /* restore regs after native call*/
#ifdef TARGET_X86_64
    tr_load_x64_8_registers_from_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    tr_load_registers_from_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                               options_to_save());

    la_fmov_d(st0_opnd, ret_value);

    /* clear c2 flag in status word */
    IR2_OPND sw_value = ra_alloc_itemp();
    la_ld_hu(sw_value, env_ir2_opnd,
                            lsenv_offset_of_status_word(lsenv));
    la_bstrins_d(sw_value,
                          zero_ir2_opnd, X86_C2_BITS, X86_C2_BITS);
    la_st_h(sw_value, env_ir2_opnd,
                         lsenv_offset_of_status_word(lsenv));
    ra_free_temp(sw_value);
#endif
    return true;
}

bool translate_fcos(IR1_INST *pir1)
{
#ifdef USE_FCOS_HELPER
    tr_gen_call_to_helper1((ADDR)helper_fcos, 1, LOAD_HELPER_FCOS);
#else
    const int X86_C2_BITS = 10;
     /*
     * For LA $fa 0 is defined as argument and return value reg.
     * To avoid reg destoried, move argument setting after storing
     * regs to env.
      */
    IR2_OPND param_0 = ir2_opnd_new(IR2_OPND_FPR, 0);
    IR2_OPND param_8 = ra_alloc_ftemp();
    IR2_OPND st0_opnd = ra_alloc_st(0);
    la_fmov_d(param_8, st0_opnd);

    /* save regs before call helper func */
    tr_save_registers_to_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                             options_to_save());
#ifdef TARGET_X86_64
    tr_save_x64_8_registers_to_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    la_fmov_d(param_0, param_8);

    /* Call the function */
    tr_gen_call_to_helper((ADDR)cos, LOAD_HOST_COS);

    /* f0 is mapped to x86 reg now, it will be destroyed by the following registers load,
     * save it
     */
    IR2_OPND ret_opnd, ret_value;
    ret_opnd = ir2_opnd_new(IR2_OPND_FPR, 0);
    ret_value = ra_alloc_ftemp();
    la_fmov_d(ret_value, ret_opnd);

    /* restore regs after native call*/
#ifdef TARGET_X86_64
    tr_load_x64_8_registers_from_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    tr_load_registers_from_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                               options_to_save());

    la_fmov_d(st0_opnd, ret_value);

    /* clear c2 flag in status word */
    IR2_OPND sw_value = ra_alloc_itemp();
    la_ld_hu(sw_value, env_ir2_opnd,
                            lsenv_offset_of_status_word(lsenv));
    la_bstrins_d(sw_value,
                          zero_ir2_opnd, X86_C2_BITS, X86_C2_BITS);
    la_st_h(sw_value, env_ir2_opnd,
                      lsenv_offset_of_status_word(lsenv));
    ra_free_temp(sw_value);
#endif
    return true;
}

double pi = +3.141592653589793239;

bool translate_fpatan(IR1_INST *pir1)
{
#ifdef USE_FPATAN_HELPER
    tr_gen_call_to_helper1((ADDR)helper_fpatan, 1, LOAD_HELPER_FPATAN);
#else
     /*
     * For LA $fa 0 is defined as argument and return value reg.
     * To avoid reg destoried, move argument setting after storing
     * regs to env.
      */
    /* dispose the arguments */
    IR2_OPND st0_opnd = ra_alloc_st(0);
    IR2_OPND st1_opnd = ra_alloc_st(1);
    IR2_OPND param_0 = ir2_opnd_new(IR2_OPND_FPR, 0);
    IR2_OPND param_1 = ir2_opnd_new(IR2_OPND_FPR, 1);
    IR2_OPND param_8 = ra_alloc_ftemp();
    IR2_OPND param_9 = ra_alloc_ftemp();
    la_fmov_d(param_8, st0_opnd);
    la_fmov_d(param_9, st1_opnd);
    /* save regs before call helper func */
    tr_save_registers_to_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                             options_to_save());
#ifdef TARGET_X86_64
    tr_save_x64_8_registers_to_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    la_fmov_d(param_1, param_8);
    la_fmov_d(param_0, param_9);

    /* Call the function */
    tr_gen_call_to_helper((ADDR)atan2, LOAD_HOST_ATAN2);

    IR2_OPND ret_opnd, ret_value;
    ret_opnd = ir2_opnd_new(IR2_OPND_FPR, 0);
    ret_value = ra_alloc_ftemp();
    la_fmov_d(ret_value, ret_opnd);

    /* restore regs after native call*/
#ifdef TARGET_X86_64
    tr_load_x64_8_registers_from_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    tr_load_registers_from_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                               options_to_save());

    tr_fpu_pop();
    IR2_OPND new_st0_opnd = ra_alloc_st(0);
    la_fmov_d(new_st0_opnd, ret_value);

#endif
    return true;
}

bool translate_fprem(IR1_INST *pir1)
{
    tr_gen_call_to_helper1((ADDR)helper_fprem, 1, LOAD_HELPER_FPREM);
    return true;
}

bool translate_fprem1(IR1_INST *pir1)
{
    tr_gen_call_to_helper1((ADDR)helper_fprem1, 1, LOAD_HELPER_FPREM1);
    return true;
}

bool translate_frndint(IR1_INST *pir1)
{
#ifdef USE_HELPER_FRNDINT
    tr_gen_call_to_helper1((ADDR)helper_frndint, 1, LOAD_HELPER_FRNDINT);
#else
    IR2_OPND st0_opnd = ra_alloc_st(0);
    la_frint_d(st0_opnd, st0_opnd);
#endif

    return true;
}

bool translate_fscale(IR1_INST *pir1)
{
    IR2_OPND st0_opnd = ra_alloc_st(0);
    IR2_OPND st1_opnd = ra_alloc_st(1);
    IR2_OPND ret_value = ra_alloc_ftemp();
    la_ftintrz_l_d(ret_value, st1_opnd);
    la_fscaleb_d(st0_opnd, st0_opnd, ret_value);

    return true;
}

bool translate_fxam(IR1_INST *pir1)
{
    IR2_OPND st0_opnd = ra_alloc_st(0);
    IR2_OPND gpr_st0_opnd = ra_alloc_itemp();
    IR2_OPND fpu_tag = ra_alloc_itemp();
    IR2_OPND top_opnd = ra_alloc_itemp();
    IR2_OPND fpus = ra_alloc_itemp();
    IR2_OPND temp1 = ra_alloc_itemp();
    IR2_OPND temp2 = ra_alloc_itemp();
    int status_offset = lsenv_offset_of_status_word(lsenv);
    IR2_OPND not_set_c1 = ra_alloc_label();
    IR2_OPND label_next = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_infinity_nan = ra_alloc_label();
    IR2_OPND label_zero_denormal = ra_alloc_label();
    IR2_OPND label_infinity = ra_alloc_label();
    IR2_OPND label_zero = ra_alloc_label();

    /*
     * C3, C2, C1, C0 ← 0000
     */
    li_wu(temp1, 0xb8ff);
    la_ld_h(fpus, env_ir2_opnd, status_offset);
    la_and(fpus, fpus, temp1);

    /*
     * C1 ← sign bit of ST0
     */
    la_movfr2gr_d(gpr_st0_opnd, st0_opnd);
    li_d(temp1, 0x8000000000000000ULL);
    la_and(temp2, gpr_st0_opnd, temp1);
    la_bne(temp2, temp1, not_set_c1);
    la_ori(fpus, fpus, 0x200);
    la_label(not_set_c1);

    if (option_fputag) {
        /* empty is judged by FPU tag word */
        int tag_offset = lsenv_offset_of_tag_word(lsenv);
        lsassert(tag_offset <= 0x7ff);
        la_ld_d(fpu_tag, env_ir2_opnd, tag_offset);

        la_x86mftop(top_opnd);
        li_d(temp1, (uint64)(0xffULL));
        la_slli_d(top_opnd, top_opnd, 3);
        la_srl_d(fpu_tag, fpu_tag, top_opnd);
        la_and(fpu_tag, fpu_tag, temp1);
        la_beq(fpu_tag, zero_ir2_opnd, label_next);
    } else {
        la_x86mftop(top_opnd);
        la_bne(top_opnd, zero_ir2_opnd, label_next);
    }
    li_wu(temp1, 0x4100UL);
    la_or(fpus, fpus, temp1);
    la_b(label_exit);

    /*
     * slow path
     */
    la_label(label_next);
    li_d(temp1, (uint64)(0x7ff0000000000000ULL));
    la_and(temp2, gpr_st0_opnd, temp1);
    la_beq(temp2, temp1, label_infinity_nan);
    la_beq(temp2, zero_ir2_opnd, label_zero_denormal);
    /* Normal finite number */
    la_ori(fpus, fpus, 0x400);
    la_b(label_exit);
    la_label(label_infinity_nan);
    li_d(temp1, 0xfffffffffffffULL);
    la_and(temp2, gpr_st0_opnd, temp1);
    la_beq(temp2, zero_ir2_opnd, label_infinity);
    /* NaN */
    la_ori(fpus, fpus, 0x100);
    la_b(label_exit);
    /* Infinity */
    la_label(label_infinity);
    la_ori(fpus, fpus, 0x500);
    la_b(label_exit);
    la_label(label_zero_denormal);
    li_d(temp1, 0xfffffffffffffULL);
    la_and(temp2, gpr_st0_opnd, temp1);
    la_beq(temp2, zero_ir2_opnd, label_zero);
    /* Denormal */
    li_wu(temp1, 0x4400UL);
    la_or(fpus, fpus, temp1);
    la_b(label_exit);
    /* Zero */
    la_label(label_zero);
    li_w(temp1, 0x4000UL);
    la_or(fpus, fpus, temp1);
    la_b(label_exit);
    /*
     * exit
     */
    la_label(label_exit);
    la_st_h(fpus, env_ir2_opnd, status_offset);

    ra_free_temp(temp1);
    ra_free_temp(temp2);
    ra_free_temp(fpu_tag);
    return true;
}

bool translate_f2xm1(IR1_INST *pir1)
{
#ifdef USE_F2XM1_HELPER
    tr_gen_call_to_helper1((ADDR)helper_f2xm1, 1, LOAD_HELPER_F2XM1);
#else
    IR2_OPND st0_opnd = ra_alloc_st(0);
    IR2_OPND param_0 = ir2_opnd_new(IR2_OPND_FPR, 0);
    IR2_OPND param_1 = ir2_opnd_new(IR2_OPND_FPR, 1);
    IR2_OPND param_8 = ra_alloc_ftemp();
    IR2_OPND param_9 = ra_alloc_ftemp();
    IR2_OPND itemp_1 = ra_alloc_itemp();
    la_ori(itemp_1, zero_ir2_opnd, 2);
    la_movgr2fr_d(param_9, itemp_1);
    la_ffint_d_l(param_9, param_9);
    ra_free_temp(itemp_1);

    la_fmov_d(param_8, st0_opnd);
    /* save regs before call helper func */
    tr_save_registers_to_env(0, FPR_USEDEF_TO_SAVE,
                             XMM_USEDEF_TO_SAVE, options_to_save());
#ifdef TARGET_X86_64
    tr_save_x64_8_registers_to_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    la_fmov_d(param_1, param_8);
    la_fmov_d(param_0, param_9);

    /* Call the function */
    tr_gen_call_to_helper((ADDR)pow, LOAD_HOST_POW);

    IR2_OPND ret_opnd, ret_value;
    ret_opnd = ir2_opnd_new(IR2_OPND_FPR, 0);
    ret_value = ra_alloc_ftemp();
    la_fmov_d(ret_value, ret_opnd);

    /* restore regs after native call*/
#ifdef TARGET_X86_64
    tr_load_x64_8_registers_from_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    tr_load_registers_from_env(0, FPR_USEDEF_TO_SAVE,
                               XMM_USEDEF_TO_SAVE, options_to_save());

    IR2_OPND new_st0_opnd = ra_alloc_st(0);
    la_fmov_d(new_st0_opnd, ret_value);

    itemp_1 = ra_alloc_itemp();
    IR2_OPND t_freg = ra_alloc_ftemp();
    la_ori(itemp_1, zero_ir2_opnd, 1);
    la_movgr2fr_d(t_freg, itemp_1);
    la_ffint_d_l(t_freg, t_freg);
    ra_free_temp(itemp_1);

    la_fsub_d(new_st0_opnd, new_st0_opnd, t_freg);

#endif
    return true;
}

bool translate_fxtract(IR1_INST *pir1)
{
#ifdef USE_FXTRACT_HELPER
    tr_gen_call_to_helper1((ADDR)helper_fxtract, 1, LOAD_HELPER_FXTRACT);
#else
    /* if the source operand is 0.0 or -0.0, goto label_zero for special process
     */
    IR2_OPND f_zero = ra_alloc_ftemp();
    la_movgr2fr_d(f_zero, zero_ir2_opnd);
    la_ffint_d_l(f_zero, f_zero);
    IR2_OPND st0_opnd = ra_alloc_st(0);
    la_fcmp_cond_d(fcc0_ir2_opnd, st0_opnd, f_zero, FCMP_COND_CEQ);
    IR2_OPND label_zero = ra_alloc_label();
    la_bcnez(fcc0_ir2_opnd, label_zero);

    IR2_OPND fp_zero = ra_alloc_itemp();
    li_d(fp_zero, 0x8000000000000000ull);
    la_movgr2fr_d(f_zero, fp_zero);
    ra_free_temp(fp_zero);
    la_fcmp_cond_d(fcc0_ir2_opnd, st0_opnd, f_zero, FCMP_COND_CEQ);
    ra_free_temp(f_zero);
    la_bcnez(fcc0_ir2_opnd, label_zero);

	//inf
    IR2_OPND label_inf = ra_alloc_label();
    IR2_OPND fp_inf = ra_alloc_itemp();
    IR2_OPND f_inf = ra_alloc_ftemp();
    li_d(fp_inf, 0x7ff0000000000000ull);
    la_movgr2fr_d(f_inf, fp_inf);
    la_fcmp_cond_d(fcc0_ir2_opnd, st0_opnd, f_inf, FCMP_COND_CEQ);
    la_bcnez(fcc0_ir2_opnd, label_inf);
	//-inf
    li_d(fp_inf, 0xfff0000000000000ull);
    la_movgr2fr_d(f_inf, fp_inf);
    ra_free_temp(fp_inf);
    la_fcmp_cond_d(fcc0_ir2_opnd, st0_opnd, f_inf, FCMP_COND_CEQ);
    la_bcnez(fcc0_ir2_opnd, label_inf);


    /* store st0 onto native stack for further use */
    la_addi_d(sp_ir2_opnd, sp_ir2_opnd, -8);
    la_fst_d(st0_opnd, sp_ir2_opnd, 0);

    IR2_OPND param_0 = ir2_opnd_new(IR2_OPND_FPR, 0);
    IR2_OPND param_1 = ir2_opnd_new(IR2_OPND_FPR, 1);
    IR2_OPND param_8 = ra_alloc_ftemp();
    la_fmov_d(param_8, st0_opnd);

    /* save regs before call helper func */
    tr_save_fcsr_to_env();
    tr_save_registers_to_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                             options_to_save());
#ifdef TARGET_X86_64
    tr_save_x64_8_registers_to_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif

    la_fmov_d(param_0, param_8);
    /* Call the function */
    /* explicitly specify function prototype to select from overloaded
     * prototypes */
    double (*logb_p)(double) = logb;
    tr_gen_call_to_helper((ADDR)logb_p, LOAD_HOST_LOGB);

    IR2_OPND ret_value = ir2_opnd_new(IR2_OPND_FPR, 0);
    la_fmov_d(param_1, ret_value); /* ret val -> pow(2.0,y) */
    /* store the new st0 value to env->st0 */
    //int fpr_index = (0 + lsenv->tr_data->curr_top) & 7;
    /* la_fst_d(ret_value, env_ir2_opnd,
     *                   lsenv_offset_of_fpr(lsenv, fpr_index)); */
	IR2_OPND env_st0_addr_opnd = ra_alloc_itemp();
    la_ld_w(env_st0_addr_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));
    la_slli_w(env_st0_addr_opnd, env_st0_addr_opnd, 4);
    la_add_d(env_st0_addr_opnd, env_st0_addr_opnd, env_ir2_opnd);
    /*la_addi_d(env_st0_addr_opnd,
     * env_st0_addr_opnd, lsenv_offset_of_fpr(lsenv, 0)); */
    la_fst_d(ret_value, env_st0_addr_opnd, lsenv_offset_of_fpr(lsenv, 0));

    IR2_OPND imm2_opnd = ra_alloc_itemp();
    la_addi_d(imm2_opnd, zero_ir2_opnd, 2);
    la_movgr2fr_w(param_0, imm2_opnd);
    la_ffint_d_w(param_0, param_0);
    ra_free_temp(imm2_opnd);

    /* Call the function */
    /* explicitly specify function prototype to select from overloaded
     * prototypes */
    double (*pow_p)(double, double) = pow;
    tr_gen_call_to_helper((ADDR)pow_p, LOAD_HOST_POW);

    IR2_OPND ret_val = ra_alloc_ftemp();
    la_fmov_d(ret_val, ret_value);

    /* restore regs after native call*/
#ifdef TARGET_X86_64
    tr_load_x64_8_registers_from_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    tr_load_registers_from_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                               options_to_save());
    /*
     * TODO: restore fcsr saved before helper to pass glibc logb tests,
     * helper fucntion may set fcsr incorrectly, should inspect later.
     */
    tr_load_fcsr_from_env();

    IR2_OPND origin_st0_value = ra_alloc_ftemp();
    la_fld_d(origin_st0_value, sp_ir2_opnd, 0);
    la_addi_d(sp_ir2_opnd, sp_ir2_opnd, 8);

    la_fdiv_d(origin_st0_value, origin_st0_value, ret_val);
    ra_free_temp(ret_val);
    tr_fpu_push();

    IR2_OPND new_st0_opnd = ra_alloc_st(0);
    la_fmov_d(new_st0_opnd, origin_st0_value);
    ra_free_temp(origin_st0_value);
    IR2_OPND label_exit = ra_alloc_label();
    la_b(label_exit);

    /* special process for 0.0 and -0.0 */
    la_label(label_zero);
    IR2_OPND temp_st0 = ra_alloc_ftemp();
    la_fmov_d(temp_st0, st0_opnd);
    /* the float register stack has been changed, now put -inf to ST1 and */
    /* 0.0(-0.0) to ST0 */
    IR2_OPND inf_opnd = ra_alloc_itemp();
    li_d(inf_opnd, 0xfff0000000000000ull);
    IR2_OPND st1_opnd = ra_alloc_st(1);
    la_movgr2fr_d(st1_opnd, inf_opnd);
    la_fmov_d(new_st0_opnd, temp_st0);
    la_b(label_exit);

    /* special process for inf and -inf */
    la_label(label_inf);
    //IR2_OPND temp_st0 = ra_alloc_ftemp();
    la_fmov_d(temp_st0, st0_opnd);
    /*put ST1 to ST0 and inf to ST1 */
    li_d(inf_opnd, 0x7ff0000000000000ull);
    //IR2_OPND st1_opnd = ra_alloc_st(1);
    la_movgr2fr_d(st1_opnd, inf_opnd);
    la_fmov_d(new_st0_opnd, temp_st0);
    /* la_b(label_exit); */

    la_label(label_exit);
    ra_free_temp(inf_opnd);
    ra_free_temp(temp_st0);
#endif

    return true;
}

bool translate_fyl2x(IR1_INST *pir1)
{
#ifdef USE_FYL2X_HELPER
    tr_gen_call_to_helper1((ADDR)helper_fyl2x, LOAD_HELPER_FYL2X);
#else
     /*
     * For LA $fa 0 is defined as argument and return value reg.
     * To avoid reg destoried, move argument setting after storing
     * regs to env.
      */
    IR2_OPND st0_opnd = ra_alloc_st(0);
    /* dispose the arguments */
    IR2_OPND param_0 = ir2_opnd_new(IR2_OPND_FPR, 0);
    IR2_OPND param_8 = ra_alloc_ftemp();
    la_fmov_d(param_8, st0_opnd);

    /* save regs before call helper func */
    tr_save_registers_to_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                             options_to_save());
#ifdef TARGET_X86_64
    tr_save_x64_8_registers_to_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    la_fmov_d(param_0, param_8);
    /* Call the function  */
    tr_gen_call_to_helper((ADDR)log2, LOAD_HOST_LOG2);

    IR2_OPND ret_opnd, ret_value;
    ret_opnd = ir2_opnd_new(IR2_OPND_FPR, 0);
    ret_value = ra_alloc_ftemp();
    la_fmov_d(ret_value, ret_opnd);

    /* restore regs after native call*/
#ifdef TARGET_X86_64
    tr_load_x64_8_registers_from_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    tr_load_registers_from_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                               options_to_save());

    la_fmov_d(st0_opnd, ret_value);

    IR2_OPND st1_opnd = ra_alloc_st(1);

    la_fmul_d(st1_opnd, st0_opnd, st1_opnd);

    tr_fpu_pop();
#endif

    return true;
}

bool translate_fyl2xp1(IR1_INST *pir1)
{
#ifdef USE_FYL2XP1_HELPER
    tr_gen_call_to_helper1((ADDR)helper_fyl2xp1, LOAD_HELPER_FYL2XP1);
#else
    IR2_OPND st0_opnd = ra_alloc_st(0);

     /*
     * For LA $fa 0 is defined as argument and return value reg.
     * To avoid reg destoried, move argument setting after storing
     * regs to env.
      */
    /* dispose the arguments */
    IR2_OPND param_0 = ir2_opnd_new(IR2_OPND_FPR, 0);
    IR2_OPND param_8 = ra_alloc_ftemp();
    IR2_OPND itemp_1 = ra_alloc_itemp();
    la_ori(itemp_1, zero_ir2_opnd, 1);
    la_movgr2fr_d(param_8, itemp_1);
    la_ffint_d_l(param_8, param_8);
    la_fadd_d(param_8, st0_opnd, param_8);
    ra_free_temp(itemp_1);

    /* save regs before call helper func */
    tr_save_registers_to_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                             options_to_save());
#ifdef TARGET_X86_64
    tr_save_x64_8_registers_to_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif

    la_fmov_d(param_0, param_8);
    /* Call the function  */
    tr_gen_call_to_helper((ADDR)log2, LOAD_HOST_LOG2);

    IR2_OPND ret_opnd, ret_value;
    ret_opnd = ir2_opnd_new(IR2_OPND_FPR, 0);
    ret_value = ra_alloc_ftemp();
    la_fmov_d(ret_value, ret_opnd);

    /* restore regs after native call*/
#ifdef TARGET_X86_64
    tr_load_x64_8_registers_from_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    tr_load_registers_from_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                               options_to_save());

    la_fmov_d(st0_opnd, ret_value);

    IR2_OPND st1_opnd = ra_alloc_st(1);

    la_fmul_d(st1_opnd, st0_opnd, st1_opnd);

    tr_fpu_pop();
#endif

    return true;
}

bool translate_fsincos(IR1_INST *pir1)
{
#ifdef USE_FSINCOS_HELPER
    tr_gen_call_to_helper1((ADDR)helper_fsincos, 1, LOAD_HELPER_FSINCOS);
#else
    const int X86_C2_BITS = 10;
    /* set status_word.c2 = 0 */
    IR2_OPND status_word = ra_alloc_itemp();
    la_ld_hu(status_word, env_ir2_opnd,
                      lsenv_offset_of_status_word(lsenv));
    /* USE LISA_BSTRINS_D to clear bit 10 of status_word */
    la_bstrins_d(status_word,
                          zero_ir2_opnd, X86_C2_BITS, X86_C2_BITS);
    la_st_h(status_word, env_ir2_opnd,
                      lsenv_offset_of_status_word(lsenv));

     /*
     * For LA $fa 0 is defined as argument and return value reg.
     * To avoid reg destoried, move argument setting after storing
     * regs to env.
      */
    /* dispose the arguments */
    IR2_OPND param_1 = ir2_opnd_new(IR2_OPND_FPR, 0);
    IR2_OPND param_8 = ra_alloc_ftemp();
    IR2_OPND st0_opnd = ra_alloc_st(0);
    la_fmov_d(param_8, st0_opnd);

    /* save regs before call helper func */
    tr_save_registers_to_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                             options_to_save());
#ifdef TARGET_X86_64
    tr_save_x64_8_registers_to_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    la_fmov_d(param_1, param_8);


     /*
     * How to transfer float parameter to func.
     * Here is a example on native C code.
      *
     * 1200008a0:   001501a5        move    $r5,$r13
     * 1200008a4:   00150184        move    $r4,$r12
     * 1200008a8:   2bbfa2c0        fld.d   $f0,$r22,-24(0xfe8)
     * 1200008ac:   54003400        bl      52(0x34) # 1200008e0 <__sincos>
     * According to above disassamble code, r4/r5 will be used as paramter.
     * f0 is used as double float parameter reg.
     * So modify parameter2/3 reg defination.
      */

    IR2_OPND top_opnd = a2_ir2_opnd;
    IR2_OPND param_2 = a0_ir2_opnd;
    /* fpreg_base + top * 16 */
    la_ld_w(top_opnd, env_ir2_opnd, lsenv_offset_of_top(lsenv));
    la_slli_w(param_2, top_opnd, 4);
    la_add_d(param_2, param_2, env_ir2_opnd);
    la_addi_d(param_2, param_2, lsenv_offset_of_fpr(lsenv, 0));

    IR2_OPND param_3 = a1_ir2_opnd;
    la_addi_w(top_opnd, top_opnd, -1);
    la_andi(top_opnd, top_opnd, 0x7);
    la_slli_w(param_3, top_opnd, 4);
    la_add_d(param_3, param_3, env_ir2_opnd);
    la_addi_d(param_3, param_3, lsenv_offset_of_fpr(lsenv, 0));

    /* Call the function  */
    tr_gen_call_to_helper((ADDR)sincos, LOAD_HOST_SINCOS);

    /* restore regs after helper call */
#ifdef TARGET_X86_64
    tr_load_x64_8_registers_from_env(GPR_USEDEF_TO_SAVE >> 8, XMM_USEDEF_TO_SAVE >> 8);
#endif
    tr_load_registers_from_env(0, FPR_USEDEF_TO_SAVE, XMM_USEDEF_TO_SAVE,
                               options_to_save());

    tr_fpu_push();
#endif

    return true;
}

bool translate_fxch(IR1_INST *pir1)
{
    IR2_OPND st0_opnd = ra_alloc_st(0);
    int opnd2_index;
    if (ir1_opnd_num(pir1) == 0) {
        opnd2_index = 1;
    } else {
        uint8_t index = 0;
        switch (ir1_get_opnd_num(pir1)) {
        case 1:
            index = 0;
            break;
        case 2:
            index = 1;
            break;
        default:
            lsassert(0);
        }
        opnd2_index = ir1_opnd_base_reg_num(ir1_get_opnd(pir1, index));
    }
    IR2_OPND opnd2 = ra_alloc_st(opnd2_index);
    IR2_OPND tmp_opnd = ra_alloc_ftemp();
    la_fmov_d(tmp_opnd, opnd2);
    la_fmov_d(opnd2, st0_opnd);
    la_fmov_d(st0_opnd, tmp_opnd);
    ra_free_temp(tmp_opnd);
    return true;
}


bool translate_ftst(IR1_INST *pir1)
{
    IR2_OPND status_word = ra_alloc_itemp();
    IR2_OPND tmp_opnd = ra_alloc_itemp();
    /* load status_word */
    int offset = lsenv_offset_of_status_word(lsenv);

    lsassert(offset <= 0x7ff);
    la_ld_hu(status_word, env_ir2_opnd, offset);

    /* clear status_word c0 c2 c3 */
    la_lu12i_w(tmp_opnd, 0xb);
    la_ori(tmp_opnd, tmp_opnd, 0xaff);
    la_and(status_word, status_word, tmp_opnd);

    IR2_OPND f_zero = ra_alloc_ftemp();
    la_movgr2fr_w(f_zero, zero_ir2_opnd);
    la_ffint_d_w(f_zero, f_zero);

    IR2_OPND st0_opnd = ra_alloc_st(0);
    IR2_OPND label_for_lt = ra_alloc_label();
    IR2_OPND label_for_un = ra_alloc_label();
    IR2_OPND label_for_eq = ra_alloc_label();
    IR2_OPND label_for_exit = ra_alloc_label();

    /* check is unordered */
    la_fcmp_cond_d(fcc0_ir2_opnd, st0_opnd, f_zero, FCMP_COND_CUN);
    la_bcnez(fcc0_ir2_opnd, label_for_un);
    /* check is equal */
    la_fcmp_cond_d(fcc0_ir2_opnd, st0_opnd, f_zero, FCMP_COND_CEQ);
    la_bcnez(fcc0_ir2_opnd, label_for_eq);
    /* check is less than */
    la_fcmp_cond_d(fcc0_ir2_opnd, st0_opnd, f_zero, FCMP_COND_CLT);
    la_bcnez(fcc0_ir2_opnd, label_for_lt);

    /* greater than */
    la_b(label_for_exit);
    /* lt: */
    la_label(label_for_lt);
    la_ori(status_word, status_word, 0x100);
    la_b(label_for_exit);
    /* eq: */
    la_label(label_for_eq);
    la_lu12i_w(tmp_opnd, 0x4);
    la_or(status_word, status_word, tmp_opnd);
    la_b(label_for_exit);
    /* un: */
    la_label(label_for_un);
    la_lu12i_w(tmp_opnd, 0x4);
    la_ori(tmp_opnd, tmp_opnd, 0x500);
    la_or(status_word, status_word, tmp_opnd);
    /* exit: */
    la_label(label_for_exit);

    la_st_h(status_word, env_ir2_opnd,
                      lsenv_offset_of_status_word(lsenv));
    ra_free_temp(status_word);
    ra_free_temp(f_zero);
    return true;
}

bool translate_fptan(IR1_INST *pir1)
{
    tr_gen_call_to_helper1((ADDR)helper_fptan, 1, LOAD_HELPER_FPTAN);
    return true;
}

bool translate_fisttp(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND st0_opnd = ra_alloc_st(0);
    IR2_OPND fp_opnd = ra_alloc_ftemp();
    IR2_OPND dest_int = ra_alloc_itemp();
    if (ir1_opnd_size(opnd0) == 32) {
        la_ftintrz_w_d(fp_opnd, st0_opnd);
    } else if (ir1_opnd_size(opnd0) == 64) {
        la_ftintrz_l_d(fp_opnd, st0_opnd);
    } else {
#ifdef CONFIG_LATX_TU
        ra_free_temp(fp_opnd);
        ra_free_temp(dest_int);
        return false;
#endif
        lsassertm(0, "Invalid operand size (%d) in %s.\n",
                    ir1_opnd_size(opnd0), __func__);
    }
    la_movfr2gr_d(dest_int, fp_opnd);
    store_ireg_to_ir1(dest_int, ir1_get_opnd(pir1, 0), false);
    ra_free_temp(fp_opnd);
    ra_free_temp(dest_int);
    tr_fpu_pop();
    return true;
}
bool translate_fsetpm(IR1_INST *pir1)
{
    fprintf(stderr, "%s not implemented. translation failed.\n", __FUNCTION__);
    return false;
}
bool translate_fbld(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
    tr_gen_call_to_helper2((ADDR)helper_fbld_ST0, mem_opnd, 0, LOAD_HELPER_FBLD_ST0);
    /* adjust top */
    tr_fpu_push();
    return true;
}
bool translate_fbstp(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
    tr_gen_call_to_helper2((ADDR)helper_fbst_ST0, mem_opnd, 0, LOAD_HELPER_FBST_ST0);
    /* adjust top */
    tr_fpu_pop();
    return true;
}
/*
 * NOTE:invoke helper is much easier than native insns, there is 512 bytes need to
 * be safed or restored.
 * On the other hand, fxsave/fxrestore is not under hot path so that is acceptable
 * from performance perspective.
 */
bool translate_fxsave(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    IR2_OPND mxcsr_opnd = ra_alloc_itemp();
    int offset = lsenv_offset_of_mxcsr(lsenv);

    lsassert(offset <= 0x7ff);
    la_ld_wu(mxcsr_opnd, env_ir2_opnd, offset);
    if (option_softfpu) {
        IR2_OPND fcsr = ra_alloc_itemp();
        IR2_OPND temp = ra_alloc_itemp();

        la_movfcsr2gr(fcsr, fcsr2_ir2_opnd);
        /* PE */
        la_bstrpick_w(temp, fcsr, 16, 16);
        la_bstrins_w(mxcsr_opnd, temp, 5, 5);
        /* UE */
        la_bstrpick_w(temp, fcsr, 17, 17);
        la_bstrins_w(mxcsr_opnd, temp, 4, 4);
        /* OE */
        la_bstrpick_w(temp, fcsr, 18, 18);
        la_bstrins_w(mxcsr_opnd, temp, 3, 3);
        /* ZE */
        la_bstrpick_w(temp, fcsr, 19, 19);
        la_bstrins_w(mxcsr_opnd, temp, 2, 2);
        /* IE */
        la_bstrpick_w(temp, fcsr, 20, 20);
        la_bstrins_w(mxcsr_opnd, temp, 0, 0);
    }

    /* 2. store  the value of the mxcsr register state to the dest_opnd */
    la_st_w(mxcsr_opnd, mem_opnd, 24);
    ra_free_temp(mxcsr_opnd);

    tr_gen_call_to_helper2((ADDR)helper_fxsave, mem_opnd, 0, LOAD_HELPER_FXSAVE);
    return true;
}
bool translate_fxrstor(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    tr_gen_call_to_helper2((ADDR)helper_fxrstor, mem_opnd, 0, LOAD_HELPER_FXRSTOR);

    return true;
}
