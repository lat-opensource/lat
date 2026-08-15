/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_WRAPPEDLIB_PREFLIGHT_H
#define LATX_WRAPPEDLIB_PREFLIGHT_H

#include <stdbool.h>
#include <stddef.h>

typedef bool (*LatxWrappedSymbolFilter)(const char *name);

bool latx_wrappedlib_preflight_guest(
    const char *guest_path, const char *host_soname,
    const char *library_label, LatxWrappedSymbolFilter symbol_filter,
    const char *const *supported_symbols, size_t supported_symbol_count,
    const char *const *required_host_symbols,
    size_t required_host_symbol_count,
    char *reason, size_t reason_size);

#endif /* LATX_WRAPPEDLIB_PREFLIGHT_H */
