/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <X11/Xlibint.h>

#include "x11-async-bridge-values.h"

typedef struct persistent_worker {
    pthread_mutex_t lock;
    pthread_cond_t ready;
    pthread_cond_t done;
    pthread_t thread;
    _XAsyncHandler *handler;
    Display *display;
    unsigned int generation;
    unsigned int completed_generation;
    int started;
    int shutdown;
} persistent_worker;

static persistent_worker worker = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .ready = PTHREAD_COND_INITIALIZER,
    .done = PTHREAD_COND_INITIALIZER,
};
static persistent_worker concurrent_workers[2];
static int concurrent_started;
static pthread_mutex_t fmt_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fmt_cond = PTHREAD_COND_INITIALIZER;
static int (*fmt_callback)(Display *display);
static Display *fmt_display;
static int fmt_entered;
static int fmt_release;
static int fmt_loader_completed;
static int fmt_callback_result;
static pthread_t semantic_stress_thread;
static int semantic_stress_running;
static int semantic_stress_started;

static void *run_semantic_stress(void *opaque)
{
    Display *display = opaque;

    while (__atomic_load_n(&semantic_stress_running,
                           __ATOMIC_ACQUIRE)) {
        xReply reply;

        memset(&reply, 0, sizeof(reply));
        display->async_handlers->handler(
            display, &reply, (char *)&reply,
            ASYNC_PROBE_REPLY_LENGTH,
            display->async_handlers->data);
    }
    return NULL;
}

static int start_semantic_stress(Display *display)
{
    if (!display->async_handlers || semantic_stress_started) {
        return -1;
    }
    semantic_stress_running = 1;
    if (pthread_create(&semantic_stress_thread, NULL,
                       run_semantic_stress, display) != 0) {
        semantic_stress_running = 0;
        return -1;
    }
    semantic_stress_started = 1;
    return ASYNC_PROBE_EVENTS_RETURN;
}

static int stop_semantic_stress(void)
{
    if (!semantic_stress_started) {
        return -1;
    }
    __atomic_store_n(&semantic_stress_running, 0,
                     __ATOMIC_RELEASE);
    if (pthread_join(semantic_stress_thread, NULL) != 0) {
        return -1;
    }
    semantic_stress_started = 0;
    return ASYNC_PROBE_EVENTS_RETURN;
}

static void *run_fmt_callback(void *opaque)
{
    (void)opaque;
    fmt_callback_result = fmt_callback(fmt_display);
    return NULL;
}

static void *run_fmt_loader_callback(void *opaque)
{
    _XAsyncHandler *handler = opaque;
    xReply reply;

    memset(&reply, 0, sizeof(reply));
    handler->handler(fmt_display, &reply, (char *)&reply,
                     ASYNC_PROBE_REPLY_LENGTH, handler->data);
    pthread_mutex_lock(&fmt_lock);
    fmt_loader_completed = 1;
    pthread_cond_broadcast(&fmt_cond);
    pthread_mutex_unlock(&fmt_lock);
    return NULL;
}

static int run_fmt_propagation_race(Display *display)
{
    const struct timespec wait_duration = {
        .tv_sec = 0,
        .tv_nsec = 100 * 1000 * 1000,
    };
    struct timespec deadline;
    pthread_t callback_thread;
    pthread_t loader_thread;
    int completed_before_release;
    int wait_result;

    if (!fmt_callback || !display->async_handlers) {
        return -1;
    }
    pthread_mutex_lock(&fmt_lock);
    fmt_display = display;
    fmt_entered = 0;
    fmt_release = 0;
    fmt_loader_completed = 0;
    fmt_callback_result = -1;
    if (pthread_create(&callback_thread, NULL,
                       run_fmt_callback, NULL) != 0) {
        pthread_mutex_unlock(&fmt_lock);
        return -1;
    }
    while (!fmt_entered) {
        pthread_cond_wait(&fmt_cond, &fmt_lock);
    }
    if (pthread_create(&loader_thread, NULL,
                       run_fmt_loader_callback,
                       display->async_handlers) != 0) {
        fmt_release = 1;
        pthread_cond_broadcast(&fmt_cond);
        pthread_mutex_unlock(&fmt_lock);
        pthread_join(callback_thread, NULL);
        return -1;
    }
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += wait_duration.tv_sec;
    deadline.tv_nsec += wait_duration.tv_nsec;
    if (deadline.tv_nsec >= 1000 * 1000 * 1000) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000 * 1000 * 1000;
    }
    wait_result = 0;
    while (!fmt_loader_completed && wait_result == 0) {
        wait_result = pthread_cond_timedwait(
            &fmt_cond, &fmt_lock, &deadline);
    }
    completed_before_release = fmt_loader_completed;
    fmt_release = 1;
    pthread_cond_broadcast(&fmt_cond);
    pthread_mutex_unlock(&fmt_lock);

    if (pthread_join(loader_thread, NULL) != 0 ||
        pthread_join(callback_thread, NULL) != 0 ||
        fmt_callback_result != 0) {
        return -1;
    }
    return completed_before_release ? -2
                                    : ASYNC_PROBE_EVENTS_RETURN;
}

int (*XSetAfterFunction(Display *display,
                        int (*callback)(Display *)))(Display *)
{
    int (*previous)(Display *) = fmt_callback;

    fmt_display = display;
    fmt_callback = callback;
    return previous;
}

int XNoOp(Display *display)
{
    (void)display;
    pthread_mutex_lock(&fmt_lock);
    fmt_entered = 1;
    pthread_cond_broadcast(&fmt_cond);
    while (!fmt_release) {
        pthread_cond_wait(&fmt_cond, &fmt_lock);
    }
    pthread_mutex_unlock(&fmt_lock);
    return 0;
}

static void *run_worker(void *opaque)
{
    persistent_worker *state = opaque;

    pthread_mutex_lock(&state->lock);
    while (!state->shutdown) {
        xReply reply;
        unsigned int generation;

        while (!state->shutdown &&
               state->completed_generation == state->generation) {
            pthread_cond_wait(&state->ready, &state->lock);
        }
        if (state->shutdown) {
            break;
        }
        generation = state->generation;
        memset(&reply, 0, sizeof(reply));
        pthread_mutex_unlock(&state->lock);
        state->handler->handler(
            state->display, &reply, (char *)&reply,
            ASYNC_PROBE_REPLY_LENGTH, state->handler->data);
        pthread_mutex_lock(&state->lock);
        state->completed_generation = generation;
        pthread_cond_signal(&state->done);
    }
    pthread_mutex_unlock(&state->lock);
    return NULL;
}

int XEventsQueued(Display *display, int mode)
{
    if (mode == 101) {
        return start_semantic_stress(display);
    }
    if (mode == 102) {
        return stop_semantic_stress();
    }
    if (mode == 100) {
        return run_fmt_propagation_race(display);
    }
    if (mode == 99) {
        if (!concurrent_started) {
            concurrent_started = 1;
            for (size_t index = 0; index < 2; ++index) {
                persistent_worker *state = &concurrent_workers[index];

                pthread_mutex_init(&state->lock, NULL);
                pthread_cond_init(&state->ready, NULL);
                pthread_cond_init(&state->done, NULL);
                state->started = 1;
                state->handler = display->async_handlers;
                state->display = display;
                if (pthread_create(
                        &state->thread, NULL, run_worker, state) != 0) {
                    return -1;
                }
            }
        }
        for (size_t index = 0; index < 2; ++index) {
            persistent_worker *state = &concurrent_workers[index];

            pthread_mutex_lock(&state->lock);
            ++state->generation;
            pthread_cond_signal(&state->ready);
            pthread_mutex_unlock(&state->lock);
        }
        for (size_t index = 0; index < 2; ++index) {
            persistent_worker *state = &concurrent_workers[index];

            pthread_mutex_lock(&state->lock);
            while (state->completed_generation != state->generation) {
                pthread_cond_wait(&state->done, &state->lock);
            }
            pthread_mutex_unlock(&state->lock);
        }
        return ASYNC_PROBE_EVENTS_RETURN;
    }
    pthread_mutex_lock(&worker.lock);
    if (!worker.started) {
        worker.started = 1;
        worker.handler = display->async_handlers;
        worker.display = display;
        if (pthread_create(&worker.thread, NULL, run_worker,
                           &worker) != 0) {
            pthread_mutex_unlock(&worker.lock);
            return -1;
        }
    }
    ++worker.generation;
    pthread_cond_signal(&worker.ready);
    while (worker.completed_generation != worker.generation) {
        pthread_cond_wait(&worker.done, &worker.lock);
    }
    pthread_mutex_unlock(&worker.lock);
    return ASYNC_PROBE_EVENTS_RETURN;
}

int XFlush(Display *display)
{
    (void)display;
    pthread_mutex_lock(&worker.lock);
    worker.shutdown = 1;
    pthread_cond_signal(&worker.ready);
    pthread_mutex_unlock(&worker.lock);
    if (pthread_join(worker.thread, NULL) != 0) {
        return -1;
    }
    if (concurrent_started) {
        for (size_t index = 0; index < 2; ++index) {
            persistent_worker *state = &concurrent_workers[index];

            pthread_mutex_lock(&state->lock);
            state->shutdown = 1;
            pthread_cond_signal(&state->ready);
            pthread_mutex_unlock(&state->lock);
        }
        for (size_t index = 0; index < 2; ++index) {
            if (pthread_join(
                    concurrent_workers[index].thread, NULL) != 0) {
                return -1;
            }
        }
    }
    return ASYNC_PROBE_FLUSH_RETURN;
}
