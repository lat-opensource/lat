/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdio.h>

#include <X11/Xlib.h>

int XEventsQueued(Display *display, int mode)
{
    (void)display;
    (void)mode;
    fputs("ASYNC_GUEST_DUMMY_CALLED:XEventsQueued\n", stderr);
    return -1;
}

int XFlush(Display *display)
{
    (void)display;
    fputs("ASYNC_GUEST_DUMMY_CALLED:XFlush\n", stderr);
    return -1;
}

int (*XSetAfterFunction(Display *display,
                        int (*callback)(Display *)))(Display *)
{
    (void)display;
    (void)callback;
    fputs("ASYNC_GUEST_DUMMY_CALLED:XSetAfterFunction\n", stderr);
    return NULL;
}

int XNoOp(Display *display)
{
    (void)display;
    fputs("ASYNC_GUEST_DUMMY_CALLED:XNoOp\n", stderr);
    return -1;
}
