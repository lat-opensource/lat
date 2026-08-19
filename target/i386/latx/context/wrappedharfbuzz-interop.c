/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include <stdint.h>

#include "bridge.h"
#include "callback.h"
#include "kzt-groups.h"
#include "wrappedharfbuzz-interop.h"

#define HB_CALLBACK_MAGIC UINT64_C(0x484243414c4c424b)
#define HB_USER_DATA_MAGIC UINT64_C(0x4842555345524441)
#define HB_DESTROY_SLOT_COUNT 32

typedef struct LatxHbUserDataContext {
    uint64_t magic;
    void *guest_data;
    uintptr_t guest_destroy;
    void *native_destroy;
    struct LatxHbUserDataContext *next;
} LatxHbUserDataContext;

typedef struct LatxHbDestroySlot {
    uintptr_t guest;
    unsigned references;
} LatxHbDestroySlot;

static GMutex hb_callback_lock;
static LatxHbDestroySlot hb_destroy_slots[HB_DESTROY_SLOT_COUNT];
static LatxHbUserDataContext *hb_user_data_contexts;

static void hb_invoke_destroy(uintptr_t guest, void *native, void *data)
{
    if (native) {
        ((void (*)(void *))native)(data);
    } else if (guest) {
        RunFunctionFmt(guest, "p", data);
    }
}

LatxHbCallbackContext *latx_hb_callback_context_new(
    void *callback, void *user_data, void *destroy)
{
    LatxHbCallbackContext *context = g_try_new0(LatxHbCallbackContext, 1);

    if (!context) {
        return NULL;
    }
    context->magic = HB_CALLBACK_MAGIC;
    context->guest_callback = (uintptr_t)callback;
    context->native_callback = GetNativeFnc((uintptr_t)callback);
    context->guest_user_data = user_data;
    context->guest_destroy = (uintptr_t)destroy;
    context->native_destroy = GetNativeFnc((uintptr_t)destroy);
    return context;
}

void latx_hb_callback_context_destroy(void *opaque)
{
    LatxHbCallbackContext *context = opaque;

    if (!context || context->magic != HB_CALLBACK_MAGIC) {
        return;
    }
    context->magic = 0;
    hb_invoke_destroy(context->guest_destroy, context->native_destroy,
                      context->guest_user_data);
    g_free(context);
}

void latx_hb_callback_context_discard(LatxHbCallbackContext *context,
                                      bool invoke_destroy)
{
    if (!context || context->magic != HB_CALLBACK_MAGIC) {
        return;
    }
    context->magic = 0;
    if (invoke_destroy) {
        hb_invoke_destroy(context->guest_destroy, context->native_destroy,
                          context->guest_user_data);
    }
    g_free(context);
}

void latx_hb_invoke_guest_destroy(void *destroy, void *data)
{
    hb_invoke_destroy((uintptr_t)destroy,
                      GetNativeFnc((uintptr_t)destroy), data);
}

static void hb_user_data_context_destroy(void *opaque)
{
    LatxHbUserDataContext *context = opaque;
    LatxHbUserDataContext **cursor;

    if (!context || context->magic != HB_USER_DATA_MAGIC) {
        return;
    }
    g_mutex_lock(&hb_callback_lock);
    cursor = &hb_user_data_contexts;
    while (*cursor && *cursor != context) {
        cursor = &(*cursor)->next;
    }
    if (*cursor) {
        *cursor = context->next;
    }
    g_mutex_unlock(&hb_callback_lock);
    context->magic = 0;
    hb_invoke_destroy(context->guest_destroy, context->native_destroy,
                      context->guest_data);
    g_free(context);
}

int32_t latx_hb_set_user_data(LatxHbSetUserDataFunc host_set,
                              void *object, void *key, void *data,
                              void *destroy, int32_t replace)
{
    LatxHbUserDataContext *context;
    int32_t result;

    if (!host_set) {
        return 0;
    }
    if (!data && !destroy) {
        return host_set(object, key, NULL, NULL, replace);
    }
    context = g_try_new0(LatxHbUserDataContext, 1);
    if (!context) {
        return 0;
    }
    context->magic = HB_USER_DATA_MAGIC;
    context->guest_data = data;
    context->guest_destroy = (uintptr_t)destroy;
    context->native_destroy = GetNativeFnc((uintptr_t)destroy);
    g_mutex_lock(&hb_callback_lock);
    context->next = hb_user_data_contexts;
    hb_user_data_contexts = context;
    g_mutex_unlock(&hb_callback_lock);
    result = host_set(object, key, context, hb_user_data_context_destroy,
                      replace);
    if (!result) {
        LatxHbUserDataContext **cursor;

        g_mutex_lock(&hb_callback_lock);
        cursor = &hb_user_data_contexts;
        while (*cursor && *cursor != context) {
            cursor = &(*cursor)->next;
        }
        if (*cursor) {
            *cursor = context->next;
        }
        g_mutex_unlock(&hb_callback_lock);
        context->magic = 0;
        g_free(context);
    }
    return result;
}

void *latx_hb_get_user_data(LatxHbGetUserDataFunc host_get,
                            void *object, void *key)
{
    LatxHbUserDataContext *context;
    void *guest_data = NULL;
    bool found = false;

    if (!host_get) {
        return NULL;
    }
    context = host_get(object, key);
    g_mutex_lock(&hb_callback_lock);
    for (LatxHbUserDataContext *candidate = hb_user_data_contexts;
         candidate; candidate = candidate->next) {
        if (candidate != context) {
            continue;
        }
        guest_data = candidate->guest_data;
        found = true;
        break;
    }
    g_mutex_unlock(&hb_callback_lock);
    return found ? guest_data : context;
}

static uintptr_t hb_destroy_slot_guest(size_t index)
{
    uintptr_t guest;

    g_mutex_lock(&hb_callback_lock);
    guest = hb_destroy_slots[index].guest;
    g_mutex_unlock(&hb_callback_lock);
    return guest;
}

static void hb_destroy_slot_release(size_t index)
{
    g_mutex_lock(&hb_callback_lock);
    if (hb_destroy_slots[index].references &&
        --hb_destroy_slots[index].references == 0) {
        hb_destroy_slots[index].guest = 0;
    }
    g_mutex_unlock(&hb_callback_lock);
}

#define HB_DESTROY_SLOT_LIST(X) \
    X(0)  X(1)  X(2)  X(3)  X(4)  X(5)  X(6)  X(7) \
    X(8)  X(9)  X(10) X(11) X(12) X(13) X(14) X(15) \
    X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) \
    X(24) X(25) X(26) X(27) X(28) X(29) X(30) X(31)

#define DEFINE_HB_DESTROY_THUNK(N) \
    static void hb_destroy_thunk_##N(void *data) \
    { \
        uintptr_t guest = hb_destroy_slot_guest(N); \
        if (guest) { \
            RunFunctionFmt(guest, "p", data); \
        } \
        hb_destroy_slot_release(N); \
    }
HB_DESTROY_SLOT_LIST(DEFINE_HB_DESTROY_THUNK)
#undef DEFINE_HB_DESTROY_THUNK

#define HB_DESTROY_THUNK_ADDRESS(N) hb_destroy_thunk_##N,
static void *const hb_destroy_thunks[] = {
    HB_DESTROY_SLOT_LIST(HB_DESTROY_THUNK_ADDRESS)
};
#undef HB_DESTROY_THUNK_ADDRESS

void *latx_hb_destroy_bridge(void *destroy, const char *soname)
{
    uintptr_t guest = (uintptr_t)destroy;
    void *native;

    if (!destroy) {
        return NULL;
    }
    native = GetNativeFnc(guest);
    if (native) {
        return native;
    }
    g_mutex_lock(&hb_callback_lock);
    for (size_t i = 0; i < ARRAY_SIZE(hb_destroy_slots); i++) {
        if (hb_destroy_slots[i].guest == guest) {
            hb_destroy_slots[i].references++;
            native = hb_destroy_thunks[i];
            g_mutex_unlock(&hb_callback_lock);
            return native;
        }
    }
    for (size_t i = 0; i < ARRAY_SIZE(hb_destroy_slots); i++) {
        if (!hb_destroy_slots[i].guest) {
            hb_destroy_slots[i].guest = guest;
            hb_destroy_slots[i].references = 1;
            native = hb_destroy_thunks[i];
            g_mutex_unlock(&hb_callback_lock);
            return native;
        }
    }
    g_mutex_unlock(&hb_callback_lock);
    kzt_groups_log_wrapper_limitation(
        soname, "HarfBuzz destroy callback slots exhausted");
    return NULL;
}

void latx_hb_callback_cleanup(void)
{
    g_mutex_lock(&hb_callback_lock);
    memset(hb_destroy_slots, 0, sizeof(hb_destroy_slots));
    g_mutex_unlock(&hb_callback_lock);
}

#undef HB_DESTROY_SLOT_LIST
