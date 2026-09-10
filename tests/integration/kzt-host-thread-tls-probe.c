#define _GNU_SOURCE
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <dlfcn.h>
#include <pthread.h>
#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlibint.h>

#include "x11-async-bridge-values.h"

#define PERSISTENT_WORKER_COUNT 2
#define PERSISTENT_GENERATIONS 8

typedef struct native_thread_call {
    _XAsyncHandler *handler;
    Display *display;
    xReply *reply;
} native_thread_call;

typedef struct persistent_pool {
    pthread_mutex_t lock;
    pthread_cond_t ready;
    pthread_cond_t done;
    pthread_t threads[PERSISTENT_WORKER_COUNT];
    native_thread_call calls[PERSISTENT_WORKER_COUNT];
    unsigned int generation;
    int completed;
    int started;
    int shutdown;
    int semantic_error;
    int guest_to_host_checked;
} persistent_pool;

static persistent_pool pool = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .ready = PTHREAD_COND_INITIALIZER,
    .done = PTHREAD_COND_INITIALIZER,
};

static locale_t create_utf8_locale(void)
{
    locale_t locale = newlocale(LC_ALL_MASK, "C.UTF-8", NULL);

    if (!locale) {
        locale = newlocale(LC_ALL_MASK, "C.utf8", NULL);
    }
    return locale;
}

static int locale_has_expected_categories(locale_t locale, int utf8)
{
    static const int categories[] = {
        LC_CTYPE, LC_NUMERIC, LC_TIME, LC_COLLATE,
        LC_MONETARY, LC_MESSAGES, LC_PAPER, LC_NAME,
        LC_ADDRESS, LC_TELEPHONE, LC_MEASUREMENT,
        LC_IDENTIFICATION,
    };

    for (size_t index = 0;
         index < sizeof(categories) / sizeof(categories[0]); ++index) {
        const char *name = nl_langinfo_l(
            _NL_LOCALE_NAME(categories[index]), locale);

        if (!name || (utf8
                ? strcmp(name, "C.UTF-8") != 0 &&
                  strcmp(name, "C.utf8") != 0
                : strcmp(name, "C") != 0)) {
            return 0;
        }
    }
    return 1;
}

static int global_locale_is_utf8(void)
{
    locale_t locale = duplocale(LC_GLOBAL_LOCALE);
    const char *name;
    int result;

    if (!locale) {
        return 0;
    }
    name = nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), locale);
    result = name && (strcmp(name, "C.UTF-8") == 0 ||
                      strcmp(name, "C.utf8") == 0);
    freelocale(locale);
    return result;
}

static void run_guest_callback(const native_thread_call *call)
{
    typedef int (*GuestCall)(uintptr_t, const long *, int, const long *, int,
                             const long *, int, long *, long *, long *, long *,
                             unsigned __int128 *);
    struct guest_callback_context {
        Display *expected_display;
        int hits;
        int error;
        uintptr_t callback_entry;
    };
    const struct guest_callback_context *context =
        (const struct guest_callback_context *)call->handler->data;
    GuestCall invoke = (GuestCall)dlsym(RTLD_DEFAULT,
                                       "latx_run_guest_callback_with_libc");
    const long args[] = {
        (long)(uintptr_t)call->display, (long)(uintptr_t)call->reply,
        (long)(uintptr_t)call->reply, ASYNC_PROBE_REPLY_LENGTH,
        (long)(uintptr_t)call->handler->data,
    };

    if (!invoke || !context) {
        __sync_val_compare_and_swap(&pool.semantic_error, 0, 85);
        return;
    }
    for (int iteration = 0; iteration < 2; ++iteration) {
        errno = ENOENT;
        h_errno = NO_RECOVERY;
        (void)dlerror();
        if (invoke(context->callback_entry, args, 5, NULL, 0, NULL, 0,
                   NULL, NULL, NULL, NULL, NULL) != 0) {
            __sync_val_compare_and_swap(&pool.semantic_error, 0, 85);
            return;
        }
        if (errno != EACCES) {
            fprintf(stderr,
                    "FAIL: Host errno after Guest callback=%d expected=%d\n",
                    errno, EACCES);
            __sync_val_compare_and_swap(
                &pool.semantic_error, 0, 68);
        }
        if (h_errno != NO_DATA) {
            __sync_val_compare_and_swap(
                &pool.semantic_error, 0, 79);
        }
        if ((iteration == 0 && MB_CUR_MAX != 1) ||
            (iteration == 1 && MB_CUR_MAX <= 1)) {
            __sync_val_compare_and_swap(
                &pool.semantic_error, 0, 80);
        }
        if (!locale_has_expected_categories(
                uselocale((locale_t)0), iteration != 0)) {
            __sync_val_compare_and_swap(
                &pool.semantic_error, 0, 83);
        }
        if (dlerror() != NULL) {
            __sync_val_compare_and_swap(
                &pool.semantic_error, 0, 84);
        }
    }
}

static void *run_persistent_guest_callback(void *opaque)
{
    size_t index = (size_t)(uintptr_t)opaque;
    unsigned int observed_generation = 0;
    locale_t locale = create_utf8_locale();
    locale_t previous_locale = (locale_t)0;
    int locale_ready = 0;

    if (locale) {
        previous_locale = uselocale(locale);
        locale_ready = previous_locale != (locale_t)0;
    }

    pthread_mutex_lock(&pool.lock);
    for (;;) {
        native_thread_call call;
        while (!pool.shutdown &&
               pool.generation == observed_generation) {
            pthread_cond_wait(&pool.ready, &pool.lock);
        }
        if (pool.shutdown) {
            break;
        }
        call = pool.calls[index];
        observed_generation = pool.generation;
        pthread_mutex_unlock(&pool.lock);

        if (locale_ready) {
            run_guest_callback(&call);
        } else {
            pool.semantic_error = 71;
        }

        pthread_mutex_lock(&pool.lock);
        ++pool.completed;
        if (pool.completed == PERSISTENT_WORKER_COUNT) {
            pthread_cond_signal(&pool.done);
        }
    }
    pthread_mutex_unlock(&pool.lock);
    if (locale_ready) {
        uselocale(previous_locale);
    }
    if (locale) {
        freelocale(locale);
    }
    return NULL;
}

static int run_persistent_generation(Display *display, xReply *reply)
{
    int created = 0;
    pthread_mutex_lock(&pool.lock);
    if (!pool.started) {
        pool.started = 1;
        for (; created < PERSISTENT_WORKER_COUNT; ++created) {
            if (pthread_create(&pool.threads[created], NULL,
                               run_persistent_guest_callback,
                               (void *)(uintptr_t)created) != 0) {
                pool.shutdown = 1;
                pthread_cond_broadcast(&pool.ready);
                pthread_mutex_unlock(&pool.lock);
                while (created-- > 0) {
                    pthread_join(pool.threads[created], NULL);
                }
                return -2;
            }
        }
    }

    for (int index = 0; index < PERSISTENT_WORKER_COUNT; ++index) {
        pool.calls[index] = (native_thread_call) {
            .handler = display->async_handlers,
            .display = display,
            .reply = reply,
        };
    }
    pool.completed = 0;
    ++pool.generation;
    pthread_cond_broadcast(&pool.ready);
    while (pool.completed != PERSISTENT_WORKER_COUNT) {
        pthread_cond_wait(&pool.done, &pool.lock);
    }
    pthread_mutex_unlock(&pool.lock);

    return ASYNC_PROBE_EVENTS_RETURN;
}

static void *run_ephemeral_guest_callback(void *opaque)
{
    locale_t locale = create_utf8_locale();
    locale_t previous_locale;

    if (!locale) {
        pool.semantic_error = 71;
        return NULL;
    }
    previous_locale = uselocale(locale);
    if (!previous_locale) {
        freelocale(locale);
        pool.semantic_error = 71;
        return NULL;
    }
    run_guest_callback(opaque);
    uselocale(previous_locale);
    freelocale(locale);
    return NULL;
}

static int run_ephemeral_generation(Display *display, xReply *reply)
{
    pthread_t threads[PERSISTENT_WORKER_COUNT];
    native_thread_call calls[PERSISTENT_WORKER_COUNT];

    for (int index = 0; index < PERSISTENT_WORKER_COUNT; ++index) {
        calls[index] = (native_thread_call) {
            .handler = display->async_handlers,
            .display = display,
            .reply = reply,
        };
        if (pthread_create(&threads[index], NULL,
                           run_ephemeral_guest_callback,
                           &calls[index]) != 0) {
            return -2;
        }
    }
    for (int index = 0; index < PERSISTENT_WORKER_COUNT; ++index) {
        if (pthread_join(threads[index], NULL) != 0) {
            return -3;
        }
    }
    return ASYNC_PROBE_EVENTS_RETURN;
}

typedef struct explicit_boundary {
    void (*leave)(void);
} explicit_boundary;

static void leave_boundary(explicit_boundary *boundary)
{
    if (boundary->leave) {
        boundary->leave();
    }
}

int XEventsQueued(Display *display, int mode)
{
    xReply reply;
    int first_call;
    int result;
    int (*enter)(void) = dlsym(RTLD_DEFAULT,
                               "kzt_libc_semantic_enter_current");
    explicit_boundary boundary
        __attribute__((cleanup(leave_boundary))) = { 0 };
    void (*leave)(void) = dlsym(RTLD_DEFAULT,
                                "kzt_libc_semantic_leave_current");

    if (!enter || !leave || enter() != 0) {
        fprintf(stderr, "FAIL: explicit libc boundary is unavailable\n");
        return -85;
    }
    boundary.leave = leave;

    (void)mode;
    memset(&reply, 0, sizeof(reply));
    if (!display->async_handlers) {
        return -1;
    }

    first_call = !__sync_lock_test_and_set(
        &pool.guest_to_host_checked, 1);
    if (first_call &&
        (errno != EAGAIN || h_errno != HOST_NOT_FOUND ||
         MB_CUR_MAX <= 1 || !global_locale_is_utf8())) {
        pool.semantic_error = 69;
    }

    fprintf(stderr, "HOST_THREAD_TLS_NATIVE_ENTER\n");
    if (pool.generation < PERSISTENT_GENERATIONS) {
        result = run_persistent_generation(display, &reply);
    } else {
        result = run_ephemeral_generation(display, &reply);
    }
    if (pool.semantic_error) {
        return -pool.semantic_error;
    }
    if (first_call && !setlocale(LC_ALL, "C")) {
        return -81;
    }
    errno = EIO;
    h_errno = TRY_AGAIN;
    return result;
}

int XFlush(Display *display)
{
    (void)display;

    pthread_mutex_lock(&pool.lock);
    if (pool.started && !pool.shutdown) {
        pool.shutdown = 1;
        pthread_cond_broadcast(&pool.ready);
        pthread_mutex_unlock(&pool.lock);
        for (int index = 0; index < PERSISTENT_WORKER_COUNT; ++index) {
            if (pthread_join(pool.threads[index], NULL) != 0) {
                return -3;
            }
        }
    } else {
        pthread_mutex_unlock(&pool.lock);
    }
    return ASYNC_PROBE_FLUSH_RETURN;
}
