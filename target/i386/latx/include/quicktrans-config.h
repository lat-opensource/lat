/*
 * Quicktrans runtime policy queries. Callers do not need build-time guards.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#ifndef LATX_QUICKTRANS_CONFIG_H
#define LATX_QUICKTRANS_CONFIG_H

#include <stdbool.h>

#if defined(CONFIG_LATX_FAST_TRANSLATOR) && defined(TARGET_X86_64)
bool latx_quicktrans_enabled(void);
bool latx_quicktrans_compact_tb_enabled(void);
#else
static inline bool latx_quicktrans_enabled(void)
{
    return false;
}

static inline bool latx_quicktrans_compact_tb_enabled(void)
{
    return false;
}
#endif

#endif /* LATX_QUICKTRANS_CONFIG_H */
