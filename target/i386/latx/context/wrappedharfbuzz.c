/*
 * Callback signatures and API coverage were cross-checked against Box64.
 * The callback ownership model and object interop are LAT-specific.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: MIT
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
#include "librarian.h"
#include "library_private.h"
#include "wrappedfont-interop.h"
#include "wrappedharfbuzz-interop.h"
#include "wrappedtext-preflight.h"
#include "wrapper.h"
#include "x86dlfun.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

const char *harfbuzzName = "libharfbuzz.so.0";
#define LIBNAME harfbuzz

#include "generated/wrappedharfbuzztypes.h"
#include "wrappercallback.h"

#define HB_COLOR_LINE_MAGIC UINT64_C(0x4842434f4c4f524c)
#define HB_MEMORY_MODE_READONLY 1

typedef struct HbColorLineAbi {
    void *data;
    void *get_color_stops;
    void *get_color_stops_user_data;
    void *get_extend;
    void *get_extend_user_data;
    void *reserved[8];
} HbColorLineAbi;

typedef struct HbGlyphInfoAbi {
    uint32_t codepoint;
    uint32_t mask;
    uint32_t cluster;
    uint32_t var1;
    uint32_t var2;
} HbGlyphInfoAbi;

typedef struct HbGlyphPositionAbi {
    int32_t x_advance;
    int32_t y_advance;
    int32_t x_offset;
    int32_t y_offset;
    uint32_t var;
} HbGlyphPositionAbi;

typedef struct HbFeatureAbi {
    uint32_t tag;
    uint32_t value;
    uint32_t start;
    uint32_t end;
} HbFeatureAbi;

typedef struct HbVariationAbi {
    uint32_t tag;
    float value;
} HbVariationAbi;

typedef struct HbSegmentPropertiesAbi {
    uint32_t direction;
    uint32_t script;
    void *language;
    void *reserved1;
    void *reserved2;
} HbSegmentPropertiesAbi;

typedef struct HbFontExtentsAbi {
    int32_t values[12];
} HbFontExtentsAbi;

typedef struct HbGlyphExtentsAbi {
    int32_t values[4];
} HbGlyphExtentsAbi;

typedef struct HbColorStopAbi {
    float offset;
    int32_t is_foreground;
    uint32_t color;
} HbColorStopAbi;

typedef struct HbColorLineBridge {
    HbColorLineAbi guest;
    uint64_t magic;
    HbColorLineAbi *native;
} HbColorLineBridge;

typedef struct HbGuestColorLineContext {
    HbColorLineAbi *guest;
} HbGuestColorLineContext;

typedef struct HbFtFaceLock {
    void *font;
    void *face;
    unsigned references;
    struct HbFtFaceLock *next;
} HbFtFaceLock;

static GMutex hb_ft_lock;
static HbFtFaceLock *hb_ft_face_locks;
static __thread HbColorLineBridge *hb_active_color_line_bridge;
static uintptr_t hb_guest_g_bytes_ref;
static uintptr_t hb_guest_g_bytes_get_data;
static uintptr_t hb_guest_g_bytes_unref;

static bool hb_resolve_guest_glib(void);

_Static_assert(sizeof(HbColorLineAbi) == 104,
               "hb_color_line_t ABI");
_Static_assert(sizeof(HbGlyphInfoAbi) == 20, "hb_glyph_info_t ABI");
_Static_assert(sizeof(HbGlyphPositionAbi) == 20,
               "hb_glyph_position_t ABI");
_Static_assert(sizeof(HbFeatureAbi) == 16, "hb_feature_t ABI");
_Static_assert(sizeof(HbVariationAbi) == 8, "hb_variation_t ABI");
_Static_assert(sizeof(HbSegmentPropertiesAbi) == 32,
               "hb_segment_properties_t ABI");
_Static_assert(sizeof(HbFontExtentsAbi) == 48,
               "hb_font_extents_t ABI");
_Static_assert(sizeof(HbGlyphExtentsAbi) == 16,
               "hb_glyph_extents_t ABI");
_Static_assert(sizeof(HbColorStopAbi) == 12, "hb_color_stop_t ABI");

#define HB_CALLBACK_VOID(context, type, format, ...) \
    do { \
        if ((context)->native_callback) { \
            ((type)(context)->native_callback)(__VA_ARGS__, \
                                                (context)->guest_user_data); \
        } else if ((context)->guest_callback) { \
            RunFunctionFmt((context)->guest_callback, format, __VA_ARGS__, \
                           (context)->guest_user_data); \
        } \
    } while (0)

#define HB_CALLBACK_RETURN(context, type, cast, format, ...) \
    ((context)->native_callback \
         ? (cast)((type)(context)->native_callback)( \
               __VA_ARGS__, (context)->guest_user_data) \
         : (cast)RunFunctionFmt((context)->guest_callback, format, \
                                __VA_ARGS__, \
                                (context)->guest_user_data))

static int32_t hb_buffer_message_callback(void *buffer, void *font,
                                          void *message, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context, int32_t (*)(void *, void *, void *, void *), int32_t,
        "pppp", buffer, font, message);
}

static void *hb_reference_table_callback(void *face, uint32_t tag,
                                         void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context, void *(*)(void *, uint32_t, void *), void *,
        "pup", face, tag);
}

static void hb_draw_close_callback(void *funcs, void *draw_data,
                                   void *state, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(context, void (*)(void *, void *, void *, void *),
                     "pppp", funcs, draw_data, state);
}

static void hb_draw_line_callback(void *funcs, void *draw_data,
                                  void *state, float x, float y,
                                  void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, void *, float, float, void *),
        "pppffp", funcs, draw_data, state, x, y);
}

static void hb_draw_quadratic_callback(void *funcs, void *draw_data,
                                       void *state, float cx, float cy,
                                       float x, float y, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, void *, float, float, float, float,
                 void *),
        "pppffffp", funcs, draw_data, state, cx, cy, x, y);
}

static void hb_draw_cubic_callback(void *funcs, void *draw_data,
                                   void *state, float c1x, float c1y,
                                   float c2x, float c2y, float x, float y,
                                   void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, void *, float, float, float, float,
                 float, float, void *),
        "pppffffffp", funcs, draw_data, state, c1x, c1y, c2x, c2y, x, y);
}

static uint32_t hb_unicode_simple_callback(void *funcs, uint32_t codepoint,
                                           void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context, uint32_t (*)(void *, uint32_t, void *), uint32_t,
        "pup", funcs, codepoint);
}

static int32_t hb_unicode_compose_callback(void *funcs, uint32_t a,
                                           uint32_t b, void *ab,
                                           void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, uint32_t, uint32_t, void *, void *),
        int32_t, "puupp", funcs, a, b, ab);
}

static uint32_t hb_unicode_decompose_compat_callback(void *funcs,
                                                     uint32_t codepoint,
                                                     void *decomposed,
                                                     void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context, uint32_t (*)(void *, uint32_t, void *, void *),
        uint32_t, "pupp", funcs, codepoint, decomposed);
}

static int32_t hb_unicode_decompose_callback(void *funcs,
                                             uint32_t codepoint,
                                             void *a, void *b,
                                             void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, uint32_t, void *, void *, void *),
        int32_t, "puppp", funcs, codepoint, a, b);
}

static int32_t hb_font_extents_callback(void *font, void *font_data,
                                        void *extents, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context, int32_t (*)(void *, void *, void *, void *), int32_t,
        "pppp", font, font_data, extents);
}

static int32_t hb_font_nominal_callback(void *font, void *font_data,
                                        uint32_t unicode, void *glyph,
                                        void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, uint32_t, void *, void *),
        int32_t, "ppupp", font, font_data, unicode, glyph);
}

static uint32_t hb_font_nominals_callback(
    void *font, void *font_data, uint32_t count, void *unicodes,
    uint32_t unicode_stride, void *glyphs, uint32_t glyph_stride,
    void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        uint32_t (*)(void *, void *, uint32_t, void *, uint32_t,
                     void *, uint32_t, void *),
        uint32_t, "ppupupup", font, font_data, count, unicodes,
        unicode_stride, glyphs, glyph_stride);
}

static int32_t hb_font_variation_callback(void *font, void *font_data,
                                          uint32_t unicode,
                                          uint32_t selector, void *glyph,
                                          void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, uint32_t, uint32_t, void *, void *),
        int32_t, "ppuupp", font, font_data, unicode, selector, glyph);
}

static int32_t hb_font_advance_callback(void *font, void *font_data,
                                        uint32_t glyph, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context, int32_t (*)(void *, void *, uint32_t, void *), int32_t,
        "ppup", font, font_data, glyph);
}

static void hb_font_advances_callback(
    void *font, void *font_data, uint32_t count, void *glyphs,
    uint32_t glyph_stride, void *advances, uint32_t advance_stride,
    void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, uint32_t, void *, uint32_t,
                 void *, uint32_t, void *),
        "ppupupup", font, font_data, count, glyphs, glyph_stride,
        advances, advance_stride);
}

static int32_t hb_font_origin_callback(void *font, void *font_data,
                                       uint32_t glyph, void *x, void *y,
                                       void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, uint32_t, void *, void *, void *),
        int32_t, "ppuppp", font, font_data, glyph, x, y);
}

static int32_t hb_font_kerning_callback(void *font, void *font_data,
                                        uint32_t left, uint32_t right,
                                        void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, uint32_t, uint32_t, void *),
        int32_t, "ppuup", font, font_data, left, right);
}

static int32_t hb_font_glyph_extents_callback(void *font, void *font_data,
                                              uint32_t glyph,
                                              void *extents, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, uint32_t, void *, void *),
        int32_t, "ppupp", font, font_data, glyph, extents);
}

static int32_t hb_font_contour_callback(void *font, void *font_data,
                                        uint32_t glyph, uint32_t point,
                                        void *x, void *y, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, uint32_t, uint32_t, void *, void *,
                    void *),
        int32_t, "ppuuppp", font, font_data, glyph, point, x, y);
}

static int32_t hb_font_glyph_name_callback(void *font, void *font_data,
                                           uint32_t glyph, void *name,
                                           uint32_t size, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, uint32_t, void *, uint32_t, void *),
        int32_t, "ppupup", font, font_data, glyph, name, size);
}

static int32_t hb_font_from_name_callback(void *font, void *font_data,
                                          void *name, int32_t length,
                                          void *glyph, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, void *, int32_t, void *, void *),
        int32_t, "pppipp", font, font_data, name, length, glyph);
}

static int32_t hb_font_glyph_callback(void *font, void *font_data,
                                      uint32_t unicode, uint32_t variation,
                                      void *glyph, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, uint32_t, uint32_t, void *, void *),
        int32_t, "ppuupp", font, font_data, unicode, variation, glyph);
}

static void hb_font_draw_callback(void *font, void *font_data,
                                  uint32_t glyph, void *funcs,
                                  void *draw_data, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, uint32_t, void *, void *, void *),
        "ppuppp", font, font_data, glyph, funcs, draw_data);
}

static void hb_font_paint_callback(void *font, void *font_data,
                                   uint32_t glyph, void *funcs,
                                   void *paint_data, uint32_t palette,
                                   uint32_t foreground, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, uint32_t, void *, void *, uint32_t,
                 uint32_t, void *),
        "ppuppuup", font, font_data, glyph, funcs, paint_data, palette,
        foreground);
}

static void hb_paint_push_transform_callback(
    void *funcs, void *paint_data, float xx, float yx, float xy, float yy,
    float dx, float dy, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, float, float, float, float, float,
                 float, void *),
        "ppffffffp", funcs, paint_data, xx, yx, xy, yy, dx, dy);
}

static void hb_paint_simple_callback(void *funcs, void *paint_data,
                                     void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(context, void (*)(void *, void *, void *),
                     "ppp", funcs, paint_data);
}

static int32_t hb_paint_color_glyph_callback(void *funcs, void *paint_data,
                                             uint32_t glyph, void *font,
                                             void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, uint32_t, void *, void *),
        int32_t, "ppupp", funcs, paint_data, glyph, font);
}

static void hb_paint_clip_glyph_callback(void *funcs, void *paint_data,
                                         uint32_t glyph, void *font,
                                         void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, uint32_t, void *, void *),
        "ppupp", funcs, paint_data, glyph, font);
}

static void hb_paint_clip_rectangle_callback(void *funcs, void *paint_data,
                                              float x_min, float y_min,
                                              float x_max, float y_max,
                                              void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, float, float, float, float, void *),
        "ppffffp", funcs, paint_data, x_min, y_min, x_max, y_max);
}

static void hb_paint_color_callback(void *funcs, void *paint_data,
                                    int32_t foreground, uint32_t color,
                                    void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, int32_t, uint32_t, void *),
        "ppiup", funcs, paint_data, foreground, color);
}

static int32_t hb_paint_image_callback(
    void *funcs, void *paint_data, void *blob, uint32_t width,
    uint32_t height, uint32_t format, float slant, void *extents,
    void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, void *, uint32_t, uint32_t,
                    uint32_t, float, void *, void *),
        int32_t, "pppuuufpp", funcs, paint_data, blob, width, height,
        format, slant, extents);
}

static void hb_paint_pop_group_callback(void *funcs, void *paint_data,
                                        uint32_t mode, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    HB_CALLBACK_VOID(
        context, void (*)(void *, void *, uint32_t, void *),
        "ppup", funcs, paint_data, mode);
}

static int32_t hb_paint_custom_color_callback(void *funcs,
                                              void *paint_data,
                                              uint32_t color_index,
                                              void *color, void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    return HB_CALLBACK_RETURN(
        context,
        int32_t (*)(void *, void *, uint32_t, void *, void *),
        int32_t, "ppupp", funcs, paint_data, color_index, color);
}

static uint32_t hb_color_line_stops_bridge(
    HbColorLineAbi *guest, void *data, uint32_t start, void *count,
    void *stops, void *opaque)
{
    HbColorLineBridge *bridge = opaque;
    HbColorLineAbi *native;

    (void)guest;
    (void)data;
    if (!bridge || bridge->magic != HB_COLOR_LINE_MAGIC ||
        !(native = bridge->native) || !native->get_color_stops) {
        return 0;
    }
    return ((uint32_t (*)(void *, void *, uint32_t, void *, void *,
                          void *))native->get_color_stops)(
        native, native->data, start, count, stops,
        native->get_color_stops_user_data);
}

static uint32_t hb_color_line_extend_bridge(HbColorLineAbi *guest,
                                            void *data, void *opaque)
{
    HbColorLineBridge *bridge = opaque;
    HbColorLineAbi *native;

    (void)guest;
    (void)data;
    if (!bridge || bridge->magic != HB_COLOR_LINE_MAGIC ||
        !(native = bridge->native) || !native->get_extend) {
        return 0;
    }
    return ((uint32_t (*)(void *, void *, void *))native->get_extend)(
        native, native->data, native->get_extend_user_data);
}

static uint32_t hb_guest_color_line_stops(
    HbColorLineAbi *host, void *data, uint32_t start, void *count,
    void *stops, void *opaque)
{
    HbGuestColorLineContext *context = opaque;
    HbColorLineAbi *guest = context ? context->guest : NULL;
    void *native;

    (void)host;
    (void)data;
    if (!guest || !guest->get_color_stops) {
        return 0;
    }
    native = GetNativeFnc((uintptr_t)guest->get_color_stops);
    if (native) {
        return ((uint32_t (*)(void *, void *, uint32_t, void *, void *,
                              void *))native)(
            guest, guest->data, start, count, stops,
            guest->get_color_stops_user_data);
    }
    return (uint32_t)RunFunctionFmt(
        (uintptr_t)guest->get_color_stops, "ppuppp", guest, guest->data,
        start, count, stops, guest->get_color_stops_user_data);
}

static uint32_t hb_guest_color_line_extend(HbColorLineAbi *host,
                                           void *data, void *opaque)
{
    HbGuestColorLineContext *context = opaque;
    HbColorLineAbi *guest = context ? context->guest : NULL;
    void *native;

    (void)host;
    (void)data;
    if (!guest || !guest->get_extend) {
        return 0;
    }
    native = GetNativeFnc((uintptr_t)guest->get_extend);
    if (native) {
        return ((uint32_t (*)(void *, void *, void *))native)(
            guest, guest->data, guest->get_extend_user_data);
    }
    return (uint32_t)RunFunctionFmt(
        (uintptr_t)guest->get_extend, "ppp", guest, guest->data,
        guest->get_extend_user_data);
}

static bool hb_guest_color_line_init(HbColorLineAbi *host,
                                     HbGuestColorLineContext *context,
                                     HbColorLineAbi *guest)
{
    if (!host || !context || !guest) {
        return false;
    }
    *host = *guest;
    context->guest = guest;
    if (guest->get_color_stops) {
        host->get_color_stops = hb_guest_color_line_stops;
        host->get_color_stops_user_data = context;
    }
    if (guest->get_extend) {
        host->get_extend = hb_guest_color_line_extend;
        host->get_extend_user_data = context;
    }
    return true;
}

static bool hb_color_line_bridge_init(HbColorLineBridge *bridge,
                                      HbColorLineAbi *native)
{
    memset(bridge, 0, sizeof(*bridge));
    if (!native) {
        return false;
    }
    bridge->guest = *native;
    bridge->magic = HB_COLOR_LINE_MAGIC;
    bridge->native = native;
    if (native->get_color_stops) {
        bridge->guest.get_color_stops = (void *)AddAutomaticBridge(
            my_context->system, uFppuppp, hb_color_line_stops_bridge, 0);
        bridge->guest.get_color_stops_user_data = bridge;
        if (!bridge->guest.get_color_stops) {
            return false;
        }
    }
    if (native->get_extend) {
        bridge->guest.get_extend = (void *)AddAutomaticBridge(
            my_context->system, uFppp, hb_color_line_extend_bridge, 0);
        bridge->guest.get_extend_user_data = bridge;
        if (!bridge->guest.get_extend) {
            return false;
        }
    }
    return true;
}

static void hb_paint_gradient_callback(
    void *funcs, void *paint_data, HbColorLineAbi *color_line,
    float x0, float y0, float x1, float y1, float x2, float y2,
    void *opaque)
{
    LatxHbCallbackContext *context = opaque;
    HbColorLineBridge bridge;
    HbColorLineBridge *previous;

    if (context->native_callback) {
        ((void (*)(void *, void *, void *, float, float, float, float,
                   float, float, void *))context->native_callback)(
            funcs, paint_data, color_line, x0, y0, x1, y1, x2, y2,
            context->guest_user_data);
        return;
    }
    if (!hb_color_line_bridge_init(&bridge, color_line)) {
        kzt_groups_log_wrapper_limitation(
            harfbuzzName, "cannot bridge HarfBuzz gradient color line");
        return;
    }
    previous = hb_active_color_line_bridge;
    hb_active_color_line_bridge = &bridge;
    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, void *, float, float, float, float,
                 float, float, void *),
        "pppffffffp", funcs, paint_data, &bridge.guest,
        x0, y0, x1, y1, x2, y2);
    hb_active_color_line_bridge = previous;
    bridge.magic = 0;
}

static void hb_paint_sweep_callback(
    void *funcs, void *paint_data, HbColorLineAbi *color_line,
    float x0, float y0, float start_angle, float end_angle, void *opaque)
{
    LatxHbCallbackContext *context = opaque;
    HbColorLineBridge bridge;
    HbColorLineBridge *previous;

    if (context->native_callback) {
        ((void (*)(void *, void *, void *, float, float, float, float,
                   void *))context->native_callback)(
            funcs, paint_data, color_line, x0, y0, start_angle, end_angle,
            context->guest_user_data);
        return;
    }
    if (!hb_color_line_bridge_init(&bridge, color_line)) {
        kzt_groups_log_wrapper_limitation(
            harfbuzzName, "cannot bridge HarfBuzz sweep color line");
        return;
    }
    previous = hb_active_color_line_bridge;
    hb_active_color_line_bridge = &bridge;
    HB_CALLBACK_VOID(
        context,
        void (*)(void *, void *, void *, float, float, float, float,
                 void *),
        "pppffffp", funcs, paint_data, &bridge.guest,
        x0, y0, start_angle, end_angle);
    hb_active_color_line_bridge = previous;
    bridge.magic = 0;
}

static void hb_set_callback(void (*host_set)(void *, void *, void *, void *),
                            void *object, void *callback, void *user_data,
                            void *destroy, void *native_callback)
{
    LatxHbCallbackContext *context;

    if (!callback && !user_data && !destroy) {
        host_set(object, NULL, NULL, NULL);
        return;
    }
    context = latx_hb_callback_context_new(callback, user_data, destroy);
    if (!context) {
        latx_hb_invoke_guest_destroy(destroy, user_data);
        kzt_groups_log_wrapper_limitation(
            harfbuzzName, "cannot allocate HarfBuzz callback context");
        return;
    }
    host_set(object, callback ? native_callback : NULL, context,
             latx_hb_callback_context_destroy);
}

#define DEFINE_HB_CALLBACK_SETTER(name, callback) \
    EXPORT void my_##name(void *object, void *guest, void *data, \
                          void *destroy) \
    { \
        hb_set_callback((void (*)(void *, void *, void *, void *))my->name, \
                        object, guest, data, destroy, callback); \
    }

DEFINE_HB_CALLBACK_SETTER(hb_buffer_set_message_func,
                          hb_buffer_message_callback)
DEFINE_HB_CALLBACK_SETTER(hb_draw_funcs_set_close_path_func,
                          hb_draw_close_callback)
DEFINE_HB_CALLBACK_SETTER(hb_draw_funcs_set_cubic_to_func,
                          hb_draw_cubic_callback)
DEFINE_HB_CALLBACK_SETTER(hb_draw_funcs_set_line_to_func,
                          hb_draw_line_callback)
DEFINE_HB_CALLBACK_SETTER(hb_draw_funcs_set_move_to_func,
                          hb_draw_line_callback)
DEFINE_HB_CALLBACK_SETTER(hb_draw_funcs_set_quadratic_to_func,
                          hb_draw_quadratic_callback)
DEFINE_HB_CALLBACK_SETTER(hb_unicode_funcs_set_combining_class_func,
                          hb_unicode_simple_callback)
DEFINE_HB_CALLBACK_SETTER(hb_unicode_funcs_set_eastasian_width_func,
                          hb_unicode_simple_callback)
DEFINE_HB_CALLBACK_SETTER(hb_unicode_funcs_set_general_category_func,
                          hb_unicode_simple_callback)
DEFINE_HB_CALLBACK_SETTER(hb_unicode_funcs_set_mirroring_func,
                          hb_unicode_simple_callback)
DEFINE_HB_CALLBACK_SETTER(hb_unicode_funcs_set_script_func,
                          hb_unicode_simple_callback)
DEFINE_HB_CALLBACK_SETTER(hb_unicode_funcs_set_compose_func,
                          hb_unicode_compose_callback)
DEFINE_HB_CALLBACK_SETTER(hb_unicode_funcs_set_decompose_compatibility_func,
                          hb_unicode_decompose_compat_callback)
DEFINE_HB_CALLBACK_SETTER(hb_unicode_funcs_set_decompose_func,
                          hb_unicode_decompose_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_font_h_extents_func,
                          hb_font_extents_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_font_v_extents_func,
                          hb_font_extents_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_nominal_glyph_func,
                          hb_font_nominal_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_nominal_glyphs_func,
                          hb_font_nominals_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_variation_glyph_func,
                          hb_font_variation_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_h_advance_func,
                          hb_font_advance_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_v_advance_func,
                          hb_font_advance_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_h_advances_func,
                          hb_font_advances_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_v_advances_func,
                          hb_font_advances_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_h_origin_func,
                          hb_font_origin_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_v_origin_func,
                          hb_font_origin_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_h_kerning_func,
                          hb_font_kerning_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_v_kerning_func,
                          hb_font_kerning_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_extents_func,
                          hb_font_glyph_extents_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_contour_point_func,
                          hb_font_contour_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_name_func,
                          hb_font_glyph_name_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_from_name_func,
                          hb_font_from_name_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_func,
                          hb_font_glyph_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_glyph_shape_func,
                          hb_font_draw_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_draw_glyph_func,
                          hb_font_draw_callback)
DEFINE_HB_CALLBACK_SETTER(hb_font_funcs_set_paint_glyph_func,
                          hb_font_paint_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_push_transform_func,
                          hb_paint_push_transform_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_pop_transform_func,
                          hb_paint_simple_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_color_glyph_func,
                          hb_paint_color_glyph_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_push_clip_glyph_func,
                          hb_paint_clip_glyph_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_push_clip_rectangle_func,
                          hb_paint_clip_rectangle_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_pop_clip_func,
                          hb_paint_simple_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_color_func,
                          hb_paint_color_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_image_func,
                          hb_paint_image_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_linear_gradient_func,
                          hb_paint_gradient_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_radial_gradient_func,
                          hb_paint_gradient_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_sweep_gradient_func,
                          hb_paint_sweep_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_push_group_func,
                          hb_paint_simple_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_pop_group_func,
                          hb_paint_pop_group_callback)
DEFINE_HB_CALLBACK_SETTER(hb_paint_funcs_set_custom_palette_color_func,
                          hb_paint_custom_color_callback)

#undef DEFINE_HB_CALLBACK_SETTER

EXPORT void *my_hb_blob_create(void *data, uint32_t length,
                               uint32_t mode, void *user_data,
                               void *destroy)
{
    LatxHbCallbackContext *context;

    if (!destroy) {
        return my->hb_blob_create(data, length, mode, user_data, NULL);
    }
    context = latx_hb_callback_context_new(NULL, user_data, destroy);
    if (!context) {
        latx_hb_invoke_guest_destroy(destroy, user_data);
        return my->hb_blob_get_empty();
    }
    return my->hb_blob_create(data, length, mode, context,
                              latx_hb_callback_context_destroy);
}

EXPORT void *my_hb_blob_create_or_fail(void *data, uint32_t length,
                                       uint32_t mode, void *user_data,
                                       void *destroy)
{
    LatxHbCallbackContext *context;

    if (!destroy) {
        return my->hb_blob_create_or_fail(data, length, mode, user_data,
                                          NULL);
    }
    context = latx_hb_callback_context_new(NULL, user_data, destroy);
    if (!context) {
        latx_hb_invoke_guest_destroy(destroy, user_data);
        return NULL;
    }
    return my->hb_blob_create_or_fail(
        data, length, mode, context, latx_hb_callback_context_destroy);
}

EXPORT void *my_hb_blob_get_empty(void)
{
    return my->hb_blob_get_empty();
}

EXPORT void *my_hb_face_create_for_tables(void *callback,
                                          void *user_data,
                                          void *destroy)
{
    LatxHbCallbackContext *context = latx_hb_callback_context_new(
        callback, user_data, destroy);

    if (!context) {
        latx_hb_invoke_guest_destroy(destroy, user_data);
        return my->hb_face_get_empty();
    }
    return my->hb_face_create_for_tables(
        callback ? hb_reference_table_callback : NULL, context,
        latx_hb_callback_context_destroy);
}

EXPORT void *my_hb_face_get_empty(void)
{
    return my->hb_face_get_empty();
}

EXPORT void *my_hb_font_get_empty(void)
{
    return my->hb_font_get_empty();
}

#define DEFINE_HB_USER_DATA(name) \
    EXPORT int32_t my_##name##_set_user_data( \
        void *object, void *key, void *data, void *destroy, int32_t replace) \
    { \
        return latx_hb_set_user_data( \
            (LatxHbSetUserDataFunc)my->name##_set_user_data, \
            object, key, data, destroy, replace); \
    } \
    EXPORT void *my_##name##_get_user_data(void *object, void *key) \
    { \
        return latx_hb_get_user_data( \
            (LatxHbGetUserDataFunc)my->name##_get_user_data, \
            object, key); \
    }

DEFINE_HB_USER_DATA(hb_blob)
DEFINE_HB_USER_DATA(hb_buffer)
DEFINE_HB_USER_DATA(hb_draw_funcs)
DEFINE_HB_USER_DATA(hb_face)
DEFINE_HB_USER_DATA(hb_font_funcs)
DEFINE_HB_USER_DATA(hb_font)
DEFINE_HB_USER_DATA(hb_map)
DEFINE_HB_USER_DATA(hb_paint_funcs)
DEFINE_HB_USER_DATA(hb_set)
DEFINE_HB_USER_DATA(hb_shape_plan)
DEFINE_HB_USER_DATA(hb_unicode_funcs)

#undef DEFINE_HB_USER_DATA

EXPORT void my_hb_font_set_funcs(void *font, void *funcs, void *data,
                                 void *destroy)
{
    void *native = latx_hb_destroy_bridge(destroy, harfbuzzName);

    if (destroy && !native) {
        return;
    }
    my->hb_font_set_funcs(font, funcs, data, native);
}

EXPORT void my_hb_font_set_funcs_data(void *font, void *data,
                                      void *destroy)
{
    void *native = latx_hb_destroy_bridge(destroy, harfbuzzName);

    if (destroy && !native) {
        return;
    }
    my->hb_font_set_funcs_data(font, data, native);
}

EXPORT void *my_hb_ft_face_create(void *face, void *destroy)
{
    void *native = latx_hb_destroy_bridge(destroy, harfbuzzName);

    if (destroy && !native) {
        return my->hb_face_get_empty();
    }
    return my->hb_ft_face_create(face, native);
}

EXPORT void *my_hb_ft_font_create(void *face, void *destroy)
{
    void *native = latx_hb_destroy_bridge(destroy, harfbuzzName);

    if (destroy && !native) {
        return my->hb_font_get_empty();
    }
    return my->hb_ft_font_create(face, native);
}

static void hb_ft_release_face(void *face)
{
    latx_freetype_face_release(face);
}

EXPORT void *my_hb_ft_face_create_referenced(void *face)
{
    if (!latx_freetype_face_reference(face)) {
        kzt_groups_log_wrapper_limitation(
            harfbuzzName, "cannot retain FT_Face for HarfBuzz face");
        return my->hb_face_get_empty();
    }
    return my->hb_ft_face_create(face, hb_ft_release_face);
}

EXPORT void *my_hb_ft_font_create_referenced(void *face)
{
    if (!latx_freetype_face_reference(face)) {
        kzt_groups_log_wrapper_limitation(
            harfbuzzName, "cannot retain FT_Face for HarfBuzz font");
        return my->hb_font_get_empty();
    }
    return my->hb_ft_font_create(face, hb_ft_release_face);
}

EXPORT void *my_hb_ft_font_lock_face(void *font)
{
    HbFtFaceLock *tracker;
    void *face = my->hb_ft_font_lock_face(font);

    if (!face) {
        return NULL;
    }
    if (!latx_freetype_face_borrow(face)) {
        my->hb_ft_font_unlock_face(font);
        return NULL;
    }
    g_mutex_lock(&hb_ft_lock);
    for (tracker = hb_ft_face_locks; tracker; tracker = tracker->next) {
        if (tracker->font == font && tracker->face == face) {
            tracker->references++;
            g_mutex_unlock(&hb_ft_lock);
            return face;
        }
    }
    tracker = g_try_new0(HbFtFaceLock, 1);
    if (tracker) {
        tracker->font = font;
        tracker->face = face;
        tracker->references = 1;
        tracker->next = hb_ft_face_locks;
        hb_ft_face_locks = tracker;
    }
    g_mutex_unlock(&hb_ft_lock);
    if (!tracker) {
        latx_freetype_face_return(face);
        my->hb_ft_font_unlock_face(font);
        return NULL;
    }
    return face;
}

EXPORT void my_hb_ft_font_unlock_face(void *font)
{
    HbFtFaceLock **cursor;
    void *face = NULL;

    g_mutex_lock(&hb_ft_lock);
    cursor = &hb_ft_face_locks;
    while (*cursor) {
        HbFtFaceLock *tracker = *cursor;

        if (tracker->font != font) {
            cursor = &tracker->next;
            continue;
        }
        face = tracker->face;
        if (--tracker->references == 0) {
            *cursor = tracker->next;
            g_free(tracker);
        }
        break;
    }
    g_mutex_unlock(&hb_ft_lock);
    if (!face) {
        kzt_groups_log_wrapper_limitation(
            harfbuzzName, "HarfBuzz FT_Face unlock has no matching lock");
        return;
    }
    latx_freetype_face_return(face);
    my->hb_ft_font_unlock_face(font);
}

EXPORT uint32_t my_hb_color_line_get_color_stops(void *color_line,
                                                 uint32_t start,
                                                 void *count, void *stops)
{
    HbColorLineBridge *bridge = hb_active_color_line_bridge;

    if (bridge && color_line == &bridge->guest &&
        bridge->magic == HB_COLOR_LINE_MAGIC) {
        return hb_color_line_stops_bridge(
            &bridge->guest, bridge->guest.data, start, count, stops, bridge);
    }
    HbColorLineAbi *guest = color_line;
    HbGuestColorLineContext context = { .guest = guest };

    return hb_guest_color_line_stops(
        guest, guest ? guest->data : NULL, start, count, stops, &context);
}

EXPORT uint32_t my_hb_color_line_get_extend(void *color_line)
{
    HbColorLineBridge *bridge = hb_active_color_line_bridge;

    if (bridge && color_line == &bridge->guest &&
        bridge->magic == HB_COLOR_LINE_MAGIC) {
        return hb_color_line_extend_bridge(
            &bridge->guest, bridge->guest.data, bridge);
    }
    HbColorLineAbi *guest = color_line;
    HbGuestColorLineContext context = { .guest = guest };

    return hb_guest_color_line_extend(
        guest, guest ? guest->data : NULL, &context);
}

EXPORT void my_hb_paint_linear_gradient(
    void *funcs, void *paint_data, HbColorLineAbi *color_line,
    float x0, float y0, float x1, float y1, float x2, float y2)
{
    HbColorLineAbi host;
    HbGuestColorLineContext context;

    if (!hb_guest_color_line_init(&host, &context, color_line)) {
        return;
    }
    my->hb_paint_linear_gradient(funcs, paint_data, &host,
                                 x0, y0, x1, y1, x2, y2);
}

EXPORT void my_hb_paint_radial_gradient(
    void *funcs, void *paint_data, HbColorLineAbi *color_line,
    float x0, float y0, float r0, float x1, float y1, float r1)
{
    HbColorLineAbi host;
    HbGuestColorLineContext context;

    if (!hb_guest_color_line_init(&host, &context, color_line)) {
        return;
    }
    my->hb_paint_radial_gradient(funcs, paint_data, &host,
                                 x0, y0, r0, x1, y1, r1);
}

EXPORT void my_hb_paint_sweep_gradient(
    void *funcs, void *paint_data, HbColorLineAbi *color_line,
    float x0, float y0, float start_angle, float end_angle)
{
    HbColorLineAbi host;
    HbGuestColorLineContext context;

    if (!hb_guest_color_line_init(&host, &context, color_line)) {
        return;
    }
    my->hb_paint_sweep_gradient(funcs, paint_data, &host,
                                x0, y0, start_angle, end_angle);
}

EXPORT void *my_hb_glib_blob_create(void *bytes)
{
    LatxHbCallbackContext *context;
    void *reference;
    void *data;
    uintptr_t length = 0;

    if (!bytes) {
        return my->hb_blob_get_empty();
    }
    if ((!hb_guest_g_bytes_ref || !hb_guest_g_bytes_get_data ||
         !hb_guest_g_bytes_unref) && !hb_resolve_guest_glib()) {
        kzt_groups_log_wrapper_limitation(
            harfbuzzName, "cannot resolve guest GLib GBytes lifecycle");
        return my->hb_blob_get_empty();
    }
    reference = (void *)RunFunctionFmt(hb_guest_g_bytes_ref, "p", bytes);
    if (!reference) {
        return my->hb_blob_get_empty();
    }
    data = (void *)RunFunctionFmt(
        hb_guest_g_bytes_get_data, "pp", reference, &length);
    if (length > UINT32_MAX || (!data && length)) {
        RunFunctionFmt(hb_guest_g_bytes_unref, "p", reference);
        return my->hb_blob_get_empty();
    }
    context = latx_hb_callback_context_new(
        NULL, reference, (void *)hb_guest_g_bytes_unref);
    if (!context) {
        RunFunctionFmt(hb_guest_g_bytes_unref, "p", reference);
        return my->hb_blob_get_empty();
    }
    return my->hb_blob_create(
        data, (uint32_t)length, HB_MEMORY_MODE_READONLY, context,
        latx_hb_callback_context_destroy);
}

EXPORT void *my_hb_graphite2_face_get_gr_face(void *face)
{
    (void)face;
    kzt_groups_log_wrapper_limitation(
        harfbuzzName, "Graphite2 object interop is not enabled");
    return NULL;
}

EXPORT void *my_hb_graphite2_font_get_gr_font(void *font)
{
    (void)font;
    kzt_groups_log_wrapper_limitation(
        harfbuzzName, "Graphite2 object interop is not enabled");
    return NULL;
}

static void hb_ft_locks_clear(void)
{
    HbFtFaceLock *tracker;

    g_mutex_lock(&hb_ft_lock);
    while ((tracker = hb_ft_face_locks)) {
        hb_ft_face_locks = tracker->next;
        while (tracker->references > 0) {
            tracker->references--;
            latx_freetype_face_return(tracker->face);
            my->hb_ft_font_unlock_face(tracker->font);
        }
        g_free(tracker);
    }
    g_mutex_unlock(&hb_ft_lock);
}

static bool hb_resolve_guest_glib(void)
{
    uintptr_t end;

    hb_guest_g_bytes_ref = 0;
    hb_guest_g_bytes_get_data = 0;
    hb_guest_g_bytes_unref = 0;
    GetGlobalSymbolStartEnd(
        my_context->maplib, "g_bytes_ref", &hb_guest_g_bytes_ref,
        &end, NULL, -1, NULL);
    GetGlobalSymbolStartEnd(
        my_context->maplib, "g_bytes_get_data",
        &hb_guest_g_bytes_get_data, &end, NULL, -1, NULL);
    GetGlobalSymbolStartEnd(
        my_context->maplib, "g_bytes_unref", &hb_guest_g_bytes_unref,
        &end, NULL, -1, NULL);
    if (!hb_guest_g_bytes_ref) {
        hb_guest_g_bytes_ref =
            (uintptr_t)my_dlsym(NULL, (void *)"g_bytes_ref");
    }
    if (!hb_guest_g_bytes_get_data) {
        hb_guest_g_bytes_get_data =
            (uintptr_t)my_dlsym(NULL, (void *)"g_bytes_get_data");
    }
    if (!hb_guest_g_bytes_unref) {
        hb_guest_g_bytes_unref =
            (uintptr_t)my_dlsym(NULL, (void *)"g_bytes_unref");
    }
    if (!hb_guest_g_bytes_ref || !hb_guest_g_bytes_get_data ||
        !hb_guest_g_bytes_unref ||
        GetNativeFnc(hb_guest_g_bytes_ref) ||
        GetNativeFnc(hb_guest_g_bytes_get_data) ||
        GetNativeFnc(hb_guest_g_bytes_unref)) {
        hb_guest_g_bytes_ref = 0;
        hb_guest_g_bytes_get_data = 0;
        hb_guest_g_bytes_unref = 0;
        return false;
    }
    return true;
}

#define PRE_INIT_GUEST \
    do { \
        if (latx_harfbuzz_preflight_or_disable( \
                &box64->box64_ld_lib, harfbuzzName) != 0) { \
            return -1; \
        } \
    } while (0);

#define CUSTOM_INIT \
    getMy(lib);

#define CUSTOM_FINI \
    hb_ft_locks_clear(); \
    latx_hb_callback_cleanup(); \
    freeMy();

#include "wrappedlib_init.h"

#undef HB_CALLBACK_RETURN
#undef HB_CALLBACK_VOID

#pragma GCC diagnostic pop
