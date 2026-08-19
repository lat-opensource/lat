/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include <stddef.h>
#include <stdint.h>

#include "wrappedlibs.h"

#include "box64context.h"
#include "bridge.h"
#include "callback.h"
#include "debug.h"
#include "kzt-groups.h"
#include "library_private.h"
#include "wrappedtext-preflight.h"
#include "wrapper.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

const char *graphite2Name = "libgraphite2.so.3";
#define LIBNAME graphite2

#include "generated/wrappedgraphite2types.h"
#include "wrappercallback.h"

#define GR_CALLBACK_MAGIC UINT64_C(0x475241504843424b)

typedef const void *(*GrGetTableCallback)(const void *, uint32_t, size_t *);
typedef void (*GrReleaseTableCallback)(const void *, const void *);
typedef float (*GrAdvanceCallback)(const void *, uint16_t);

typedef struct GrFaceOpsAbi {
    size_t size;
    void *get_table;
    void *release_table;
} GrFaceOpsAbi;

typedef struct GrFontOpsAbi {
    size_t size;
    void *glyph_advance_x;
    void *glyph_advance_y;
} GrFontOpsAbi;

typedef struct GrCallbackContext {
    uint64_t magic;
    void *object;
    void *guest_handle;
    uintptr_t guest_callbacks[2];
    void *native_callbacks[2];
    struct GrCallbackContext *next;
} GrCallbackContext;

static GMutex gr_callback_lock;
static GrCallbackContext *gr_face_contexts;
static GrCallbackContext *gr_font_contexts;

_Static_assert(sizeof(GrFaceOpsAbi) == 24, "gr_face_ops ABI");
_Static_assert(sizeof(GrFontOpsAbi) == 24, "gr_font_ops ABI");

static GrCallbackContext *gr_callback_context_new(
    void *guest_handle, void *callback0, void *callback1)
{
    GrCallbackContext *context = g_try_new0(GrCallbackContext, 1);

    if (!context) {
        return NULL;
    }
    context->magic = GR_CALLBACK_MAGIC;
    context->guest_handle = guest_handle;
    context->guest_callbacks[0] = (uintptr_t)callback0;
    context->guest_callbacks[1] = (uintptr_t)callback1;
    context->native_callbacks[0] = GetNativeFnc((uintptr_t)callback0);
    context->native_callbacks[1] = GetNativeFnc((uintptr_t)callback1);
    return context;
}

static void gr_callback_context_discard(GrCallbackContext *context)
{
    if (!context || context->magic != GR_CALLBACK_MAGIC) {
        return;
    }
    context->magic = 0;
    g_free(context);
}

static void gr_callback_context_publish(GrCallbackContext **contexts,
                                        GrCallbackContext *context,
                                        void *object)
{
    context->object = object;
    g_mutex_lock(&gr_callback_lock);
    context->next = *contexts;
    *contexts = context;
    g_mutex_unlock(&gr_callback_lock);
}

static GrCallbackContext *gr_callback_context_take(
    GrCallbackContext **contexts, void *object)
{
    GrCallbackContext **cursor;
    GrCallbackContext *context = NULL;

    g_mutex_lock(&gr_callback_lock);
    cursor = contexts;
    while (*cursor && (*cursor)->object != object) {
        cursor = &(*cursor)->next;
    }
    if (*cursor) {
        context = *cursor;
        *cursor = context->next;
        context->next = NULL;
    }
    g_mutex_unlock(&gr_callback_lock);
    return context;
}

static const void *gr_get_table_callback(const void *opaque, uint32_t tag,
                                         size_t *length)
{
    const GrCallbackContext *context = opaque;

    if (!context || context->magic != GR_CALLBACK_MAGIC ||
        !context->guest_callbacks[0]) {
        return NULL;
    }
    if (context->native_callbacks[0]) {
        return ((GrGetTableCallback)context->native_callbacks[0])(
            context->guest_handle, tag, length);
    }
    return (const void *)(uintptr_t)RunFunctionFmt(
        context->guest_callbacks[0], "pup", context->guest_handle, tag,
        length);
}

static void gr_release_table_callback(const void *opaque,
                                      const void *table)
{
    const GrCallbackContext *context = opaque;

    if (!context || context->magic != GR_CALLBACK_MAGIC ||
        !context->guest_callbacks[1]) {
        return;
    }
    if (context->native_callbacks[1]) {
        ((GrReleaseTableCallback)context->native_callbacks[1])(
            context->guest_handle, table);
    } else {
        RunFunctionFmt(context->guest_callbacks[1], "pp",
                       context->guest_handle, table);
    }
}

static float gr_advance_callback(const void *opaque, uint16_t glyph)
{
    const GrCallbackContext *context = opaque;

    if (!context || context->magic != GR_CALLBACK_MAGIC ||
        !context->guest_callbacks[0]) {
        return 0.0f;
    }
    if (context->native_callbacks[0]) {
        return ((GrAdvanceCallback)context->native_callbacks[0])(
            context->guest_handle, glyph);
    }
    return RunFunctionFmtFloat(context->guest_callbacks[0], "pW",
                               context->guest_handle, glyph);
}

static float gr_advance_y_callback(const void *opaque, uint16_t glyph)
{
    const GrCallbackContext *context = opaque;

    if (!context || context->magic != GR_CALLBACK_MAGIC ||
        !context->guest_callbacks[1]) {
        return 0.0f;
    }
    if (context->native_callbacks[1]) {
        return ((GrAdvanceCallback)context->native_callbacks[1])(
            context->guest_handle, glyph);
    }
    return RunFunctionFmtFloat(context->guest_callbacks[1], "pW",
                               context->guest_handle, glyph);
}

static void *gr_make_face_common(void *guest_handle, void *get_table,
                                 void *release_table, uint32_t cache_size,
                                 uint32_t options, bool use_ops,
                                 bool use_cache)
{
    GrCallbackContext *context;
    GrFaceOpsAbi host_ops = {
        .size = sizeof(host_ops),
        .get_table = get_table ? gr_get_table_callback : NULL,
        .release_table = release_table ? gr_release_table_callback : NULL,
    };
    void *face;

    if (!get_table && !release_table) {
        if (use_ops) {
            return use_cache
                       ? my->gr_make_face_with_seg_cache_and_ops(
                             guest_handle, NULL, cache_size, options)
                       : my->gr_make_face_with_ops(
                             guest_handle, NULL, options);
        }
        return use_cache
                   ? my->gr_make_face_with_seg_cache(
                         guest_handle, NULL, cache_size, options)
                   : my->gr_make_face(guest_handle, NULL, options);
    }
    if (!get_table) {
        kzt_groups_log_wrapper_limitation(
            graphite2Name,
            "Graphite2 release-table callback has no get-table callback");
        return NULL;
    }
    context = gr_callback_context_new(
        guest_handle, get_table, release_table);
    if (!context) {
        kzt_groups_log_wrapper_limitation(
            graphite2Name,
            "cannot allocate Graphite2 face callback context");
        return NULL;
    }
    if (use_ops) {
        face = use_cache
                   ? my->gr_make_face_with_seg_cache_and_ops(
                         context, &host_ops, cache_size, options)
                   : my->gr_make_face_with_ops(context, &host_ops, options);
    } else {
        face = use_cache
                   ? my->gr_make_face_with_seg_cache(
                         context, gr_get_table_callback, cache_size, options)
                   : my->gr_make_face(
                         context, gr_get_table_callback, options);
    }
    if (!face) {
        gr_callback_context_discard(context);
        return NULL;
    }
    gr_callback_context_publish(&gr_face_contexts, context, face);
    return face;
}

EXPORT void *my_gr_make_face(void *handle, void *get_table, uint32_t options)
{
    return gr_make_face_common(
        handle, get_table, NULL, 0, options, false, false);
}

EXPORT void *my_gr_make_face_with_seg_cache(
    void *handle, void *get_table, uint32_t cache_size, uint32_t options)
{
    return gr_make_face_common(
        handle, get_table, NULL, cache_size, options, false, true);
}

static bool gr_face_ops_callbacks(const GrFaceOpsAbi *ops,
                                  void **get_table, void **release_table)
{
    *get_table = NULL;
    *release_table = NULL;
    if (!ops) {
        return true;
    }
    if (ops->size < offsetof(GrFaceOpsAbi, get_table) +
                        sizeof(ops->get_table)) {
        kzt_groups_log_wrapper_limitation(
            graphite2Name, "guest gr_face_ops is too small");
        return false;
    }
    *get_table = ops->get_table;
    if (ops->size >= offsetof(GrFaceOpsAbi, release_table) +
                         sizeof(ops->release_table)) {
        *release_table = ops->release_table;
    }
    return true;
}

EXPORT void *my_gr_make_face_with_ops(
    void *handle, const GrFaceOpsAbi *ops, uint32_t options)
{
    void *get_table;
    void *release_table;

    if (!gr_face_ops_callbacks(ops, &get_table, &release_table)) {
        return NULL;
    }
    return gr_make_face_common(
        handle, get_table, release_table, 0, options, true, false);
}

EXPORT void *my_gr_make_face_with_seg_cache_and_ops(
    void *handle, const GrFaceOpsAbi *ops, uint32_t cache_size,
    uint32_t options)
{
    void *get_table;
    void *release_table;

    if (!gr_face_ops_callbacks(ops, &get_table, &release_table)) {
        return NULL;
    }
    return gr_make_face_common(
        handle, get_table, release_table, cache_size, options, true, true);
}

EXPORT void my_gr_face_destroy(void *face)
{
    GrCallbackContext *context =
        gr_callback_context_take(&gr_face_contexts, face);

    my->gr_face_destroy(face);
    gr_callback_context_discard(context);
}

static void *gr_make_font_common(float ppm, void *guest_handle,
                                 void *advance_x, void *advance_y,
                                 void *face, bool use_ops)
{
    GrCallbackContext *context;
    GrFontOpsAbi host_ops = {
        .size = sizeof(host_ops),
        .glyph_advance_x = advance_x ? gr_advance_callback : NULL,
        .glyph_advance_y = advance_y ? gr_advance_y_callback : NULL,
    };
    void *font;

    if (!advance_x && !advance_y) {
        return use_ops
                   ? my->gr_make_font_with_ops(
                         ppm, guest_handle, NULL, face)
                   : my->gr_make_font_with_advance_fn(
                         ppm, guest_handle, NULL, face);
    }
    context = gr_callback_context_new(
        guest_handle, advance_x, advance_y);
    if (!context) {
        kzt_groups_log_wrapper_limitation(
            graphite2Name,
            "cannot allocate Graphite2 font callback context");
        return NULL;
    }
    font = use_ops
               ? my->gr_make_font_with_ops(ppm, context, &host_ops, face)
               : my->gr_make_font_with_advance_fn(
                     ppm, context, gr_advance_callback, face);
    if (!font) {
        gr_callback_context_discard(context);
        return NULL;
    }
    gr_callback_context_publish(&gr_font_contexts, context, font);
    return font;
}

EXPORT void *my_gr_make_font_with_advance_fn(
    float ppm, void *handle, void *advance, void *face)
{
    return gr_make_font_common(
        ppm, handle, advance, NULL, face, false);
}

EXPORT void *my_gr_make_font_with_ops(
    float ppm, void *handle, const GrFontOpsAbi *ops, void *face)
{
    void *advance_x = NULL;
    void *advance_y = NULL;

    if (ops) {
        if (ops->size < offsetof(GrFontOpsAbi, glyph_advance_x) +
                            sizeof(ops->glyph_advance_x)) {
            kzt_groups_log_wrapper_limitation(
                graphite2Name, "guest gr_font_ops is too small");
            return NULL;
        }
        advance_x = ops->glyph_advance_x;
        if (ops->size >= offsetof(GrFontOpsAbi, glyph_advance_y) +
                            sizeof(ops->glyph_advance_y)) {
            advance_y = ops->glyph_advance_y;
        }
    }
    return gr_make_font_common(
        ppm, handle, advance_x, advance_y, face, true);
}

EXPORT void my_gr_font_destroy(void *font)
{
    GrCallbackContext *context =
        gr_callback_context_take(&gr_font_contexts, font);

    my->gr_font_destroy(font);
    gr_callback_context_discard(context);
}

EXPORT uint8_t my_graphite_start_logging(void *file, int32_t mask)
{
    (void)file;
    (void)mask;
    kzt_groups_log_wrapper_limitation(
        graphite2Name,
        "deprecated FILE-based Graphite2 logging remains disabled");
    return 0;
}

static void gr_callback_contexts_clear(GrCallbackContext **contexts)
{
    GrCallbackContext *context;

    g_mutex_lock(&gr_callback_lock);
    while ((context = *contexts)) {
        *contexts = context->next;
        context->magic = 0;
        g_free(context);
    }
    g_mutex_unlock(&gr_callback_lock);
}

#define PRE_INIT_GUEST \
    do { \
        if (latx_text_family_preflight_or_disable( \
                &box64->box64_ld_lib, graphite2Name) != 0) { \
            return -1; \
        } \
    } while (0);

#define CUSTOM_INIT \
    getMy(lib);

#define CUSTOM_FINI \
    gr_callback_contexts_clear(&gr_font_contexts); \
    gr_callback_contexts_clear(&gr_face_contexts); \
    freeMy();

#include "wrappedlib_init.h"

#pragma GCC diagnostic pop
