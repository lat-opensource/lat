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
#include "fileutils.h"
#include "kzt-groups.h"
#include "librarian.h"
#include "library_private.h"
#include "myalign.h"
#include "wrappedcairo-preflight.h"
#include "wrapper.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

const char *cairoName = "libcairo.so.2";
#define LIBNAME cairo

#include "generated/wrappedcairotypes.h"

#include "wrappercallback.h"

enum {
    CAIRO_STATUS_SUCCESS = 0,
    CAIRO_STATUS_NO_MEMORY = 1,
    CAIRO_STATUS_FONT_TYPE_MISMATCH = 27,
};

/* All public Cairo ABI structures are passed by pointer. */
typedef struct { double xx, yx, xy, yy, x0, y0; } cairo_matrix_abi_t;
typedef struct { double x_bearing, y_bearing, width, height, x_advance,
                        y_advance; } cairo_text_extents_abi_t;
typedef struct { double ascent, descent, height, max_x_advance,
                        max_y_advance; } cairo_font_extents_abi_t;
typedef struct { double x, y, width, height; } cairo_rectangle_abi_t;
typedef struct { int32_t x, y, width, height; } cairo_rectangle_int_abi_t;
typedef struct { unsigned long index; double x, y; } cairo_glyph_abi_t;
typedef struct { int32_t num_bytes, num_glyphs; } cairo_text_cluster_abi_t;
typedef union {
    struct { int32_t type, length; } header;
    struct { double x, y; } point;
} cairo_path_data_abi_t;
typedef struct {
    int32_t status;
    cairo_path_data_abi_t *data;
    int32_t num_data;
} cairo_path_abi_t;

_Static_assert(sizeof(cairo_matrix_abi_t) == 48, "cairo_matrix_t ABI");
_Static_assert(sizeof(cairo_text_extents_abi_t) == 48,
               "cairo_text_extents_t ABI");
_Static_assert(sizeof(cairo_font_extents_abi_t) == 40,
               "cairo_font_extents_t ABI");
_Static_assert(sizeof(cairo_rectangle_abi_t) == 32,
               "cairo_rectangle_t ABI");
_Static_assert(sizeof(cairo_rectangle_int_abi_t) == 16,
               "cairo_rectangle_int_t ABI");
_Static_assert(sizeof(cairo_glyph_abi_t) == 24, "cairo_glyph_t ABI");
_Static_assert(sizeof(cairo_text_cluster_abi_t) == 8,
               "cairo_text_cluster_t ABI");
_Static_assert(sizeof(cairo_path_data_abi_t) == 16,
               "cairo_path_data_t ABI");
_Static_assert(sizeof(cairo_path_abi_t) == 24, "cairo_path_t ABI");

#define CAIRO_CALLBACK_SLOT_COUNT 16
#define CAIRO_CALLBACK_SLOT_LIST(X) \
    X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) \
    X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15)

typedef struct CairoCallbackSlot {
    uintptr_t guest;
    unsigned int references;
} CairoCallbackSlot;

typedef enum CairoBindingKind {
    CAIRO_BINDING_SURFACE_STREAM,
    CAIRO_BINDING_DEVICE_STREAM,
    CAIRO_BINDING_USER_INIT,
    CAIRO_BINDING_USER_RENDER,
    CAIRO_BINDING_USER_RENDER_COLOR,
    CAIRO_BINDING_USER_TEXT_TO_GLYPHS,
    CAIRO_BINDING_USER_UNICODE_TO_GLYPH,
    CAIRO_BINDING_OBSERVER,
    CAIRO_BINDING_RASTER_ACQUIRE,
    CAIRO_BINDING_RASTER_RELEASE,
    CAIRO_BINDING_RASTER_SNAPSHOT,
    CAIRO_BINDING_RASTER_COPY,
    CAIRO_BINDING_RASTER_FINISH,
} CairoBindingKind;

typedef struct CairoCallbackBinding {
    void *object;
    CairoBindingKind kind;
    uintptr_t guest;
    CairoCallbackSlot *slots;
    struct CairoCallbackBinding *next;
} CairoCallbackBinding;

typedef enum CairoObjectKind {
    CAIRO_OBJECT_SURFACE,
    CAIRO_OBJECT_DEVICE,
    CAIRO_OBJECT_FONT_FACE,
    CAIRO_OBJECT_PATTERN,
} CairoObjectKind;

typedef struct CairoObjectTracker {
    void *object;
    CairoObjectKind kind;
    struct CairoObjectTracker *next;
} CairoObjectTracker;

static GMutex cairo_callback_lock;
static CairoCallbackBinding *cairo_bindings;
static CairoObjectTracker *cairo_trackers;
static char cairo_surface_tracker_key;
static char cairo_device_tracker_key;
static char cairo_font_face_tracker_key;
static char cairo_pattern_tracker_key;

static CairoCallbackSlot cairo_destroy_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_io_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_user_init_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_user_render_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_user_text_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_user_unicode_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_observer_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_raster_acquire_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_raster_release_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_raster_snapshot_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_raster_copy_slots[CAIRO_CALLBACK_SLOT_COUNT];
static CairoCallbackSlot cairo_raster_finish_slots[CAIRO_CALLBACK_SLOT_COUNT];

static void cairo_tracker_destroy(void *data);

static CairoObjectKind cairo_binding_object_kind(CairoBindingKind kind)
{
    switch (kind) {
    case CAIRO_BINDING_SURFACE_STREAM:
    case CAIRO_BINDING_OBSERVER:
        return CAIRO_OBJECT_SURFACE;
    case CAIRO_BINDING_DEVICE_STREAM:
        return CAIRO_OBJECT_DEVICE;
    case CAIRO_BINDING_USER_INIT:
    case CAIRO_BINDING_USER_RENDER:
    case CAIRO_BINDING_USER_RENDER_COLOR:
    case CAIRO_BINDING_USER_TEXT_TO_GLYPHS:
    case CAIRO_BINDING_USER_UNICODE_TO_GLYPH:
        return CAIRO_OBJECT_FONT_FACE;
    default:
        return CAIRO_OBJECT_PATTERN;
    }
}

static bool cairo_binding_needs_tracker(CairoBindingKind kind)
{
    return kind < CAIRO_BINDING_RASTER_ACQUIRE ||
           kind > CAIRO_BINDING_RASTER_FINISH;
}

static void *cairo_tracker_key(CairoObjectKind kind)
{
    switch (kind) {
    case CAIRO_OBJECT_SURFACE:
        return &cairo_surface_tracker_key;
    case CAIRO_OBJECT_DEVICE:
        return &cairo_device_tracker_key;
    case CAIRO_OBJECT_FONT_FACE:
        return &cairo_font_face_tracker_key;
    case CAIRO_OBJECT_PATTERN:
        return &cairo_pattern_tracker_key;
    }
    g_assert_not_reached();
}

static uint32_t cairo_set_tracker(CairoObjectTracker *tracker, void *data,
                                  void *destroy)
{
    void *key = cairo_tracker_key(tracker->kind);

    switch (tracker->kind) {
    case CAIRO_OBJECT_SURFACE:
        return my->cairo_surface_set_user_data(tracker->object, key,
                                                data, destroy);
    case CAIRO_OBJECT_DEVICE:
        return my->cairo_device_set_user_data(tracker->object, key,
                                               data, destroy);
    case CAIRO_OBJECT_FONT_FACE:
        return my->cairo_font_face_set_user_data(tracker->object, key,
                                                  data, destroy);
    case CAIRO_OBJECT_PATTERN:
        return my->cairo_pattern_set_user_data(tracker->object, key,
                                                data, destroy);
    }
    g_assert_not_reached();
}

static bool cairo_track_object_locked(void *object, CairoObjectKind kind)
{
    CairoObjectTracker *tracker;

    for (tracker = cairo_trackers; tracker; tracker = tracker->next) {
        if (tracker->object == object) {
            return true;
        }
    }
    tracker = calloc(1, sizeof(*tracker));
    if (!tracker) {
        return false;
    }
    tracker->object = object;
    tracker->kind = kind;
    if (cairo_set_tracker(tracker, tracker, cairo_tracker_destroy) !=
        CAIRO_STATUS_SUCCESS) {
        free(tracker);
        return false;
    }
    tracker->next = cairo_trackers;
    cairo_trackers = tracker;
    return true;
}

static uintptr_t cairo_slot_guest(CairoCallbackSlot *slots, size_t index)
{
    uintptr_t guest;

    g_mutex_lock(&cairo_callback_lock);
    guest = slots[index].guest;
    g_mutex_unlock(&cairo_callback_lock);
    return guest;
}

static void cairo_slot_release_locked(CairoCallbackSlot *slots,
                                      uintptr_t guest)
{
    if (!slots || !guest) {
        return;
    }
    for (size_t i = 0; i < CAIRO_CALLBACK_SLOT_COUNT; i++) {
        if (slots[i].guest != guest) {
            continue;
        }
        if (slots[i].references && --slots[i].references == 0) {
            slots[i].guest = 0;
        }
        return;
    }
}

static void cairo_slot_release(CairoCallbackSlot *slots, uintptr_t guest)
{
    g_mutex_lock(&cairo_callback_lock);
    cairo_slot_release_locked(slots, guest);
    g_mutex_unlock(&cairo_callback_lock);
}

static void *cairo_slot_acquire(CairoCallbackSlot *slots,
                                void *const *thunks, void *callback,
                                const char *kind,
                                CairoCallbackSlot **managed_slots)
{
    void *native;
    uintptr_t guest = (uintptr_t)callback;

    *managed_slots = NULL;
    if (!callback) {
        return NULL;
    }
    native = GetNativeFnc(guest);
    if (native) {
        return native;
    }

    g_mutex_lock(&cairo_callback_lock);
    for (size_t i = 0; i < CAIRO_CALLBACK_SLOT_COUNT; i++) {
        if (slots[i].guest == guest) {
            slots[i].references++;
            *managed_slots = slots;
            native = thunks[i];
            g_mutex_unlock(&cairo_callback_lock);
            return native;
        }
    }
    for (size_t i = 0; i < CAIRO_CALLBACK_SLOT_COUNT; i++) {
        if (!slots[i].guest) {
            slots[i].guest = guest;
            slots[i].references = 1;
            *managed_slots = slots;
            native = thunks[i];
            g_mutex_unlock(&cairo_callback_lock);
            return native;
        }
    }
    g_mutex_unlock(&cairo_callback_lock);
    kzt_groups_log_wrapper_limitation(cairoName, kind);
    return NULL;
}

static void *cairo_slot_acquire_forced(CairoCallbackSlot *slots,
                                       void *const *thunks, void *callback,
                                       const char *kind,
                                       CairoCallbackSlot **managed_slots)
{
    void *native;
    uintptr_t guest = (uintptr_t)callback;

    *managed_slots = NULL;
    if (!callback) {
        return NULL;
    }
    g_mutex_lock(&cairo_callback_lock);
    for (size_t i = 0; i < CAIRO_CALLBACK_SLOT_COUNT; i++) {
        if (slots[i].guest == guest) {
            slots[i].references++;
            *managed_slots = slots;
            native = thunks[i];
            g_mutex_unlock(&cairo_callback_lock);
            return native;
        }
    }
    for (size_t i = 0; i < CAIRO_CALLBACK_SLOT_COUNT; i++) {
        if (!slots[i].guest) {
            slots[i].guest = guest;
            slots[i].references = 1;
            *managed_slots = slots;
            native = thunks[i];
            g_mutex_unlock(&cairo_callback_lock);
            return native;
        }
    }
    g_mutex_unlock(&cairo_callback_lock);
    kzt_groups_log_wrapper_limitation(cairoName, kind);
    return NULL;
}

static void cairo_binding_replace(void *object, CairoBindingKind kind,
                                  void *guest, CairoCallbackSlot *slots)
{
    CairoCallbackBinding **cursor;
    CairoCallbackBinding *binding = guest ? calloc(1, sizeof(*binding)) : NULL;
    bool tracked = !guest;

    if (binding) {
        binding->object = object;
        binding->kind = kind;
        binding->guest = (uintptr_t)guest;
        binding->slots = slots;
    }

    g_mutex_lock(&cairo_callback_lock);
    if (guest && binding && cairo_binding_needs_tracker(kind)) {
        tracked = cairo_track_object_locked(
            object, cairo_binding_object_kind(kind));
    } else if (guest && binding) {
        tracked = true;
    }
    cursor = &cairo_bindings;
    while (*cursor) {
        CairoCallbackBinding *old = *cursor;

        if (old->object != object || old->kind != kind) {
            cursor = &old->next;
            continue;
        }
        *cursor = old->next;
        cairo_slot_release_locked(old->slots, old->guest);
        free(old);
    }
    if (binding && tracked) {
        binding->next = cairo_bindings;
        cairo_bindings = binding;
    } else {
        free(binding);
    }
    g_mutex_unlock(&cairo_callback_lock);

    if (guest && (!binding || !tracked)) {
        kzt_groups_log_wrapper_limitation(
            cairoName, "cannot track Cairo callback lifetime; retaining its "
                       "native thunk slot");
    }
}

static void cairo_binding_add(void *object, CairoBindingKind kind,
                              void *guest, CairoCallbackSlot *slots)
{
    CairoCallbackBinding *binding;

    if (!guest) {
        return;
    }
    binding = calloc(1, sizeof(*binding));
    if (!binding) {
        kzt_groups_log_wrapper_limitation(
            cairoName, "cannot track Cairo callback lifetime");
        return;
    }
    binding->object = object;
    binding->kind = kind;
    binding->guest = (uintptr_t)guest;
    binding->slots = slots;

    g_mutex_lock(&cairo_callback_lock);
    if (cairo_binding_needs_tracker(kind) &&
        !cairo_track_object_locked(object,
                                   cairo_binding_object_kind(kind))) {
        g_mutex_unlock(&cairo_callback_lock);
        free(binding);
        kzt_groups_log_wrapper_limitation(
            cairoName, "cannot track Cairo callback lifetime; retaining its "
                       "native thunk slot");
        return;
    }
    binding->next = cairo_bindings;
    cairo_bindings = binding;
    g_mutex_unlock(&cairo_callback_lock);
}

static void *cairo_binding_guest(void *object, CairoBindingKind kind)
{
    uintptr_t guest = 0;

    g_mutex_lock(&cairo_callback_lock);
    for (CairoCallbackBinding *binding = cairo_bindings; binding;
         binding = binding->next) {
        if (binding->object == object && binding->kind == kind) {
            guest = binding->guest;
            break;
        }
    }
    g_mutex_unlock(&cairo_callback_lock);
    return (void *)guest;
}

static void cairo_binding_release_object(void *object)
{
    CairoCallbackBinding **cursor;

    g_mutex_lock(&cairo_callback_lock);
    cursor = &cairo_bindings;
    while (*cursor) {
        CairoCallbackBinding *binding = *cursor;

        if (binding->object != object) {
            cursor = &binding->next;
            continue;
        }
        *cursor = binding->next;
        cairo_slot_release_locked(binding->slots, binding->guest);
        free(binding);
    }
    g_mutex_unlock(&cairo_callback_lock);
}

static void cairo_tracker_destroy(void *data)
{
    CairoObjectTracker *tracker = data;
    CairoObjectTracker **cursor;

    g_mutex_lock(&cairo_callback_lock);
    cursor = &cairo_trackers;
    while (*cursor && *cursor != tracker) {
        cursor = &(*cursor)->next;
    }
    if (*cursor) {
        *cursor = tracker->next;
    }
    g_mutex_unlock(&cairo_callback_lock);
    cairo_binding_release_object(tracker->object);
    free(tracker);
}

static bool cairo_binding_is_raster(CairoBindingKind kind)
{
    return kind >= CAIRO_BINDING_RASTER_ACQUIRE &&
           kind <= CAIRO_BINDING_RASTER_FINISH;
}

static bool cairo_binding_clone_raster(void *source, void *destination)
{
    CairoCallbackBinding *copies = NULL;
    CairoCallbackBinding *copy;

    g_mutex_lock(&cairo_callback_lock);
    for (CairoCallbackBinding *binding = cairo_bindings; binding;
         binding = binding->next) {
        if (binding->object != source ||
            !cairo_binding_is_raster(binding->kind)) {
            continue;
        }
        /*
         * The guest copy callback may replace a callback on the destination.
         * Keep that replacement instead of cloning the source binding over it.
         */
        for (copy = cairo_bindings; copy; copy = copy->next) {
            if (copy->object == destination &&
                copy->kind == binding->kind) {
                break;
            }
        }
        if (copy) {
            continue;
        }
        copy = calloc(1, sizeof(*copy));
        if (!copy) {
            while ((copy = copies)) {
                copies = copy->next;
                free(copy);
            }
            g_mutex_unlock(&cairo_callback_lock);
            kzt_groups_log_wrapper_limitation(
                cairoName, "cannot clone raster callback lifetime");
            return false;
        }
        *copy = *binding;
        copy->object = destination;
        copy->next = copies;
        copies = copy;
    }

    while ((copy = copies)) {
        copies = copy->next;
        if (copy->slots) {
            for (size_t i = 0; i < CAIRO_CALLBACK_SLOT_COUNT; i++) {
                if (copy->slots[i].guest == copy->guest) {
                    copy->slots[i].references++;
                    break;
                }
            }
        }
        copy->next = cairo_bindings;
        cairo_bindings = copy;
    }
    g_mutex_unlock(&cairo_callback_lock);
    return true;
}

#define DEFINE_DESTROY_THUNK(N) \
    static void cairo_destroy_thunk_##N(void *data) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_destroy_slots, N); \
        if (guest) { \
            RunFunctionFmt(guest, "p", data); \
            cairo_slot_release(cairo_destroy_slots, guest); \
        } \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_DESTROY_THUNK)
#undef DEFINE_DESTROY_THUNK

#define DEFINE_IO_THUNK(N) \
    static uint32_t cairo_io_thunk_##N(void *closure, void *data, uint32_t length) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_io_slots, N); \
        return guest ? (uint32_t)RunFunctionFmt(guest, "ppu", closure, data, length) \
                     : CAIRO_STATUS_NO_MEMORY; \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_IO_THUNK)
#undef DEFINE_IO_THUNK

#define DEFINE_USER_INIT_THUNK(N) \
    static uint32_t cairo_user_init_thunk_##N(void *font, void *cr, void *extents) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_user_init_slots, N); \
        return guest ? (uint32_t)RunFunctionFmt(guest, "ppp", font, cr, extents) \
                     : CAIRO_STATUS_NO_MEMORY; \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_USER_INIT_THUNK)
#undef DEFINE_USER_INIT_THUNK

#define DEFINE_USER_RENDER_THUNK(N) \
    static uint32_t cairo_user_render_thunk_##N(void *font, unsigned long glyph, \
                                                void *cr, void *extents) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_user_render_slots, N); \
        return guest ? (uint32_t)RunFunctionFmt(guest, "pLpp", font, glyph, cr, extents) \
                     : CAIRO_STATUS_NO_MEMORY; \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_USER_RENDER_THUNK)
#undef DEFINE_USER_RENDER_THUNK

#define DEFINE_USER_TEXT_THUNK(N) \
    static uint32_t cairo_user_text_thunk_##N(void *font, void *utf8, int length, \
                                              void *glyphs, void *num_glyphs, \
                                              void *clusters, void *num_clusters, \
                                              void *flags) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_user_text_slots, N); \
        return guest ? (uint32_t)RunFunctionFmt(guest, "ppippppp", font, utf8, \
                                                length, glyphs, num_glyphs, clusters, \
                                                num_clusters, flags) \
                     : CAIRO_STATUS_NO_MEMORY; \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_USER_TEXT_THUNK)
#undef DEFINE_USER_TEXT_THUNK

#define DEFINE_USER_UNICODE_THUNK(N) \
    static uint32_t cairo_user_unicode_thunk_##N(void *font, unsigned long unicode, \
                                                 void *glyph) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_user_unicode_slots, N); \
        return guest ? (uint32_t)RunFunctionFmt(guest, "pLp", font, unicode, glyph) \
                     : CAIRO_STATUS_NO_MEMORY; \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_USER_UNICODE_THUNK)
#undef DEFINE_USER_UNICODE_THUNK

#define DEFINE_OBSERVER_THUNK(N) \
    static void cairo_observer_thunk_##N(void *observer, void *target, void *data) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_observer_slots, N); \
        if (guest) RunFunctionFmt(guest, "ppp", observer, target, data); \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_OBSERVER_THUNK)
#undef DEFINE_OBSERVER_THUNK

#define DEFINE_RASTER_ACQUIRE_THUNK(N) \
    static void *cairo_raster_acquire_thunk_##N(void *pattern, void *data, \
                                                void *target, void *extents) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_raster_acquire_slots, N); \
        return guest ? (void *)(uintptr_t)RunFunctionFmt(guest, "pppp", pattern, data, \
                                                        target, extents) : NULL; \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_RASTER_ACQUIRE_THUNK)
#undef DEFINE_RASTER_ACQUIRE_THUNK

#define DEFINE_RASTER_RELEASE_THUNK(N) \
    static void cairo_raster_release_thunk_##N(void *pattern, void *data, void *surface) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_raster_release_slots, N); \
        if (guest) RunFunctionFmt(guest, "ppp", pattern, data, surface); \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_RASTER_RELEASE_THUNK)
#undef DEFINE_RASTER_RELEASE_THUNK

#define DEFINE_RASTER_SNAPSHOT_THUNK(N) \
    static uint32_t cairo_raster_snapshot_thunk_##N(void *pattern, void *data) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_raster_snapshot_slots, N); \
        return guest ? (uint32_t)RunFunctionFmt(guest, "pp", pattern, data) \
                     : CAIRO_STATUS_NO_MEMORY; \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_RASTER_SNAPSHOT_THUNK)
#undef DEFINE_RASTER_SNAPSHOT_THUNK

#define DEFINE_RASTER_COPY_THUNK(N) \
    static uint32_t cairo_raster_copy_thunk_##N(void *pattern, void *data, void *other) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_raster_copy_slots, N); \
        uint32_t status = guest \
            ? (uint32_t)RunFunctionFmt(guest, "ppp", pattern, data, other) \
            : CAIRO_STATUS_NO_MEMORY; \
        if (status == CAIRO_STATUS_SUCCESS && \
            !cairo_binding_clone_raster(other, pattern)) \
            status = CAIRO_STATUS_NO_MEMORY; \
        return status; \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_RASTER_COPY_THUNK)
#undef DEFINE_RASTER_COPY_THUNK

static void cairo_raster_finish_cleanup(void *pattern, void *data)
{
    (void)data;
    cairo_binding_release_object(pattern);
}

#define DEFINE_RASTER_FINISH_THUNK(N) \
    static void cairo_raster_finish_thunk_##N(void *pattern, void *data) \
    { \
        uintptr_t guest = cairo_slot_guest(cairo_raster_finish_slots, N); \
        void *native = GetNativeFnc(guest); \
        if (native) \
            ((void (*)(void *, void *))native)(pattern, data); \
        else if (guest) \
            RunFunctionFmt(guest, "pp", pattern, data); \
        cairo_raster_finish_cleanup(pattern, data); \
    }
CAIRO_CALLBACK_SLOT_LIST(DEFINE_RASTER_FINISH_THUNK)
#undef DEFINE_RASTER_FINISH_THUNK

#define DESTROY_THUNK_ADDRESS(N) (void *)cairo_destroy_thunk_##N,
static void *const cairo_destroy_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(DESTROY_THUNK_ADDRESS)
};
#undef DESTROY_THUNK_ADDRESS
#define IO_THUNK_ADDRESS(N) (void *)cairo_io_thunk_##N,
static void *const cairo_io_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(IO_THUNK_ADDRESS)
};
#undef IO_THUNK_ADDRESS
#define USER_INIT_THUNK_ADDRESS(N) (void *)cairo_user_init_thunk_##N,
static void *const cairo_user_init_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(USER_INIT_THUNK_ADDRESS)
};
#undef USER_INIT_THUNK_ADDRESS
#define USER_RENDER_THUNK_ADDRESS(N) (void *)cairo_user_render_thunk_##N,
static void *const cairo_user_render_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(USER_RENDER_THUNK_ADDRESS)
};
#undef USER_RENDER_THUNK_ADDRESS
#define USER_TEXT_THUNK_ADDRESS(N) (void *)cairo_user_text_thunk_##N,
static void *const cairo_user_text_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(USER_TEXT_THUNK_ADDRESS)
};
#undef USER_TEXT_THUNK_ADDRESS
#define USER_UNICODE_THUNK_ADDRESS(N) (void *)cairo_user_unicode_thunk_##N,
static void *const cairo_user_unicode_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(USER_UNICODE_THUNK_ADDRESS)
};
#undef USER_UNICODE_THUNK_ADDRESS
#define OBSERVER_THUNK_ADDRESS(N) (void *)cairo_observer_thunk_##N,
static void *const cairo_observer_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(OBSERVER_THUNK_ADDRESS)
};
#undef OBSERVER_THUNK_ADDRESS
#define RASTER_ACQUIRE_THUNK_ADDRESS(N) (void *)cairo_raster_acquire_thunk_##N,
static void *const cairo_raster_acquire_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(RASTER_ACQUIRE_THUNK_ADDRESS)
};
#undef RASTER_ACQUIRE_THUNK_ADDRESS
#define RASTER_RELEASE_THUNK_ADDRESS(N) (void *)cairo_raster_release_thunk_##N,
static void *const cairo_raster_release_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(RASTER_RELEASE_THUNK_ADDRESS)
};
#undef RASTER_RELEASE_THUNK_ADDRESS
#define RASTER_SNAPSHOT_THUNK_ADDRESS(N) (void *)cairo_raster_snapshot_thunk_##N,
static void *const cairo_raster_snapshot_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(RASTER_SNAPSHOT_THUNK_ADDRESS)
};
#undef RASTER_SNAPSHOT_THUNK_ADDRESS
#define RASTER_COPY_THUNK_ADDRESS(N) (void *)cairo_raster_copy_thunk_##N,
static void *const cairo_raster_copy_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(RASTER_COPY_THUNK_ADDRESS)
};
#undef RASTER_COPY_THUNK_ADDRESS
#define RASTER_FINISH_THUNK_ADDRESS(N) (void *)cairo_raster_finish_thunk_##N,
static void *const cairo_raster_finish_thunks[] = {
    CAIRO_CALLBACK_SLOT_LIST(RASTER_FINISH_THUNK_ADDRESS)
};
#undef RASTER_FINISH_THUNK_ADDRESS

static void *cairo_acquire_destroy(void *callback, CairoCallbackSlot **slots)
{
    return cairo_slot_acquire(cairo_destroy_slots, cairo_destroy_thunks,
                              callback, "destroy callback slots exhausted",
                              slots);
}

static void *cairo_acquire_io(void *callback, CairoCallbackSlot **slots)
{
    return cairo_slot_acquire(cairo_io_slots, cairo_io_thunks, callback,
                              "read/write callback slots exhausted", slots);
}

#define DEFINE_USER_CALLBACK_ACCESSOR(name, slot_array, thunk_array, message) \
    static void *cairo_acquire_##name(void *callback, CairoCallbackSlot **slots) \
    { \
        return cairo_slot_acquire(slot_array, thunk_array, callback, message, slots); \
    }
DEFINE_USER_CALLBACK_ACCESSOR(user_init, cairo_user_init_slots,
                              cairo_user_init_thunks,
                              "user-font init callback slots exhausted")
DEFINE_USER_CALLBACK_ACCESSOR(user_render, cairo_user_render_slots,
                              cairo_user_render_thunks,
                              "user-font render callback slots exhausted")
DEFINE_USER_CALLBACK_ACCESSOR(user_text, cairo_user_text_slots,
                              cairo_user_text_thunks,
                              "user-font text callback slots exhausted")
DEFINE_USER_CALLBACK_ACCESSOR(user_unicode, cairo_user_unicode_slots,
                              cairo_user_unicode_thunks,
                              "user-font unicode callback slots exhausted")
DEFINE_USER_CALLBACK_ACCESSOR(observer, cairo_observer_slots,
                              cairo_observer_thunks,
                              "observer callback slots exhausted")
DEFINE_USER_CALLBACK_ACCESSOR(raster_acquire, cairo_raster_acquire_slots,
                              cairo_raster_acquire_thunks,
                              "raster acquire callback slots exhausted")
DEFINE_USER_CALLBACK_ACCESSOR(raster_release, cairo_raster_release_slots,
                              cairo_raster_release_thunks,
                              "raster release callback slots exhausted")
DEFINE_USER_CALLBACK_ACCESSOR(raster_snapshot, cairo_raster_snapshot_slots,
                              cairo_raster_snapshot_thunks,
                              "raster snapshot callback slots exhausted")
DEFINE_USER_CALLBACK_ACCESSOR(raster_copy, cairo_raster_copy_slots,
                              cairo_raster_copy_thunks,
                              "raster copy callback slots exhausted")
#undef DEFINE_USER_CALLBACK_ACCESSOR

static void *cairo_acquire_raster_finish(void *callback,
                                         CairoCallbackSlot **slots)
{
    return cairo_slot_acquire_forced(
        cairo_raster_finish_slots, cairo_raster_finish_thunks, callback,
        "raster finish callback slots exhausted", slots);
}

static void cairo_ensure_raster_cleanup(void *pattern)
{
    void *finish = my->cairo_raster_source_pattern_get_finish(pattern);

    if (!finish) {
        my->cairo_raster_source_pattern_set_finish(
            pattern, cairo_raster_finish_cleanup);
    }
}

static void cairo_release_sync_callback(CairoCallbackSlot *slots, void *guest)
{
    if (slots) {
        cairo_slot_release(slots, (uintptr_t)guest);
    }
}

static uint32_t cairo_io_failure(void *closure, void *data, uint32_t length)
{
    (void)closure;
    (void)data;
    (void)length;
    return CAIRO_STATUS_NO_MEMORY;
}

static uint32_t cairo_ft_interop_failure(void *font, void *cr, void *extents)
{
    (void)font;
    (void)cr;
    (void)extents;
    return CAIRO_STATUS_FONT_TYPE_MISMATCH;
}

/*
 * LAT does not yet wrap FreeType or Fontconfig.  Their opaque guest objects
 * cannot be passed to host Cairo, and a host FT_Face cannot be returned to
 * guest code.  Return a valid host Cairo face that fails explicitly on use.
 */
static void *cairo_ft_unsupported_font_face(void)
{
    void *font_face = my->cairo_user_font_face_create();

    if (font_face) {
        my->cairo_user_font_face_set_init_func(font_face,
                                               cairo_ft_interop_failure);
    }
    kzt_groups_log_wrapper_limitation(
        cairoName, "Cairo FreeType/Fontconfig interop requires native "
                   "FreeType and Fontconfig wrappers");
    return font_face;
}

EXPORT void *my_cairo_user_font_face_create(void)
{
    return my->cairo_user_font_face_create();
}

EXPORT void *my_cairo_ft_font_face_create_for_ft_face(void *face,
                                                       int32_t load_flags)
{
    (void)face;
    (void)load_flags;
    return cairo_ft_unsupported_font_face();
}

EXPORT void *my_cairo_ft_font_face_create_for_pattern(void *pattern)
{
    (void)pattern;
    return cairo_ft_unsupported_font_face();
}

EXPORT void my_cairo_ft_font_options_substitute(void *pattern, void *options)
{
    (void)pattern;
    (void)options;
    kzt_groups_log_wrapper_limitation(
        cairoName, "Cairo Fontconfig interop requires a native Fontconfig "
                   "wrapper");
}

EXPORT void *my_cairo_ft_scaled_font_lock_face(void *scaled_font)
{
    (void)scaled_font;
    kzt_groups_log_wrapper_limitation(
        cairoName, "returning a host FT_Face to guest code is unsupported");
    return NULL;
}

EXPORT void my_cairo_ft_scaled_font_unlock_face(void *scaled_font)
{
    (void)scaled_font;
}

#define DEFINE_USER_DATA_WRAPPER(name, object_type) \
    EXPORT uint32_t my_##name(void *object, void *key, void *data, void *destroy) \
    { \
        CairoCallbackSlot *slots; \
        void *native = cairo_acquire_destroy(destroy, &slots); \
        uint32_t status; \
        if (destroy && !native) return CAIRO_STATUS_NO_MEMORY; \
        status = my->name(object, key, data, native); \
        if (status != CAIRO_STATUS_SUCCESS) \
            cairo_release_sync_callback(slots, destroy); \
        return status; \
    }
DEFINE_USER_DATA_WRAPPER(cairo_device_set_user_data, device)
DEFINE_USER_DATA_WRAPPER(cairo_font_face_set_user_data, font_face)
DEFINE_USER_DATA_WRAPPER(cairo_pattern_set_user_data, pattern)
DEFINE_USER_DATA_WRAPPER(cairo_scaled_font_set_user_data, scaled_font)
DEFINE_USER_DATA_WRAPPER(cairo_set_user_data, context)
DEFINE_USER_DATA_WRAPPER(cairo_surface_set_user_data, surface)
#undef DEFINE_USER_DATA_WRAPPER

EXPORT uint32_t my_cairo_surface_set_mime_data(void *surface, void *mime,
                                                void *data, uintptr_t length,
                                                void *destroy, void *closure)
{
    CairoCallbackSlot *slots;
    void *native = cairo_acquire_destroy(destroy, &slots);
    uint32_t status;

    if (destroy && !native) {
        return CAIRO_STATUS_NO_MEMORY;
    }
    status = my->cairo_surface_set_mime_data(surface, mime, data, length,
                                             native, closure);
    if (status != CAIRO_STATUS_SUCCESS) {
        cairo_release_sync_callback(slots, destroy);
    }
    return status;
}

static void *cairo_stream_surface(void *(*create)(void *, void *, double, double),
                                  void *callback, void *closure,
                                  double width, double height)
{
    CairoCallbackSlot *slots;
    void *native = cairo_acquire_io(callback, &slots);
    void *surface;

    if (callback && !native) {
        return create(cairo_io_failure, NULL, width, height);
    }
    surface = create(native, closure, width, height);
    if (surface && slots) {
        cairo_binding_add(surface, CAIRO_BINDING_SURFACE_STREAM,
                          callback, slots);
    } else {
        cairo_release_sync_callback(slots, callback);
    }
    return surface;
}

EXPORT void *my_cairo_pdf_surface_create_for_stream(void *callback,
                                                     void *closure,
                                                     double width,
                                                     double height)
{
    return cairo_stream_surface(my->cairo_pdf_surface_create_for_stream,
                                callback, closure, width, height);
}

EXPORT void *my_cairo_ps_surface_create_for_stream(void *callback,
                                                    void *closure,
                                                    double width,
                                                    double height)
{
    return cairo_stream_surface(my->cairo_ps_surface_create_for_stream,
                                callback, closure, width, height);
}

EXPORT void *my_cairo_svg_surface_create_for_stream(void *callback,
                                                     void *closure,
                                                     double width,
                                                     double height)
{
    return cairo_stream_surface(my->cairo_svg_surface_create_for_stream,
                                callback, closure, width, height);
}

EXPORT void *my_cairo_script_create_for_stream(void *callback, void *closure)
{
    CairoCallbackSlot *slots;
    void *native = cairo_acquire_io(callback, &slots);
    void *device;

    if (callback && !native) {
        return my->cairo_script_create_for_stream(cairo_io_failure, NULL);
    }
    device = my->cairo_script_create_for_stream(native, closure);
    if (device && slots) {
        cairo_binding_add(device, CAIRO_BINDING_DEVICE_STREAM,
                          callback, slots);
    } else {
        cairo_release_sync_callback(slots, callback);
    }
    return device;
}

EXPORT void *my_cairo_image_surface_create_from_png_stream(void *callback,
                                                            void *closure)
{
    CairoCallbackSlot *slots;
    void *native = cairo_acquire_io(callback, &slots);
    void *surface;

    if (callback && !native) {
        return my->cairo_image_surface_create_from_png_stream(
            cairo_io_failure, NULL);
    }
    surface = my->cairo_image_surface_create_from_png_stream(native, closure);
    cairo_release_sync_callback(slots, callback);
    return surface;
}

EXPORT uint32_t my_cairo_surface_write_to_png_stream(void *surface,
                                                      void *callback,
                                                      void *closure)
{
    CairoCallbackSlot *slots;
    void *native = cairo_acquire_io(callback, &slots);
    uint32_t status;

    if (callback && !native) {
        return CAIRO_STATUS_NO_MEMORY;
    }
    status = my->cairo_surface_write_to_png_stream(surface, native, closure);
    cairo_release_sync_callback(slots, callback);
    return status;
}

EXPORT uint32_t my_cairo_surface_observer_print(void *surface,
                                                 void *callback,
                                                 void *closure)
{
    CairoCallbackSlot *slots;
    void *native = cairo_acquire_io(callback, &slots);
    uint32_t status;

    if (callback && !native) {
        return CAIRO_STATUS_NO_MEMORY;
    }
    status = my->cairo_surface_observer_print(surface, native, closure);
    cairo_release_sync_callback(slots, callback);
    return status;
}

EXPORT uint32_t my_cairo_device_observer_print(void *device,
                                                void *callback,
                                                void *closure)
{
    CairoCallbackSlot *slots;
    void *native = cairo_acquire_io(callback, &slots);
    uint32_t status;

    if (callback && !native) {
        return CAIRO_STATUS_NO_MEMORY;
    }
    status = my->cairo_device_observer_print(device, native, closure);
    cairo_release_sync_callback(slots, callback);
    return status;
}

#define DEFINE_OBSERVER_ADD_WRAPPER(name) \
    EXPORT uint32_t my_##name(void *surface, void *callback, void *data) \
    { \
        CairoCallbackSlot *slots; \
        void *native = cairo_acquire_observer(callback, &slots); \
        uint32_t status; \
        if (callback && !native) return CAIRO_STATUS_NO_MEMORY; \
        status = my->name(surface, native, data); \
        if (status == CAIRO_STATUS_SUCCESS && slots) \
            cairo_binding_add(surface, CAIRO_BINDING_OBSERVER, callback, slots); \
        else \
            cairo_release_sync_callback(slots, callback); \
        return status; \
    }
DEFINE_OBSERVER_ADD_WRAPPER(cairo_surface_observer_add_fill_callback)
DEFINE_OBSERVER_ADD_WRAPPER(cairo_surface_observer_add_finish_callback)
DEFINE_OBSERVER_ADD_WRAPPER(cairo_surface_observer_add_flush_callback)
DEFINE_OBSERVER_ADD_WRAPPER(cairo_surface_observer_add_glyphs_callback)
DEFINE_OBSERVER_ADD_WRAPPER(cairo_surface_observer_add_mask_callback)
DEFINE_OBSERVER_ADD_WRAPPER(cairo_surface_observer_add_paint_callback)
DEFINE_OBSERVER_ADD_WRAPPER(cairo_surface_observer_add_stroke_callback)
#undef DEFINE_OBSERVER_ADD_WRAPPER

static void cairo_set_user_callback(void *font_face, void *callback,
                                    CairoBindingKind kind,
                                    void *(*acquire)(void *, CairoCallbackSlot **),
                                    void (*setter)(void *, void *))
{
    CairoCallbackSlot *slots;
    void *native = acquire(callback, &slots);

    if (callback && !native) {
        return;
    }
    setter(font_face, native);
    cairo_binding_replace(font_face, kind, callback, slots);
}

EXPORT void my_cairo_user_font_face_set_init_func(void *font_face,
                                                   void *callback)
{
    cairo_set_user_callback(font_face, callback, CAIRO_BINDING_USER_INIT,
                            cairo_acquire_user_init,
                            my->cairo_user_font_face_set_init_func);
}

EXPORT void my_cairo_user_font_face_set_render_glyph_func(void *font_face,
                                                           void *callback)
{
    cairo_set_user_callback(font_face, callback, CAIRO_BINDING_USER_RENDER,
                            cairo_acquire_user_render,
                            my->cairo_user_font_face_set_render_glyph_func);
}

EXPORT void my_cairo_user_font_face_set_render_color_glyph_func(
    void *font_face, void *callback)
{
    cairo_set_user_callback(
        font_face, callback, CAIRO_BINDING_USER_RENDER_COLOR,
        cairo_acquire_user_render,
        my->cairo_user_font_face_set_render_color_glyph_func);
}

EXPORT void my_cairo_user_font_face_set_text_to_glyphs_func(
    void *font_face, void *callback)
{
    cairo_set_user_callback(
        font_face, callback, CAIRO_BINDING_USER_TEXT_TO_GLYPHS,
        cairo_acquire_user_text,
        my->cairo_user_font_face_set_text_to_glyphs_func);
}

EXPORT void my_cairo_user_font_face_set_unicode_to_glyph_func(
    void *font_face, void *callback)
{
    cairo_set_user_callback(
        font_face, callback, CAIRO_BINDING_USER_UNICODE_TO_GLYPH,
        cairo_acquire_user_unicode,
        my->cairo_user_font_face_set_unicode_to_glyph_func);
}

EXPORT void *my_cairo_user_font_face_get_init_func(void *font_face)
{
    return cairo_binding_guest(font_face, CAIRO_BINDING_USER_INIT);
}

EXPORT void *my_cairo_user_font_face_get_render_glyph_func(void *font_face)
{
    return cairo_binding_guest(font_face, CAIRO_BINDING_USER_RENDER);
}

EXPORT void *my_cairo_user_font_face_get_render_color_glyph_func(
    void *font_face)
{
    return cairo_binding_guest(font_face, CAIRO_BINDING_USER_RENDER_COLOR);
}

EXPORT void *my_cairo_user_font_face_get_text_to_glyphs_func(void *font_face)
{
    return cairo_binding_guest(font_face,
                               CAIRO_BINDING_USER_TEXT_TO_GLYPHS);
}

EXPORT void *my_cairo_user_font_face_get_unicode_to_glyph_func(void *font_face)
{
    return cairo_binding_guest(font_face,
                               CAIRO_BINDING_USER_UNICODE_TO_GLYPH);
}

EXPORT void my_cairo_raster_source_pattern_set_acquire(void *pattern,
                                                        void *acquire,
                                                        void *release)
{
    CairoCallbackSlot *acquire_slots;
    CairoCallbackSlot *release_slots;
    void *native_acquire = cairo_acquire_raster_acquire(acquire,
                                                        &acquire_slots);
    void *native_release = cairo_acquire_raster_release(release,
                                                        &release_slots);

    if ((acquire && !native_acquire) || (release && !native_release)) {
        cairo_release_sync_callback(acquire_slots, acquire);
        cairo_release_sync_callback(release_slots, release);
        return;
    }
    cairo_ensure_raster_cleanup(pattern);
    my->cairo_raster_source_pattern_set_acquire(pattern, native_acquire,
                                                 native_release);
    cairo_binding_replace(pattern, CAIRO_BINDING_RASTER_ACQUIRE,
                          acquire, acquire_slots);
    cairo_binding_replace(pattern, CAIRO_BINDING_RASTER_RELEASE,
                          release, release_slots);
}

EXPORT void my_cairo_raster_source_pattern_get_acquire(void *pattern,
                                                        void **acquire,
                                                        void **release)
{
    if (acquire) {
        *acquire = cairo_binding_guest(pattern,
                                       CAIRO_BINDING_RASTER_ACQUIRE);
    }
    if (release) {
        *release = cairo_binding_guest(pattern,
                                       CAIRO_BINDING_RASTER_RELEASE);
    }
}

#define DEFINE_RASTER_SETTER(name, kind, acquire_callback) \
    EXPORT void my_##name(void *pattern, void *callback) \
    { \
        CairoCallbackSlot *slots; \
        void *native = acquire_callback(callback, &slots); \
        if (callback && !native) return; \
        cairo_ensure_raster_cleanup(pattern); \
        my->name(pattern, native); \
        cairo_binding_replace(pattern, kind, callback, slots); \
    }
DEFINE_RASTER_SETTER(cairo_raster_source_pattern_set_snapshot,
                     CAIRO_BINDING_RASTER_SNAPSHOT,
                     cairo_acquire_raster_snapshot)
DEFINE_RASTER_SETTER(cairo_raster_source_pattern_set_copy,
                     CAIRO_BINDING_RASTER_COPY,
                     cairo_acquire_raster_copy)
#undef DEFINE_RASTER_SETTER

EXPORT void my_cairo_raster_source_pattern_set_finish(void *pattern,
                                                       void *callback)
{
    CairoCallbackSlot *slots;
    void *native = cairo_acquire_raster_finish(callback, &slots);

    if (callback && !native) {
        return;
    }
    my->cairo_raster_source_pattern_set_finish(
        pattern, native ? native : cairo_raster_finish_cleanup);
    cairo_binding_replace(pattern, CAIRO_BINDING_RASTER_FINISH,
                          callback, slots);
}

EXPORT void *my_cairo_raster_source_pattern_get_snapshot(void *pattern)
{
    return cairo_binding_guest(pattern, CAIRO_BINDING_RASTER_SNAPSHOT);
}

EXPORT void *my_cairo_raster_source_pattern_get_copy(void *pattern)
{
    return cairo_binding_guest(pattern, CAIRO_BINDING_RASTER_COPY);
}

EXPORT void *my_cairo_raster_source_pattern_get_finish(void *pattern)
{
    return cairo_binding_guest(pattern, CAIRO_BINDING_RASTER_FINISH);
}

EXPORT void *my_cairo_xcb_device_get_connection(void *device)
{
    void *native = my->cairo_xcb_device_get_connection(device);
    void *guest = add_xcb_connection(native);

    if (native && guest == native) {
        kzt_groups_log_wrapper_limitation(
            cairoName, "cannot represent host XCB connection in guest");
        return NULL;
    }
    return guest;
}

static void cairo_callbacks_clear(void)
{
    CairoCallbackBinding *binding;
    CairoObjectTracker *tracker;

    for (;;) {
        g_mutex_lock(&cairo_callback_lock);
        tracker = cairo_trackers;
        g_mutex_unlock(&cairo_callback_lock);
        if (!tracker) {
            break;
        }
        if (cairo_set_tracker(tracker, NULL, NULL) !=
            CAIRO_STATUS_SUCCESS) {
            kzt_groups_log_wrapper_limitation(
                cairoName, "cannot detach Cairo callback lifetime tracker");
            return;
        }
    }

    g_mutex_lock(&cairo_callback_lock);
    while ((binding = cairo_bindings)) {
        cairo_bindings = binding->next;
        free(binding);
    }
    memset(cairo_destroy_slots, 0, sizeof(cairo_destroy_slots));
    memset(cairo_io_slots, 0, sizeof(cairo_io_slots));
    memset(cairo_user_init_slots, 0, sizeof(cairo_user_init_slots));
    memset(cairo_user_render_slots, 0, sizeof(cairo_user_render_slots));
    memset(cairo_user_text_slots, 0, sizeof(cairo_user_text_slots));
    memset(cairo_user_unicode_slots, 0, sizeof(cairo_user_unicode_slots));
    memset(cairo_observer_slots, 0, sizeof(cairo_observer_slots));
    memset(cairo_raster_acquire_slots, 0,
           sizeof(cairo_raster_acquire_slots));
    memset(cairo_raster_release_slots, 0,
           sizeof(cairo_raster_release_slots));
    memset(cairo_raster_snapshot_slots, 0,
           sizeof(cairo_raster_snapshot_slots));
    memset(cairo_raster_copy_slots, 0, sizeof(cairo_raster_copy_slots));
    memset(cairo_raster_finish_slots, 0,
           sizeof(cairo_raster_finish_slots));
    g_mutex_unlock(&cairo_callback_lock);
}

#define PRE_INIT_GUEST \
    do { \
        char reason[256]; \
        char *guest_path = ResolveFile(lib->path, &box64->box64_ld_lib); \
        bool safe = latx_cairo_preflight_guest(guest_path, cairoName, reason, \
                                               sizeof(reason)); \
        box_free(guest_path); \
        if (!safe) { \
            kzt_groups_log_wrapper_rejection(cairoName, reason); \
            return -1; \
        } \
    } while (0);

#define CUSTOM_INIT \
    getMy(lib);

#define CUSTOM_FINI \
    cairo_callbacks_clear(); \
    freeMy();

#include "wrappedlib_init.h"
