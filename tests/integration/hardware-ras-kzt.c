/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlibint.h>
#include "x11-async-bridge-values.h"

static unsigned callbacks;

static __attribute__((noinline)) unsigned nested(unsigned depth, unsigned n)
{
    if (!depth) {
        return n;
    }
    unsigned result = nested(depth - 1, n + 1);
    __asm__ volatile("" : "+r"(result));
    return result + 1;
}

static Bool callback(Display *display, xReply *reply, char *buffer,
                     int length, XPointer opaque)
{
    if (!display || !reply || buffer != (char *)reply ||
        length != ASYNC_PROBE_REPLY_LENGTH || opaque != (XPointer)&callbacks ||
        nested(20, callbacks) != callbacks + 40) {
        exit(2);
    }
    ++callbacks;
    return False;
}

int run(void)
{
    Display *display = calloc(1, sizeof(*display));
    _XAsyncHandler handler = {
        .handler = callback, .data = (void *)&callbacks,
    };

    if (!display) {
        return 1;
    }
    display->async_handlers = &handler;
    for (unsigned i = 0; i < 100; ++i) {
        int result = XEventsQueued(display, QueuedAlready);
        if (result != ASYNC_PROBE_EVENTS_RETURN) {
            return 3;
        }
    }
    if (callbacks != 100) {
        return 4;
    }
    fprintf(stderr, "PASS: native Xlib / guest callback / nested CALL+RET\n");
    return 0;
}

/* The dynamic loader initializes libc; avoid a newer startup ABI here. */
__asm__(".globl _start\n"
        "_start: xor %ebp,%ebp; andq $-16,%rsp; call run;"
        "mov %eax,%edi; mov $60,%eax; syscall\n");
