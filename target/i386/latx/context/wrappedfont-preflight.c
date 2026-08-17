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
#include "wrappedfont-preflight.h"
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
static const char *const freetype_supported_symbols[] = {
#include "wrappedfreetype_private.h"
};
static const char *const fontconfig_supported_symbols[] = {
#include "wrappedfontconfig_private.h"
};
static const char *const libxft_supported_symbols[] = {
#include "wrappedlibxft_private.h"
};
static const char *const freetype_required_host_symbols[] = {
    "FT_Done_Face",
    "FT_Reference_Face",
};
static const char *const fontconfig_required_host_symbols[] = {
    "FcPatternGetFTFace",
};
static const char *const libxft_required_host_symbols[] = {
    "FcFontList",
    "FcObjectSetDestroy",
    "FcPatternDestroy",
    "XftFontMatch",
    "XftFontOpenPattern",
};
enum {
    FREETYPE_OBSERVED_OUTLINE_NEW_INTERNAL = 1u << 0,
    FREETYPE_OBSERVED_OUTLINE_DONE_INTERNAL = 1u << 1,
    FREETYPE_OBSERVED_LEGACY_OUTLINE_MEMORY =
        FREETYPE_OBSERVED_OUTLINE_NEW_INTERNAL |
        FREETYPE_OBSERVED_OUTLINE_DONE_INTERNAL,
};
static const LatxWrappedObservedSymbol freetype_observed_symbols[] = {
    {
        .name = "FT_Outline_New_Internal",
        .bit = FREETYPE_OBSERVED_OUTLINE_NEW_INTERNAL,
    },
    {
        .name = "FT_Outline_Done_Internal",
        .bit = FREETYPE_OBSERVED_OUTLINE_DONE_INTERNAL,
    },
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

typedef struct FontLibraryPreflight {
    const char *guest_soname;
    const char *host_soname;
    const char *label;
    LatxWrappedSymbolFilter symbol_filter;
    const char *const *supported_symbols;
    size_t supported_symbol_count;
    const char *const *required_host_symbols;
    size_t required_host_symbol_count;
    const LatxWrappedObservedSymbol *observed_symbols;
    size_t observed_symbol_count;
} FontLibraryPreflight;

static gint font_capabilities;

static bool freetype_symbol_filter(const char *name)
{
    return !strncmp(name, "FT_", 3) || !strncmp(name, "FTC_", 4) ||
           !strncmp(name, "TT_", 3);
}

static bool fontconfig_symbol_filter(const char *name)
{
    return !strncmp(name, "Fc", 2);
}

static bool xft_symbol_filter(const char *name)
{
    return !strncmp(name, "Xft", 3);
}

bool latx_font_preflight_guest(path_collection_t *guest_paths,
                               char *reason, size_t reason_size)
{
    gint capabilities = 0;
    static const FontLibraryPreflight libraries[] = {
        {
            .guest_soname = "libfreetype.so.6",
            .host_soname = "libfreetype.so.6",
            .label = "FreeType",
            .symbol_filter = freetype_symbol_filter,
            .supported_symbols = freetype_supported_symbols,
            .supported_symbol_count = ARRAY_SIZE(freetype_supported_symbols),
            .required_host_symbols = freetype_required_host_symbols,
            .required_host_symbol_count =
                ARRAY_SIZE(freetype_required_host_symbols),
            .observed_symbols = freetype_observed_symbols,
            .observed_symbol_count =
                ARRAY_SIZE(freetype_observed_symbols),
        },
        {
            .guest_soname = "libfontconfig.so.1",
            .host_soname = "libfontconfig.so.1",
            .label = "Fontconfig",
            .symbol_filter = fontconfig_symbol_filter,
            .supported_symbols = fontconfig_supported_symbols,
            .supported_symbol_count = ARRAY_SIZE(fontconfig_supported_symbols),
            .required_host_symbols = fontconfig_required_host_symbols,
            .required_host_symbol_count =
                ARRAY_SIZE(fontconfig_required_host_symbols),
        },
        {
            .guest_soname = "libXft.so.2",
            .host_soname = "libXft.so.2",
            .label = "Xft",
            .symbol_filter = xft_symbol_filter,
            .supported_symbols = libxft_supported_symbols,
            .supported_symbol_count = ARRAY_SIZE(libxft_supported_symbols),
            .required_host_symbols = libxft_required_host_symbols,
            .required_host_symbol_count =
                ARRAY_SIZE(libxft_required_host_symbols),
        },
    };

    if (!guest_paths || !reason || !reason_size) {
        return false;
    }
    reason[0] = '\0';
    for (size_t i = 0; i < ARRAY_SIZE(libraries); i++) {
        const FontLibraryPreflight *library = &libraries[i];
        char *guest_path = ResolveFile(library->guest_soname, guest_paths);
        uint64_t observed_symbol_mask = 0;
        bool safe = latx_wrappedlib_preflight_guest(
            guest_path, library->host_soname, library->label,
            library->symbol_filter, library->supported_symbols,
            library->supported_symbol_count,
            library->required_host_symbols,
            library->required_host_symbol_count,
            library->observed_symbols, library->observed_symbol_count,
            library->observed_symbol_count ? &observed_symbol_mask : NULL,
            reason, reason_size);

        box_free(guest_path);
        if (!safe) {
            return false;
        }
        if (library->observed_symbols && observed_symbol_mask &&
            observed_symbol_mask !=
                FREETYPE_OBSERVED_LEGACY_OUTLINE_MEMORY) {
            snprintf(reason, reason_size,
                     "guest FreeType exposes an incomplete legacy "
                     "outline-memory API");
            return false;
        }
        if (observed_symbol_mask ==
            FREETYPE_OBSERVED_LEGACY_OUTLINE_MEMORY) {
            capabilities |= LATX_FONT_CAP_LEGACY_OUTLINE_MEMORY;
        }
    }
    g_atomic_int_set(&font_capabilities, capabilities);
    return true;
}

bool latx_font_capability_enabled(LatxFontCapability capability)
{
    return (g_atomic_int_get(&font_capabilities) & capability) == capability;
}

int latx_font_preflight_or_disable(path_collection_t *guest_paths,
                                   const char *requesting_soname)
{
    char reason[256];

    if (latx_font_preflight_guest(guest_paths, reason, sizeof(reason))) {
        return 0;
    }
    kzt_groups_log_wrapper_rejection(requesting_soname, reason);
    kzt_group_disable(KZT_GROUP_FONT, reason);
    return -1;
}
