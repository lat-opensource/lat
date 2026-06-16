/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "flag-lbt.h"
#include "translate.h"

bool translate_fcmovcc(IR1_INST *pir1)
{
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND cond = ra_alloc_itemp();
    IR2_OPND label_exit = ra_alloc_label();

    get_eflag_condition(&cond, pir1);

    la_beq(cond, zero_ir2_opnd, label_exit);

    IR2_OPND dst_opnd = ra_alloc_st(0);
    IR2_OPND src_opnd = load_freg_from_ir1_1(opnd1, false, true);
    la_fmov_d(dst_opnd, src_opnd);

    la_label(label_exit);

    ra_free_temp(cond);

    return true;
}
