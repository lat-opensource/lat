/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <ctype.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlibint.h>

#ifdef HOST_PROBE
typedef struct Worker {
    Display *display;
    int index;
    int result;
    int synchronize;
} Worker;

static __thread int host_tls = 51;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int start;
static int finished;
static Worker parked;
static pthread_t parked_thread;
static int park_ready;
static int park_release;

static void *invoke(void *opaque)
{
    Worker *worker = opaque;
    _XAsyncHandler *handler = worker->display->async_handlers;
    xReply reply = { 0 };

    if (worker->synchronize) {
        pthread_mutex_lock(&lock);
        while (!start) {
            pthread_cond_wait(&cond, &lock);
        }
        pthread_mutex_unlock(&lock);
        if (start < 0) {
            return NULL;
        }
    }
    for (int iteration = 0; iteration < 3; ++iteration) {
        host_tls = 51 + iteration;
        if (!handler->handler(worker->display, &reply, NULL,
                              worker->index * 10 + iteration,
                              handler->data) ||
            host_tls != 51 + iteration) {
            worker->result = -1;
            break;
        }
    }
    if (worker->synchronize) {
        pthread_mutex_lock(&lock);
        ++finished;
        pthread_cond_broadcast(&cond);
        while (finished != 2) {
            pthread_cond_wait(&cond, &lock);
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

static void *park_worker(void *opaque)
{
    invoke(opaque);
    pthread_mutex_lock(&lock);
    park_ready = 1;
    pthread_cond_broadcast(&cond);
    while (!park_release) {
        pthread_cond_wait(&cond, &lock);
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

int XFlush(Display *display)
{
    (void)display;
    pthread_mutex_lock(&lock);
    park_release = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
    return pthread_join(parked_thread, NULL);
}

int XEventsQueued(Display *display, int mode)
{
    Worker workers[2] = { { .display = display },
                          { .display = display, .index = 1 } };
    pthread_t threads[2];

    if (mode == 2) {
        parked.display = display;
        if (pthread_create(&parked_thread, NULL, park_worker, &parked)) {
            return -2;
        }
        pthread_mutex_lock(&lock);
        while (!park_ready) {
            pthread_cond_wait(&cond, &lock);
        }
        pthread_mutex_unlock(&lock);
        return parked.result;
    }
    if (!mode) {
        invoke(&workers[0]);
        return workers[0].result;
    }
    start = 0;
    finished = 0;
    workers[0].synchronize = 1;
    workers[1].synchronize = 1;
    if (pthread_create(&threads[0], NULL, invoke, &workers[0])) {
        return -2;
    }
    if (pthread_create(&threads[1], NULL, invoke, &workers[1])) {
        pthread_mutex_lock(&lock);
        start = -1;
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&lock);
        pthread_join(threads[0], NULL);
        return -2;
    }
    pthread_mutex_lock(&lock);
    start = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    return workers[0].result || workers[1].result ? -3 : 0;
}
#else
static __thread int guest_tls = 7;
static __thread int guest_child_mode;
static uintptr_t addresses[2];
static int hits;
static int destructor_count;
static pthread_key_t key;
static int attached;
static int (*late_value)(void);

static void *guest_child(void *opaque)
{
    guest_child_mode = 1;
    if (XEventsQueued(opaque, 0) || guest_tls != 10) {
        return (void *)(uintptr_t)1;
    }
    return NULL;
}

static void destroy_value(void *value)
{
    if (value != &guest_tls || guest_tls != 10) {
        abort();
    }
    __atomic_add_fetch(&destructor_count, 1, __ATOMIC_RELAXED);
}

static Bool callback(Display *display, xReply *reply, char *buffer,
                     int length, XPointer opaque)
{
    int index = length / 10;
    int iteration = length % 10;
    (void)display;
    (void)reply;
    (void)buffer;
    (void)opaque;

    if (index < 0 || index > 1 || iteration > 2 ||
        guest_tls != 7 + iteration || tolower('A') != 'a') {
        fprintf(stderr, "FAIL: callback index=%d iteration=%d tls=%d lower=%d\n",
                index, iteration, guest_tls, tolower('A'));
        return False;
    }
    if (guest_child_mode) {
        ++guest_tls;
        return True;
    }
    if (late_value && late_value() != 19 + iteration) {
        return False;
    }
    if (attached && index == 0 && iteration == 0) {
        pthread_t child;
        void *child_result = NULL;

        if (pthread_create(&child, NULL, guest_child, display) ||
            pthread_join(child, &child_result) || child_result) {
            return False;
        }
    }
    if (attached && pthread_setspecific(key, &guest_tls)) {
        fprintf(stderr, "FAIL: setspecific key=%u\n", (unsigned)key);
        return False;
    }
    addresses[index] = (uintptr_t)&guest_tls;
    ++guest_tls;
    __atomic_add_fetch(&hits, 1, __ATOMIC_RELAXED);
    return True;
}

int main(int argc, char **argv)
{
    Display *display = calloc(1, sizeof(*display));
    _XAsyncHandler handler = { .handler = callback };
    uintptr_t main_address = (uintptr_t)&guest_tls;
    void *late_handle = NULL;
    int phases;

    if (!display || argc < 2 || argc > 3) {
        return 2;
    }
    attached = !strcmp(argv[1], "attached");
    display->async_handlers = &handler;
    if (attached) {
        guest_tls = 77;
        if (pthread_key_create(&key, destroy_value)) {
            return 3;
        }
    }
    phases = attached && argc == 3 ? 3 : 1;
    if (phases > 1 && XEventsQueued(display, 2)) {
        return 9;
    }
    for (int phase = 0; phase < phases; ++phase) {
        int result;

        if (phase == 1) {
            late_handle = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
            late_value = late_handle
                ? (int (*)(void))dlsym(late_handle, "kzt_opt_in_tls_value")
                : NULL;
            if (!late_value) {
                return 7;
            }
        } else if (phase == 2) {
            late_value = NULL;
            if (dlclose(late_handle)) {
                return 8;
            }
        }
        hits = 0;
        destructor_count = 0;
        result = XEventsQueued(display, attached);
        if (result || hits != (attached ? 6 : 3)) {
            fprintf(stderr, "FAIL: phase=%d native result=%d hits=%d\n",
                    phase, result, hits);
            return 4;
        }
        if (attached) {
            if (guest_tls != 77 || addresses[0] == addresses[1] ||
                addresses[0] == main_address || addresses[1] == main_address ||
                destructor_count != 2) {
                return 5;
            }
        } else if (guest_tls != 10 || addresses[0] != main_address) {
            return 6;
        }
    }
    if (attached) {
        if (phases > 1 && XFlush(display)) {
            return 10;
        }
        pthread_key_delete(key);
    }
    free(display);
    printf("PASS: %s Guest TLS callback\n", attached ? "attached" : "existing");
    return 0;
}
#endif
