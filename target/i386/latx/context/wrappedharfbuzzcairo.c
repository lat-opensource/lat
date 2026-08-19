/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include "wrappedlibs.h"

#include "box64context.h"
#include "bridge.h"
#include "callback.h"
#include "debug.h"
#include "kzt-groups.h"
#include "library_private.h"
#include "wrappedharfbuzz-interop.h"
#include "wrappedtext-preflight.h"
#include "wrapper.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

const char *harfbuzzcairoName = "libharfbuzz-cairo.so.0";
#define LIBNAME harfbuzzcairo

#include "generated/wrappedharfbuzzcairotypes.h"
#include "wrappercallback.h"

static void *hb_cairo_font_init_callback(void *font, void *scaled_font,
                                         void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    if (context->native_callback) {
        return ((void *(*)(void *, void *, void *))
                    context->native_callback)(
            font, scaled_font, context->guest_user_data);
    }
    return (void *)RunFunctionFmt(
        context->guest_callback, "ppp", font, scaled_font,
        context->guest_user_data);
}

static bool hb_cairo_interop_enabled(void)
{
    if (kzt_group_is_enabled(KZT_GROUP_CAIRO)) {
        return true;
    }
    kzt_groups_log_wrapper_limitation(
        harfbuzzcairoName,
        "HarfBuzz Cairo interop requires the cairo group");
    return false;
}

EXPORT void *my_hb_cairo_font_face_create_for_face(void *face)
{
    return hb_cairo_interop_enabled()
               ? my->hb_cairo_font_face_create_for_face(face) : NULL;
}

EXPORT void *my_hb_cairo_font_face_create_for_font(void *font)
{
    return hb_cairo_interop_enabled()
               ? my->hb_cairo_font_face_create_for_font(font) : NULL;
}

EXPORT void *my_hb_cairo_font_face_get_face(void *font_face)
{
    return hb_cairo_interop_enabled()
               ? my->hb_cairo_font_face_get_face(font_face) : NULL;
}

EXPORT void *my_hb_cairo_font_face_get_font(void *font_face)
{
    return hb_cairo_interop_enabled()
               ? my->hb_cairo_font_face_get_font(font_face) : NULL;
}

EXPORT uint32_t my_hb_cairo_font_face_get_scale_factor(void *font_face)
{
    return hb_cairo_interop_enabled()
               ? my->hb_cairo_font_face_get_scale_factor(font_face) : 0;
}

EXPORT void my_hb_cairo_font_face_set_font_init_func(
    void *font_face, void *callback, void *user_data, void *destroy)
{
    LatxHbCallbackContext *context;

    if (!hb_cairo_interop_enabled()) {
        latx_hb_invoke_guest_destroy(destroy, user_data);
        return;
    }

    if (!callback && !user_data && !destroy) {
        my->hb_cairo_font_face_set_font_init_func(
            font_face, NULL, NULL, NULL);
        return;
    }
    context = latx_hb_callback_context_new(
        callback, user_data, destroy);
    if (!context) {
        latx_hb_invoke_guest_destroy(destroy, user_data);
        kzt_groups_log_wrapper_limitation(
            harfbuzzcairoName,
            "cannot allocate HarfBuzz Cairo callback context");
        return;
    }
    my->hb_cairo_font_face_set_font_init_func(
        font_face, callback ? hb_cairo_font_init_callback : NULL,
        context, latx_hb_callback_context_destroy);
}

EXPORT void my_hb_cairo_font_face_set_scale_factor(void *font_face,
                                                   uint32_t factor)
{
    if (hb_cairo_interop_enabled()) {
        my->hb_cairo_font_face_set_scale_factor(font_face, factor);
    }
}

EXPORT void *my_hb_cairo_scaled_font_get_font(void *scaled_font)
{
    return hb_cairo_interop_enabled()
               ? my->hb_cairo_scaled_font_get_font(scaled_font) : NULL;
}

EXPORT void my_hb_cairo_glyphs_from_buffer(
    void *buffer, int32_t utf8_clusters, double x_scale, double y_scale,
    double x, double y, void *utf8, int32_t utf8_len, void *glyphs,
    void *num_glyphs, void *clusters, void *num_clusters,
    void *cluster_flags)
{
    if (hb_cairo_interop_enabled()) {
        my->hb_cairo_glyphs_from_buffer(
            buffer, utf8_clusters, x_scale, y_scale, x, y, utf8, utf8_len,
            glyphs, num_glyphs, clusters, num_clusters, cluster_flags);
        return;
    }
    if (glyphs) {
        *(void **)glyphs = NULL;
    }
    if (num_glyphs) {
        *(uint32_t *)num_glyphs = 0;
    }
    if (clusters) {
        *(void **)clusters = NULL;
    }
    if (num_clusters) {
        *(uint32_t *)num_clusters = 0;
    }
    if (cluster_flags) {
        *(int32_t *)cluster_flags = 0;
    }
}

#define PRE_INIT_GUEST \
    do { \
        if (latx_harfbuzz_preflight_or_disable( \
                &box64->box64_ld_lib, harfbuzzcairoName) != 0) { \
            return -1; \
        } \
    } while (0);

#define CUSTOM_INIT \
    getMy(lib);

#define CUSTOM_FINI \
    freeMy();

#include "wrappedlib_init.h"

#pragma GCC diagnostic pop
