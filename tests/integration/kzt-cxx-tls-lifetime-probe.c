/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <pthread.h>
#include <stdint.h>
#include <string.h>

#include <X11/Xlibint.h>

#include "x11-async-bridge-values.h"

typedef struct lifetime_call {
    _XAsyncHandler *handler;
    Display *display;
    xReply reply;
} lifetime_call;

typedef struct callback_control {
    int ready;
    int release;
    int error;
} callback_control;

static lifetime_call call;
static pthread_t worker;
static int worker_started;

static void *run_lifetime_callback(void *opaque)
{
    lifetime_call *current = opaque;

    current->handler->handler(
        current->display, &current->reply,
        (char *)&current->reply, ASYNC_PROBE_REPLY_LENGTH,
        current->handler->data);
    return NULL;
}

int XEventsQueued(Display *display, int mode)
{
    (void)mode;
    if (!display || !display->async_handlers || worker_started) {
        return -1;
    }
    memset(&call, 0, sizeof(call));
    call.handler = display->async_handlers;
    call.display = display;
    if (pthread_create(&worker, NULL, run_lifetime_callback,
                       &call) != 0) {
        return -1;
    }
    worker_started = 1;
    return ASYNC_PROBE_EVENTS_RETURN;
}

int XFlush(Display *display)
{
    callback_control *control;
    void *thread_result = NULL;

    (void)display;
    if (!worker_started || !call.handler || !call.handler->data) {
        return -1;
    }
    control = (callback_control *)call.handler->data;
    if (pthread_cancel(worker) != 0) {
        return -1;
    }
    __atomic_store_n(&control->release, 1, __ATOMIC_RELEASE);
    if (pthread_join(worker, &thread_result) != 0 ||
        thread_result != PTHREAD_CANCELED) {
        return -1;
    }
    worker_started = 0;
    return ASYNC_PROBE_FLUSH_RETURN;
}
