/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_KZT_RUNTIME_H
#define LATX_KZT_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

/*
 * option_kzt is the process-level 0/1/2 mode after loader policy, including
 * direct-exec and Wine deferral.  kzt_effective_groups is the library
 * selection after dependency and compatibility pruning.  Keeping these as
 * separate facts makes every execution-time KZT hook follow both decisions.
 *
 * Group mutations are confined to kzt-groups.c and finish before guest code
 * starts, so the hot execution paths can read the mask without another
 * cached "active" flag.
 */
extern int option_kzt;
extern uint32_t kzt_effective_groups;

static inline bool latx_kzt_runtime_enabled(void)
{
    return option_kzt != 0 && kzt_effective_groups != 0;
}

#endif /* LATX_KZT_RUNTIME_H */
