/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_WRAPPEDLIB_PREFLIGHT_H
#define LATX_WRAPPEDLIB_PREFLIGHT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*LatxWrappedSymbolFilter)(const char *name);

/* Records guest exports only after the matching host symbol is resolved. */
typedef struct LatxWrappedObservedSymbol {
    const char *name;
    uint64_t bit;
} LatxWrappedObservedSymbol;

bool latx_wrappedlib_preflight_guest(
    const char *guest_path, const char *host_soname,
    const char *library_label, LatxWrappedSymbolFilter symbol_filter,
    const char *const *supported_symbols, size_t supported_symbol_count,
    const char *const *required_host_symbols,
    size_t required_host_symbol_count,
    const LatxWrappedObservedSymbol *observed_symbols,
    size_t observed_symbol_count, uint64_t *observed_symbol_mask,
    char *reason, size_t reason_size);

#endif /* LATX_WRAPPEDLIB_PREFLIGHT_H */
