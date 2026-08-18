/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_KZT_GROUP_LIST_H
#define LATX_KZT_GROUP_LIST_H

/*
 * KZT library-group registry.
 *
 * This is the single source of truth for group names, bit positions,
 * stability, and dependencies.  Keep concrete shared-library assignments in
 * library_list.h.
 *
 * KZT_GROUP_LIST(X) entries are:
 *
 *     X(identifier, name, bit, STABLE|EXPERIMENTAL, dependencies)
 */
#define KZT_GROUP_LIST(X)                                                \
    X(CORE,   "core",   0, STABLE,      KZT_GROUP_NONE)                 \
    X(X11,    "x11",    1, STABLE,      KZT_GROUP_CORE)                 \
    X(GL,     "gl",     2, STABLE,      KZT_GROUP_CORE | KZT_GROUP_X11) \
    X(VULKAN, "vulkan", 3, STABLE,      KZT_GROUP_CORE | KZT_GROUP_X11) \
    X(VAAPI,  "vaapi",  4, STABLE,      KZT_GROUP_CORE | KZT_GROUP_X11) \
    X(FONT,   "font",   6, EXPERIMENTAL, KZT_GROUP_CORE | KZT_GROUP_X11) \
    X(INPUT,  "input",  7, EXPERIMENTAL, KZT_GROUP_CORE | KZT_GROUP_X11) \
    X(TEXT,   "text",   8, EXPERIMENTAL, KZT_GROUP_CORE)                 \
    X(CAIRO,  "cairo",  5, EXPERIMENTAL, KZT_GROUP_FONT)

#endif /* LATX_KZT_GROUP_LIST_H */
