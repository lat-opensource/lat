/* SPDX-License-Identifier: GPL-2.0-only */
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef ITERATIONS
#define ITERATIONS 100000ULL
#endif

typedef uint64_t (*bench_fn)(uint64_t);

static __attribute__((noinline)) uint64_t fn_add(uint64_t value)
{
    return value + 1;
}

static __attribute__((noinline)) uint64_t fn_xor(uint64_t value)
{
    return value ^ 3;
}

static __attribute__((noinline)) uint64_t fn_rotate(uint64_t value)
{
    return (value << 1) | (value >> 63);
}

static __attribute__((noinline)) uint64_t fn_mul(uint64_t value)
{
    return value * 5 + 7;
}

/* volatile: force a target load for every indirect call. */
static bench_fn volatile indirect_functions[] = {
    fn_add,
    fn_xor,
    fn_rotate,
    fn_mul,
};

/* Exercise several callers and a nested path to the same indirect call site. */
static __attribute__((noinline)) uint64_t apply_one(bench_fn fn, uint64_t value)
{
    uint64_t result = fn(value);

    /* Keep real work after CALL so the compiler cannot emit a tail JMP. */
    __asm__ volatile("" : "+r"(result));
    return result;
}

static __attribute__((noinline)) uint64_t apply_middle(bench_fn fn,
                                                       uint64_t value)
{
    uint64_t result = fn(value);

    __asm__ volatile("" : "+r"(result));
    return result;
}

static __attribute__((noinline)) uint64_t apply_outer(bench_fn fn,
                                                      uint64_t value)
{
    return apply_middle(fn, value);
}

static __attribute__((noinline)) uint64_t call_direct(unsigned index,
                                                      uint64_t value)
{
    uint64_t result;

    switch (index) {
    case 0:
        result = fn_add(value);
        break;
    case 1:
        result = fn_xor(value);
        break;
    case 2:
        result = fn_rotate(value);
        break;
    default:
        result = fn_mul(value);
        break;
    }
    __asm__ volatile("" : "+r"(result));
    return result;
}

static uint64_t apply_expected(unsigned index, uint64_t value)
{
    switch (index) {
    case 0:
        return value + 1;
    case 1:
        return value ^ 3;
    case 2:
        return (value << 1) | (value >> 63);
    default:
        return value * 5 + 7;
    }
}

struct worker_data {
    uint64_t seed;
    uint64_t iterations;
    uint64_t result;
};

static volatile sig_atomic_t signal_count;

static void handle_signal(int sig)
{
    (void)sig;
    ++signal_count;
}

static uint64_t run_loop(uint64_t seed, uint64_t iterations)
{
    uint64_t value = seed + 1;
    uint64_t expected = value;

    for (uint64_t i = 0; i < iterations; ++i) {
        unsigned index = (i + seed) & 3;
        bench_fn fn = indirect_functions[index];

        expected = apply_expected(index, expected);
        switch ((i + seed) % 3) {
        case 0:
            value = apply_one(fn, value);
            break;
        case 1:
            value = apply_outer(fn, value);
            break;
        default:
            /* Interleave a direct call to the same callee set. */
            value = call_direct(index, value);
            break;
        }
        __asm__ volatile("" : "+r"(value) : : "memory");
    }
    return value ^ expected;
}

static void *run_worker(void *opaque)
{
    struct worker_data *data = opaque;

    data->result = run_loop(data->seed, data->iterations);
    return NULL;
}

int main(int argc, char **argv)
{
    struct worker_data workers[] = {
        { .seed = 0, .iterations = ITERATIONS },
        { .seed = 1, .iterations = ITERATIONS },
    };
    pthread_t threads[2];
    bool with_signals;

    if (argc == 1) {
        return run_loop(workers[0].seed, ITERATIONS) != 0;
    }
    if (argc != 2) {
        return 2;
    }
    if (!strcmp(argv[1], "--parallel")) {
        with_signals = false;
    } else if (!strcmp(argv[1], "--parallel-signals")) {
        struct sigaction action = {
            .sa_handler = handle_signal,
            .sa_flags = SA_RESTART,
        };
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGUSR1, &action, NULL)) {
            return 1;
        }
        with_signals = true;
        workers[0].iterations *= 4;
        workers[1].iterations *= 4;
    } else {
        return 2;
    }

    for (unsigned i = 0; i < 2; ++i) {
        if (pthread_create(&threads[i], NULL, run_worker, &workers[i])) {
            return 1;
        }
    }
    if (with_signals) {
        for (unsigned i = 0; i < 10000; ++i) {
            pthread_kill(threads[i & 1], SIGUSR1);
        }
    }
    for (unsigned i = 0; i < 2; ++i) {
        if (pthread_join(threads[i], NULL)) {
            return 1;
        }
    }
    if (with_signals && !signal_count) {
        return 1;
    }
    return workers[0].result || workers[1].result;
}
