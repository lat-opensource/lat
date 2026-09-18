#define _GNU_SOURCE
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <dlfcn.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlibint.h>

#include "x11-async-bridge-values.h"

typedef int (*plugin_check_fn)(void);
typedef void (*plugin_set_counters_fn)(int *, int *);

static plugin_check_fn plugin_check;
typedef struct callback_control {
    int ready;
    int release;
    int error;
} callback_control;

static callback_control control;
static int constructor_count;
static int destructor_count;

static Bool lifetime_callback(Display *display, xReply *reply,
                              char *buffer, int length,
                              XPointer opaque)
{
    callback_control *state = (callback_control *)opaque;

    (void)display;
    if (!reply || !buffer || length != ASYNC_PROBE_REPLY_LENGTH ||
        !plugin_check || plugin_check() != 0) {
        state->error = 1;
    }
    __atomic_store_n(&state->ready, 1, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&state->release, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }
    return False;
}

int main(int argc, char **argv)
{
    Display *display;
    _XAsyncHandler handler = { 0 };
    plugin_set_counters_fn set_counters;
    void *handle;
    void *still_loaded;

    if (argc != 2) {
        return 2;
    }
    display = calloc(1, sizeof(*display));
    if (!display) {
        return 3;
    }
    handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "FAIL: cannot load C++ TLS plugin: %s\n",
                dlerror());
        return 4;
    }
    plugin_check = (plugin_check_fn)dlsym(
        handle, "kzt_cxx_tls_lifetime_check");
    set_counters = (plugin_set_counters_fn)dlsym(
        handle, "kzt_cxx_tls_lifetime_set_counters");
    if (!plugin_check || !set_counters) {
        return 5;
    }
    set_counters(&constructor_count, &destructor_count);

    handler.handler = lifetime_callback;
    handler.data = (XPointer)&control;
    display->async_handlers = &handler;
    if (XEventsQueued(display, QueuedAfterReading) !=
        ASYNC_PROBE_EVENTS_RETURN) {
        return 6;
    }
    while (!__atomic_load_n(&control.ready, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }
    if (control.error || constructor_count != 1 ||
        destructor_count != 0) {
        fprintf(stderr,
                "FAIL: C++ TLS callback setup error=%d ctor=%d dtor=%d\n",
                control.error, constructor_count, destructor_count);
        return 7;
    }
    if (dlclose(handle) != 0) {
        return 8;
    }
    handle = NULL;
    still_loaded = dlopen(argv[1], RTLD_LAZY | RTLD_NOLOAD);
    if (!still_loaded) {
        fprintf(stderr,
                "FAIL: C++ TLS owner unloaded before destructor\n");
        return 9;
    }
    dlclose(still_loaded);
    if (XFlush(display) != ASYNC_PROBE_FLUSH_RETURN) {
        return 10;
    }
    if (control.error || constructor_count != 1 ||
        destructor_count != 1) {
        fprintf(stderr,
                "FAIL: C++ TLS teardown error=%d ctor=%d dtor=%d\n",
                control.error, constructor_count, destructor_count);
        return 11;
    }
    still_loaded = dlopen(
        argv[1], RTLD_LAZY | RTLD_NOLOAD);
    if (still_loaded) {
        dlclose(still_loaded);
        fprintf(stderr,
                "FAIL: C++ TLS plugin remained loaded after destructor\n");
        return 12;
    }
    free(display);
    fputs("PASS: C++ thread_local DSO survived until Host-thread exit\n",
          stderr);
    return 0;
}
