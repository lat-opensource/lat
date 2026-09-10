/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "cleanup.h"
#include "latx/liblat.h"
#include "elfloader_private.h"
#include "box64context.h"
#include "callback.h"

typedef struct cleanup_s {
    void *function;
    void *argument;
    bool has_argument;
    uint64_t sequence;
} cleanup_t;

static pthread_mutex_t cleanup_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t cleanup_sequence;

static void add_cleanup(cleanup_t **entries, int *count, int *capacity,
                        void *function, void *argument, bool has_argument)
{
    if (*count == *capacity) {
        if (*capacity > INT_MAX - 32) {
            g_error("Too many Guest cleanup callbacks");
        }
        *capacity += 32;
        *entries = g_renew(cleanup_t, *entries, *capacity);
    }
    if (cleanup_sequence == UINT64_MAX) {
        g_error("Guest cleanup sequence overflow");
    }
    (*entries)[(*count)++] = (cleanup_t) {
        .function = function,
        .argument = argument,
        .has_argument = has_argument,
        .sequence = ++cleanup_sequence,
    };
}

void AddCleanup(void *function)
{
    pthread_mutex_lock(&cleanup_lock);
    add_cleanup(&my_context->cleanups, &my_context->clean_sz,
                &my_context->clean_cap, function, NULL, false);
    pthread_mutex_unlock(&cleanup_lock);
}

void AddCleanup1Arg(void *function, void *argument, elfheader_t *head)
{
    pthread_mutex_lock(&cleanup_lock);
    if (head) {
        add_cleanup(&head->cleanups, &head->clean_sz, &head->clean_cap,
                    function, argument, true);
    } else {
        add_cleanup(&my_context->cleanups, &my_context->clean_sz,
                    &my_context->clean_cap, function, argument, true);
    }
    pthread_mutex_unlock(&cleanup_lock);
}

static void run_cleanup(const cleanup_t *entry)
{
    if (entry->has_argument) {
        RunFunctionWithState((uintptr_t)entry->function, 1,
                             (uintptr_t)entry->argument);
    } else {
        RunFunctionWithState((uintptr_t)entry->function, 0);
    }
}

static bool pop_cleanup(cleanup_t **entries, int *count, int *capacity,
                        cleanup_t *entry)
{
    if (*count) {
        /* Remove before calling Guest code: callbacks may reenter cleanup. */
        *entry = (*entries)[--*count];
        if (!*count) {
            g_clear_pointer(entries, g_free);
            *capacity = 0;
        }
        return true;
    }
    g_clear_pointer(entries, g_free);
    *capacity = 0;
    return false;
}

void CallCleanup(elfheader_t *head)
{
    cleanup_t entry;
    bool found;

    if (!head) {
        return;
    }
    do {
        pthread_mutex_lock(&cleanup_lock);
        found = pop_cleanup(&head->cleanups, &head->clean_sz,
                            &head->clean_cap, &entry);
        pthread_mutex_unlock(&cleanup_lock);
        if (found) {
            run_cleanup(&entry);
        }
    } while (found);
}

void CallAllCleanup(void)
{
    cleanup_t entry;
    bool found;

    if (!my_context) {
        return;
    }
    do {
        cleanup_t **entries = &my_context->cleanups;
        int *count = &my_context->clean_sz;
        int *capacity = &my_context->clean_cap;
        uint64_t newest;

        pthread_mutex_lock(&cleanup_lock);
        newest = *count ? (*entries)[*count - 1].sequence : 0;
        for (int i = 0; i < my_context->elfsize; i++) {
            elfheader_t *head = my_context->elfs[i];

            if (head && head->clean_sz &&
                head->cleanups[head->clean_sz - 1].sequence > newest) {
                entries = &head->cleanups;
                count = &head->clean_sz;
                capacity = &head->clean_cap;
                newest = (*entries)[*count - 1].sequence;
            }
        }
        found = pop_cleanup(entries, count, capacity, &entry);
        pthread_mutex_unlock(&cleanup_lock);
        if (found) {
            run_cleanup(&entry);
        }
    } while (found);
}

void lat_end(void)
{
    /* Finalization requires the embedding application to quiesce callers. */
    CallAllCleanup();
}

void cleanup_fork_start(void)
{
    pthread_mutex_lock(&cleanup_lock);
}

void cleanup_fork_end(void)
{
    pthread_mutex_unlock(&cleanup_lock);
}
