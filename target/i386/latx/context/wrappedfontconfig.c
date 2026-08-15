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
#include <stdlib.h>
#include <string.h>

#include "wrappedlibs.h"

#include "box64context.h"
#include "bridge.h"
#include "callback.h"
#include "debug.h"
#include "kzt-groups.h"
#include "library_private.h"
#include "wrappedfont-vaargs.h"
#include "wrappedfont-preflight.h"
#include "wrappedfont-interop.h"
#include "wrapper.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

const char *fontconfigName = "libfontconfig.so.1";
#define LIBNAME fontconfig

typedef union FcValueUnionAbi {
    const char *string;
    int32_t integer;
    int32_t boolean;
    double real;
    const void *pointer;
} FcValueUnionAbi;

typedef struct FcValueAbi {
    int32_t type;
    FcValueUnionAbi value;
} FcValueAbi;

_Static_assert(sizeof(FcValueAbi) == 16, "FcValue ABI");

typedef struct FcFontSetAbi {
    int32_t nfont;
    int32_t sfont;
    void **fonts;
} FcFontSetAbi;

_Static_assert(sizeof(FcFontSetAbi) == 16, "FcFontSet ABI");
_Static_assert(offsetof(FcFontSetAbi, fonts) == 8, "FcFontSet fonts ABI");

typedef void *(*fc_pFv_t)(void);
typedef void *(*fc_pFp_t)(void *);
typedef void *(*fc_pFpp_t)(void *, void *);
typedef int32_t (*fc_iFpp_t)(void *, void *);
typedef int32_t (*fc_iFppi_t)(void *, void *, int32_t);
typedef int32_t (*fc_iFppd_t)(void *, void *, double);
typedef int32_t (*fc_iFppp_t)(void *, void *, void *);
typedef int32_t (*fc_iFppip_t)(void *, void *, int32_t, void *);
typedef void (*fc_vFp_t)(void *);

typedef int32_t (*fc_scandir_select_t)(const void *entry);
typedef int32_t (*fc_scandir_compare_t)(const void *left,
                                        const void *right);

typedef struct FcScandirCallbackContext {
    void *callbacks[2];
    void *native_callbacks[2];
    struct FcScandirCallbackContext *previous;
} FcScandirCallbackContext;

static __thread FcScandirCallbackContext *fontconfig_scandir_context;

#define ADDED_FUNCTIONS() \
    GO(FcObjectSetAdd, fc_iFpp_t) \
    GO(FcObjectSetCreate, fc_pFv_t) \
    GO(FcObjectSetDestroy, fc_vFp_t) \
    GO(FcFontSetDestroy, fc_vFp_t) \
    GO(FcFontSetSortDestroy, fc_vFp_t) \
    GO(FcPatternAddBool, fc_iFppi_t) \
    GO(FcPatternAddCharSet, fc_iFppp_t) \
    GO(FcPatternAddDouble, fc_iFppd_t) \
    GO(FcPatternAddFTFace, fc_iFppp_t) \
    GO(FcPatternAddInteger, fc_iFppi_t) \
    GO(FcPatternAddLangSet, fc_iFppp_t) \
    GO(FcPatternAddMatrix, fc_iFppp_t) \
    GO(FcPatternAddRange, fc_iFppp_t) \
    GO(FcPatternAddString, fc_iFppp_t) \
    GO(FcPatternCreate, fc_pFv_t) \
    GO(FcPatternDestroy, fc_vFp_t) \
    GO(FcPatternGetFTFace, fc_iFppip_t) \
    GO(FcStrCopy, fc_pFp_t)

#include "generated/wrappedfontconfigtypes.h"

#include "wrappercallback.h"

enum {
    FC_TYPE_VOID = 0,
    FC_TYPE_INTEGER = 1,
    FC_TYPE_DOUBLE = 2,
    FC_TYPE_STRING = 3,
    FC_TYPE_BOOL = 4,
    FC_TYPE_MATRIX = 5,
    FC_TYPE_CHARSET = 6,
    FC_TYPE_FT_FACE = 7,
    FC_TYPE_LANGSET = 8,
    FC_TYPE_RANGE = 9,
};

void *latx_fontconfig_pattern_ft_face(void *pattern)
{
    static const char fc_ft_face[] = "ftface";
    void *face = NULL;

    if (my_lib && my->FcPatternGetFTFace(pattern, (void *)fc_ft_face, 0,
                                        &face) == 0) {
        return face;
    }
    return NULL;
}

static bool fontconfig_pattern_add_value(void *pattern, const char *object,
                                         int32_t type,
                                         LatxX64VaReader *reader)
{
    switch (type) {
    case FC_TYPE_INTEGER:
        return my->FcPatternAddInteger(
            pattern, (void *)object, (int32_t)latx_x64_va_gp(reader));
    case FC_TYPE_DOUBLE:
        return my->FcPatternAddDouble(pattern, (void *)object,
                                      latx_x64_va_double(reader));
    case FC_TYPE_STRING:
        return my->FcPatternAddString(
            pattern, (void *)object,
            (void *)(uintptr_t)latx_x64_va_gp(reader));
    case FC_TYPE_BOOL:
        return my->FcPatternAddBool(
            pattern, (void *)object, (int32_t)latx_x64_va_gp(reader));
    case FC_TYPE_MATRIX:
        return my->FcPatternAddMatrix(
            pattern, (void *)object,
            (void *)(uintptr_t)latx_x64_va_gp(reader));
    case FC_TYPE_CHARSET:
        return my->FcPatternAddCharSet(
            pattern, (void *)object,
            (void *)(uintptr_t)latx_x64_va_gp(reader));
    case FC_TYPE_FT_FACE:
        return my->FcPatternAddFTFace(
            pattern, (void *)object,
            (void *)(uintptr_t)latx_x64_va_gp(reader));
    case FC_TYPE_LANGSET:
        return my->FcPatternAddLangSet(
            pattern, (void *)object,
            (void *)(uintptr_t)latx_x64_va_gp(reader));
    case FC_TYPE_RANGE:
        return my->FcPatternAddRange(
            pattern, (void *)object,
            (void *)(uintptr_t)latx_x64_va_gp(reader));
    default:
        kzt_groups_log_wrapper_limitation(
            fontconfigName, "unsupported FcPatternBuild value type");
        return false;
    }
}

void *latx_fontconfig_pattern_build(LatxX64VaReader *reader,
                                    void *pattern)
{
    bool created = pattern == NULL;

    if (created) {
        pattern = my->FcPatternCreate();
        if (!pattern) {
            return NULL;
        }
    }
    for (;;) {
        const char *object =
            (const char *)(uintptr_t)latx_x64_va_gp(reader);
        int32_t type;

        if (!object) {
            return pattern;
        }
        type = (int32_t)latx_x64_va_gp(reader);
        if (type == FC_TYPE_VOID ||
            !fontconfig_pattern_add_value(pattern, object, type, reader)) {
            if (created) {
                my->FcPatternDestroy(pattern);
            }
            return NULL;
        }
    }
}

void *latx_fontconfig_object_set_build(LatxX64VaReader *reader,
                                       const char *first)
{
    void *set = my->FcObjectSetCreate();
    const char *object = first;

    if (!set) {
        return NULL;
    }
    while (object) {
        if (!my->FcObjectSetAdd(set, (void *)object)) {
            my->FcObjectSetDestroy(set);
            return NULL;
        }
        object = (const char *)(uintptr_t)latx_x64_va_gp(reader);
    }
    return set;
}

EXPORT void *my_FcPatternBuild(void *pattern, void *stack)
{
    LatxX64VaReader reader;

    (void)stack;
    latx_x64_va_reader_live(&reader, 1);
    return latx_fontconfig_pattern_build(&reader, pattern);
}

EXPORT void *my_FcPatternVaBuild(void *pattern, x64_va_list_t list)
{
    LatxX64VaReader reader;

    latx_x64_va_reader_list(&reader, list);
    return latx_fontconfig_pattern_build(&reader, pattern);
}

EXPORT void *my_FcObjectSetBuild(void *first, void *stack)
{
    LatxX64VaReader reader;

    (void)stack;
    latx_x64_va_reader_live(&reader, 1);
    return latx_fontconfig_object_set_build(&reader, first);
}

EXPORT void *my_FcObjectSetVaBuild(void *first, x64_va_list_t list)
{
    LatxX64VaReader reader;

    latx_x64_va_reader_list(&reader, list);
    return latx_fontconfig_object_set_build(&reader, first);
}

EXPORT int32_t my_FcPatternAdd(void *pattern, const char *object,
                               unsigned __int128 bits, int32_t append)
{
    FcValueAbi value;

    memcpy(&value, &bits, sizeof(value));
    return my->FcPatternAdd(pattern, (void *)object, value, append);
}

EXPORT int32_t my_FcPatternAddWeak(void *pattern, const char *object,
                                   unsigned __int128 bits, int32_t append)
{
    FcValueAbi value;

    memcpy(&value, &bits, sizeof(value));
    return my->FcPatternAddWeak(pattern, (void *)object, value, append);
}

EXPORT void my_FcValueDestroy(unsigned __int128 bits)
{
    FcValueAbi value;

    memcpy(&value, &bits, sizeof(value));
    my->FcValueDestroy(value);
}

EXPORT int32_t my_FcValueEqual(unsigned __int128 left_bits,
                               unsigned __int128 right_bits)
{
    FcValueAbi left;
    FcValueAbi right;

    memcpy(&left, &left_bits, sizeof(left));
    memcpy(&right, &right_bits, sizeof(right));
    return my->FcValueEqual(left, right);
}

EXPORT void my_FcValuePrint(unsigned __int128 bits)
{
    FcValueAbi value;

    memcpy(&value, &bits, sizeof(value));
    my->FcValuePrint(value);
}

extern void *x86free;

static void fontconfig_free_container(void *pointer)
{
    if (!pointer) {
        return;
    }
    if ((uintptr_t)pointer >= reserved_va) {
        free(pointer);
        return;
    }
    if (!x86free) {
        kzt_groups_log_wrapper_limitation(
            fontconfigName, "guest FcFontSet container cannot be freed");
        return;
    }
    RunFunctionFmt((uintptr_t)x86free, "p", pointer);
}

static void fontconfig_font_set_destroy(void *pointer, fc_vFp_t destroy)
{
    FcFontSetAbi *font_set = pointer;
    FcFontSetAbi *native_set;
    void **native_fonts = NULL;
    size_t font_count;

    if (!font_set) {
        destroy(NULL);
        return;
    }
    if ((uintptr_t)font_set >= reserved_va &&
        (!font_set->fonts || (uintptr_t)font_set->fonts >= reserved_va)) {
        destroy(font_set);
        return;
    }
    if (font_set->nfont < 0 || font_set->sfont < font_set->nfont ||
        (font_set->nfont && !font_set->fonts)) {
        kzt_groups_log_wrapper_limitation(
            fontconfigName, "invalid guest FcFontSet container");
        return;
    }

    font_count = (size_t)font_set->nfont;
    if (font_count > SIZE_MAX / sizeof(*native_fonts)) {
        kzt_groups_log_wrapper_limitation(
            fontconfigName, "oversized guest FcFontSet container");
        return;
    }
    native_set = malloc(sizeof(*native_set));
    if (font_count) {
        native_fonts = malloc(font_count * sizeof(*native_fonts));
    }
    if (!native_set || (font_count && !native_fonts)) {
        free(native_fonts);
        free(native_set);
        kzt_groups_log_wrapper_limitation(
            fontconfigName, "guest FcFontSet transfer allocation failed");
        return;
    }

    if (font_count) {
        memcpy(native_fonts, font_set->fonts,
               font_count * sizeof(*native_fonts));
    }
    native_set->nfont = font_set->nfont;
    native_set->sfont = font_set->nfont;
    native_set->fonts = native_fonts;

    fontconfig_free_container(font_set->fonts);
    fontconfig_free_container(font_set);
    destroy(native_set);
}

EXPORT void my_FcFontSetDestroy(void *font_set)
{
    fontconfig_font_set_destroy(font_set, my->FcFontSetDestroy);
}

EXPORT void my_FcFontSetSortDestroy(void *font_set)
{
    fontconfig_font_set_destroy(font_set, my->FcFontSetSortDestroy);
}

EXPORT unsigned __int128 my_FcValueSave(unsigned __int128 bits)
{
    FcValueAbi value;
    FcValueAbi result;
    unsigned __int128 result_bits;

    memcpy(&value, &bits, sizeof(value));
    result = my->FcValueSave(value);
    memcpy(&result_bits, &result, sizeof(result_bits));
    return result_bits;
}

static int32_t fontconfig_scandir_select(const void *entry)
{
    FcScandirCallbackContext *context = fontconfig_scandir_context;

    if (!context || !context->callbacks[0]) {
        return 0;
    }
    if (context->native_callbacks[0]) {
        return ((fc_scandir_select_t)context->native_callbacks[0])(entry);
    }
    return (int32_t)RunFunctionFmt((uintptr_t)context->callbacks[0], "p",
                                   entry);
}

static int32_t fontconfig_scandir_compare(const void *left,
                                          const void *right)
{
    FcScandirCallbackContext *context = fontconfig_scandir_context;

    if (!context || !context->callbacks[1]) {
        return 0;
    }
    if (context->native_callbacks[1]) {
        return ((fc_scandir_compare_t)context->native_callbacks[1])(
            left, right);
    }
    return (int32_t)RunFunctionFmt((uintptr_t)context->callbacks[1], "pp",
                                   left, right);
}

EXPORT int32_t my_FcScandir(void *directory, void *namelist,
                            void *select_callback, void *compare_callback)
{
    FcScandirCallbackContext context = {
        .callbacks = {select_callback, compare_callback},
        .native_callbacks = {
            GetNativeFnc((uintptr_t)select_callback),
            GetNativeFnc((uintptr_t)compare_callback),
        },
        .previous = fontconfig_scandir_context,
    };
    void *native_select = context.native_callbacks[0];
    void *native_compare = context.native_callbacks[1];
    int32_t result;

    if ((select_callback && !native_select) ||
        (compare_callback && !native_compare)) {
        native_select = select_callback ? fontconfig_scandir_select : NULL;
        native_compare = compare_callback ? fontconfig_scandir_compare : NULL;
        fontconfig_scandir_context = &context;
    }
    result = my->FcScandir(directory, namelist, native_select, native_compare);
    fontconfig_scandir_context = context.previous;
    return result;
}

EXPORT void *my_FcStrBuildFilename(const char *first, void *stack)
{
    LatxX64VaReader reader;
    GString *path;
    const char *part;
    void *result;

    (void)stack;
    if (!first) {
        return NULL;
    }
    latx_x64_va_reader_live(&reader, 1);
    path = g_string_new(first);
    while ((part = (const char *)(uintptr_t)latx_x64_va_gp(&reader))) {
        g_string_append_c(path, '/');
        g_string_append(path, part);
    }
    result = my->FcStrCopy(path->str);
    g_string_free(path, true);
    return result;
}

#define PRE_INIT_GUEST \
    do { \
        if (latx_font_preflight_or_disable(&box64->box64_ld_lib, \
                                           fontconfigName) != 0) { \
            return -1; \
        } \
    } while (0);

#define CUSTOM_INIT \
    getMy(lib); \
    setNeededLibs(lib, 2, "libexpat.so.1", "libfreetype.so.6");

#define CUSTOM_FINI \
    freeMy();

#include "wrappedlib_init.h"

#pragma GCC diagnostic pop
