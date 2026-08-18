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
#include "wrappedlib-preflight.h"
#include "wrappedtext-preflight.h"

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
static const char *const fribidi_supported_symbols[] = {
#include "wrappedfribidi_private.h"
};
static const char *const fribidi_required_host_symbols[] = {
    "fribidi_unicode_version",
    "fribidi_version_info",
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

static bool fribidi_symbol_filter(const char *name)
{
    return !strncmp(name, "fribidi_", 8);
}

bool latx_text_preflight_guest(path_collection_t *guest_paths,
                               char *reason, size_t reason_size)
{
    char *guest_path;
    bool safe;

    if (!guest_paths || !reason || !reason_size) {
        return false;
    }
    reason[0] = '\0';
    guest_path = ResolveFile("libfribidi.so.0", guest_paths);
    safe = latx_wrappedlib_preflight_guest(
        guest_path, "libfribidi.so.0", "FriBidi", fribidi_symbol_filter,
        fribidi_supported_symbols, ARRAY_SIZE(fribidi_supported_symbols),
        fribidi_required_host_symbols,
        ARRAY_SIZE(fribidi_required_host_symbols), NULL, 0, NULL,
        reason, reason_size);
    box_free(guest_path);
    return safe;
}

int latx_text_preflight_or_disable(path_collection_t *guest_paths,
                                   const char *requesting_soname)
{
    char reason[256];

    if (latx_text_preflight_guest(guest_paths, reason, sizeof(reason))) {
        return 0;
    }
    kzt_groups_log_wrapper_rejection(requesting_soname, reason);
    kzt_group_disable(KZT_GROUP_TEXT, reason);
    return -1;
}
