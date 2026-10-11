/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef _QEMU_DEF_H_
#define _QEMU_DEF_H_

#include "qemu/osdep.h"
#include "qemu/host-utils.h"
#include "cpu.h"
#include "disas/disas.h"
#include "exec/exec-all.h"
#include "tcg/tcg-op.h"
#include "exec/cpu_ldst.h"
#include "exec/helper-proto.h"
#include "exec/helper-gen.h"
#include "trace-tcg.h"
#include "exec/log.h"
#include "qemu/qht.h"

/*#include "exec/gen-icount.h"*/

typedef struct TranslationBlock TB;
typedef struct qht QHT;

#endif
