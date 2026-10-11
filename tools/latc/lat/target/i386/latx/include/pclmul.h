/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef _PCLMUL_H_
#define _PCLMUL_H_

#include "common.h"
#include "ir2.h"
#include "la-append.h"
#include "macro-inst.h"
#include "reg-alloc.h"

static inline void emit_pclmul_ctz_loop(IR2_OPND lhs, IR2_OPND rhs,
                                        IR2_OPND res_lo, IR2_OPND res_hi)
{
    IR2_OPND shift = ra_alloc_itemp();
    IR2_OPND tmp = ra_alloc_itemp();
    IR2_OPND loop_label = ra_alloc_label();
    IR2_OPND end_label = ra_alloc_label();

    la_xor(res_lo, res_lo, res_lo);
    la_xor(res_hi, res_hi, res_hi);
    la_beqz(lhs, end_label);

    la_label(loop_label);
    la_ctz_d(shift, lhs);
    la_addi_d(tmp, lhs, -1);
    la_and(lhs, lhs, tmp);
    la_sll_d(tmp, rhs, shift);
    la_xor(res_lo, res_lo, tmp);
    li_d(tmp, 64);
    la_sub_d(tmp, tmp, shift);
    la_srl_d(tmp, rhs, tmp);
    la_sltu(shift, zero_ir2_opnd, shift);
    la_sub_d(shift, zero_ir2_opnd, shift);
    la_and(tmp, tmp, shift);
    la_xor(res_hi, res_hi, tmp);
    la_bnez(lhs, loop_label);

    la_label(end_label);
    ra_free_temp(shift);
    ra_free_temp(tmp);
}

#endif
