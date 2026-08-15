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

_Static_assert(sizeof(FtVectorAbi) == 16, "FT_Vector ABI");
_Static_assert(sizeof(FtColorAbi) == 4, "FT_Color ABI");
_Static_assert(sizeof(FtOpaquePaintAbi) == 16, "FT_OpaquePaint ABI");
_Static_assert(offsetof(FtFaceAbi, generic) == 88, "FT_Face generic ABI");
_Static_assert(offsetof(FtFaceAbi, glyph) == 152, "FT_Face glyph ABI");
_Static_assert(offsetof(FtGlyphSlotAbi, generic) == 32,
               "FT_GlyphSlot generic ABI");
_Static_assert(offsetof(FtSizeAbi, generic) == 8, "FT_Size generic ABI");
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
    struct FtFaceTracker *next;
} FtFaceTracker;

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

static GMutex freetype_lock;
static FtFaceTracker *freetype_faces;
static FtSizeTracker *freetype_sizes;
static FtBorrowTracker *freetype_borrowed_faces;

static bool freetype_generic_is_safe(const FtGenericAbi *generic)
{
    return !generic || !generic->finalizer ||
           GetNativeFnc((uintptr_t)generic->finalizer) != NULL;
}

static bool freetype_face_is_safe(void *face)
{
    FtFaceAbi *record = face;

    if (!record || !freetype_generic_is_safe(&record->generic)) {
        return record == NULL;
    }
    if (record->glyph &&
        !freetype_generic_is_safe(
            &((FtGlyphSlotAbi *)record->glyph)->generic)) {
        return false;
    }
    if (record->size &&
        !freetype_generic_is_safe(&((FtSizeAbi *)record->size)->generic)) {
        return false;
    }
    for (FtSizeTracker *size = freetype_sizes; size; size = size->next) {
        if (size->face == face &&
            !freetype_generic_is_safe(&((FtSizeAbi *)size->size)->generic)) {
            return false;
        }
    }
    return true;
}

static bool freetype_library_is_safe(void *library)
{
    for (FtFaceTracker *face = freetype_faces; face; face = face->next) {
        if (face->library == library && !freetype_face_is_safe(face->face)) {
            return false;
        }
    }
    return true;
}

static bool freetype_track_face(void *library, void *face)
{
    FtFaceTracker *tracker = calloc(1, sizeof(*tracker));

    if (!tracker) {
        return false;
    }
    tracker->library = library;
    tracker->face = face;
    tracker->references = 1;
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

static void freetype_untrack_face_locked(void *face)
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
        free(tracker);
        return;
    }
}

static void freetype_untrack_library_locked(void *library)
{
    FtFaceTracker *face = freetype_faces;

    while (face) {
        FtFaceTracker *next = face->next;

        if (face->library == library) {
            freetype_untrack_face_locked(face->face);
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
#undef THUNK_ADDRESS

static int32_t freetype_finish_face_creation(void *library, void **face,
                                             int32_t status)
{
    if (status != FT_ERROR_SUCCESS || !face || !*face) {
        return status;
    }
    if (freetype_track_face(library, *face)) {
        return status;
    }
    my->FT_Done_Face(*face);
    *face = NULL;
    return FT_ERROR_OUT_OF_MEMORY;
}

EXPORT int32_t my_FT_New_Face(void *library, void *path, intptr_t index,
                              void **face)
{
    return freetype_finish_face_creation(
        library, face, my->FT_New_Face(library, path, index, face));
}

EXPORT int32_t my_FT_New_Memory_Face(void *library, void *base,
                                     intptr_t size, intptr_t index,
                                     void **face)
{
    return freetype_finish_face_creation(
        library, face,
        my->FT_New_Memory_Face(library, base, size, index, face));
}

EXPORT int32_t my_FT_Open_Face(void *library, FtOpenArgsAbi *args,
                               intptr_t index, void **face)
{
    int32_t status;

    if (args && (args->flags & FT_OPEN_STREAM) && args->stream &&
        (args->stream->read || args->stream->close)) {
        kzt_groups_log_wrapper_limitation(
            freetypeName, "FT_Open_Face guest stream callbacks are unsupported");
        return FT_ERROR_UNIMPLEMENTED_FEATURE;
    }
    status = my->FT_Open_Face(library, args, index, face);
    return freetype_finish_face_creation(library, face, status);
}

EXPORT int32_t my_FT_Attach_Stream(void *face, FtOpenArgsAbi *args)
{
    if (args && (args->flags & FT_OPEN_STREAM) && args->stream &&
        (args->stream->read || args->stream->close)) {
        kzt_groups_log_wrapper_limitation(
            freetypeName,
            "FT_Attach_Stream guest stream callbacks are unsupported");
        return FT_ERROR_UNIMPLEMENTED_FEATURE;
    }
    return my->FT_Attach_Stream(face, args);
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
    int32_t status;

    g_mutex_lock(&freetype_lock);
    if (size &&
        !freetype_generic_is_safe(&((FtSizeAbi *)size)->generic)) {
        g_mutex_unlock(&freetype_lock);
        kzt_groups_log_wrapper_limitation(
            freetypeName, "guest FT_Size generic finalizer is unsupported");
        return FT_ERROR_UNIMPLEMENTED_FEATURE;
    }
    g_mutex_unlock(&freetype_lock);
    status = my->FT_Done_Size(size);
    if (status == FT_ERROR_SUCCESS) {
        g_mutex_lock(&freetype_lock);
        freetype_untrack_size_locked(size);
        g_mutex_unlock(&freetype_lock);
    }
    return status;
}

EXPORT int32_t my_FT_Done_Face(void *face)
{
    int32_t status;

    g_mutex_lock(&freetype_lock);
    if (freetype_face_is_borrowed_locked(face)) {
        g_mutex_unlock(&freetype_lock);
        kzt_groups_log_wrapper_limitation(
            freetypeName, "cannot destroy a Cairo-borrowed FT_Face");
        return FT_ERROR_INVALID_ARGUMENT;
    }
    if (!freetype_face_is_safe(face)) {
        g_mutex_unlock(&freetype_lock);
        kzt_groups_log_wrapper_limitation(
            freetypeName, "guest FT_Face generic finalizer is unsupported");
        return FT_ERROR_UNIMPLEMENTED_FEATURE;
    }
    g_mutex_unlock(&freetype_lock);
    status = my->FT_Done_Face(face);
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
                freetype_untrack_face_locked(face);
            }
            break;
        }
        g_mutex_unlock(&freetype_lock);
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

    if (library && freetype_track_face(library, face)) {
        return status;
    }
    my->FT_Done_Face(face);
    return FT_ERROR_OUT_OF_MEMORY;
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
                                     int32_t (*finish)(void *))
{
    int32_t status;

    g_mutex_lock(&freetype_lock);
    if (!freetype_library_is_safe(library)) {
        g_mutex_unlock(&freetype_lock);
        kzt_groups_log_wrapper_limitation(
            freetypeName, "guest FreeType generic finalizer is unsupported");
        return FT_ERROR_UNIMPLEMENTED_FEATURE;
    }
    g_mutex_unlock(&freetype_lock);
    status = finish(library);
    if (status == FT_ERROR_SUCCESS) {
        g_mutex_lock(&freetype_lock);
        freetype_untrack_library_locked(library);
        g_mutex_unlock(&freetype_lock);
    }
    return status;
}

EXPORT int32_t my_FT_Done_FreeType(void *library)
{
    return freetype_done_library(library, my->FT_Done_FreeType);
}

EXPORT int32_t my_FT_Done_Library(void *library)
{
    return freetype_done_library(library, my->FT_Done_Library);
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

EXPORT int32_t my_FT_Outline_Render(void *library, void *outline,
                                    FtRasterParamsAbi *params)
{
    FtRasterParamsAbi native;
    void **callbacks;

    if (!params) {
        return FT_ERROR_INVALID_ARGUMENT;
    }
    native = *params;
    callbacks = &native.gray_spans;
    for (size_t i = 0; i < 4; i++) {
        void *callback = callbacks[i];

        if (!callback) {
            continue;
        }
        callbacks[i] = GetNativeFnc((uintptr_t)callback);
        if (!callbacks[i]) {
            kzt_groups_log_wrapper_limitation(
                freetypeName,
                "FT_Outline_Render guest callbacks are unsupported");
            return FT_ERROR_UNIMPLEMENTED_FEATURE;
        }
    }
    return my->FT_Outline_Render(library, outline, &native);
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
    (void)memory;
    if (library) {
        *library = NULL;
    }
    kzt_groups_log_wrapper_limitation(
        freetypeName, "FT_New_Library guest memory callbacks are unsupported");
    return FT_ERROR_UNIMPLEMENTED_FEATURE;
}

EXPORT int32_t my_FTC_Manager_New(void *library, uint32_t max_faces,
                                  uint32_t max_sizes, uintptr_t max_bytes,
                                  void *requester, void *request_data,
                                  void **manager)
{
    void *native = GetNativeFnc((uintptr_t)requester);

    if (requester && !native) {
        if (manager) {
            *manager = NULL;
        }
        kzt_groups_log_wrapper_limitation(
            freetypeName, "FTC_Manager_New guest requester is unsupported");
        return FT_ERROR_UNIMPLEMENTED_FEATURE;
    }
    return my->FTC_Manager_New(library, max_faces, max_sizes, max_bytes,
                               native, request_data, manager);
}

EXPORT void my_FT_Set_Debug_Hook(void *library, uint32_t index,
                                 void *callback)
{
    void *native = GetNativeFnc((uintptr_t)callback);

    if (callback && !native) {
        kzt_groups_log_wrapper_limitation(
            freetypeName, "FT_Set_Debug_Hook guest callback is unsupported");
        return;
    }
    my->FT_Set_Debug_Hook(library, index, native);
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
    while (freetype_faces) {
        FtFaceTracker *next = freetype_faces->next;

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
    memset(ft_move_slots, 0, sizeof(ft_move_slots));
    memset(ft_line_slots, 0, sizeof(ft_line_slots));
    memset(ft_conic_slots, 0, sizeof(ft_conic_slots));
    memset(ft_cubic_slots, 0, sizeof(ft_cubic_slots));
    memset(ft_list_iterator_slots, 0, sizeof(ft_list_iterator_slots));
    memset(ft_list_destroy_slots, 0, sizeof(ft_list_destroy_slots));
    g_mutex_unlock(&freetype_lock);
}

#define CUSTOM_INIT \
    getMy(lib);

#define CUSTOM_FINI \
    freetype_state_clear(); \
    freeMy();

#include "wrappedlib_init.h"

#pragma GCC diagnostic pop
