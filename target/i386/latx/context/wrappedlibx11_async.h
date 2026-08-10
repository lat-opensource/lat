/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 * SPDX-License-Identifier: MIT
 */

#ifndef LATX_WRAPPEDLIBX11_ASYNC_H
#define LATX_WRAPPEDLIBX11_ASYNC_H

#include <stddef.h>
#include <stdint.h>

typedef struct latx_XInternalAsync {
    struct latx_XInternalAsync *next;
    int (*handler)(void *, void *, char *, int, void *);
    void *data;
} latx_XInternalAsync;

static inline void latx_x11_bridge_async_handler_list(
    latx_XInternalAsync *async, uintptr_t guest_addr_limit,
    void *(*bridge_handler)(void *))
{
    latx_XInternalAsync *next;

    for (; async; async = next) {
        next = async->next;
        if ((uintptr_t)async->handler < guest_addr_limit) {
            async->handler = (int (*)(void *, void *, char *, int, void *))
                bridge_handler((void *)async->handler);
        }
    }
}

#endif
