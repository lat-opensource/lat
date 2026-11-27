/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_FLAG_STATE_H
#define LATX_FLAG_STATE_H

#include "ir2.h"

/* Emit x86 state operations using the selected hardware/software backend. */
void latx_write_eflags(IR2_OPND value, uint8_t mask);
void latx_read_eflags(IR2_OPND value, uint8_t mask);
void latx_set_eflag_condition(IR2_OPND dest, int condition);
void latx_write_top(IR2_OPND value);
void latx_write_top_const(int value);
void latx_read_top(IR2_OPND value);
void latx_inc_top(void);
void latx_dec_top(void);

#endif
