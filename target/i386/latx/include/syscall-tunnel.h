/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef _SYSCALL_TUNNEL_H_
#define _SYSCALL_TUNNEL_H_

#include "latx-types.h"
#include "ir1.h"

bool syscall_is_optimized(int64_t sys_num);
extern const bool syscall_optimize_confirm[];
bool translate_int_syscall(IR1_INST *pir1);

#endif
