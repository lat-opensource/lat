/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_UNWIND_H
#define LATX_UNWIND_H

/* Layout of the second per-instruction search-table word, also on i386. */
#define LATX_UNWIND_CC_OP_MASK 0xff
#define LATX_UNWIND_INSTPTN_SHIFT 8
#define LATX_UNWIND_INSTPTN_MASK 0xff
#define LATX_UNWIND_YMMH_SHIFT 16

/* Delayed integer flags use the saved host a0/a1, never temporary IR1. */
enum {
    INSTPTN_FLAGS_NONE,
    INSTPTN_FLAGS_CMP,
    INSTPTN_FLAGS_TEST,
    INSTPTN_FLAGS_BT,
};

#define INSTPTN_FLAGS_OP_MASK 3
#define INSTPTN_FLAGS_SIZE_SHIFT 2
#define INSTPTN_FLAGS_SIZE_MASK 3
#define INSTPTN_FLAGS_SAME_REG (1 << 4)

#endif
