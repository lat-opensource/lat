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
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wrappedlibs.h"

#include "box64context.h"
#include "bridge.h"
#include "callback.h"
#include "debug.h"
#include "kzt-groups.h"
#include "library_private.h"
#include "myalign.h"
#include "wrappedinput-preflight.h"
#include "wrapper.h"
#include "x64-vaargs.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

const char *xkbcommonName = "libxkbcommon.so.0";
#define LIBNAME xkbcommon

#define PRE_INIT_GUEST \
    do { \
        if (latx_input_preflight_or_disable(&box64->box64_ld_lib, \
                                            xkbcommonName) != 0) { \
            return -1; \
        } \
    } while (0);

enum {
    XKB_LOG_SLOT_COUNT = 16,
    XKB_VARIADIC_MOD_LIMIT = 8,
    XKB_MOD_INVALID = UINT32_MAX,
};

typedef void (*XkbLogFn)(void *context, int32_t level,
                         const char *format, va_list args);
typedef int32_t (*XkbModNamesActiveFn)(void *state, uint32_t type,
                                      uint32_t match, ...);
typedef int32_t (*XkbModIndicesActiveFn)(void *state, uint32_t type,
                                        uint32_t match, ...);

typedef struct XkbKeyIterContext {
    void *guest_callback;
    struct XkbKeyIterContext *previous;
} XkbKeyIterContext;

static uintptr_t xkb_log_slots[XKB_LOG_SLOT_COUNT];
static GMutex xkb_log_lock;
static __thread XkbKeyIterContext *xkb_key_iter_context;
static XkbModNamesActiveFn xkb_mod_names_active;
static XkbModIndicesActiveFn xkb_mod_indices_active;

#include "generated/wrappedxkbcommontypes.h"
#include "wrappercallback.h"

static void xkb_log_bridge(size_t slot, void *context, int32_t level,
                           const char *format, va_list args)
{
    static const char guest_format[] = "%s";
    char message[2048];
    uint64_t reg_save_area[X64_VA_MAX_REG / sizeof(uint64_t)] = {0};
    uint64_t overflow_arg_area[1] = {0};
    x64_va_list_t guest_args;
    uintptr_t guest_callback;
    va_list copy;

    va_copy(copy, args);
    if (vsnprintf(message, sizeof(message), format, copy) < 0) {
        snprintf(message, sizeof(message), "%s", format ? format : "");
    }
    va_end(copy);

    g_mutex_lock(&xkb_log_lock);
    guest_callback = xkb_log_slots[slot];
    g_mutex_unlock(&xkb_log_lock);
    if (!guest_callback) {
        return;
    }

    reg_save_area[3] = (uintptr_t)message;
    guest_args->gp_offset = 3 * sizeof(uint64_t);
    guest_args->fp_offset = X64_VA_MAX_REG;
    guest_args->overflow_arg_area = overflow_arg_area;
    guest_args->reg_save_area = reg_save_area;
    RunFunctionFmt(guest_callback, "pipp", context, level,
                   guest_format, guest_args);
}

#define XKB_LOG_THUNK(index) \
    static void xkb_log_thunk_##index(void *context, int32_t level, \
                                      const char *format, va_list args) \
    { \
        xkb_log_bridge(index, context, level, format, args); \
    }
XKB_LOG_THUNK(0)
XKB_LOG_THUNK(1)
XKB_LOG_THUNK(2)
XKB_LOG_THUNK(3)
XKB_LOG_THUNK(4)
XKB_LOG_THUNK(5)
XKB_LOG_THUNK(6)
XKB_LOG_THUNK(7)
XKB_LOG_THUNK(8)
XKB_LOG_THUNK(9)
XKB_LOG_THUNK(10)
XKB_LOG_THUNK(11)
XKB_LOG_THUNK(12)
XKB_LOG_THUNK(13)
XKB_LOG_THUNK(14)
XKB_LOG_THUNK(15)
#undef XKB_LOG_THUNK

static XkbLogFn xkb_log_thunks[XKB_LOG_SLOT_COUNT] = {
    xkb_log_thunk_0, xkb_log_thunk_1, xkb_log_thunk_2,
    xkb_log_thunk_3, xkb_log_thunk_4, xkb_log_thunk_5,
    xkb_log_thunk_6, xkb_log_thunk_7, xkb_log_thunk_8,
    xkb_log_thunk_9, xkb_log_thunk_10, xkb_log_thunk_11,
    xkb_log_thunk_12, xkb_log_thunk_13, xkb_log_thunk_14,
    xkb_log_thunk_15,
};

static void *xkb_log_callback(void *callback)
{
    void *native;

    if (!callback || (native = GetNativeFnc((uintptr_t)callback))) {
        return callback ? native : NULL;
    }
    g_mutex_lock(&xkb_log_lock);
    for (size_t i = 0; i < ARRAY_SIZE(xkb_log_slots); i++) {
        if (xkb_log_slots[i] == (uintptr_t)callback) {
            native = xkb_log_thunks[i];
            g_mutex_unlock(&xkb_log_lock);
            return native;
        }
    }
    for (size_t i = 0; i < ARRAY_SIZE(xkb_log_slots); i++) {
        if (!xkb_log_slots[i]) {
            xkb_log_slots[i] = (uintptr_t)callback;
            native = xkb_log_thunks[i];
            g_mutex_unlock(&xkb_log_lock);
            return native;
        }
    }
    g_mutex_unlock(&xkb_log_lock);
    kzt_groups_log_wrapper_limitation(
        xkbcommonName, "xkb log callback slots exhausted");
    return NULL;
}

static void xkb_key_iter_bridge(void *keymap, uint32_t key, void *data)
{
    XkbKeyIterContext *context = xkb_key_iter_context;

    if (context && context->guest_callback) {
        RunFunctionFmt((uintptr_t)context->guest_callback, "pup",
                       keymap, key, data);
    }
}

static void *xkb_guest_malloc(size_t size)
{
    static char guest_libc_name[] = "libc.so.6";
    struct malloc_map *map;

    if (!my_context) {
        return NULL;
    }
    map = SearchMallocMap(my_context, guest_libc_name);
    if (!map || !map->mallocp) {
        return NULL;
    }
    return (void *)(uintptr_t)RunFunctionWithState(
        (uintptr_t)map->mallocp, 1, size);
}

EXPORT void my_xkb_context_set_log_fn(void *context, void *callback)
{
    void *native = xkb_log_callback(callback);

    if (callback && !native) {
        kzt_groups_log_wrapper_limitation(
            xkbcommonName, "cannot bridge xkb log callback; using default");
    }
    my->xkb_context_set_log_fn(context, native);
}

EXPORT void my_xkb_keymap_key_for_each(void *keymap, void *callback,
                                       void *data)
{
    void *native = GetNativeFnc((uintptr_t)callback);
    XkbKeyIterContext context;

    if (!callback || native) {
        my->xkb_keymap_key_for_each(keymap, callback ? native : NULL, data);
        return;
    }
    context.guest_callback = callback;
    context.previous = xkb_key_iter_context;
    xkb_key_iter_context = &context;
    my->xkb_keymap_key_for_each(keymap, xkb_key_iter_bridge, data);
    xkb_key_iter_context = context.previous;
}

EXPORT char *my_xkb_keymap_get_as_string(void *keymap, uint32_t format)
{
    char *native = my->xkb_keymap_get_as_string(keymap, format);
    char *guest;
    size_t size;

    if (!native) {
        return NULL;
    }
    size = strlen(native) + 1;
    guest = xkb_guest_malloc(size);
    if (guest) {
        memcpy(guest, native, size);
    } else {
        kzt_groups_log_wrapper_limitation(
            xkbcommonName, "cannot allocate guest keymap string");
    }
    free(native);
    return guest;
}

EXPORT void *my_xkb_keymap_new_from_file(void *context, void *file,
                                         uint32_t format, uint32_t flags)
{
    (void)context;
    (void)file;
    (void)format;
    (void)flags;
    kzt_groups_log_wrapper_limitation(
        xkbcommonName, "guest FILE keymap input is unsupported");
    return NULL;
}

EXPORT void *my_xkb_compose_table_new_from_file(void *context, void *file,
                                                 const char *locale,
                                                 uint32_t format,
                                                 uint32_t flags)
{
    (void)context;
    (void)file;
    (void)locale;
    (void)format;
    (void)flags;
    kzt_groups_log_wrapper_limitation(
        xkbcommonName, "guest FILE compose input is unsupported");
    return NULL;
}

static int32_t call_mod_names(void *state, uint32_t type, uint32_t match,
                              const char **mods, size_t count)
{
    if (!xkb_mod_names_active) {
        return -1;
    }
#define XKB_NAMES_CALL(...) \
    xkb_mod_names_active(state, type, match, __VA_ARGS__, NULL)
    switch (count) {
    case 0: return xkb_mod_names_active(state, type, match, NULL);
    case 1: return XKB_NAMES_CALL(mods[0]);
    case 2: return XKB_NAMES_CALL(mods[0], mods[1]);
    case 3: return XKB_NAMES_CALL(mods[0], mods[1], mods[2]);
    case 4: return XKB_NAMES_CALL(mods[0], mods[1], mods[2], mods[3]);
    case 5: return XKB_NAMES_CALL(mods[0], mods[1], mods[2], mods[3],
                                  mods[4]);
    case 6: return XKB_NAMES_CALL(mods[0], mods[1], mods[2], mods[3],
                                  mods[4], mods[5]);
    case 7: return XKB_NAMES_CALL(mods[0], mods[1], mods[2], mods[3],
                                  mods[4], mods[5], mods[6]);
    case 8: return XKB_NAMES_CALL(mods[0], mods[1], mods[2], mods[3],
                                  mods[4], mods[5], mods[6], mods[7]);
    default:
        kzt_groups_log_wrapper_limitation(
            xkbcommonName, "more than eight modifier names are unsupported");
        return -1;
    }
#undef XKB_NAMES_CALL
}

static int32_t call_mod_indices(void *state, uint32_t type, uint32_t match,
                                const uint32_t *mods, size_t count)
{
    if (!xkb_mod_indices_active) {
        return -1;
    }
#define XKB_INDICES_CALL(...) \
    xkb_mod_indices_active(state, type, match, __VA_ARGS__, XKB_MOD_INVALID)
    switch (count) {
    case 0:
        return xkb_mod_indices_active(state, type, match, XKB_MOD_INVALID);
    case 1: return XKB_INDICES_CALL(mods[0]);
    case 2: return XKB_INDICES_CALL(mods[0], mods[1]);
    case 3: return XKB_INDICES_CALL(mods[0], mods[1], mods[2]);
    case 4: return XKB_INDICES_CALL(mods[0], mods[1], mods[2], mods[3]);
    case 5: return XKB_INDICES_CALL(mods[0], mods[1], mods[2], mods[3],
                                    mods[4]);
    case 6: return XKB_INDICES_CALL(mods[0], mods[1], mods[2], mods[3],
                                    mods[4], mods[5]);
    case 7: return XKB_INDICES_CALL(mods[0], mods[1], mods[2], mods[3],
                                    mods[4], mods[5], mods[6]);
    case 8: return XKB_INDICES_CALL(mods[0], mods[1], mods[2], mods[3],
                                    mods[4], mods[5], mods[6], mods[7]);
    default:
        kzt_groups_log_wrapper_limitation(
            xkbcommonName, "more than eight modifier indices are unsupported");
        return -1;
    }
#undef XKB_INDICES_CALL
}

EXPORT int32_t my_xkb_state_mod_names_are_active(void *state, uint32_t type,
                                                  uint32_t match)
{
    const char *mods[XKB_VARIADIC_MOD_LIMIT + 1];
    LatxX64VaReader reader;
    size_t count = 0;

    latx_x64_va_reader_live(&reader, 3);
    while (count < ARRAY_SIZE(mods)) {
        const char *name =
            (const char *)(uintptr_t)latx_x64_va_gp(&reader);

        if (!name) {
            return call_mod_names(state, type, match, mods, count);
        }
        mods[count++] = name;
    }
    return call_mod_names(state, type, match, mods, count);
}

EXPORT int32_t my_xkb_state_mod_indices_are_active(void *state,
                                                    uint32_t type,
                                                    uint32_t match)
{
    uint32_t mods[XKB_VARIADIC_MOD_LIMIT + 1];
    LatxX64VaReader reader;
    size_t count = 0;

    latx_x64_va_reader_live(&reader, 3);
    while (count < ARRAY_SIZE(mods)) {
        uint32_t index = (uint32_t)latx_x64_va_gp(&reader);

        if (index == XKB_MOD_INVALID) {
            return call_mod_indices(state, type, match, mods, count);
        }
        mods[count++] = index;
    }
    return call_mod_indices(state, type, match, mods, count);
}

static void xkbcommon_state_clear(void)
{
    g_mutex_lock(&xkb_log_lock);
    memset(xkb_log_slots, 0, sizeof(xkb_log_slots));
    g_mutex_unlock(&xkb_log_lock);
    xkb_mod_names_active = NULL;
    xkb_mod_indices_active = NULL;
}

#define CUSTOM_INIT \
    getMy(lib); \
    xkb_mod_names_active = (XkbModNamesActiveFn)dlsym( \
        lib->priv.w.lib, "xkb_state_mod_names_are_active"); \
    xkb_mod_indices_active = (XkbModIndicesActiveFn)dlsym( \
        lib->priv.w.lib, "xkb_state_mod_indices_are_active");

#define CUSTOM_FINI \
    xkbcommon_state_clear(); \
    freeMy();

#include "wrappedlib_init.h"

#pragma GCC diagnostic pop
