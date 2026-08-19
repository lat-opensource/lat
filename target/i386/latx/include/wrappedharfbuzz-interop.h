/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_WRAPPEDHARFBUZZ_INTEROP_H
#define LATX_WRAPPEDHARFBUZZ_INTEROP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct LatxHbCallbackContext {
    uint64_t magic;
    uintptr_t guest_callback;
    void *native_callback;
    void *guest_user_data;
    uintptr_t guest_destroy;
    void *native_destroy;
} LatxHbCallbackContext;

typedef int32_t (*LatxHbSetUserDataFunc)(void *object, void *key,
                                        void *data, void *destroy,
                                        int32_t replace);
typedef void *(*LatxHbGetUserDataFunc)(void *object, void *key);

LatxHbCallbackContext *latx_hb_callback_context_new(
    void *callback, void *user_data, void *destroy);
void latx_hb_callback_context_destroy(void *opaque);
void latx_hb_callback_context_discard(LatxHbCallbackContext *context,
                                      bool invoke_destroy);
void latx_hb_invoke_guest_destroy(void *destroy, void *data);

int32_t latx_hb_set_user_data(LatxHbSetUserDataFunc host_set,
                              void *object, void *key, void *data,
                              void *destroy, int32_t replace);
void *latx_hb_get_user_data(LatxHbGetUserDataFunc host_get,
                            void *object, void *key);

void *latx_hb_destroy_bridge(void *destroy, const char *soname);
void latx_hb_callback_cleanup(void);

#endif /* LATX_WRAPPEDHARFBUZZ_INTEROP_H */
