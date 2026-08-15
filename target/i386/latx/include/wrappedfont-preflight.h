/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_WRAPPEDFONT_PREFLIGHT_H
#define LATX_WRAPPEDFONT_PREFLIGHT_H

#include <stdbool.h>
#include <stddef.h>

#include "pathcoll.h"

bool latx_font_preflight_guest(path_collection_t *guest_paths,
                               char *reason, size_t reason_size);
int latx_font_preflight_or_disable(path_collection_t *guest_paths,
                                   const char *requesting_soname);

#endif /* LATX_WRAPPEDFONT_PREFLIGHT_H */
