/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlibint.h>

#include "x11-async-bridge-values.h"

static void dispatch_async_handlers(Display *display, const char *route)
{
    _XAsyncHandler *async;
    _XAsyncHandler *next;
    xReply reply;

    memset(&reply, 0, sizeof(reply));
    fprintf(stderr, "ASYNC_NATIVE_ENTER:%s\n", route);
    for (async = display->async_handlers; async; async = next) {
        next = async->next;
        fprintf(stderr, "ASYNC_NATIVE_BRIDGE_TARGET:%p\n",
                (void *)(uintptr_t)async->handler);
        async->handler(display, &reply, (char *)&reply,
                       ASYNC_PROBE_REPLY_LENGTH, async->data);
    }
}

int XEventsQueued(Display *display, int mode)
{
    (void)mode;
    dispatch_async_handlers(display, "XEventsQueued");
    return ASYNC_PROBE_EVENTS_RETURN;
}

int XFlush(Display *display)
{
    dispatch_async_handlers(display, "XFlush");
    return ASYNC_PROBE_FLUSH_RETURN;
}
