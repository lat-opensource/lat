/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_DISASSEMBLE_LACAPSTONE_H
#define LATX_DISASSEMBLE_LACAPSTONE_H
#include "latx-disassemble-trace.h"
#include "../capStone/include/capstone/capstone.h"

extern csh handle;
struct la_dt_insn *lacapstone_get_from_insn(cs_insn *inputinfo,
        int ir1_num, void *pir1_base);
cs_insn* lacapstone_post(struct la_dt_insn* inputinfo,int ir1_num, void *pir1_base);

#endif
