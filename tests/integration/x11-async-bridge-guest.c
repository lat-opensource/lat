/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlibint.h>

#include "x11-async-bridge-values.h"

#define ASYNC_CALLBACK_COUNT 17
#define ASYNC_CONCURRENT_PER_DISPLAY 8

typedef struct callback_context {
    Display *expected_display;
    int id;
    int hits;
    int mismatch;
} callback_context;

#define DEFINE_CALLBACK(index)                                             \
    static Bool guest_callback_##index(Display *display, xReply *reply,    \
                                       char *buffer, int length,           \
                                       XPointer opaque)                    \
    {                                                                      \
        callback_context *context = (callback_context *)opaque;            \
        context->hits++;                                                    \
        if (display != context->expected_display || !reply || !buffer ||   \
            length != ASYNC_PROBE_REPLY_LENGTH || context->id != index) {  \
            context->mismatch = 1;                                         \
        }                                                                  \
        fprintf(stderr, "ASYNC_GUEST_CALLBACK:%d:%d\n", index,           \
                context->hits);                                            \
        return False;                                                      \
    }

DEFINE_CALLBACK(0)
DEFINE_CALLBACK(1)
DEFINE_CALLBACK(2)
DEFINE_CALLBACK(3)
DEFINE_CALLBACK(4)
DEFINE_CALLBACK(5)
DEFINE_CALLBACK(6)
DEFINE_CALLBACK(7)
DEFINE_CALLBACK(8)
DEFINE_CALLBACK(9)
DEFINE_CALLBACK(10)
DEFINE_CALLBACK(11)
DEFINE_CALLBACK(12)
DEFINE_CALLBACK(13)
DEFINE_CALLBACK(14)
DEFINE_CALLBACK(15)
DEFINE_CALLBACK(16)

typedef Bool (*guest_async_callback)(Display *, xReply *, char *, int,
                                     XPointer);

static guest_async_callback callbacks[ASYNC_CALLBACK_COUNT] = {
    guest_callback_0,
    guest_callback_1,
    guest_callback_2,
    guest_callback_3,
    guest_callback_4,
    guest_callback_5,
    guest_callback_6,
    guest_callback_7,
    guest_callback_8,
    guest_callback_9,
    guest_callback_10,
    guest_callback_11,
    guest_callback_12,
    guest_callback_13,
    guest_callback_14,
    guest_callback_15,
    guest_callback_16,
};

static Display *new_fake_display(void)
{
    Display *display = calloc(1, sizeof(*display));

    if (!display) {
        perror("calloc Display");
        exit(2);
    }
    return display;
}

static void init_handlers(Display *display, _XAsyncHandler *handlers,
                          callback_context *contexts, int first, int count)
{
    int index;

    memset(handlers, 0, sizeof(*handlers) * count);
    memset(contexts, 0, sizeof(*contexts) * count);
    for (index = 0; index < count; index++) {
        const int callback = first + index;

        contexts[index].expected_display = display;
        contexts[index].id = callback;
        handlers[index].handler = callbacks[callback];
        handlers[index].data = (XPointer)&contexts[index];
        if (index + 1 < count) {
            handlers[index].next = &handlers[index + 1];
        }
    }
    display->async_handlers = handlers;
}

static int contexts_passed(callback_context *contexts, int count)
{
    int index;

    for (index = 0; index < count; index++) {
        if (contexts[index].hits != 1 || contexts[index].mismatch) {
            return 0;
        }
    }
    return 1;
}

static int run_dispatch(void)
{
    Display *display = new_fake_display();
    _XAsyncHandler handler;
    callback_context context;
    int result;

    init_handlers(display, &handler, &context, 0, 1);
    result = XEventsQueued(display, QueuedAfterReading);
    if (result != ASYNC_PROBE_EVENTS_RETURN ||
        !contexts_passed(&context, 1) || handler.handler == callbacks[0]) {
        return 10;
    }

    init_handlers(display, &handler, &context, 1, 1);
    result = XFlush(display);
    if (result != ASYNC_PROBE_FLUSH_RETURN ||
        !contexts_passed(&context, 1) || handler.handler == callbacks[1]) {
        return 11;
    }

    free(display);
    puts("PASS:x11-async-dispatch");
    return 0;
}

static int run_deterministic(void)
{
    Display *display_a = new_fake_display();
    Display *display_b = new_fake_display();
    _XAsyncHandler handler_a;
    _XAsyncHandler handler_b;
    callback_context context_a;
    callback_context context_b;

    init_handlers(display_a, &handler_a, &context_a, 0, 1);
    init_handlers(display_b, &handler_b, &context_b, 0, 1);
    if (XEventsQueued(display_a, QueuedAfterReading) !=
            ASYNC_PROBE_EVENTS_RETURN ||
        XEventsQueued(display_b, QueuedAfterReading) !=
            ASYNC_PROBE_EVENTS_RETURN ||
        !contexts_passed(&context_a, 1) ||
        !contexts_passed(&context_b, 1) ||
        handler_a.handler != handler_b.handler ||
        handler_a.handler == callbacks[0]) {
        return 20;
    }

    free(display_a);
    free(display_b);
    puts("PASS:x11-async-deterministic-dispatch");
    return 0;
}

typedef struct concurrent_call {
    Display *display;
    int result;
} concurrent_call;

static void *call_events_queued(void *opaque)
{
    concurrent_call *call = opaque;

    call->result = XEventsQueued(call->display, QueuedAfterReading);
    return NULL;
}

static int run_concurrent(void)
{
    Display *display_a = new_fake_display();
    Display *display_b = new_fake_display();
    _XAsyncHandler handlers_a[ASYNC_CONCURRENT_PER_DISPLAY];
    _XAsyncHandler handlers_b[ASYNC_CONCURRENT_PER_DISPLAY];
    callback_context contexts_a[ASYNC_CONCURRENT_PER_DISPLAY];
    callback_context contexts_b[ASYNC_CONCURRENT_PER_DISPLAY];
    concurrent_call call_a = { .display = display_a };
    concurrent_call call_b = { .display = display_b };
    pthread_t thread_a;
    pthread_t thread_b;

    init_handlers(display_a, handlers_a, contexts_a, 0,
                  ASYNC_CONCURRENT_PER_DISPLAY);
    init_handlers(display_b, handlers_b, contexts_b,
                  ASYNC_CONCURRENT_PER_DISPLAY,
                  ASYNC_CONCURRENT_PER_DISPLAY);
    if (pthread_create(&thread_a, NULL, call_events_queued, &call_a) ||
        pthread_create(&thread_b, NULL, call_events_queued, &call_b)) {
        return 30;
    }
    pthread_join(thread_a, NULL);
    pthread_join(thread_b, NULL);

    if (call_a.result != ASYNC_PROBE_EVENTS_RETURN ||
        call_b.result != ASYNC_PROBE_EVENTS_RETURN ||
        !contexts_passed(contexts_a, ASYNC_CONCURRENT_PER_DISPLAY) ||
        !contexts_passed(contexts_b, ASYNC_CONCURRENT_PER_DISPLAY)) {
        return 31;
    }

    free(display_a);
    free(display_b);
    puts("PASS:x11-async-two-display-concurrent");
    return 0;
}

static int run_exhaust(void)
{
    Display *display = new_fake_display();
    _XAsyncHandler handlers[ASYNC_CALLBACK_COUNT];
    callback_context contexts[ASYNC_CALLBACK_COUNT];

    init_handlers(display, handlers, contexts, 0, ASYNC_CALLBACK_COUNT);
    (void)XEventsQueued(display, QueuedAfterReading);
    fputs("FAIL:native Xlib was called after callback slot exhaustion\n",
          stderr);
    return 40;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s MODE\n", argv[0]);
        return 1;
    }
    if (!strcmp(argv[1], "dispatch")) {
        return run_dispatch();
    }
    if (!strcmp(argv[1], "deterministic")) {
        return run_deterministic();
    }
    if (!strcmp(argv[1], "concurrent")) {
        return run_concurrent();
    }
    if (!strcmp(argv[1], "exhaust")) {
        return run_exhaust();
    }
    fprintf(stderr, "unknown mode: %s\n", argv[1]);
    return 1;
}
