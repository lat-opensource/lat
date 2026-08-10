/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 * SPDX-License-Identifier: MIT
 */

#ifndef LATX_WRAPPEDLIBX11_ASYNC_H
#define LATX_WRAPPEDLIBX11_ASYNC_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct latx_XInternalAsync {
    struct latx_XInternalAsync *next;
    int (*handler)(void *, void *, char *, int, void *);
    void *data;
} latx_XInternalAsync;

typedef struct latx_x11_async_bridge_state {
    pthread_mutex_t mutex;
} latx_x11_async_bridge_state;

#define LATX_X11_ASYNC_BRIDGE_STATE_INITIALIZER \
    { PTHREAD_MUTEX_INITIALIZER }

typedef enum latx_x11_async_bridge_result {
    LATX_X11_ASYNC_BRIDGE_OK = 0,
    LATX_X11_ASYNC_BRIDGE_NO_MEMORY,
    LATX_X11_ASYNC_BRIDGE_HANDLER_UNAVAILABLE,
} latx_x11_async_bridge_result;

typedef struct latx_x11_async_handler_update {
    latx_XInternalAsync *async;
    void *handler;
} latx_x11_async_handler_update;

static inline latx_x11_async_bridge_result
latx_x11_bridge_async_handler_list(
    latx_x11_async_bridge_state *state, latx_XInternalAsync *async,
    uintptr_t guest_addr_limit,
    void *(*bridge_handler)(void *))
{
    latx_x11_async_handler_update *updates = NULL;
    latx_XInternalAsync *cursor;
    size_t count = 0;
    size_t index = 0;

    pthread_mutex_lock(&state->mutex);
    for (cursor = async; cursor; cursor = cursor->next) {
        if ((uintptr_t)cursor->handler < guest_addr_limit) {
            count++;
        }
    }

    if (count) {
        updates = calloc(count, sizeof(*updates));
        if (!updates) {
            pthread_mutex_unlock(&state->mutex);
            return LATX_X11_ASYNC_BRIDGE_NO_MEMORY;
        }
    }

    for (cursor = async; cursor; cursor = cursor->next) {
        void *bridged;

        if ((uintptr_t)cursor->handler >= guest_addr_limit) {
            continue;
        }
        bridged = bridge_handler((void *)cursor->handler);
        if (!bridged) {
            free(updates);
            pthread_mutex_unlock(&state->mutex);
            return LATX_X11_ASYNC_BRIDGE_HANDLER_UNAVAILABLE;
        }
        updates[index].async = cursor;
        updates[index].handler = bridged;
        index++;
    }

    for (index = 0; index < count; index++) {
        updates[index].async->handler =
            (int (*)(void *, void *, char *, int, void *))updates[index].handler;
    }
    free(updates);
    pthread_mutex_unlock(&state->mutex);
    return LATX_X11_ASYNC_BRIDGE_OK;
}

#endif
