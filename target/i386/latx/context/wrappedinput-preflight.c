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
#include "wrappedinput-preflight.h"
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
static const char *const xkbcommon_supported_symbols[] = {
#include "wrappedxkbcommon_private.h"
};
static const char *const xkbcommon_x11_supported_symbols[] = {
#include "wrappedxkbcommonx11_private.h"
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

typedef struct InputLibraryPreflight {
    const char *guest_soname;
    const char *host_soname;
    const char *label;
    LatxWrappedSymbolFilter symbol_filter;
    const char *const *supported_symbols;
    size_t supported_symbol_count;
} InputLibraryPreflight;

static bool xkbcommon_symbol_filter(const char *name)
{
    return !strncmp(name, "xkb_", 4) && strncmp(name, "xkb_x11_", 8);
}

static bool xkbcommon_x11_symbol_filter(const char *name)
{
    return !strncmp(name, "xkb_x11_", 8);
}

bool latx_input_preflight_guest(path_collection_t *guest_paths,
                                char *reason, size_t reason_size)
{
    static const InputLibraryPreflight libraries[] = {
        {
            .guest_soname = "libxkbcommon.so.0",
            .host_soname = "libxkbcommon.so.0",
            .label = "xkbcommon",
            .symbol_filter = xkbcommon_symbol_filter,
            .supported_symbols = xkbcommon_supported_symbols,
            .supported_symbol_count =
                ARRAY_SIZE(xkbcommon_supported_symbols),
        },
        {
            .guest_soname = "libxkbcommon-x11.so.0",
            .host_soname = "libxkbcommon-x11.so.0",
            .label = "xkbcommon-x11",
            .symbol_filter = xkbcommon_x11_symbol_filter,
            .supported_symbols = xkbcommon_x11_supported_symbols,
            .supported_symbol_count =
                ARRAY_SIZE(xkbcommon_x11_supported_symbols),
        },
    };

    if (!guest_paths || !reason || !reason_size) {
        return false;
    }
    reason[0] = '\0';
    for (size_t i = 0; i < ARRAY_SIZE(libraries); i++) {
        const InputLibraryPreflight *library = &libraries[i];
        char *guest_path = ResolveFile(library->guest_soname, guest_paths);
        bool safe = latx_wrappedlib_preflight_guest(
            guest_path, library->host_soname, library->label,
            library->symbol_filter, library->supported_symbols,
            library->supported_symbol_count, NULL, 0, NULL, 0, NULL,
            reason, reason_size);

        box_free(guest_path);
        if (!safe) {
            return false;
        }
    }
    return true;
}

int latx_input_preflight_or_disable(path_collection_t *guest_paths,
                                    const char *requesting_soname)
{
    char reason[256];

    if (latx_input_preflight_guest(guest_paths, reason, sizeof(reason))) {
        return 0;
    }
    kzt_groups_log_wrapper_rejection(requesting_soname, reason);
    kzt_group_disable(KZT_GROUP_INPUT, reason);
    return -1;
}
