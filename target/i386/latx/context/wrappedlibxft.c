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
#include "kzt-groups.h"
#include "library_private.h"
#include "wrappedfont-vaargs.h"
#include "wrappedfont-preflight.h"
#include "wrappedfont-interop.h"
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
typedef void (*xft_vFpp_t)(void *, void *);
typedef void *(*xft_pFp_t)(void *);
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

#define XFT_FACE_LOCK_DEPTH 16

typedef struct XftFaceLockEntry {
    void *font;
    void *face;
} XftFaceLockEntry;

static __thread XftFaceLockEntry xft_face_locks[XFT_FACE_LOCK_DEPTH];
static __thread size_t xft_face_lock_depth;

EXPORT void *my_XftLockFace(void *font)
{
    void *face = my->XftLockFace(font);

    if (!face) {
        return NULL;
    }
    if (xft_face_lock_depth == XFT_FACE_LOCK_DEPTH ||
        !latx_freetype_face_prepare_host_destruction(face) ||
        !latx_freetype_face_borrow(face)) {
        my->XftUnlockFace(font);
        if (xft_face_lock_depth == XFT_FACE_LOCK_DEPTH) {
            kzt_groups_log_wrapper_limitation(
                libxftName, "XftLockFace nesting limit reached");
        }
        return NULL;
    }
    xft_face_locks[xft_face_lock_depth++] = (XftFaceLockEntry) {
        .font = font,
        .face = face,
    };
    return face;
}

EXPORT void my_XftUnlockFace(void *font)
{
    size_t index = xft_face_lock_depth;
    void *face;

    while (index && xft_face_locks[index - 1].font != font) {
        index--;
    }
    if (!index) {
        kzt_groups_log_wrapper_limitation(
            libxftName, "XftUnlockFace has no matching guest lock");
        return;
    }
    index--;
    face = xft_face_locks[index].face;
    if (!latx_freetype_face_prepare_host_destruction(face)) {
        return;
    }
    memmove(&xft_face_locks[index], &xft_face_locks[index + 1],
            (xft_face_lock_depth - index - 1) * sizeof(xft_face_locks[0]));
    xft_face_lock_depth--;
    my->XftUnlockFace(font);
    latx_freetype_face_return(face);
}

EXPORT void my_XftFontClose(void *display, void *font)
{
    void *face = my->XftLockFace(font);

    if (face) {
        bool safe = latx_freetype_face_prepare_host_destruction(face);

        my->XftUnlockFace(font);
        if (!safe) {
            return;
        }
    }
    my->XftFontClose(display, font);
}

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
