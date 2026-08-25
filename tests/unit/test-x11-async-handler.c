#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "target/i386/latx/context/wrappedlibx11_async.h"

#define ASYNC_HANDLER(addr) \
    ((int (*)(void *, void *, char *, int, void *))(uintptr_t)(addr))

#define TEST_CHECK(condition)                                          \
    do {                                                               \
        if (!(condition)) {                                            \
            fprintf(stderr, "%s:%d: check failed: %s\n",            \
                    __FILE__, __LINE__, #condition);                  \
            abort();                                                   \
        }                                                              \
    } while (0)

#define TEST_SLOT_COUNT 16

typedef struct test_slot {
    uintptr_t guest;
    uintptr_t native;
} test_slot;

typedef struct test_bridge_pool {
    test_slot slots[TEST_SLOT_COUNT];
    atomic_int callback_active;
    atomic_int callback_overlap;
    int delay_callback;
} test_bridge_pool;

static test_bridge_pool slot_pool;

static void reset_slot_pool(int delay_callback)
{
    memset(&slot_pool, 0, sizeof(slot_pool));
    atomic_init(&slot_pool.callback_active, 0);
    atomic_init(&slot_pool.callback_overlap, 0);
    slot_pool.delay_callback = delay_callback;
}

static void *slot_bridge_handler(void *handler)
{
    const uintptr_t guest = (uintptr_t)handler;
    void *result = NULL;
    size_t index;

    if (atomic_exchange(&slot_pool.callback_active, 1)) {
        atomic_store(&slot_pool.callback_overlap, 1);
    }
    if (slot_pool.delay_callback) {
        const struct timespec delay = { .tv_nsec = 1000000 };
        nanosleep(&delay, NULL);
    }

    for (index = 0; index < TEST_SLOT_COUNT; index++) {
        if (slot_pool.slots[index].guest == guest) {
            result = (void *)slot_pool.slots[index].native;
            goto out;
        }
    }
    for (index = 0; index < TEST_SLOT_COUNT; index++) {
        if (!slot_pool.slots[index].guest) {
            slot_pool.slots[index].guest = guest;
            slot_pool.slots[index].native = 0xa000 + index * 0x100;
            result = (void *)slot_pool.slots[index].native;
            goto out;
        }
    }

out:
    atomic_store(&slot_pool.callback_active, 0);
    return result;
}

static uintptr_t guest_for_native(uintptr_t native)
{
    size_t index;

    for (index = 0; index < TEST_SLOT_COUNT; index++) {
        if (slot_pool.slots[index].native == native) {
            return slot_pool.slots[index].guest;
        }
    }
    return 0;
}

static void *bridge_handler(void *handler)
{
    if ((uintptr_t)handler == 0x1000) {
        return (void *)0xa000;
    }
    if ((uintptr_t)handler == 0x2000) {
        return (void *)0xb000;
    }
    TEST_CHECK(0 && "unexpected handler");
    return NULL;
}

static void *reject_handler(void *handler)
{
    TEST_CHECK((uintptr_t)handler == 0x1000);
    return NULL;
}

static void test_guest_handlers_are_bridged(void)
{
    latx_x11_async_bridge_state state =
        LATX_X11_ASYNC_BRIDGE_STATE_INITIALIZER;
    latx_XInternalAsync third = {
        .next = NULL,
        .handler = ASYNC_HANDLER(0x9000),
    };
    latx_XInternalAsync second = {
        .next = &third,
        .handler = ASYNC_HANDLER(0x2000),
    };
    latx_XInternalAsync first = {
        .next = &second,
        .handler = ASYNC_HANDLER(0x1000),
    };

    latx_x11_async_bridge_result result =
        latx_x11_bridge_async_handler_list(&state, &first, 0x8000,
                                           bridge_handler);

    TEST_CHECK(result == LATX_X11_ASYNC_BRIDGE_OK);
    TEST_CHECK((uintptr_t)first.handler == 0xa000);
    TEST_CHECK((uintptr_t)second.handler == 0xb000);
    TEST_CHECK((uintptr_t)third.handler == 0x9000);
    pthread_mutex_destroy(&state.mutex);
}

static void test_empty_handler_list_is_accepted(void)
{
    latx_x11_async_bridge_state state =
        LATX_X11_ASYNC_BRIDGE_STATE_INITIALIZER;

    TEST_CHECK(latx_x11_bridge_async_handler_list(&state, NULL, 0x8000,
                                                   bridge_handler) ==
               LATX_X11_ASYNC_BRIDGE_OK);
    pthread_mutex_destroy(&state.mutex);
}

static void test_failed_bridge_is_transactional(void)
{
    latx_x11_async_bridge_state state =
        LATX_X11_ASYNC_BRIDGE_STATE_INITIALIZER;
    latx_XInternalAsync async = {
        .next = NULL,
        .handler = ASYNC_HANDLER(0x1000),
    };

    latx_x11_async_bridge_result result =
        latx_x11_bridge_async_handler_list(&state, &async, 0x8000,
                                           reject_handler);

    TEST_CHECK(result == LATX_X11_ASYNC_BRIDGE_HANDLER_UNAVAILABLE);
    TEST_CHECK((uintptr_t)async.handler == 0x1000);
    pthread_mutex_destroy(&state.mutex);
}

static void test_same_guest_handler_has_deterministic_dispatch(void)
{
    latx_x11_async_bridge_state state =
        LATX_X11_ASYNC_BRIDGE_STATE_INITIALIZER;
    latx_XInternalAsync first = {
        .handler = ASYNC_HANDLER(0x1000),
    };
    latx_XInternalAsync second = {
        .handler = ASYNC_HANDLER(0x1000),
    };

    reset_slot_pool(0);
    TEST_CHECK(latx_x11_bridge_async_handler_list(&state, &first, 0x8000,
                                                   slot_bridge_handler) ==
               LATX_X11_ASYNC_BRIDGE_OK);
    TEST_CHECK(latx_x11_bridge_async_handler_list(&state, &second, 0x8000,
                                                   slot_bridge_handler) ==
               LATX_X11_ASYNC_BRIDGE_OK);
    TEST_CHECK(first.handler == second.handler);
    TEST_CHECK(guest_for_native((uintptr_t)first.handler) == 0x1000);
    pthread_mutex_destroy(&state.mutex);
}

static void test_seventeenth_guest_handler_fails_without_partial_publish(void)
{
    latx_x11_async_bridge_state state =
        LATX_X11_ASYNC_BRIDGE_STATE_INITIALIZER;
    latx_XInternalAsync handlers[TEST_SLOT_COUNT + 1];
    uintptr_t original[TEST_SLOT_COUNT + 1];
    size_t index;

    reset_slot_pool(0);
    memset(handlers, 0, sizeof(handlers));
    for (index = 0; index < TEST_SLOT_COUNT + 1; index++) {
        original[index] = 0x1000 + index * 0x100;
        handlers[index].handler = ASYNC_HANDLER(original[index]);
        if (index + 1 < TEST_SLOT_COUNT + 1) {
            handlers[index].next = &handlers[index + 1];
        }
    }

    TEST_CHECK(latx_x11_bridge_async_handler_list(&state, handlers, 0x8000,
                                                   slot_bridge_handler) ==
               LATX_X11_ASYNC_BRIDGE_HANDLER_UNAVAILABLE);
    for (index = 0; index < TEST_SLOT_COUNT + 1; index++) {
        TEST_CHECK((uintptr_t)handlers[index].handler == original[index]);
    }
    pthread_mutex_destroy(&state.mutex);
}

typedef struct test_start_gate {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int ready;
    int start;
} test_start_gate;

typedef struct test_bridge_thread {
    latx_x11_async_bridge_state *state;
    latx_XInternalAsync *handlers;
    test_start_gate *gate;
    latx_x11_async_bridge_result result;
} test_bridge_thread;

static void *bridge_display_thread(void *opaque)
{
    test_bridge_thread *thread = opaque;

    pthread_mutex_lock(&thread->gate->mutex);
    thread->gate->ready++;
    pthread_cond_broadcast(&thread->gate->condition);
    while (!thread->gate->start) {
        pthread_cond_wait(&thread->gate->condition, &thread->gate->mutex);
    }
    pthread_mutex_unlock(&thread->gate->mutex);

    thread->result = latx_x11_bridge_async_handler_list(
        thread->state, thread->handlers, 0x8000, slot_bridge_handler);
    return NULL;
}

static void init_handler_chain(latx_XInternalAsync *handlers,
                               uintptr_t *original, uintptr_t first)
{
    size_t index;

    memset(handlers, 0, sizeof(*handlers) * 8);
    for (index = 0; index < 8; index++) {
        original[index] = first + index * 0x100;
        handlers[index].handler = ASYNC_HANDLER(original[index]);
        if (index + 1 < 8) {
            handlers[index].next = &handlers[index + 1];
        }
    }
}

static void test_two_displays_bridge_concurrently_without_cross_dispatch(void)
{
    latx_x11_async_bridge_state state =
        LATX_X11_ASYNC_BRIDGE_STATE_INITIALIZER;
    latx_XInternalAsync display_a[8];
    latx_XInternalAsync display_b[8];
    uintptr_t original_a[8];
    uintptr_t original_b[8];
    test_start_gate gate = {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .condition = PTHREAD_COND_INITIALIZER,
    };
    test_bridge_thread work_a = {
        .state = &state,
        .handlers = display_a,
        .gate = &gate,
    };
    test_bridge_thread work_b = {
        .state = &state,
        .handlers = display_b,
        .gate = &gate,
    };
    pthread_t thread_a;
    pthread_t thread_b;
    size_t index;

    reset_slot_pool(1);
    init_handler_chain(display_a, original_a, 0x1000);
    init_handler_chain(display_b, original_b, 0x2000);
    TEST_CHECK(pthread_create(&thread_a, NULL, bridge_display_thread,
                              &work_a) == 0);
    TEST_CHECK(pthread_create(&thread_b, NULL, bridge_display_thread,
                              &work_b) == 0);

    pthread_mutex_lock(&gate.mutex);
    while (gate.ready != 2) {
        pthread_cond_wait(&gate.condition, &gate.mutex);
    }
    gate.start = 1;
    pthread_cond_broadcast(&gate.condition);
    pthread_mutex_unlock(&gate.mutex);

    TEST_CHECK(pthread_join(thread_a, NULL) == 0);
    TEST_CHECK(pthread_join(thread_b, NULL) == 0);
    TEST_CHECK(work_a.result == LATX_X11_ASYNC_BRIDGE_OK);
    TEST_CHECK(work_b.result == LATX_X11_ASYNC_BRIDGE_OK);
    TEST_CHECK(!atomic_load(&slot_pool.callback_overlap));
    for (index = 0; index < 8; index++) {
        TEST_CHECK(guest_for_native((uintptr_t)display_a[index].handler) ==
                   original_a[index]);
        TEST_CHECK(guest_for_native((uintptr_t)display_b[index].handler) ==
                   original_b[index]);
    }

    pthread_cond_destroy(&gate.condition);
    pthread_mutex_destroy(&gate.mutex);
    pthread_mutex_destroy(&state.mutex);
}

int main(void)
{
    test_guest_handlers_are_bridged();
    test_empty_handler_list_is_accepted();
    test_failed_bridge_is_transactional();
    test_same_guest_handler_has_deterministic_dispatch();
    test_seventeenth_guest_handler_fails_without_partial_publish();
    test_two_displays_bridge_concurrently_without_cross_dispatch();
    return 0;
}
