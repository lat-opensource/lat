/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_TR_VPAES_H
#define LATX_TR_VPAES_H

#include "ir1.h"
#include "la-ir2.h"

bool latx_translate_aesenc_vpaes(IR1_INST *pir1);
bool latx_translate_aesenclast_vpaes(IR1_INST *pir1);
bool latx_translate_aesdec_vpaes(IR1_INST *pir1);
bool latx_translate_aesdeclast_vpaes(IR1_INST *pir1);
bool latx_translate_aesimc_vpaes(IR1_INST *pir1);
bool latx_translate_aeskeygenassist_vpaes(IR1_INST *pir1);

bool latx_translate_vaesenc_vpaes(IR1_INST *pir1);
bool latx_translate_vaesenclast_vpaes(IR1_INST *pir1);
bool latx_translate_vaesdec_vpaes(IR1_INST *pir1);
bool latx_translate_vaesdeclast_vpaes(IR1_INST *pir1);
bool latx_translate_vaesimc_vpaes(IR1_INST *pir1);
bool latx_translate_vaeskeygenassist_vpaes(IR1_INST *pir1);

const void *latx_vpaes_get_table_addr(int kind);
extern const uint8 latx_vpaes_enc_tables[9][16];
extern const uint8 latx_vpaes_dec_tables[9][16];
extern const uint8 latx_vpaes_keygen_tables[8][16];
extern const uint8 latx_vpaes_enc_tables_xv[9][32];
extern const uint8 latx_vpaes_dec_tables_xv[9][32];

#endif
