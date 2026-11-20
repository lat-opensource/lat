/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/* Shared access to x86 flags and x87 TOP. */
#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"
#include "flag-lbt.h"

void latx_write_eflags(IR2_OPND value, uint8_t mask)
{
    la_x86mtflag(value, mask);
}

void latx_read_eflags(IR2_OPND value, uint8_t mask)
{
    la_x86mfflag(value, mask);
}

void latx_set_eflag_condition(IR2_OPND dest, int condition)
{
    la_setx86j(dest, condition);
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
