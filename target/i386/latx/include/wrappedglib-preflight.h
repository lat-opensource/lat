/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_WRAPPEDGLIB_PREFLIGHT_H
#define LATX_WRAPPEDGLIB_PREFLIGHT_H

#include "pathcoll.h"

int latx_glib_leaf_preflight_or_disable(path_collection_t *guest_paths,
                                        const char *requesting_soname);

#endif /* LATX_WRAPPEDGLIB_PREFLIGHT_H */
