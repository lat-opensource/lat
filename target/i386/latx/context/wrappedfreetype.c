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
#include <stddef.h>
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
#include "wrappedfont-preflight.h"
#include "wrappedfont-interop.h"
#include "wrapper.h"
#include "wrappertbbridge.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

const char *freetypeName = "libfreetype.so.6";
#define LIBNAME freetype

#define PRE_INIT_GUEST \
    do { \
        if (latx_font_preflight_or_disable(&box64->box64_ld_lib, \
                                           freetypeName) != 0) { \
            return -1; \
        } \
    } while (0);

typedef struct FtVectorAbi {
    intptr_t x;
    intptr_t y;
} FtVectorAbi;

typedef struct FtColorAbi {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t alpha;
} FtColorAbi;

typedef struct FtOpaquePaintAbi {
    uint8_t *paint;
    int32_t insert_root_transform;
} FtOpaquePaintAbi;

typedef struct FtGenericAbi {
    void *data;
    void *finalizer;
} FtGenericAbi;

typedef struct FtBBoxAbi {
    intptr_t x_min;
    intptr_t y_min;
    intptr_t x_max;
    intptr_t y_max;
} FtBBoxAbi;

typedef struct FtFaceAbi {
    intptr_t num_faces;
    intptr_t face_index;
    intptr_t face_flags;
    intptr_t style_flags;
    intptr_t num_glyphs;
    char *family_name;
    char *style_name;
    int32_t num_fixed_sizes;
    void *available_sizes;
    int32_t num_charmaps;
    void *charmaps;
    FtGenericAbi generic;
    FtBBoxAbi bbox;
    uint16_t units_per_em;
    int16_t ascender;
    int16_t descender;
    int16_t height;
    int16_t max_advance_width;
    int16_t max_advance_height;
    int16_t underline_position;
    int16_t underline_thickness;
    void *glyph;
    void *size;
    void *charmap;
} FtFaceAbi;

typedef struct FtGlyphSlotAbi {
    void *library;
    void *face;
    void *next;
    uint32_t glyph_index;
    FtGenericAbi generic;
} FtGlyphSlotAbi;

typedef struct FtSizeAbi {
    void *face;
    FtGenericAbi generic;
} FtSizeAbi;

typedef union FtStreamDescAbi {
    intptr_t value;
    void *pointer;
} FtStreamDescAbi;

typedef struct FtStreamAbi {
    uint8_t *base;
    uintptr_t size;
    uintptr_t pos;
    FtStreamDescAbi descriptor;
    FtStreamDescAbi pathname;
    void *read;
    void *close;
    void *memory;
    uint8_t *cursor;
    uint8_t *limit;
} FtStreamAbi;

typedef struct FtOpenArgsAbi {
    uint32_t flags;
    const uint8_t *memory_base;
    intptr_t memory_size;
    char *pathname;
    FtStreamAbi *stream;
    void *driver;
    int32_t num_params;
    void *params;
} FtOpenArgsAbi;

typedef struct FtMemoryAbi {
    void *user;
    void *alloc;
    void *free;
    void *realloc;
} FtMemoryAbi;

typedef struct FtOutlineFuncsAbi {
    void *move_to;
    void *line_to;
    void *conic_to;
    void *cubic_to;
    int32_t shift;
    intptr_t delta;
} FtOutlineFuncsAbi;

typedef struct FtRasterParamsAbi {
    const void *target;
    const void *source;
    int32_t flags;
    void *gray_spans;
    void *black_spans;
    void *bit_test;
    void *bit_set;
    void *user;
    FtBBoxAbi clip_box;
} FtRasterParamsAbi;

typedef void (*FtSpanFuncAbi)(int32_t y, int32_t count, const void *spans,
                              void *user);
typedef int32_t (*FtRasterBitTestFuncAbi)(int32_t y, int32_t x, void *user);
typedef void (*FtRasterBitSetFuncAbi)(int32_t y, int32_t x, void *user);

typedef struct FtRasterCallbackContext {
    void *callbacks[4];
    void *native_callbacks[4];
    void *guest_user;
} FtRasterCallbackContext;

typedef uintptr_t (*FtStreamIoFuncAbi)(FtStreamAbi *stream,
                                       uintptr_t offset, uint8_t *buffer,
                                       uintptr_t count);
typedef void (*FtStreamCloseFuncAbi)(FtStreamAbi *stream);

typedef struct FtStreamBridge {
    FtStreamAbi native;
    FtStreamAbi *guest;
    void *callbacks[2];
    void *native_callbacks[2];
} FtStreamBridge;

typedef void *(*FtAllocFuncAbi)(FtMemoryAbi *memory, intptr_t size);
typedef void (*FtFreeFuncAbi)(FtMemoryAbi *memory, void *block);
typedef void *(*FtReallocFuncAbi)(FtMemoryAbi *memory, intptr_t current_size,
                                  intptr_t new_size, void *block);

typedef struct FtMemoryBridge {
    FtMemoryAbi native;
    FtMemoryAbi *guest;
    void *callbacks[3];
    void *native_callbacks[3];
    void *library;
    struct FtMemoryBridge *next;
} FtMemoryBridge;

_Static_assert(sizeof(FtVectorAbi) == 16, "FT_Vector ABI");
_Static_assert(sizeof(FtColorAbi) == 4, "FT_Color ABI");
_Static_assert(sizeof(FtOpaquePaintAbi) == 16, "FT_OpaquePaint ABI");
_Static_assert(offsetof(FtFaceAbi, generic) == 88, "FT_Face generic ABI");
_Static_assert(offsetof(FtFaceAbi, glyph) == 152, "FT_Face glyph ABI");
_Static_assert(offsetof(FtGlyphSlotAbi, generic) == 32,
               "FT_GlyphSlot generic ABI");
_Static_assert(offsetof(FtSizeAbi, generic) == 8, "FT_Size generic ABI");
_Static_assert(offsetof(FtStreamAbi, read) == 40, "FT_Stream read ABI");
_Static_assert(offsetof(FtStreamAbi, close) == 48, "FT_Stream close ABI");
_Static_assert(sizeof(FtStreamAbi) == 80, "FT_Stream ABI");
_Static_assert(sizeof(FtMemoryAbi) == 32, "FT_Memory ABI");
_Static_assert(offsetof(FtRasterParamsAbi, gray_spans) == 24,
               "FT_Raster_Params callback ABI");
_Static_assert(sizeof(FtRasterParamsAbi) == 96,
               "FT_Raster_Params ABI");

#include "generated/wrappedfreetypetypes.h"

#include "wrappercallback.h"

enum {
    FT_ERROR_SUCCESS = 0,
    FT_ERROR_INVALID_ARGUMENT = 0x06,
    FT_ERROR_UNIMPLEMENTED_FEATURE = 0x07,
    FT_ERROR_OUT_OF_MEMORY = 0x40,
    FT_OPEN_STREAM = 0x02,
};

typedef struct FtFaceTracker {
    void *face;
    void *library;
    unsigned references;
    FtStreamBridge *stream_bridge;
    struct FtFaceTracker *next;
} FtFaceTracker;

typedef struct FtLibraryTracker {
    void *library;
    unsigned references;
    struct FtLibraryTracker *next;
} FtLibraryTracker;

typedef struct FtSizeTracker {
    void *size;
    void *face;
    struct FtSizeTracker *next;
} FtSizeTracker;

typedef struct FtBorrowTracker {
    void *face;
    unsigned references;
    struct FtBorrowTracker *next;
} FtBorrowTracker;

typedef struct FtCacheManagerTracker {
    void *manager;
    uintptr_t guest_requester;
    struct FtCacheManagerTracker *next;
} FtCacheManagerTracker;

typedef struct FtDebugHookTracker {
    void *library;
    uint32_t index;
    uintptr_t guest_callback;
    struct FtDebugHookTracker *next;
} FtDebugHookTracker;

typedef struct FtGenericPatch {
    FtGenericAbi *generic;
    void *object;
    void *guest_finalizer;
    void *native_finalizer;
    bool persistent;
    bool invoked;
    bool retire_face;
    struct FtGenericPatch *global_next;
    struct FtGenericPatch *context_next;
} FtGenericPatch;

typedef struct FtGenericContext {
    FtGenericPatch *patches;
} FtGenericContext;

static GMutex freetype_lock;
static FtLibraryTracker *freetype_libraries;
static FtFaceTracker *freetype_faces;
static FtSizeTracker *freetype_sizes;
static FtBorrowTracker *freetype_borrowed_faces;
static FtCacheManagerTracker *freetype_cache_managers;
static FtDebugHookTracker *freetype_debug_hooks;
static FtGenericPatch *freetype_generic_patches;
static FtMemoryBridge *freetype_memory_bridges;

static uintptr_t freetype_stream_read(FtStreamAbi *stream, uintptr_t offset,
                                      uint8_t *buffer, uintptr_t count);
static void freetype_stream_close(FtStreamAbi *stream);
static FtStreamBridge *freetype_untrack_face_locked(void *face);

static void freetype_stream_guest_view(FtStreamBridge *bridge)
{
    *bridge->guest = bridge->native;
    bridge->guest->read = bridge->callbacks[0];
    bridge->guest->close = bridge->callbacks[1];
}

static void freetype_stream_native_view(FtStreamBridge *bridge)
{
    bridge->native = *bridge->guest;
    bridge->native.read = bridge->callbacks[0]
        ? freetype_stream_read : NULL;
    bridge->native.close = bridge->callbacks[1]
        ? freetype_stream_close : NULL;
}

static uintptr_t freetype_stream_read(FtStreamAbi *stream, uintptr_t offset,
                                      uint8_t *buffer, uintptr_t count)
{
    FtStreamBridge *bridge = (FtStreamBridge *)((char *)stream -
        offsetof(FtStreamBridge, native));
    uintptr_t result;

    freetype_stream_guest_view(bridge);
    if (bridge->native_callbacks[0]) {
        result = ((FtStreamIoFuncAbi)bridge->native_callbacks[0])(
            bridge->guest, offset, buffer, count);
    } else {
        result = (uintptr_t)RunFunctionFmt(
            (uintptr_t)bridge->callbacks[0], "pLpL", bridge->guest,
            offset, buffer, count);
    }
    freetype_stream_native_view(bridge);
    return result;
}

static void freetype_stream_close(FtStreamAbi *stream)
{
    FtStreamBridge *bridge = (FtStreamBridge *)((char *)stream -
        offsetof(FtStreamBridge, native));

    freetype_stream_guest_view(bridge);
    if (bridge->native_callbacks[1]) {
        ((FtStreamCloseFuncAbi)bridge->native_callbacks[1])(bridge->guest);
    } else {
        RunFunctionFmt((uintptr_t)bridge->callbacks[1], "p", bridge->guest);
    }
}

static bool freetype_stream_requires_bridge(const FtOpenArgsAbi *args)
{
    if (!args || !(args->flags & FT_OPEN_STREAM) || !args->stream) {
        return false;
    }
    return (args->stream->read &&
            !GetNativeFnc((uintptr_t)args->stream->read)) ||
           (args->stream->close &&
            !GetNativeFnc((uintptr_t)args->stream->close));
}

static FtStreamBridge *freetype_stream_bridge_new(FtOpenArgsAbi *args)
{
    FtStreamBridge *bridge;

    if (!freetype_stream_requires_bridge(args)) {
        return NULL;
    }
    bridge = calloc(1, sizeof(*bridge));
    if (!bridge) {
        return NULL;
    }
    bridge->guest = args->stream;
    bridge->callbacks[0] = args->stream->read;
    bridge->callbacks[1] = args->stream->close;
    bridge->native_callbacks[0] =
        GetNativeFnc((uintptr_t)bridge->callbacks[0]);
    bridge->native_callbacks[1] =
        GetNativeFnc((uintptr_t)bridge->callbacks[1]);
    freetype_stream_native_view(bridge);
    return bridge;
}

static void *freetype_memory_alloc(FtMemoryAbi *memory, intptr_t size)
{
    FtMemoryBridge *bridge = (FtMemoryBridge *)((char *)memory -
        offsetof(FtMemoryBridge, native));

    if (bridge->native_callbacks[0]) {
        return ((FtAllocFuncAbi)bridge->native_callbacks[0])(
            bridge->guest, size);
    }
    return (void *)(uintptr_t)RunFunctionFmt(
        (uintptr_t)bridge->callbacks[0], "pl", bridge->guest, size);
}

static void freetype_memory_free(FtMemoryAbi *memory, void *block)
{
    FtMemoryBridge *bridge = (FtMemoryBridge *)((char *)memory -
        offsetof(FtMemoryBridge, native));

    if (bridge->native_callbacks[1]) {
        ((FtFreeFuncAbi)bridge->native_callbacks[1])(bridge->guest, block);
        return;
    }
    RunFunctionFmt((uintptr_t)bridge->callbacks[1], "pp", bridge->guest,
                   block);
}

static void *freetype_memory_realloc(FtMemoryAbi *memory,
                                     intptr_t current_size,
                                     intptr_t new_size, void *block)
{
    FtMemoryBridge *bridge = (FtMemoryBridge *)((char *)memory -
        offsetof(FtMemoryBridge, native));

    if (bridge->native_callbacks[2]) {
        return ((FtReallocFuncAbi)bridge->native_callbacks[2])(
            bridge->guest, current_size, new_size, block);
    }
    return (void *)(uintptr_t)RunFunctionFmt(
        (uintptr_t)bridge->callbacks[2], "pllp", bridge->guest,
        current_size, new_size, block);
}

static bool freetype_memory_requires_bridge(const FtMemoryAbi *memory)
{
    if (!memory) {
        return false;
    }
    return (memory->alloc && !GetNativeFnc((uintptr_t)memory->alloc)) ||
           (memory->free && !GetNativeFnc((uintptr_t)memory->free)) ||
           (memory->realloc && !GetNativeFnc((uintptr_t)memory->realloc));
}

static FtMemoryBridge *freetype_memory_bridge_new(FtMemoryAbi *memory)
{
    FtMemoryBridge *bridge = calloc(1, sizeof(*bridge));

    if (!bridge) {
        return NULL;
    }
    bridge->guest = memory;
    bridge->callbacks[0] = memory->alloc;
    bridge->callbacks[1] = memory->free;
    bridge->callbacks[2] = memory->realloc;
    for (size_t i = 0; i < ARRAY_SIZE(bridge->callbacks); i++) {
        bridge->native_callbacks[i] =
            GetNativeFnc((uintptr_t)bridge->callbacks[i]);
    }
    bridge->native.user = memory->user;
    bridge->native.alloc = memory->alloc ? freetype_memory_alloc : NULL;
    bridge->native.free = memory->free ? freetype_memory_free : NULL;
    bridge->native.realloc = memory->realloc
        ? freetype_memory_realloc : NULL;
    return bridge;
}

static void freetype_memory_bridge_track(FtMemoryBridge *bridge,
                                         void *library)
{
    bridge->library = library;
    g_mutex_lock(&freetype_lock);
    bridge->next = freetype_memory_bridges;
    freetype_memory_bridges = bridge;
    g_mutex_unlock(&freetype_lock);
}

static FtMemoryBridge *freetype_memory_bridge_untrack_locked(void *library)
{
    for (FtMemoryBridge **link = &freetype_memory_bridges; *link;
         link = &(*link)->next) {
        FtMemoryBridge *bridge = *link;

        if (bridge->library != library) {
            continue;
        }
        *link = bridge->next;
        return bridge;
    }
    return NULL;
}

static FtDebugHookTracker *freetype_debug_hooks_take_locked(void *library)
{
    FtDebugHookTracker *result = NULL;
    FtDebugHookTracker **link = &freetype_debug_hooks;

    while (*link) {
        FtDebugHookTracker *tracker = *link;

        if (tracker->library != library) {
            link = &tracker->next;
            continue;
        }
        *link = tracker->next;
        tracker->next = result;
        result = tracker;
    }
    return result;
}

static void freetype_generic_finalizer(void *object)
{
    FtGenericPatch **link;
    FtGenericPatch *patch = NULL;
    void *guest_finalizer = NULL;
    void *native_finalizer = NULL;
    FtStreamBridge *stream_bridge = NULL;

    g_mutex_lock(&freetype_lock);
    for (link = &freetype_generic_patches; *link;
         link = &(*link)->global_next) {
        if ((*link)->object != object) {
            continue;
        }
        patch = *link;
        patch->invoked = true;
        guest_finalizer = patch->guest_finalizer;
        native_finalizer = patch->native_finalizer;
        if (patch->retire_face) {
            stream_bridge = freetype_untrack_face_locked(object);
        }
        if (patch->persistent) {
            *link = patch->global_next;
        }
        break;
    }
    g_mutex_unlock(&freetype_lock);

    if (native_finalizer) {
        ((void (*)(void *))native_finalizer)(object);
    } else if (guest_finalizer) {
        RunFunctionFmt((uintptr_t)guest_finalizer, "p", object);
    } else if (!patch) {
        kzt_groups_log_wrapper_limitation(
            freetypeName, "untracked FT_Generic finalizer invocation");
    }
    free(stream_bridge);
    if (patch && patch->persistent) {
        free(patch);
    }
}

static bool freetype_generic_patch_locked(FtGenericContext *context,
                                          void *object,
                                          FtGenericAbi *generic,
                                          bool persistent,
                                          bool force,
                                          bool retire_face)
{
    FtGenericPatch *patch;
    void *native_finalizer;

    if (!generic) {
        return true;
    }
    for (patch = freetype_generic_patches; patch;
         patch = patch->global_next) {
        if (patch->generic == generic) {
            if (generic->finalizer != freetype_generic_finalizer) {
                patch->guest_finalizer = generic->finalizer;
                patch->native_finalizer = generic->finalizer
                    ? GetNativeFnc((uintptr_t)generic->finalizer) : NULL;
                generic->finalizer = freetype_generic_finalizer;
            }
            patch->retire_face |= retire_face;
            return true;
        }
    }
    if (!generic->finalizer && !force) {
        return true;
    }
    native_finalizer = generic->finalizer
        ? GetNativeFnc((uintptr_t)generic->finalizer) : NULL;
    if (native_finalizer && !force) {
        return true;
    }
    if (kzt_tbbridge_insert((uintptr_t)freetype_generic_finalizer,
                            (ADDR)freetype_generic_finalizer, vFp) < 0) {
        return false;
    }
    patch = calloc(1, sizeof(*patch));
    if (!patch) {
        return false;
    }
    patch->generic = generic;
    patch->object = object;
    patch->guest_finalizer = generic->finalizer;
    patch->native_finalizer = native_finalizer;
    patch->persistent = persistent;
    patch->retire_face = retire_face;
    patch->global_next = freetype_generic_patches;
    freetype_generic_patches = patch;
    if (context) {
        patch->context_next = context->patches;
        context->patches = patch;
    }
    generic->finalizer = freetype_generic_finalizer;
    return true;
}

static bool freetype_generic_patch_face_locked(FtGenericContext *context,
                                               void *face,
                                               bool persistent)
{
    FtFaceAbi *record = face;
    FtGenericPatch *old_global = freetype_generic_patches;
    FtGenericPatch *old_context = context ? context->patches : NULL;

    if (!record) {
        return true;
    }
    if (!freetype_generic_patch_locked(context, face, &record->generic,
                                       persistent, persistent,
                                       persistent)) {
        goto fail;
    }
    for (FtGlyphSlotAbi *slot = record->glyph; slot; slot = slot->next) {
        if (!freetype_generic_patch_locked(
                context, slot, &slot->generic, persistent, false, false)) {
            goto fail;
        }
    }
    if (record->size &&
        !freetype_generic_patch_locked(
            context, record->size,
            &((FtSizeAbi *)record->size)->generic, persistent,
            false, false)) {
        goto fail;
    }
    for (FtSizeTracker *size = freetype_sizes; size; size = size->next) {
        if (size->face == face &&
            !freetype_generic_patch_locked(
                context, size->size,
                &((FtSizeAbi *)size->size)->generic, persistent,
                false, false)) {
            goto fail;
        }
    }
    return true;

fail:
    while (freetype_generic_patches != old_global) {
        FtGenericPatch *patch = freetype_generic_patches;

        freetype_generic_patches = patch->global_next;
        if (patch->generic->finalizer == freetype_generic_finalizer) {
            patch->generic->finalizer = patch->guest_finalizer;
        }
        free(patch);
    }
    if (context) {
        context->patches = old_context;
    }
    return false;
}

static bool freetype_generic_patch_library_locked(FtGenericContext *context,
                                                  void *library)
{
    for (FtFaceTracker *face = freetype_faces; face; face = face->next) {
        if (face->library == library &&
            !freetype_generic_patch_face_locked(context, face->face,
                                                false)) {
            return false;
        }
    }
    return true;
}

static void freetype_generic_context_begin(FtGenericContext *context)
{
    (void)context;
}

static void freetype_generic_context_finish(FtGenericContext *context)
{
    FtGenericPatch *patch = context->patches;

    g_mutex_lock(&freetype_lock);
    while (patch) {
        FtGenericPatch *next = patch->context_next;
        FtGenericPatch **link;

        for (link = &freetype_generic_patches; *link;
             link = &(*link)->global_next) {
            if (*link == patch) {
                *link = patch->global_next;
                break;
            }
        }

        if (!patch->invoked &&
            patch->generic->finalizer == freetype_generic_finalizer) {
            patch->generic->finalizer = patch->guest_finalizer;
        }
        free(patch);
        patch = next;
    }
    g_mutex_unlock(&freetype_lock);
    context->patches = NULL;
}

static bool freetype_track_library(void *library)
{
    FtLibraryTracker *tracker = calloc(1, sizeof(*tracker));

    if (!tracker) {
        return false;
    }
    tracker->library = library;
    tracker->references = 1;
    g_mutex_lock(&freetype_lock);
    tracker->next = freetype_libraries;
    freetype_libraries = tracker;
    g_mutex_unlock(&freetype_lock);
    return true;
}

static bool freetype_library_is_last_reference_locked(void *library)
{
    for (FtLibraryTracker *tracker = freetype_libraries; tracker;
         tracker = tracker->next) {
        if (tracker->library == library) {
            return tracker->references == 1;
        }
    }
    return true;
}

static void freetype_library_release_locked(void *library)
{
    FtLibraryTracker **link = &freetype_libraries;

    while (*link) {
        FtLibraryTracker *tracker = *link;

        if (tracker->library != library) {
            link = &tracker->next;
            continue;
        }
        if (--tracker->references == 0) {
            *link = tracker->next;
            free(tracker);
        }
        return;
    }
}

static void freetype_library_remove_locked(void *library)
{
    FtLibraryTracker **link = &freetype_libraries;

    while (*link) {
        FtLibraryTracker *tracker = *link;

        if (tracker->library != library) {
            link = &tracker->next;
            continue;
        }
        *link = tracker->next;
        free(tracker);
        return;
    }
}

static bool freetype_track_face(void *library, void *face,
                                FtStreamBridge *stream_bridge)
{
    FtFaceTracker *tracker = calloc(1, sizeof(*tracker));

    if (!tracker) {
        return false;
    }
    tracker->library = library;
    tracker->face = face;
    tracker->references = 1;
    tracker->stream_bridge = stream_bridge;
    g_mutex_lock(&freetype_lock);
    tracker->next = freetype_faces;
    freetype_faces = tracker;
    g_mutex_unlock(&freetype_lock);
    return true;
}

static bool freetype_track_size(void *face, void *size)
{
    FtSizeTracker *tracker = calloc(1, sizeof(*tracker));

    if (!tracker) {
        return false;
    }
    tracker->face = face;
    tracker->size = size;
    g_mutex_lock(&freetype_lock);
    tracker->next = freetype_sizes;
    freetype_sizes = tracker;
    g_mutex_unlock(&freetype_lock);
    return true;
}

static void freetype_untrack_size_locked(void *size)
{
    FtSizeTracker **cursor = &freetype_sizes;

    while (*cursor) {
        FtSizeTracker *tracker = *cursor;

        if (tracker->size != size) {
            cursor = &tracker->next;
            continue;
        }
        *cursor = tracker->next;
        free(tracker);
        return;
    }
}

static FtStreamBridge *freetype_untrack_face_locked(void *face)
{
    FtFaceTracker **cursor = &freetype_faces;
    FtSizeTracker **size_cursor = &freetype_sizes;

    while (*size_cursor) {
        FtSizeTracker *size = *size_cursor;

        if (size->face != face) {
            size_cursor = &size->next;
            continue;
        }
        *size_cursor = size->next;
        free(size);
    }
    while (*cursor) {
        FtFaceTracker *tracker = *cursor;

        if (tracker->face != face) {
            cursor = &tracker->next;
            continue;
        }
        *cursor = tracker->next;
        FtStreamBridge *stream_bridge = tracker->stream_bridge;

        free(tracker);
        return stream_bridge;
    }
    return NULL;
}

static void freetype_untrack_library_locked(void *library)
{
    FtFaceTracker *face = freetype_faces;

    while (face) {
        FtFaceTracker *next = face->next;

        if (face->library == library) {
            free(freetype_untrack_face_locked(face->face));
        }
        face = next;
    }
}

static bool freetype_face_is_borrowed_locked(void *face)
{
    for (FtBorrowTracker *tracker = freetype_borrowed_faces; tracker;
         tracker = tracker->next) {
        if (tracker->face == face) {
            return true;
        }
    }
    return false;
}

bool latx_freetype_face_borrow(void *face)
{
    FtBorrowTracker *tracker;

    if (!face) {
        return false;
    }
    g_mutex_lock(&freetype_lock);
    for (tracker = freetype_borrowed_faces; tracker;
         tracker = tracker->next) {
        if (tracker->face == face) {
            tracker->references++;
            g_mutex_unlock(&freetype_lock);
            return true;
        }
    }
    tracker = calloc(1, sizeof(*tracker));
    if (!tracker) {
        g_mutex_unlock(&freetype_lock);
        return false;
    }
    tracker->face = face;
    tracker->references = 1;
    tracker->next = freetype_borrowed_faces;
    freetype_borrowed_faces = tracker;
    g_mutex_unlock(&freetype_lock);
    return true;
}

bool latx_freetype_face_prepare_host_destruction(void *face)
{
    bool safe;

    g_mutex_lock(&freetype_lock);
    safe = freetype_generic_patch_face_locked(NULL, face, true);
    g_mutex_unlock(&freetype_lock);
    if (!safe) {
        kzt_groups_log_wrapper_limitation(
            freetypeName, "cannot bridge host-owned FT_Face finalizers");
    }
    return safe;
}

void latx_freetype_face_return(void *face)
{
    FtBorrowTracker **cursor;

    g_mutex_lock(&freetype_lock);
    cursor = &freetype_borrowed_faces;
    while (*cursor) {
        FtBorrowTracker *tracker = *cursor;

        if (tracker->face != face) {
            cursor = &tracker->next;
            continue;
        }
        if (--tracker->references == 0) {
            *cursor = tracker->next;
            free(tracker);
        }
        break;
    }
    g_mutex_unlock(&freetype_lock);
}

#define FT_CALLBACK_SLOTS 16
#define FT_SLOT_LIST(X) \
    X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) \
    X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15)

typedef struct FtCallbackSlot {
    uintptr_t guest;
    unsigned references;
} FtCallbackSlot;

static FtCallbackSlot ft_move_slots[FT_CALLBACK_SLOTS];
static FtCallbackSlot ft_line_slots[FT_CALLBACK_SLOTS];
static FtCallbackSlot ft_conic_slots[FT_CALLBACK_SLOTS];
static FtCallbackSlot ft_cubic_slots[FT_CALLBACK_SLOTS];
static FtCallbackSlot ft_list_iterator_slots[FT_CALLBACK_SLOTS];
static FtCallbackSlot ft_list_destroy_slots[FT_CALLBACK_SLOTS];
static FtCallbackSlot ft_face_requester_slots[FT_CALLBACK_SLOTS];
static FtCallbackSlot ft_debug_hook_slots[FT_CALLBACK_SLOTS];

int32_t my_FT_Done_Face(void *face);

static uintptr_t freetype_slot_guest(FtCallbackSlot *slots, size_t index)
{
    uintptr_t guest;

    g_mutex_lock(&freetype_lock);
    guest = slots[index].guest;
    g_mutex_unlock(&freetype_lock);
    return guest;
}

static void freetype_slot_release(FtCallbackSlot *slots, uintptr_t guest)
{
    g_mutex_lock(&freetype_lock);
    for (size_t i = 0; i < FT_CALLBACK_SLOTS; i++) {
        if (slots[i].guest != guest) {
            continue;
        }
        if (slots[i].references && --slots[i].references == 0) {
            slots[i].guest = 0;
        }
        break;
    }
    g_mutex_unlock(&freetype_lock);
}

static void *freetype_slot_acquire(FtCallbackSlot *slots,
                                   void *const *thunks, void *callback,
                                   const char *kind)
{
    uintptr_t guest = (uintptr_t)callback;
    void *native;

    if (!callback) {
        return NULL;
    }
    native = GetNativeFnc(guest);
    if (native) {
        return native;
    }
    g_mutex_lock(&freetype_lock);
    for (size_t i = 0; i < FT_CALLBACK_SLOTS; i++) {
        if (slots[i].guest == guest) {
            slots[i].references++;
            native = thunks[i];
            g_mutex_unlock(&freetype_lock);
            return native;
        }
    }
    for (size_t i = 0; i < FT_CALLBACK_SLOTS; i++) {
        if (!slots[i].guest) {
            slots[i].guest = guest;
            slots[i].references = 1;
            native = thunks[i];
            g_mutex_unlock(&freetype_lock);
            return native;
        }
    }
    g_mutex_unlock(&freetype_lock);
    kzt_groups_log_wrapper_limitation(freetypeName, kind);
    return NULL;
}

#define DEFINE_MOVE_THUNK(N) \
    static int32_t ft_move_thunk_##N(void *to, void *user) \
    { \
        uintptr_t guest = freetype_slot_guest(ft_move_slots, N); \
        return guest ? (int32_t)RunFunctionFmt(guest, "pp", to, user) : 0; \
    }
FT_SLOT_LIST(DEFINE_MOVE_THUNK)
#undef DEFINE_MOVE_THUNK

#define DEFINE_LINE_THUNK(N) \
    static int32_t ft_line_thunk_##N(void *to, void *user) \
    { \
        uintptr_t guest = freetype_slot_guest(ft_line_slots, N); \
        return guest ? (int32_t)RunFunctionFmt(guest, "pp", to, user) : 0; \
    }
FT_SLOT_LIST(DEFINE_LINE_THUNK)
#undef DEFINE_LINE_THUNK

#define DEFINE_CONIC_THUNK(N) \
    static int32_t ft_conic_thunk_##N(void *control, void *to, void *user) \
    { \
        uintptr_t guest = freetype_slot_guest(ft_conic_slots, N); \
        return guest ? (int32_t)RunFunctionFmt(guest, "ppp", control, to, user) : 0; \
    }
FT_SLOT_LIST(DEFINE_CONIC_THUNK)
#undef DEFINE_CONIC_THUNK

#define DEFINE_CUBIC_THUNK(N) \
    static int32_t ft_cubic_thunk_##N(void *control1, void *control2, \
                                      void *to, void *user) \
    { \
        uintptr_t guest = freetype_slot_guest(ft_cubic_slots, N); \
        return guest ? (int32_t)RunFunctionFmt(guest, "pppp", control1, \
                                               control2, to, user) : 0; \
    }
FT_SLOT_LIST(DEFINE_CUBIC_THUNK)
#undef DEFINE_CUBIC_THUNK

#define DEFINE_LIST_ITERATOR_THUNK(N) \
    static int32_t ft_list_iterator_thunk_##N(void *node, void *user) \
    { \
        uintptr_t guest = freetype_slot_guest(ft_list_iterator_slots, N); \
        return guest ? (int32_t)RunFunctionFmt(guest, "pp", node, user) : 0; \
    }
FT_SLOT_LIST(DEFINE_LIST_ITERATOR_THUNK)
#undef DEFINE_LIST_ITERATOR_THUNK

#define DEFINE_LIST_DESTROY_THUNK(N) \
    static void ft_list_destroy_thunk_##N(void *memory, void *data, void *user) \
    { \
        uintptr_t guest = freetype_slot_guest(ft_list_destroy_slots, N); \
        if (guest) RunFunctionFmt(guest, "ppp", memory, data, user); \
    }
FT_SLOT_LIST(DEFINE_LIST_DESTROY_THUNK)
#undef DEFINE_LIST_DESTROY_THUNK

#define DEFINE_FACE_REQUESTER_THUNK(N) \
    static int32_t ft_face_requester_thunk_##N( \
        void *face_id, void *library, void *request_data, void **face) \
    { \
        uintptr_t guest = freetype_slot_guest(ft_face_requester_slots, N); \
        int32_t status = guest \
            ? (int32_t)RunFunctionFmt(guest, "pppp", face_id, library, \
                                      request_data, face) \
            : FT_ERROR_UNIMPLEMENTED_FEATURE; \
        if (status == FT_ERROR_SUCCESS && face && *face) { \
            bool safe; \
            g_mutex_lock(&freetype_lock); \
            safe = freetype_generic_patch_face_locked(NULL, *face, true); \
            g_mutex_unlock(&freetype_lock); \
            if (!safe) { \
                my_FT_Done_Face(*face); \
                *face = NULL; \
                return FT_ERROR_OUT_OF_MEMORY; \
            } \
        } \
        return status; \
    }
FT_SLOT_LIST(DEFINE_FACE_REQUESTER_THUNK)
#undef DEFINE_FACE_REQUESTER_THUNK

#define DEFINE_DEBUG_HOOK_THUNK(N) \
    static void ft_debug_hook_thunk_##N(void *argument) \
    { \
        uintptr_t guest = freetype_slot_guest(ft_debug_hook_slots, N); \
        if (guest) RunFunctionFmt(guest, "p", argument); \
    }
FT_SLOT_LIST(DEFINE_DEBUG_HOOK_THUNK)
#undef DEFINE_DEBUG_HOOK_THUNK

#define THUNK_ADDRESS(kind, N) (void *)ft_##kind##_thunk_##N,
#define MOVE_ADDRESS(N) THUNK_ADDRESS(move, N)
static void *const ft_move_thunks[] = { FT_SLOT_LIST(MOVE_ADDRESS) };
#undef MOVE_ADDRESS
#define LINE_ADDRESS(N) THUNK_ADDRESS(line, N)
static void *const ft_line_thunks[] = { FT_SLOT_LIST(LINE_ADDRESS) };
#undef LINE_ADDRESS
#define CONIC_ADDRESS(N) THUNK_ADDRESS(conic, N)
static void *const ft_conic_thunks[] = { FT_SLOT_LIST(CONIC_ADDRESS) };
#undef CONIC_ADDRESS
#define CUBIC_ADDRESS(N) THUNK_ADDRESS(cubic, N)
static void *const ft_cubic_thunks[] = { FT_SLOT_LIST(CUBIC_ADDRESS) };
#undef CUBIC_ADDRESS
#define LIST_ITERATOR_ADDRESS(N) THUNK_ADDRESS(list_iterator, N)
static void *const ft_list_iterator_thunks[] = {
    FT_SLOT_LIST(LIST_ITERATOR_ADDRESS)
};
#undef LIST_ITERATOR_ADDRESS
#define LIST_DESTROY_ADDRESS(N) THUNK_ADDRESS(list_destroy, N)
static void *const ft_list_destroy_thunks[] = {
    FT_SLOT_LIST(LIST_DESTROY_ADDRESS)
};
#undef LIST_DESTROY_ADDRESS
#define FACE_REQUESTER_ADDRESS(N) THUNK_ADDRESS(face_requester, N)
static void *const ft_face_requester_thunks[] = {
    FT_SLOT_LIST(FACE_REQUESTER_ADDRESS)
};
#undef FACE_REQUESTER_ADDRESS
#define DEBUG_HOOK_ADDRESS(N) THUNK_ADDRESS(debug_hook, N)
static void *const ft_debug_hook_thunks[] = {
    FT_SLOT_LIST(DEBUG_HOOK_ADDRESS)
};
#undef DEBUG_HOOK_ADDRESS
#undef THUNK_ADDRESS

static int32_t freetype_finish_face_creation(void *library, void **face,
                                             int32_t status,
                                             FtStreamBridge *stream_bridge)
{
    if (status != FT_ERROR_SUCCESS || !face || !*face) {
        free(stream_bridge);
        return status;
    }
    if (freetype_track_face(library, *face, stream_bridge)) {
        return status;
    }
    my->FT_Done_Face(*face);
    free(stream_bridge);
    *face = NULL;
    return FT_ERROR_OUT_OF_MEMORY;
}

EXPORT int32_t my_FT_Init_FreeType(void **library)
{
    int32_t status = my->FT_Init_FreeType(library);

    if (status != FT_ERROR_SUCCESS || !library || !*library) {
        return status;
    }
    if (freetype_track_library(*library)) {
        return status;
    }
    my->FT_Done_FreeType(*library);
    *library = NULL;
    return FT_ERROR_OUT_OF_MEMORY;
}

EXPORT int32_t my_FT_New_Face(void *library, void *path, intptr_t index,
                              void **face)
{
    return freetype_finish_face_creation(
        library, face, my->FT_New_Face(library, path, index, face), NULL);
}

EXPORT int32_t my_FT_New_Memory_Face(void *library, void *base,
                                     intptr_t size, intptr_t index,
                                     void **face)
{
    return freetype_finish_face_creation(
        library, face,
        my->FT_New_Memory_Face(library, base, size, index, face), NULL);
}

EXPORT int32_t my_FT_Open_Face(void *library, FtOpenArgsAbi *args,
                               intptr_t index, void **face)
{
    FtOpenArgsAbi native_args;
    FtStreamBridge *stream_bridge = NULL;
    int32_t status;

    if (freetype_stream_requires_bridge(args)) {
        stream_bridge = freetype_stream_bridge_new(args);
        if (!stream_bridge) {
            return FT_ERROR_OUT_OF_MEMORY;
        }
        native_args = *args;
        native_args.stream = &stream_bridge->native;
        args = &native_args;
    }
    status = my->FT_Open_Face(library, args, index, face);
    return freetype_finish_face_creation(library, face, status,
                                         stream_bridge);
}

EXPORT int32_t my_FT_Attach_Stream(void *face, FtOpenArgsAbi *args)
{
    FtOpenArgsAbi native_args;
    FtStreamBridge *stream_bridge = NULL;
    int32_t status;

    if (freetype_stream_requires_bridge(args)) {
        stream_bridge = freetype_stream_bridge_new(args);
        if (!stream_bridge) {
            return FT_ERROR_OUT_OF_MEMORY;
        }
        native_args = *args;
        native_args.stream = &stream_bridge->native;
        args = &native_args;
    }
    status = my->FT_Attach_Stream(face, args);
    free(stream_bridge);
    return status;
}

EXPORT int32_t my_FT_New_Size(void *face, void **size)
{
    int32_t status = my->FT_New_Size(face, size);

    if (status != FT_ERROR_SUCCESS || !size || !*size) {
        return status;
    }
    if (freetype_track_size(face, *size)) {
        return status;
    }
    my->FT_Done_Size(*size);
    *size = NULL;
    return FT_ERROR_OUT_OF_MEMORY;
}

EXPORT int32_t my_FT_Done_Size(void *size)
{
    FtGenericContext context = {0};
    int32_t status;

    g_mutex_lock(&freetype_lock);
    if (size && !freetype_generic_patch_locked(
                    &context, size, &((FtSizeAbi *)size)->generic, false,
                    false, false)) {
        g_mutex_unlock(&freetype_lock);
        freetype_generic_context_finish(&context);
        return FT_ERROR_OUT_OF_MEMORY;
    }
    g_mutex_unlock(&freetype_lock);
    freetype_generic_context_begin(&context);
    status = my->FT_Done_Size(size);
    freetype_generic_context_finish(&context);
    if (status == FT_ERROR_SUCCESS) {
        g_mutex_lock(&freetype_lock);
        freetype_untrack_size_locked(size);
        g_mutex_unlock(&freetype_lock);
    }
    return status;
}

EXPORT int32_t my_FT_Done_Face(void *face)
{
    FtGenericContext context = {0};
    FtStreamBridge *stream_bridge = NULL;
    int32_t status;

    g_mutex_lock(&freetype_lock);
    if (freetype_face_is_borrowed_locked(face)) {
        g_mutex_unlock(&freetype_lock);
        kzt_groups_log_wrapper_limitation(
            freetypeName, "cannot destroy a Cairo-borrowed FT_Face");
        return FT_ERROR_INVALID_ARGUMENT;
    }
    if (!freetype_generic_patch_face_locked(&context, face, false)) {
        g_mutex_unlock(&freetype_lock);
        freetype_generic_context_finish(&context);
        return FT_ERROR_OUT_OF_MEMORY;
    }
    g_mutex_unlock(&freetype_lock);
    freetype_generic_context_begin(&context);
    status = my->FT_Done_Face(face);
    freetype_generic_context_finish(&context);
    if (status == FT_ERROR_SUCCESS) {
        g_mutex_lock(&freetype_lock);
        for (FtFaceTracker *tracker = freetype_faces; tracker;
             tracker = tracker->next) {
            if (tracker->face != face) {
                continue;
            }
            if (tracker->references > 1) {
                tracker->references--;
            } else {
                stream_bridge = freetype_untrack_face_locked(face);
            }
            break;
        }
        g_mutex_unlock(&freetype_lock);
        free(stream_bridge);
    }
    return status;
}

EXPORT int32_t my_FT_Reference_Face(void *face)
{
    int32_t status = my->FT_Reference_Face(face);

    if (status != FT_ERROR_SUCCESS || !face) {
        return status;
    }
    g_mutex_lock(&freetype_lock);
    for (FtFaceTracker *tracker = freetype_faces; tracker;
         tracker = tracker->next) {
        if (tracker->face == face) {
            tracker->references++;
            g_mutex_unlock(&freetype_lock);
            return status;
        }
    }
    g_mutex_unlock(&freetype_lock);

    FtFaceAbi *record = face;
    void *library = record->glyph
        ? ((FtGlyphSlotAbi *)record->glyph)->library : NULL;

    if (library && freetype_track_face(library, face, NULL)) {
        return status;
    }
    my->FT_Done_Face(face);
    return FT_ERROR_OUT_OF_MEMORY;
}

EXPORT int32_t my_FT_Reference_Library(void *library)
{
    FtLibraryTracker *new_tracker = calloc(1, sizeof(*new_tracker));
    int32_t status;

    if (!new_tracker) {
        return FT_ERROR_OUT_OF_MEMORY;
    }
    status = my->FT_Reference_Library(library);
    if (status != FT_ERROR_SUCCESS) {
        free(new_tracker);
        return status;
    }

    g_mutex_lock(&freetype_lock);
    for (FtLibraryTracker *tracker = freetype_libraries; tracker;
         tracker = tracker->next) {
        if (tracker->library != library) {
            continue;
        }
        tracker->references++;
        g_mutex_unlock(&freetype_lock);
        free(new_tracker);
        return status;
    }
    new_tracker->library = library;
    new_tracker->references = 2;
    new_tracker->next = freetype_libraries;
    freetype_libraries = new_tracker;
    g_mutex_unlock(&freetype_lock);
    return status;
}

bool latx_freetype_face_reference(void *face)
{
    return my_lib && my_FT_Reference_Face(face) == FT_ERROR_SUCCESS;
}

void latx_freetype_face_release(void *face)
{
    if (!my_lib || my_FT_Done_Face(face) != FT_ERROR_SUCCESS) {
        kzt_groups_log_wrapper_limitation(
            freetypeName, "cannot release Cairo-owned FT_Face reference");
    }
}

static int32_t freetype_done_library(void *library,
                                     int32_t (*finish)(void *),
                                     bool honor_references)
{
    FtGenericContext context = {0};
    FtMemoryBridge *memory_bridge = NULL;
    FtDebugHookTracker *debug_hooks = NULL;
    bool last_reference;
    int32_t status;

    g_mutex_lock(&freetype_lock);
    last_reference = !honor_references ||
        freetype_library_is_last_reference_locked(library);
    if (!last_reference) {
        g_mutex_unlock(&freetype_lock);
        status = finish(library);
        if (status == FT_ERROR_SUCCESS) {
            g_mutex_lock(&freetype_lock);
            freetype_library_release_locked(library);
            g_mutex_unlock(&freetype_lock);
        }
        return status;
    }
    if (!freetype_generic_patch_library_locked(&context, library)) {
        g_mutex_unlock(&freetype_lock);
        freetype_generic_context_finish(&context);
        return FT_ERROR_OUT_OF_MEMORY;
    }
    g_mutex_unlock(&freetype_lock);
    freetype_generic_context_begin(&context);
    status = finish(library);
    freetype_generic_context_finish(&context);
    if (status == FT_ERROR_SUCCESS) {
        g_mutex_lock(&freetype_lock);
        if (honor_references) {
            freetype_library_release_locked(library);
        } else {
            freetype_library_remove_locked(library);
        }
        freetype_untrack_library_locked(library);
        memory_bridge = freetype_memory_bridge_untrack_locked(library);
        debug_hooks = freetype_debug_hooks_take_locked(library);
        g_mutex_unlock(&freetype_lock);
        free(memory_bridge);
        while (debug_hooks) {
            FtDebugHookTracker *next = debug_hooks->next;

            freetype_slot_release(ft_debug_hook_slots,
                                  debug_hooks->guest_callback);
            free(debug_hooks);
            debug_hooks = next;
        }
    }
    return status;
}

EXPORT int32_t my_FT_Done_FreeType(void *library)
{
    return freetype_done_library(library, my->FT_Done_FreeType, false);
}

EXPORT int32_t my_FT_Done_Library(void *library)
{
    return freetype_done_library(library, my->FT_Done_Library, true);
}

EXPORT int32_t my_FT_Bitmap_Blend(void *library, void *source,
                                  uintptr_t source_x, uintptr_t source_y,
                                  void *target, void *target_offset,
                                  uint32_t color_bits)
{
    FtVectorAbi source_offset = {(intptr_t)source_x, (intptr_t)source_y};
    FtColorAbi color;

    memcpy(&color, &color_bits, sizeof(color));
    return my->FT_Bitmap_Blend(library, source, source_offset, target,
                               target_offset, color);
}

EXPORT int32_t my_FT_Palette_Set_Foreground_Color(void *face,
                                                   uint32_t color_bits)
{
    FtColorAbi color;

    memcpy(&color, &color_bits, sizeof(color));
    return my->FT_Palette_Set_Foreground_Color(face, color);
}

EXPORT int32_t my_FT_Get_Paint(void *face, void *paint,
                               int32_t insert_root_transform, void *result)
{
    FtOpaquePaintAbi opaque = {
        .paint = paint,
        .insert_root_transform = insert_root_transform,
    };

    return my->FT_Get_Paint(face, opaque, result);
}

EXPORT int32_t my_FT_Outline_Decompose(void *outline,
                                       FtOutlineFuncsAbi *functions,
                                       void *user)
{
    FtOutlineFuncsAbi native = {0};
    void *callbacks[4] = {0};
    FtCallbackSlot *slots[4] = {
        ft_move_slots, ft_line_slots, ft_conic_slots, ft_cubic_slots,
    };
    void *const *thunks[4] = {
        ft_move_thunks, ft_line_thunks, ft_conic_thunks, ft_cubic_thunks,
    };
    const char *limitations[4] = {
        "outline move callback slots exhausted",
        "outline line callback slots exhausted",
        "outline conic callback slots exhausted",
        "outline cubic callback slots exhausted",
    };
    void **guest_callbacks;
    int32_t status;

    if (!functions) {
        return FT_ERROR_INVALID_ARGUMENT;
    }
    native = *functions;
    guest_callbacks = &functions->move_to;
    for (size_t i = 0; i < 4; i++) {
        callbacks[i] = freetype_slot_acquire(slots[i], thunks[i],
                                             guest_callbacks[i],
                                             limitations[i]);
        if (guest_callbacks[i] && !callbacks[i]) {
            for (size_t j = 0; j < i; j++) {
                if (GetNativeFnc((uintptr_t)guest_callbacks[j]) == NULL) {
                    freetype_slot_release(slots[j],
                                          (uintptr_t)guest_callbacks[j]);
                }
            }
            return FT_ERROR_OUT_OF_MEMORY;
        }
    }
    native.move_to = callbacks[0];
    native.line_to = callbacks[1];
    native.conic_to = callbacks[2];
    native.cubic_to = callbacks[3];
    status = my->FT_Outline_Decompose(outline, &native, user);
    for (size_t i = 0; i < 4; i++) {
        if (guest_callbacks[i] &&
            GetNativeFnc((uintptr_t)guest_callbacks[i]) == NULL) {
            freetype_slot_release(slots[i],
                                  (uintptr_t)guest_callbacks[i]);
        }
    }
    return status;
}

static void freetype_raster_span(FtRasterCallbackContext *context,
                                 size_t index, int32_t y, int32_t count,
                                 const void *spans)
{
    if (!context->callbacks[index]) {
        return;
    }
    if (context->native_callbacks[index]) {
        ((FtSpanFuncAbi)context->native_callbacks[index])(
            y, count, spans, context->guest_user);
        return;
    }
    RunFunctionFmt((uintptr_t)context->callbacks[index], "iipp", y, count,
                   spans, context->guest_user);
}

static void freetype_raster_gray_spans(int32_t y, int32_t count,
                                       const void *spans, void *user)
{
    freetype_raster_span(user, 0, y, count, spans);
}

static void freetype_raster_black_spans(int32_t y, int32_t count,
                                        const void *spans, void *user)
{
    freetype_raster_span(user, 1, y, count, spans);
}

static int32_t freetype_raster_bit_test(int32_t y, int32_t x, void *user)
{
    FtRasterCallbackContext *context = user;

    if (!context->callbacks[2]) {
        return 0;
    }
    if (context->native_callbacks[2]) {
        return ((FtRasterBitTestFuncAbi)context->native_callbacks[2])(
            y, x, context->guest_user);
    }
    return (int32_t)RunFunctionFmt((uintptr_t)context->callbacks[2], "iip",
                                   y, x, context->guest_user);
}

static void freetype_raster_bit_set(int32_t y, int32_t x, void *user)
{
    FtRasterCallbackContext *context = user;

    if (!context->callbacks[3]) {
        return;
    }
    if (context->native_callbacks[3]) {
        ((FtRasterBitSetFuncAbi)context->native_callbacks[3])(
            y, x, context->guest_user);
        return;
    }
    RunFunctionFmt((uintptr_t)context->callbacks[3], "iip", y, x,
                   context->guest_user);
}

EXPORT int32_t my_FT_Outline_Render(void *library, void *outline,
                                    FtRasterParamsAbi *params)
{
    FtRasterParamsAbi native;
    FtRasterCallbackContext context = {0};
    void **callbacks;
    bool needs_bridge = false;

    if (!params) {
        return FT_ERROR_INVALID_ARGUMENT;
    }
    native = *params;
    callbacks = &native.gray_spans;
    for (size_t i = 0; i < 4; i++) {
        context.callbacks[i] = callbacks[i];

        if (!context.callbacks[i]) {
            continue;
        }
        context.native_callbacks[i] =
            GetNativeFnc((uintptr_t)context.callbacks[i]);
        if (!context.native_callbacks[i]) {
            needs_bridge = true;
        }
    }
    if (!needs_bridge) {
        return my->FT_Outline_Render(library, outline, &native);
    }

    context.guest_user = native.user;
    native.gray_spans = context.callbacks[0]
        ? freetype_raster_gray_spans : NULL;
    native.black_spans = context.callbacks[1]
        ? freetype_raster_black_spans : NULL;
    native.bit_test = context.callbacks[2]
        ? freetype_raster_bit_test : NULL;
    native.bit_set = context.callbacks[3]
        ? freetype_raster_bit_set : NULL;
    native.user = &context;
    return my->FT_Outline_Render(library, outline, &native);
}

static FtMemoryAbi *freetype_outline_memory_native(
    FtMemoryAbi *memory, FtMemoryBridge **bridge)
{
    *bridge = NULL;
    if (!freetype_memory_requires_bridge(memory)) {
        return memory;
    }
    *bridge = freetype_memory_bridge_new(memory);
    return *bridge ? &(*bridge)->native : NULL;
}

EXPORT int32_t my_FT_Outline_New_Internal(FtMemoryAbi *memory,
                                           uint32_t num_points,
                                           int32_t num_contours,
                                           void *outline)
{
    FtMemoryBridge *bridge;
    FtMemoryAbi *native_memory =
        freetype_outline_memory_native(memory, &bridge);
    int32_t status;

    if ((memory && !native_memory) || !my->FT_Outline_New_Internal) {
        free(bridge);
        return memory && !native_memory
            ? FT_ERROR_OUT_OF_MEMORY : FT_ERROR_UNIMPLEMENTED_FEATURE;
    }
    status = my->FT_Outline_New_Internal(
        native_memory, num_points, num_contours, outline);
    free(bridge);
    return status;
}

EXPORT int32_t my_FT_Outline_Done_Internal(FtMemoryAbi *memory,
                                            void *outline)
{
    FtMemoryBridge *bridge;
    FtMemoryAbi *native_memory =
        freetype_outline_memory_native(memory, &bridge);
    int32_t status;

    if ((memory && !native_memory) || !my->FT_Outline_Done_Internal) {
        free(bridge);
        return memory && !native_memory
            ? FT_ERROR_OUT_OF_MEMORY : FT_ERROR_UNIMPLEMENTED_FEATURE;
    }
    status = my->FT_Outline_Done_Internal(native_memory, outline);
    free(bridge);
    return status;
}

EXPORT int32_t my_FT_List_Iterate(void *list, void *iterator, void *user)
{
    void *native = freetype_slot_acquire(
        ft_list_iterator_slots, ft_list_iterator_thunks, iterator,
        "list iterator callback slots exhausted");
    int32_t status;

    if (iterator && !native) {
        return FT_ERROR_OUT_OF_MEMORY;
    }
    status = my->FT_List_Iterate(list, native, user);
    if (iterator && GetNativeFnc((uintptr_t)iterator) == NULL) {
        freetype_slot_release(ft_list_iterator_slots, (uintptr_t)iterator);
    }
    return status;
}

EXPORT void my_FT_List_Finalize(void *list, void *destroy, void *memory,
                                void *user)
{
    void *native = freetype_slot_acquire(
        ft_list_destroy_slots, ft_list_destroy_thunks, destroy,
        "list destructor callback slots exhausted");

    if (destroy && !native) {
        return;
    }
    my->FT_List_Finalize(list, native, memory, user);
    if (destroy && GetNativeFnc((uintptr_t)destroy) == NULL) {
        freetype_slot_release(ft_list_destroy_slots, (uintptr_t)destroy);
    }
}

EXPORT int32_t my_FT_Add_Module(void *library, void *module_class)
{
    (void)library;
    (void)module_class;
    kzt_groups_log_wrapper_limitation(
        freetypeName, "FT_Add_Module callback-bearing class is unsupported");
    return FT_ERROR_UNIMPLEMENTED_FEATURE;
}

EXPORT int32_t my_FT_New_Library(void *memory, void **library)
{
    FtMemoryBridge *bridge = NULL;
    void *native_memory = memory;
    int32_t status;

    if (freetype_memory_requires_bridge(memory)) {
        bridge = freetype_memory_bridge_new(memory);
        if (!bridge) {
            if (library) {
                *library = NULL;
            }
            return FT_ERROR_OUT_OF_MEMORY;
        }
        native_memory = &bridge->native;
    }
    status = my->FT_New_Library(native_memory, library);
    if (status != FT_ERROR_SUCCESS || !library || !*library) {
        free(bridge);
        return status;
    }
    if (!freetype_track_library(*library)) {
        my->FT_Done_Library(*library);
        free(bridge);
        *library = NULL;
        return FT_ERROR_OUT_OF_MEMORY;
    }
    if (bridge) {
        freetype_memory_bridge_track(bridge, *library);
    }
    return status;
}

EXPORT int32_t my_FTC_Manager_New(void *library, uint32_t max_faces,
                                  uint32_t max_sizes, uintptr_t max_bytes,
                                  void *requester, void *request_data,
                                  void **manager)
{
    uintptr_t guest = (uintptr_t)requester;
    void *native = freetype_slot_acquire(
        ft_face_requester_slots, ft_face_requester_thunks, requester,
        "FTC face requester callback slots exhausted");
    FtCacheManagerTracker *tracker = NULL;
    int32_t status;

    if (requester && !native) {
        if (manager) {
            *manager = NULL;
        }
        return FT_ERROR_OUT_OF_MEMORY;
    }
    if (requester && !GetNativeFnc(guest)) {
        tracker = calloc(1, sizeof(*tracker));
        if (!tracker) {
            freetype_slot_release(ft_face_requester_slots, guest);
            if (manager) {
                *manager = NULL;
            }
            return FT_ERROR_OUT_OF_MEMORY;
        }
    }
    status = my->FTC_Manager_New(library, max_faces, max_sizes, max_bytes,
                                 native, request_data, manager);
    if (status != FT_ERROR_SUCCESS || !manager || !*manager || !tracker) {
        if (tracker) {
            freetype_slot_release(ft_face_requester_slots, guest);
            free(tracker);
        }
        return status;
    }
    tracker->manager = *manager;
    tracker->guest_requester = guest;
    g_mutex_lock(&freetype_lock);
    tracker->next = freetype_cache_managers;
    freetype_cache_managers = tracker;
    g_mutex_unlock(&freetype_lock);
    return status;
}

EXPORT void my_FTC_Manager_Done(void *manager)
{
    FtCacheManagerTracker *tracker = NULL;

    my->FTC_Manager_Done(manager);
    g_mutex_lock(&freetype_lock);
    for (FtCacheManagerTracker **link = &freetype_cache_managers; *link;
         link = &(*link)->next) {
        if ((*link)->manager != manager) {
            continue;
        }
        tracker = *link;
        *link = tracker->next;
        break;
    }
    g_mutex_unlock(&freetype_lock);
    if (tracker) {
        freetype_slot_release(ft_face_requester_slots,
                              tracker->guest_requester);
        free(tracker);
    }
}

EXPORT void my_FT_Set_Debug_Hook(void *library, uint32_t index,
                                 void *callback)
{
    uintptr_t guest = (uintptr_t)callback;
    void *native = GetNativeFnc(guest);
    FtDebugHookTracker *tracker = NULL;
    FtDebugHookTracker *old = NULL;

    if (!library || !callback || index >= 4) {
        my->FT_Set_Debug_Hook(library, index, native);
        return;
    }

    if (!native) {
        native = freetype_slot_acquire(
            ft_debug_hook_slots, ft_debug_hook_thunks, callback,
            "debug hook callback slots exhausted");
        if (!native) {
            return;
        }
        tracker = calloc(1, sizeof(*tracker));
        if (!tracker) {
            freetype_slot_release(ft_debug_hook_slots, guest);
            return;
        }
        tracker->library = library;
        tracker->index = index;
        tracker->guest_callback = guest;
    }
    my->FT_Set_Debug_Hook(library, index, native);
    g_mutex_lock(&freetype_lock);
    for (FtDebugHookTracker **link = &freetype_debug_hooks; *link;
         link = &(*link)->next) {
        if ((*link)->library != library || (*link)->index != index) {
            continue;
        }
        old = *link;
        *link = old->next;
        break;
    }
    if (tracker) {
        tracker->next = freetype_debug_hooks;
        freetype_debug_hooks = tracker;
    }
    g_mutex_unlock(&freetype_lock);
    if (old) {
        freetype_slot_release(ft_debug_hook_slots, old->guest_callback);
        free(old);
    }
}

EXPORT void my_FT_Set_Log_Handler(void *callback)
{
    void *native = GetNativeFnc((uintptr_t)callback);

    if (callback && !native) {
        kzt_groups_log_wrapper_limitation(
            freetypeName,
            "FT_Set_Log_Handler guest va_list callback is unsupported");
        return;
    }
    my->FT_Set_Log_Handler(native);
}

#define DEFINE_UNSUPPORTED_STREAM(name) \
    EXPORT int32_t my_##name(void *target, void *source) \
    { \
        (void)target; \
        (void)source; \
        kzt_groups_log_wrapper_limitation( \
            freetypeName, #name " callback-bearing stream is unsupported"); \
        return FT_ERROR_UNIMPLEMENTED_FEATURE; \
    }
DEFINE_UNSUPPORTED_STREAM(FT_Stream_OpenBzip2)
DEFINE_UNSUPPORTED_STREAM(FT_Stream_OpenGzip)
DEFINE_UNSUPPORTED_STREAM(FT_Stream_OpenLZW)
#undef DEFINE_UNSUPPORTED_STREAM

EXPORT void *my_TT_New_Context(void)
{
    kzt_groups_log_wrapper_limitation(
        freetypeName, "internal TT_New_Context ABI is unsupported");
    return NULL;
}

EXPORT int32_t my_TT_RunIns(void)
{
    kzt_groups_log_wrapper_limitation(
        freetypeName, "internal TT_RunIns ABI is unsupported");
    return FT_ERROR_UNIMPLEMENTED_FEATURE;
}

static void freetype_state_clear(void)
{
    g_mutex_lock(&freetype_lock);
    while (freetype_libraries) {
        FtLibraryTracker *next = freetype_libraries->next;

        free(freetype_libraries);
        freetype_libraries = next;
    }
    while (freetype_faces) {
        FtFaceTracker *next = freetype_faces->next;

        free(freetype_faces->stream_bridge);
        free(freetype_faces);
        freetype_faces = next;
    }
    while (freetype_sizes) {
        FtSizeTracker *next = freetype_sizes->next;

        free(freetype_sizes);
        freetype_sizes = next;
    }
    while (freetype_borrowed_faces) {
        FtBorrowTracker *next = freetype_borrowed_faces->next;

        free(freetype_borrowed_faces);
        freetype_borrowed_faces = next;
    }
    while (freetype_cache_managers) {
        FtCacheManagerTracker *next = freetype_cache_managers->next;

        free(freetype_cache_managers);
        freetype_cache_managers = next;
    }
    while (freetype_debug_hooks) {
        FtDebugHookTracker *next = freetype_debug_hooks->next;

        free(freetype_debug_hooks);
        freetype_debug_hooks = next;
    }
    while (freetype_generic_patches) {
        FtGenericPatch *next = freetype_generic_patches->global_next;

        if (!freetype_generic_patches->invoked &&
            freetype_generic_patches->generic->finalizer ==
                freetype_generic_finalizer) {
            freetype_generic_patches->generic->finalizer =
                freetype_generic_patches->guest_finalizer;
        }
        free(freetype_generic_patches);
        freetype_generic_patches = next;
    }
    while (freetype_memory_bridges) {
        FtMemoryBridge *next = freetype_memory_bridges->next;

        free(freetype_memory_bridges);
        freetype_memory_bridges = next;
    }
    memset(ft_move_slots, 0, sizeof(ft_move_slots));
    memset(ft_line_slots, 0, sizeof(ft_line_slots));
    memset(ft_conic_slots, 0, sizeof(ft_conic_slots));
    memset(ft_cubic_slots, 0, sizeof(ft_cubic_slots));
    memset(ft_list_iterator_slots, 0, sizeof(ft_list_iterator_slots));
    memset(ft_list_destroy_slots, 0, sizeof(ft_list_destroy_slots));
    memset(ft_face_requester_slots, 0, sizeof(ft_face_requester_slots));
    memset(ft_debug_hook_slots, 0, sizeof(ft_debug_hook_slots));
    g_mutex_unlock(&freetype_lock);
}

static bool freetype_wrapper_function_enabled(const char *name)
{
    if (strcmp(name, "FT_Outline_New_Internal") &&
        strcmp(name, "FT_Outline_Done_Internal")) {
        return true;
    }
    return latx_font_capability_enabled(
        LATX_FONT_CAP_LEGACY_OUTLINE_MEMORY);
}

#define CUSTOM_INIT \
    getMy(lib);

#define CUSTOM_FINI \
    freetype_state_clear(); \
    freeMy();

#define WRAPPEDLIB_FUNCTION_ENABLED(name) \
    freetype_wrapper_function_enabled(name)

#include "wrappedlib_init.h"

#pragma GCC diagnostic pop
