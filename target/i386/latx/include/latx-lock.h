/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_LOCK_H
#define LATX_LOCK_H

#include "qemu/typedefs.h"

void latx_i386_unlock_owned_lock(CPUState *cpu);

#endif /* LATX_LOCK_H */
