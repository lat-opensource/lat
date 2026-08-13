/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_WRAPPEDCAIRO_PREFLIGHT_H
#define LATX_WRAPPEDCAIRO_PREFLIGHT_H

#include <stdbool.h>
#include <stddef.h>

bool latx_cairo_preflight_guest(const char *guest_path,
                                const char *host_soname,
                                char *reason, size_t reason_size);

#endif /* LATX_WRAPPEDCAIRO_PREFLIGHT_H */
