/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "profile.h"

#ifdef CONFIG_LATX_PROFILER
inline void per_tb_count(void *area, int inc)
{
    la_profile_begin();
    /* the function can count TB run times, although all of tb have linked */
    IR2_OPND temp0 = ra_alloc_itemp();
    IR2_OPND temp1 = ra_alloc_itemp();
    IR2_OPND label_ll_d = ra_alloc_label();

    ptrdiff_t addr = (ptrdiff_t)area;
    lsassert((addr & 0b11) == 0);
    ptrdiff_t upper, lower;
    lower = (int16_t)addr;
    upper = addr - lower;

    li_d(temp0, upper);
    la_label(label_ll_d);
    la_ll_d(temp1, temp0, lower);
    la_addi_d(temp1, temp1, inc);
    la_sc_d(temp1, temp0, lower);
    la_beq(temp1, zero_ir2_opnd, label_ll_d);
    ra_free_temp(temp0);
    ra_free_temp(temp1);
    la_profile_end();
}
#endif
