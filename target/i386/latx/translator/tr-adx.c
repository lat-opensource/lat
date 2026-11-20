/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "flag-lbt.h"
#include "translate.h"

/* ADCX updates CF; ADOX updates OF. Preserve the other flags and keep
 * carry input live until the arithmetic result has consumed it. */
static bool translate_adx_soft(IR1_INST *pir1, bool overflow)
{
    IR2_OPND src = load_ireg_from_ir1(ir1_get_opnd(pir1, 1),
                                      ZERO_EXTENSION, false);
    IR2_OPND dest = load_ireg_from_ir1(ir1_get_opnd(pir1, 0),
                                       ZERO_EXTENSION, false);
    IR2_OPND carry_in = ra_alloc_itemp();
    IR2_OPND result = ra_alloc_itemp();
    IR2_OPND carry_out = ra_alloc_itemp();
    int size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    int bit = overflow ? OF_BIT_INDEX : CF_BIT_INDEX;
    int mask = overflow ? OF_USEDEF_BIT : CF_USEDEF_BIT;

    latx_read_eflags(carry_in, mask);
    la_bstrpick_d(carry_in, carry_in, bit, bit);
    la_add_d(result, dest, src);
    if (size == 32) {
        la_add_d(result, result, carry_in);
        la_srli_d(carry_out, result, 32);
        la_bstrpick_d(result, result, 31, 0);
    } else {
        la_sltu(carry_out, result, dest);
        la_add_d(result, result, carry_in);
        la_sltu(carry_in, result, carry_in);
        la_or(carry_out, carry_out, carry_in);
    }
    ra_free_temp(carry_in);
    if (overflow) {
        la_slli_d(carry_out, carry_out, OF_BIT_INDEX);
    }
    latx_write_eflags(carry_out, mask);
    store_ireg_to_ir1(result, ir1_get_opnd(pir1, 0), false);
    return true;
}

bool translate_adcx(IR1_INST *pir1)
{
    if (!option_enable_lbt) {
        return translate_adx_soft(pir1, false);
    }
    IR2_OPND src_opnd = load_ireg_from_ir1(ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND dest_opnd = load_ireg_from_ir1(ir1_get_opnd(pir1, 0), ZERO_EXTENSION, false);
    IR2_OPND flag_opnd = ra_alloc_itemp();
    IR2_OPND cflag_opnd = ra_alloc_itemp();
    IR2_OPND temp1_opnd = ra_alloc_itemp();

    latx_read_eflags(flag_opnd, 0x3f);
    la_bstrpick_d(cflag_opnd, flag_opnd, 0, 0);

    if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32) {
        la_add_w(temp1_opnd, dest_opnd, src_opnd);
        la_add_w(temp1_opnd, temp1_opnd, cflag_opnd);
        la_x86adc_w(dest_opnd, src_opnd);
    } else {
        la_add_d(temp1_opnd, dest_opnd, src_opnd);
        la_add_d(temp1_opnd, temp1_opnd, cflag_opnd);
        la_x86adc_d(dest_opnd, src_opnd);
    }
    latx_write_eflags(flag_opnd, 0x3e); //cf is not recovered

    store_ireg_to_ir1(temp1_opnd, ir1_get_opnd(pir1, 0), false);
    return true;
}

bool translate_adox(IR1_INST *pir1)
{
    if (!option_enable_lbt) {
        return translate_adx_soft(pir1, true);
    }
    IR2_OPND src_opnd = load_ireg_from_ir1(ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND dest_opnd = load_ireg_from_ir1(ir1_get_opnd(pir1, 0), ZERO_EXTENSION, false);
    IR2_OPND flag_opnd = ra_alloc_itemp();
    IR2_OPND oflag_opnd = ra_alloc_itemp();
    IR2_OPND temp1_opnd = ra_alloc_itemp();

    /* save eflags and set of -> cf */
    latx_read_eflags(flag_opnd, 0x3f);
    la_bstrpick_d(oflag_opnd, flag_opnd, 11, 11);
    latx_write_eflags(oflag_opnd, 0x1);

    if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32) {
        la_add_w(temp1_opnd, dest_opnd, src_opnd);
        la_add_w(temp1_opnd, temp1_opnd, oflag_opnd);
        la_x86adc_w(dest_opnd, src_opnd);
    } else {
        la_add_d(temp1_opnd, dest_opnd, src_opnd);
        la_add_d(temp1_opnd, temp1_opnd, oflag_opnd);
        la_x86adc_d(dest_opnd, src_opnd);
    }
    /* set cf -> of */
    latx_read_eflags(oflag_opnd, 0x1);
    la_bstrins_d(flag_opnd, oflag_opnd, 11, 11);
    latx_write_eflags(flag_opnd, 0x3f);

    store_ireg_to_ir1(temp1_opnd, ir1_get_opnd(pir1, 0), false);
    return true;
}
