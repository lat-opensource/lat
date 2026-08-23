/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_LOCK_LOWERING_H
#define LATX_LOCK_LOWERING_H

/*
 * x86 permits locked operands that cross an 8-byte host atomic boundary,
 * but LoongArch LL/SC and AMO instructions require natural alignment.  The
 * i386 LL/SC fallback handles that fault by relocating the guest host page,
 * which makes guest-address syscalls observe a transient invalid address.
 *
 * The i386 translators use LATX-owned aligned storage to serialize ordinary
 * loads and stores.  i386 selects one process-wide lock: a hashed lock is
 * not sufficient because an x86 locked operand can overlap adjacent hash
 * stripes.  This keeps the guest mapping at its original host address.
 * x86-64 retains the existing LL/SC lowering.
 */
#if !defined(TARGET_X86_64)
#undef CONFIG_LATX_LLSC
#endif

#endif /* LATX_LOCK_LOWERING_H */
