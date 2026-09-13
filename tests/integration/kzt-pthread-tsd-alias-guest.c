#define _GNU_SOURCE
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "kzt-pthread-tsd-alias.h"
#include <string.h>

#define WORKER_COUNT 16
#define DESTRUCTOR_PASSES 3

typedef struct worker_state {
    pthread_key_t key;
    uintptr_t expected;
    int remaining;
} worker_state;

static pthread_barrier_t start_barrier;
static pthread_key_t worker_keys[WORKER_COUNT];
static int worker_errors[WORKER_COUNT];
static int destructor_calls;
static int destructor_completions;

static int check_guest_libc_symbol(const char *name, void *address)
{
    Dl_info info = { 0 };

    if (!dladdr(address, &info) || !info.dli_fname ||
        (!strstr(info.dli_fname, "libc.so") &&
         !strstr(info.dli_fname, "libpthread.so"))) {
        fprintf(stderr,
                "FAIL: %s address=%p owner=%s\n",
                name, address,
                info.dli_fname ? info.dli_fname : "<unresolved>");
        return -1;
    }
    return 0;
}

static void iterating_destructor(void *opaque)
{
    worker_state *state = opaque;

    __sync_fetch_and_add(&destructor_calls, 1);
    if (--state->remaining > 0) {
        if (pthread_setspecific(state->key, state) != 0) {
            __sync_fetch_and_add(&worker_errors[0], 1);
        }
        return;
    }
    __sync_fetch_and_add(&destructor_completions, 1);
    free(state);
}

static void *run_worker(void *opaque)
{
    size_t index = (size_t)(uintptr_t)opaque;
    worker_state *state = calloc(1, sizeof(*state));
    int result;

    if (!state) {
        worker_errors[index] = 1;
        return NULL;
    }
    state->expected = UINT64_C(0x10000) + index;
    state->remaining = DESTRUCTOR_PASSES;
    pthread_barrier_wait(&start_barrier);

    if (index & 1) {
        result = pthread_key_create(&state->key, iterating_destructor);
    } else {
        result = direct_pthread_key_create(
            &state->key, iterating_destructor);
    }
    if (result != 0) {
        worker_errors[index] = 2;
        free(state);
        return NULL;
    }
    worker_keys[index] = state->key;

    if (index & 1) {
        result = direct_pthread_setspecific(state->key, state);
    } else {
        result = pthread_setspecific(state->key, state);
    }
    if (result != 0 ||
        pthread_getspecific(state->key) != state ||
        direct_pthread_getspecific(state->key) != state) {
        worker_errors[index] = 3;
        return NULL;
    }

    if ((index & 1)
            ? pthread_setspecific(state->key, NULL) != 0
            : direct_pthread_setspecific(state->key, NULL) != 0) {
        worker_errors[index] = 4;
        return NULL;
    }
    if (pthread_getspecific(state->key) != NULL ||
        direct_pthread_getspecific(state->key) != NULL) {
        worker_errors[index] = 5;
        return NULL;
    }
    if ((index & 1)
            ? direct_pthread_setspecific(state->key, state) != 0
            : pthread_setspecific(state->key, state) != 0) {
        worker_errors[index] = 6;
    }
    return NULL;
}

int main(int argc, char **argv)
{
    pthread_t workers[WORKER_COUNT];
    pthread_key_t first_key;
    pthread_key_t reused_key;
    uintptr_t main_value = UINT64_C(0x12345678);
    int run_workers;

    if (check_guest_libc_symbol(
            "pthread_getspecific", (void *)pthread_getspecific) != 0 ||
        check_guest_libc_symbol(
            "pthread_setspecific", (void *)pthread_setspecific) != 0) {
        return 11;
    }

    if (argc == 1) {
        run_workers = 1;
    } else if (argc == 2 &&
               strcmp(argv[1], "--single-thread") == 0) {
        run_workers = 0;
    } else {
        return 2;
    }
    if (run_workers) {
        if (pthread_barrier_init(&start_barrier, NULL,
                                 WORKER_COUNT + 1) != 0) {
            return 2;
        }
        for (size_t index = 0; index < WORKER_COUNT; ++index) {
            int create_result = pthread_create(
                &workers[index], NULL, run_worker,
                (void *)(uintptr_t)index);

            if (create_result != 0) {
                fprintf(stderr,
                        "FAIL: Guest pthread_create index=%zu error=%d\n",
                        index, create_result);
                return 3;
            }
        }
        pthread_barrier_wait(&start_barrier);
        for (size_t index = 0; index < WORKER_COUNT; ++index) {
            if (pthread_join(workers[index], NULL) != 0) {
                return 4;
            }
        }
        pthread_barrier_destroy(&start_barrier);

        for (size_t index = 0; index < WORKER_COUNT; ++index) {
            if (worker_errors[index]) {
                fprintf(stderr, "FAIL: Guest TSD worker %zu error=%d\n",
                        index, worker_errors[index]);
                return 5;
            }
            for (size_t other = 0; other < index; ++other) {
                if (worker_keys[index] == worker_keys[other]) {
                    fprintf(stderr,
                            "FAIL: concurrent Guest TSD key collision\n");
                    return 6;
                }
            }
        }
        if (destructor_calls != WORKER_COUNT * DESTRUCTOR_PASSES ||
            destructor_completions != WORKER_COUNT) {
            fprintf(stderr,
                    "FAIL: Guest TSD destructors calls=%d "
                    "completions=%d\n",
                    destructor_calls, destructor_completions);
            return 7;
        }
        for (size_t index = 0; index < WORKER_COUNT; ++index) {
            if (pthread_key_delete(worker_keys[index]) != 0) {
                return 8;
            }
        }
    }

    if (pthread_key_create(&first_key, NULL) != 0 ||
        direct_pthread_setspecific(first_key, &main_value) != 0 ||
        pthread_getspecific(first_key) != &main_value ||
        direct_pthread_getspecific(first_key) != &main_value ||
        pthread_key_delete(first_key) != 0 ||
        direct_pthread_key_create(&reused_key, NULL) != 0) {
        fprintf(stderr, "FAIL: Guest TSD public/private alias matrix\n");
        return 9;
    }
    if (reused_key != first_key ||
        pthread_getspecific(reused_key) != NULL ||
        direct_pthread_getspecific(reused_key) != NULL ||
        pthread_setspecific(reused_key, &main_value) != 0 ||
        direct_pthread_getspecific(reused_key) != &main_value ||
        pthread_key_delete(reused_key) != 0) {
        fprintf(stderr, "FAIL: Guest TSD key reuse retained stale state\n");
        return 10;
    }

    fputs("PASS: Guest pthread TSD remains Guest-libc authoritative\n",
          stderr);
    return 0;
}
