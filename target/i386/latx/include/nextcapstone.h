/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_DISASSEMBLE_FROM_NEXTCAPSTONE_H
#define LATX_DISASSEMBLE_FROM_NEXTCAPSTONE_H
#include "latx-disassemble-trace.h"
#include "../capStone_Next/include/capstone/capstone.h"

extern csh handle;
struct la_dt_insn *nextcapstone_get_from_insn(cs_insn *inputinfo,
        int ir1_num, void *pir1_base);
cs_insn *nextcapstone_post(struct la_dt_insn *inputinfo,
        int ir1_num, void *pir1_base);

#endif
