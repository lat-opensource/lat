/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "profile.h"

#ifdef CONFIG_LATX_PROFILER
#include "lsenv.h"

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

#ifdef CONFIG_LATX_AOT
    if (option_aot && in_pre_translate) {
        TranslationBlock *current_tb = lsenv->tr_data->curr_tb;

        /* All callers count a field of the TB currently being translated.
         * Persist its relocatable base, never the pretranslation allocation.
         * LOAD_TB_ADDR does not apply an addend: add the field offset here.
         */
        lsassert(current_tb != NULL);
        lsassert(area == (void *)&current_tb->profile.exec_times ||
                 area == (void *)&current_tb->profile.jrra_in ||
                 area == (void *)&current_tb->profile.jrra_miss);
        uintptr_t field_offset = (uintptr_t)area - (uintptr_t)current_tb;
        lsassert(((uintptr_t)area & 7) == 0);
        aot_load_host_addr(temp0, (ADDR)current_tb, LOAD_TB_ADDR, 0);
        li_d(temp1, field_offset);
        la_add_d(temp0, temp0, temp1);
        lower = 0;
    } else
#endif
    {
        /* Keep the ordinary JIT counter sequence unchanged. */
        li_d(temp0, upper);
    }
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
