/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include <string.h>

#include "debug.h"
#include "fileutils.h"
#include "kzt-groups.h"
#include "wrappedglib-preflight.h"
#include "wrappedlib-preflight.h"

#define GO(name, signature) #name,
#define GOM(name, signature) #name,
#define GOW(name, signature) #name,
#define GOWM(name, signature) #name,
#define GO2(name, signature, alias) #name,
#define GOS(name, signature) #name,
#define DATA(name, size)
#define DATAV(name, size)
#define DATAB(name, size)
#define DATAM(name, size)
static const char *const glib_leaf_supported_symbols[] = {
#include "wrappedglib2_private.h"
};
#undef GO
#undef GOM
#undef GOW
#undef GOWM
#undef GO2
#undef GOS
#undef DATA
#undef DATAV
#undef DATAB
#undef DATAM

static bool glib_leaf_symbol_filter(const char *name)
{
    for (size_t i = 0; i < ARRAY_SIZE(glib_leaf_supported_symbols); i++) {
        if (!strcmp(name, glib_leaf_supported_symbols[i])) {
            return true;
        }
    }
    return false;
}

int latx_glib_leaf_preflight_or_disable(path_collection_t *guest_paths,
                                        const char *requesting_soname)
{
    char *guest_path;
    char reason[256];
    bool safe;

    guest_path = ResolveFile("libglib-2.0.so.0", guest_paths);
    safe = latx_wrappedlib_preflight_guest(
        guest_path, "libglib-2.0.so.0", "GLib leaf ABI",
        glib_leaf_symbol_filter, glib_leaf_supported_symbols,
        ARRAY_SIZE(glib_leaf_supported_symbols), NULL, 0, NULL, 0, NULL,
        reason, sizeof(reason));
    box_free(guest_path);
    if (safe) {
        return 0;
    }
    kzt_groups_log_wrapper_rejection(requesting_soname, reason);
    kzt_group_disable(KZT_GROUP_GLIB, reason);
    return -1;
}
