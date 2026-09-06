/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <locale.h>
#include <netdb.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlibint.h>

#ifdef HOST_PROBE
typedef int (*GuestCall)(uintptr_t, const long *, int, const long *, int,
                         const long *, int, long *, long *, long *, long *,
                         unsigned __int128 *);
typedef struct Worker {
    Display *display;
    int result;
    int index;
} Worker;

static void *callback_thread(void *opaque)
{
    Worker *worker = opaque;
    _XAsyncHandler *handler = worker->display->async_handlers;
    xReply reply = { 0 };
    locale_t locale = newlocale(LC_ALL_MASK, "C.UTF-8", NULL);
    locale_t previous;
    long result = 0;
    GuestCall invoke = (GuestCall)dlsym(RTLD_DEFAULT,
                                       "latx_run_guest_callback_with_libc");
    const long args[] = {
        (long)(uintptr_t)worker->display, (long)(uintptr_t)&reply,
        0, worker->index * 10, (long)(uintptr_t)handler->data,
    };
    int status;

    if (!invoke || !handler->data) {
        if (locale) {
            freelocale(locale);
        }
        worker->result = 1;
        return NULL;
    }
    if (!locale) {
        locale = newlocale(LC_ALL_MASK, "C.utf8", NULL);
    }
    if (!locale) {
        worker->result = 1;
        return NULL;
    }
    previous = uselocale(locale);
    errno = ENOENT;
    h_errno = NO_RECOVERY;
    status = invoke(*(uintptr_t *)handler->data, args, 5, NULL, 0, NULL, 0,
                    &result, NULL, NULL, NULL, NULL);
    worker->result = status || !result || errno != EACCES ||
                     h_errno != NO_DATA || MB_CUR_MAX != 1;
    uselocale(previous);
    freelocale(locale);
    return NULL;
}

int XEventsQueued(Display *display, int mode)
{
    int (*enter)(void) = dlsym(RTLD_DEFAULT, "kzt_libc_semantic_enter_current");
    void (*leave)(void) = dlsym(RTLD_DEFAULT, "kzt_libc_semantic_leave_current");
    Worker workers[2] = { { display, 0, 0 }, { display, 0, 1 } };
    pthread_t threads[2];
    int result = 0;
    (void)mode;

    if (!enter || !leave || enter() != 0) {
        return 1;
    }
    if (errno != EAGAIN || h_errno != HOST_NOT_FOUND || MB_CUR_MAX <= 1) {
        result = 2;
    } else if (pthread_create(&threads[0], NULL, callback_thread, &workers[0])) {
        result = 3;
    } else {
        if (pthread_create(&threads[1], NULL, callback_thread, &workers[1])) {
            result = 3;
        } else {
            pthread_join(threads[1], NULL);
        }
        pthread_join(threads[0], NULL);
        if (!result && (workers[0].result || workers[1].result)) {
            result = 4;
        }
    }
    if (!result) {
        xReply reply = { 0 };
        _XAsyncHandler *handler = display->async_handlers;

        errno = ENOENT;
        h_errno = NO_RECOVERY;
        if (!handler->handler(display, &reply, NULL, 1, handler->data) ||
            errno != ENOENT || h_errno != NO_RECOVERY) {
            result = 5;
        }
    }
    errno = EIO;
    h_errno = TRY_AGAIN;
    leave();
    return result;
}
#else
static int calls;
static pthread_barrier_t projection_barrier;
static locale_t projected[2];

static Bool callback(Display *display, xReply *reply, char *buffer,
                     int length, XPointer opaque)
{
    int valid = errno == ENOENT && h_errno == NO_RECOVERY && MB_CUR_MAX > 1;
    (void)display;
    (void)reply;
    (void)buffer;
    (void)length;
    (void)opaque;

    if (length == 1) {
        valid = errno == EAGAIN && h_errno == HOST_NOT_FOUND &&
                MB_CUR_MAX > 1;
        errno = ESRCH;
        h_errno = NO_DATA;
        __atomic_add_fetch(&calls, 1, __ATOMIC_RELAXED);
        return valid;
    }
    projected[length / 10] = uselocale((locale_t)0);
    pthread_barrier_wait(&projection_barrier);
    if (projected[0] == projected[1]) {
        fprintf(stderr, "FAIL: attached contexts share a locale projection\n");
        valid = 0;
    }
    /* A current projection is borrowed. Mutate/free only an owned copy. */
    locale_t owned = duplocale(projected[length / 10]);
    if (owned) {
        locale_t changed = newlocale(LC_NUMERIC_MASK, "C", owned);

        if (changed) {
            owned = changed;
        } else {
            valid = 0;
        }
        freelocale(owned);
    } else {
        valid = 0;
    }
    if (MB_CUR_MAX <= 1) {
        valid = 0;
    }
    if (!uselocale(LC_GLOBAL_LOCALE) || MB_CUR_MAX != 1) {
        valid = 0;
    }
    __atomic_add_fetch(&calls, 1, __ATOMIC_RELAXED);
    errno = EACCES;
    h_errno = NO_DATA;
    return valid;
}

int main(void)
{
    Display *display = calloc(1, sizeof(*display));
    _XAsyncHandler handler = { .handler = callback };
    uintptr_t callback_entry = (uintptr_t)callback;
    locale_t locale = newlocale(LC_ALL_MASK, "C.UTF-8", NULL);
    locale_t previous;
    int result;

    if (!locale) {
        locale = newlocale(LC_ALL_MASK, "C.utf8", NULL);
    }
    if (!display || !locale) {
        return 2;
    }
    previous = uselocale(locale);
    if (pthread_barrier_init(&projection_barrier, NULL, 2)) {
        return 4;
    }
    handler.data = (XPointer)&callback_entry;
    display->async_handlers = &handler;
    errno = EAGAIN;
    h_errno = HOST_NOT_FOUND;
    result = XEventsQueued(display, 0);
    if (result || calls != 3 || errno != EIO || h_errno != TRY_AGAIN ||
        MB_CUR_MAX <= 1) {
        fprintf(stderr, "FAIL: boundary result=%d calls=%d errno=%d h_errno=%d\n",
                result, calls, errno, h_errno);
        return 3;
    }
    uselocale(previous);
    freelocale(locale);
    pthread_barrier_destroy(&projection_barrier);
    free(display);
    puts("PASS: explicit bidirectional errno, h_errno and locale boundaries");
    return 0;
}
#endif
