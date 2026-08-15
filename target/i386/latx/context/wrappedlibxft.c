/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: MIT
 */

#include "qemu/osdep.h"

#include <dlfcn.h>
#include <stdint.h>

#include "wrappedlibs.h"

#include "box64context.h"
#include "bridge.h"
#include "library_private.h"
#include "wrappedfont-vaargs.h"
#include "wrappedfont-preflight.h"
#include "wrapper.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

#ifdef ANDROID
const char *libxftName = "libXft.so";
#else
const char *libxftName = "libXft.so.2";
#endif

#define LIBNAME libxft

#define PRE_INIT_GUEST \
    do { \
        if (latx_font_preflight_or_disable(&box64->box64_ld_lib, \
                                           libxftName) != 0) { \
            return -1; \
        } \
    } while (0);

typedef void (*xft_vFp_t)(void *);
typedef void *(*xft_pFpp_t)(void *, void *);
typedef void *(*xft_pFpipp_t)(void *, int32_t, void *, void *);
typedef void *(*xft_pFppp_t)(void *, void *, void *);

#define ADDED_FUNCTIONS() \
    GO(FcFontList, xft_pFppp_t) \
    GO(FcObjectSetDestroy, xft_vFp_t) \
    GO(FcPatternDestroy, xft_vFp_t) \
    GO(XftFontMatch, xft_pFpipp_t) \
    GO(XftFontOpenPattern, xft_pFpp_t)

#include "generated/wrappedlibxfttypes.h"

#include "wrappercallback.h"

EXPORT void *my_XftFontOpen(void *display, int32_t screen, void *stack)
{
    LatxX64VaReader reader;
    void *pattern;
    void *match;
    void *font;
    int32_t result;

    (void)stack;
    latx_x64_va_reader_live(&reader, 2);
    pattern = latx_fontconfig_pattern_build(&reader, NULL);
    if (!pattern) {
        return NULL;
    }
    match = my->XftFontMatch(display, screen, pattern, &result);
    my->FcPatternDestroy(pattern);
    if (!match) {
        return NULL;
    }
    font = my->XftFontOpenPattern(display, match);
    if (!font) {
        my->FcPatternDestroy(match);
    }
    return font;
}

EXPORT void *my_XftListFonts(void *display, int32_t screen, void *stack)
{
    LatxX64VaReader reader;
    void *pattern;
    void *objects;
    void *fonts;
    const char *first;

    (void)display;
    (void)screen;
    (void)stack;
    latx_x64_va_reader_live(&reader, 2);
    pattern = latx_fontconfig_pattern_build(&reader, NULL);
    if (!pattern) {
        return NULL;
    }
    first = (const char *)(uintptr_t)latx_x64_va_gp(&reader);
    objects = latx_fontconfig_object_set_build(&reader, first);
    if (!objects) {
        my->FcPatternDestroy(pattern);
        return NULL;
    }
    fonts = my->FcFontList(NULL, pattern, objects);
    my->FcPatternDestroy(pattern);
    my->FcObjectSetDestroy(objects);
    return fonts;
}

#ifdef ANDROID
#define CUSTOM_INIT \
    getMy(lib); \
    setNeededLibs(lib, 4, "libX11.so", "libfontconfig.so", \
                  "libXrender.so", "libfreetype.so");
#else
#define CUSTOM_INIT \
    getMy(lib); \
    setNeededLibs(lib, 4, "libX11.so.6", "libfontconfig.so.1", \
                  "libXrender.so.1", "libfreetype.so.6");
#endif

#define CUSTOM_FINI \
    freeMy();

#include "wrappedlib_init.h"

#pragma GCC diagnostic pop
