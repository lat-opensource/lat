#include "common.h"
#include "reg-alloc.h"
#include "flag-lbt-bd.h"
#include "translate-bd.h"

static bool translate_fcmovcc_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR2_OPND cond = ra_alloc_itemp();
    IR2_OPND label_exit = ra_alloc_label();

    get_eflag_condition_bd(&cond, pir1);

    la_beq(cond, zero_ir2_opnd, label_exit);

    IR2_OPND dst_opnd = ra_alloc_st(0);
    IR2_OPND src_opnd = load_freg_from_ir1_1_bd(opnd1, false, true);
    la_fmov_d(dst_opnd, src_opnd);

    la_label(label_exit);

    ra_free_temp(cond);

    return true;
}

bool translate_fcmovb_bd(IR1_INST *pir1)
{
    return translate_fcmovcc_bd(pir1);
}

bool translate_fcmovbe_bd(IR1_INST *pir1)
{
    return translate_fcmovcc_bd(pir1);
}

bool translate_fcmove_bd(IR1_INST *pir1)
{
    return translate_fcmovcc_bd(pir1);
}

bool translate_fcmovnb_bd(IR1_INST *pir1)
{
    return translate_fcmovcc_bd(pir1);
}

bool translate_fcmovnbe_bd(IR1_INST *pir1)
{
    return translate_fcmovcc_bd(pir1);
}

bool translate_fcmovne_bd(IR1_INST *pir1)
{
    return translate_fcmovcc_bd(pir1);
}

bool translate_fcmovnu_bd(IR1_INST *pir1)
{
    return translate_fcmovcc_bd(pir1);
}

bool translate_fcmovu_bd(IR1_INST *pir1)
{
    return translate_fcmovcc_bd(pir1);
}