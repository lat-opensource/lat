/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/* x86 flags and x87 TOP: LBT hardware or ordinary registers and memory. */
#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"
#include "flag-lbt.h"

static uint16_t usedef_to_eflags_mask(uint8_t mask)
{
    uint16_t eflags_mask = 0;

    eflags_mask |= (mask & CF_USEDEF_BIT) ? CF_BIT : 0;
    eflags_mask |= (mask & PF_USEDEF_BIT) ? PF_BIT : 0;
    eflags_mask |= (mask & AF_USEDEF_BIT) ? AF_BIT : 0;
    eflags_mask |= (mask & ZF_USEDEF_BIT) ? ZF_BIT : 0;
    eflags_mask |= (mask & SF_USEDEF_BIT) ? SF_BIT : 0;
    eflags_mask |= (mask & OF_USEDEF_BIT) ? OF_BIT : 0;
    return eflags_mask;
}

void latx_write_eflags(IR2_OPND value, uint8_t mask)
{
    IR2_OPND eflags;
    IR2_OPND selected;
    uint16_t eflags_mask;

    if (option_enable_lbt) {
        la_x86mtflag(value, mask);
        return;
    }

    eflags_mask = usedef_to_eflags_mask(mask);
    eflags = ra_alloc_eflags();
    selected = ra_alloc_itemp();
    /* Preserve value before clearing bits: it may alias the flags register. */
    la_andi(selected, value, eflags_mask);
    if (eflags_mask & CF_BIT) {
        la_bstrins_d(eflags, zero_ir2_opnd, CF_BIT_INDEX, CF_BIT_INDEX);
    }
    if (eflags_mask & PF_BIT) {
        la_bstrins_d(eflags, zero_ir2_opnd, PF_BIT_INDEX, PF_BIT_INDEX);
    }
    if (eflags_mask & AF_BIT) {
        la_bstrins_d(eflags, zero_ir2_opnd, AF_BIT_INDEX, AF_BIT_INDEX);
    }
    if (eflags_mask & ZF_BIT) {
        la_bstrins_d(eflags, zero_ir2_opnd, ZF_BIT_INDEX, ZF_BIT_INDEX);
    }
    if (eflags_mask & SF_BIT) {
        la_bstrins_d(eflags, zero_ir2_opnd, SF_BIT_INDEX, SF_BIT_INDEX);
    }
    if (eflags_mask & OF_BIT) {
        la_bstrins_d(eflags, zero_ir2_opnd, OF_BIT_INDEX, OF_BIT_INDEX);
    }
    la_or(eflags, eflags, selected);
    ra_free_temp(selected);
}

void latx_read_eflags(IR2_OPND value, uint8_t mask)
{
    if (option_enable_lbt) {
        la_x86mfflag(value, mask);
        return;
    }

    la_andi(value, ra_alloc_eflags(), usedef_to_eflags_mask(mask));
}

static void soft_eflag_bit(IR2_OPND dest, int bit)
{
    la_bstrpick_d(dest, ra_alloc_eflags(), bit, bit);
}

void latx_set_eflag_condition(IR2_OPND dest, int condition)
{
    IR2_OPND lhs;
    IR2_OPND rhs;

    if (option_enable_lbt) {
        la_setx86j(dest, condition);
        return;
    }

    lhs = ra_alloc_itemp();
    rhs = ra_alloc_itemp();
    switch (condition) {
    case COND_AE:
        soft_eflag_bit(dest, CF_BIT_INDEX);
        la_xori(dest, dest, 1);
        break;
    case COND_B:
        soft_eflag_bit(dest, CF_BIT_INDEX);
        break;
    case COND_PO:
        soft_eflag_bit(dest, PF_BIT_INDEX);
        la_xori(dest, dest, 1);
        break;
    case COND_PE:
        soft_eflag_bit(dest, PF_BIT_INDEX);
        break;
    case COND_E:
        soft_eflag_bit(dest, ZF_BIT_INDEX);
        break;
    case COND_NE:
        soft_eflag_bit(dest, ZF_BIT_INDEX);
        la_xori(dest, dest, 1);
        break;
    case COND_S:
        soft_eflag_bit(dest, SF_BIT_INDEX);
        break;
    case COND_NS:
        soft_eflag_bit(dest, SF_BIT_INDEX);
        la_xori(dest, dest, 1);
        break;
    case COND_O:
        soft_eflag_bit(dest, OF_BIT_INDEX);
        break;
    case COND_NO:
        soft_eflag_bit(dest, OF_BIT_INDEX);
        la_xori(dest, dest, 1);
        break;
    case COND_BE:
    case COND_A:
        soft_eflag_bit(lhs, CF_BIT_INDEX);
        soft_eflag_bit(rhs, ZF_BIT_INDEX);
        la_or(dest, lhs, rhs);
        if (condition == COND_A) {
            la_xori(dest, dest, 1);
        }
        break;
    case COND_L:
    case COND_GE:
        soft_eflag_bit(lhs, SF_BIT_INDEX);
        soft_eflag_bit(rhs, OF_BIT_INDEX);
        la_xor(dest, lhs, rhs);
        if (condition == COND_GE) {
            la_xori(dest, dest, 1);
        }
        break;
    case COND_LE:
    case COND_G:
        soft_eflag_bit(lhs, SF_BIT_INDEX);
        soft_eflag_bit(rhs, OF_BIT_INDEX);
        la_xor(lhs, lhs, rhs);
        soft_eflag_bit(rhs, ZF_BIT_INDEX);
        la_or(dest, lhs, rhs);
        if (condition == COND_G) {
            la_xori(dest, dest, 1);
        }
        break;
    default:
        lsassertm(0, "unsupported software eflags condition %d\n", condition);
    }
    ra_free_temp(lhs);
    ra_free_temp(rhs);
}

void latx_write_top_const(int value)
{
    la_x86mttop(value);
}

void latx_read_top(IR2_OPND value)
{
    la_x86mftop(value);
}

void latx_inc_top(void)
{
    la_x86inctop();
}

void latx_dec_top(void)
{
    la_x86dectop();
}
