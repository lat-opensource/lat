/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_EFLAG_INTERNAL_H
#define LATX_EFLAG_INTERNAL_H

/* Shared per-flag fallbacks used by both EFLAGS backends. */
void generate_cf(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1, IR1_INST *pir1);
void generate_pf(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1);
void generate_af(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1, IR1_INST *pir1);
void generate_zf(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1, IR1_INST *pir1);
void generate_sf(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1);
void generate_of(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1, IR1_INST *pir1);
void generate_eflags_soft(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1,
                          IR1_INST *pir1);

#endif
