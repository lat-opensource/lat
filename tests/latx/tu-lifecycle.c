#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>

/*
 * gcc -m32/-m64 -O2 -static -pthread -no-pie tu-lifecycle.c
 *     tu-lifecycle-code.S -o tu-lifecycle32/64
 */
#include "tu-lifecycle.h"
static pthread_barrier_t phase_barrier;
static atomic_int failed;
static int threaded_smc;

#if UINTPTR_MAX > UINT32_MAX
static void test_vsyscall(void)
{
    struct timeval tv;
    time_t now;
    unsigned int cpu;
    int (*gettimeofday_entry)(struct timeval *, void *) =
        (void *)0xffffffffff600000UL;
    time_t (*time_entry)(time_t *) = (void *)0xffffffffff600400UL;
    int (*getcpu_entry)(unsigned int *, unsigned int *, void *) =
        (void *)0xffffffffff600800UL;

    assert(gettimeofday_entry(&tv, NULL) == 0);
    time_t result = time_entry(&now);
    assert(result != (time_t)-1 && result == now);
    assert(getcpu_entry(&cpu, NULL, NULL) == 0);
}
#endif

static void patch_member(unsigned char value)
{
    assert(mprotect(tu_f1, 4096, PROT_READ | PROT_WRITE | PROT_EXEC) == 0);
    *tu_f1_imm = value;
    assert(mprotect(tu_f1, 4096, PROT_READ | PROT_EXEC) == 0);
}

static void *worker(void *arg)
{
    (void)arg;
    for (int phase = 0; phase < 3; phase++) {
        pthread_barrier_wait(&phase_barrier);
        for (int i = 0; i < 1000; i++) {
            int expected = threaded_smc && phase == 1 ? 5 : 3;
            if (tu_f0() != expected) {
                atomic_store(&failed, 1);
            }
        }
        pthread_barrier_wait(&phase_barrier);
    }
    return NULL;
}

static void test_threads(int smc)
{
    pthread_t threads[2];

    threaded_smc = smc;
    assert(pthread_barrier_init(&phase_barrier, NULL, 3) == 0);
    for (int i = 0; i < 2; i++) {
        assert(pthread_create(&threads[i], NULL, worker, NULL) == 0);
    }
    for (int phase = 0; phase < 3; phase++) {
        if (smc && phase) {
            patch_member(phase == 1 ? 4 : 2);
        }
        pthread_barrier_wait(&phase_barrier);
        pthread_barrier_wait(&phase_barrier);
    }
    for (int i = 0; i < 2; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
    }
    assert(!atomic_load(&failed));
    assert(pthread_barrier_destroy(&phase_barrier) == 0);
}

int main(int argc, char **argv)
{
    int mode = argc > 1 ? atoi(argv[1]) : 0;
#if UINTPTR_MAX > UINT32_MAX
    if (mode == 6) {
        test_vsyscall();
        puts("vsyscall PASS");
        return 0;
    }
#endif
    for (int i = 0; i < 32; i++) {
        assert(tu_f0() == 3);
    }
    if (mode == 3) {
        patch_member(4);
        assert(tu_f0() == 5);
        patch_member(2);
        assert(tu_f0() == 3);
    } else if (mode == 0 || mode == 4) {
        test_threads(mode == 4);
    }
    printf("lifecycle mode %d PASS\n", mode);
    return 0;
}
