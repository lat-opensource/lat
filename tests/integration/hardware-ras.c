/* SPDX-License-Identifier: GPL-2.0-only */
#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#include "test-hardware-ras.h"

#ifdef LATX_HRAS_STRESS
#define RAS_ASYNC_ITERS 5000000
#define RAS_WORKER_ITERS 2000
#define RAS_SMC_ITERS 1000
#define RAS_NESTED_DEPTH 1800
#define RAS_UNWIND_DEPTH 700
#define RAS_SPIN_ROUNDS 32
#else
#define RAS_ASYNC_ITERS 20000
#define RAS_WORKER_ITERS 200
#define RAS_SMC_ITERS 50
#define RAS_NESTED_DEPTH 300
#define RAS_UNWIND_DEPTH 100
#define RAS_SPIN_ROUNDS 4
#endif

static __attribute__((noinline)) unsigned leaf(unsigned x)
{
    __asm__ volatile("" : "+r"(x));
    return x * 3 + 1;
}

static __attribute__((noinline)) unsigned nested(unsigned depth, unsigned x)
{
    if (!depth) {
        return leaf(x);
    }
    unsigned value = nested(depth - 1, x + 1);
    __asm__ volatile("" : "+r"(value));
    return value + 1;
}

/* Volatile prevents the compiler from turning this into a direct CALL. */
static unsigned (*volatile indirect)(unsigned) = leaf;
static jmp_buf escape;

static __attribute__((noinline)) void unwind(unsigned depth)
{
    if (!depth) {
        longjmp(escape, 1);
    }
    unwind(depth - 1);
    __asm__ volatile("" : : : "memory");
}

static void *worker(void *arg)
{
    uintptr_t i;
    for (i = 0; i < RAS_WORKER_ITERS; ++i) {
        assert(nested(20, i) == leaf(i + 20) + 20);
        assert(indirect(i) == leaf(i));
    }
    return arg;
}

static volatile sig_atomic_t deliveries;
static volatile sig_atomic_t ticks;
static volatile sig_atomic_t spin_done;
/* A fixed, page-safe CALL/RET cycle with no syscall or other escape route. */
#if UINTPTR_MAX > UINT32_MAX
__asm__(".text\n.balign 64\n.globl ras_signal_spin\n"
        "ras_signal_spin: push %rbx; mov %rdi,%rbx;"
        "1: call ras_signal_leaf; cmpl $0,(%rbx); je 1b; pop %rbx; ret\n"
        ".balign 64\nras_signal_leaf: ret\n");
#else
__asm__(".text\n.balign 64\n.globl ras_signal_spin\n"
        "ras_signal_spin: push %ebx; mov 8(%esp),%ebx;"
        "1: call ras_signal_leaf; cmpl $0,(%ebx); je 1b; pop %ebx; ret\n"
        ".balign 64\nras_signal_leaf: ret\n");
#endif

static void timer_handler(int sig __attribute__((unused)))
{
    ++ticks;
    spin_done = 1;
}

static void *send_alarm(void *opaque)
{
    usleep(1000);
    pthread_kill(*(pthread_t *)opaque, SIGALRM);
    return NULL;
}

static void test_async_signals(void)
{
    struct sigaction action = { .sa_handler = timer_handler };
    pthread_t main_thread = pthread_self();
    pthread_t sender;

    sigemptyset(&action.sa_mask);
    assert(sigaction(SIGALRM, &action, NULL) == 0);
    assert(pthread_create(&sender, NULL, send_alarm, &main_thread) == 0);
    for (unsigned i = 0; i < RAS_ASYNC_ITERS; ++i) {
        assert(indirect(i) == i * 3 + 1);
    }
    assert(pthread_join(sender, NULL) == 0);
    assert(ticks > 0);

    /*
     * Keep the async path independent from host timer delivery. Each round
     * interrupts the guest CALL/RET cycle from a separate guest thread.
     */
    for (unsigned round = 0; round < RAS_SPIN_ROUNDS; ++round) {
        sig_atomic_t before = ticks;
        spin_done = 0;
        assert(pthread_create(&sender, NULL, send_alarm, &main_thread) == 0);
        ras_signal_spin(&spin_done);
        assert(pthread_join(sender, NULL) == 0);
        assert(ticks != before);
    }
}

static void handler(int sig)
{
    assert(nested(30, 2) == leaf(32) + 30);
    ++deliveries;
    if (sig == SIGUSR1) {
        assert(raise(SIGUSR2) == 0);
    }
}

static ucontext_t main_context, other_context;
static void other_stack(void)
{
    assert(nested(80, 3) == leaf(83) + 80);
    assert(swapcontext(&other_context, &main_context) == 0);
    abort();
}

/* Keep a guest return address outstanding while invalidating its target TB. */
#if UINTPTR_MAX > UINT32_MAX
#define SMC_VALUE_OFFSET 13
#define SMC_CALLBACK_OFFSET 2
static const unsigned char smc_template[] = {
    0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, /* movabs callback,rax */
    0xff, 0xd0,                         /* call *rax */
    0xb8, 1, 0, 0, 0,                   /* mov value,eax */
    0xc3,                               /* ret */
};
#else
#define SMC_VALUE_OFFSET 8
#define SMC_CALLBACK_OFFSET 1
static const unsigned char smc_template[] = {
    0xb8, 0, 0, 0, 0,                   /* mov callback,eax */
    0xff, 0xd0,                         /* call *eax */
    0xb8, 1, 0, 0, 0,                   /* mov value,eax */
    0xc3,                               /* ret */
};
#endif
static unsigned char *smc_code;
static unsigned char smc_value;
static __attribute__((noinline)) void change_return_target(void)
{
    smc_code[SMC_VALUE_OFFSET] = smc_value;
    __asm__ volatile("" : : : "memory");
}

static void test_smc(void)
{
    uintptr_t callback = (uintptr_t)change_return_target;
    smc_code = mmap(NULL, 65536, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(smc_code != MAP_FAILED);
    memcpy(smc_code, smc_template, sizeof(smc_template));
    memcpy(smc_code + SMC_CALLBACK_OFFSET, &callback, sizeof(callback));
    for (unsigned i = 0; i < RAS_SMC_ITERS; ++i) {
        smc_value = i % 2 + 1;
        assert(((int (*)(void))smc_code)() == smc_value);
    }
    assert(munmap(smc_code, 65536) == 0);
}

static atomic_uint smc_entered, smc_released;
static unsigned smc_phase;

static __attribute__((noinline)) void wait_for_smc(void)
{
    atomic_store(&smc_entered, smc_phase);
    while (atomic_load(&smc_released) != smc_phase) {
        __asm__ volatile("" : : : "memory");
    }
}

static void *smc_writer(void *unused)
{
    for (unsigned phase = 1; phase <= RAS_SMC_ITERS; ++phase) {
        while (atomic_load(&smc_entered) != phase) {
            __asm__ volatile("" : : : "memory");
        }
        smc_code[SMC_VALUE_OFFSET] = phase % 2 + 1;
        atomic_store(&smc_released, phase);
    }
    return unused;
}

static void test_parallel_smc(void)
{
    pthread_t writer;
    uintptr_t callback = (uintptr_t)wait_for_smc;

    smc_code = mmap(NULL, 65536, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(smc_code != MAP_FAILED);
    memcpy(smc_code, smc_template, sizeof(smc_template));
    memcpy(smc_code + SMC_CALLBACK_OFFSET, &callback, sizeof(callback));
    assert(pthread_create(&writer, NULL, smc_writer, NULL) == 0);
    for (smc_phase = 1; smc_phase <= RAS_SMC_ITERS; ++smc_phase) {
        assert(((int (*)(void))smc_code)() == smc_phase % 2 + 1);
    }
    assert(pthread_join(writer, NULL) == 0);
    assert(munmap(smc_code, 65536) == 0);
}

#if UINTPTR_MAX > UINT32_MAX
static void no_return(void)
{
    _exit(0);
}

/* A CALL may be valid even when its return address cannot be decoded. */
static void test_page_end(void)
{
    for (unsigned tail = 0; tail < 16; tail += 8) {
        pid_t child = fork();
        int status;
        assert(child >= 0);
        if (!child) {
            unsigned char *p = mmap(NULL, 131072, PROT_NONE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            assert(p != MAP_FAILED);
            assert(mprotect(p, 65536, PROT_READ | PROT_WRITE | PROT_EXEC) == 0);
            uintptr_t callback = (uintptr_t)no_return;
            unsigned char *entry = p + 65536 - tail - 12;
            entry[0] = 0x48;
            entry[1] = 0xb8;
            memcpy(entry + 2, &callback, 8);
            entry[10] = 0xff;
            entry[11] = 0xd0;
            ((void (*)(void))entry)();
            abort();
        }
        assert(waitpid(child, &status, 0) == child && status == 0);
    }
}

__asm__(
    ".text\n"
    ".type ras_ret8,@function\n"
    "ras_ret8: mov 8(%rsp),%rax; ret $8\n"
    ".type ras_return_pc,@function\n"
    "ras_return_pc: mov (%rsp),%rax; ret\n"
    ".type ras_change_pc,@function\n"
    "ras_change_pc: addq $5,(%rsp); ret\n");

static void test_guest_stack(void)
{
    unsigned long value, expected;
    __asm__ volatile("pushq $7; call ras_ret8"
                     : "=a"(value) : : "memory", "cc");
    assert(value == 7);
    __asm__ volatile("call ras_return_pc; 1: lea 1b(%%rip),%1"
                     : "=a"(value), "=r"(expected) : : "memory", "cc");
    assert(value == expected);
    __asm__ volatile("mov $1,%%eax; call ras_change_pc; mov $0,%%eax"
                     : "=a"(value) : : "memory", "cc");
    assert(value == 1);
}
#else
__asm__(
    ".text\n"
    ".type ras_ret4,@function\n"
    "ras_ret4: mov 4(%esp),%eax; ret $4\n"
    ".type ras_return_pc,@function\n"
    "ras_return_pc: mov (%esp),%eax; ret\n"
    ".type ras_change_pc,@function\n"
    "ras_change_pc: addl $5,(%esp); ret\n");

static void test_guest_stack(void)
{
    unsigned long value, expected;

    __asm__ volatile("pushl $7; call ras_ret4"
                     : "=a"(value) : : "memory", "cc");
    assert(value == 7);
    __asm__ volatile("call ras_return_pc; 1: lea 1b,%1"
                     : "=a"(value), "=r"(expected) : : "memory", "cc");
    assert(value == expected);
    __asm__ volatile("mov $1,%%eax; call ras_change_pc; mov $0,%%eax"
                     : "=a"(value) : : "memory", "cc");
    assert(value == 1);
}
#endif

int main(void)
{
    pthread_t threads[4];
    void *stack = malloc(128 * 1024);
    stack_t alt = { .ss_sp = stack, .ss_size = 128 * 1024 };
    struct sigaction action = { .sa_handler = handler, .sa_flags = SA_ONSTACK };
    int status;

    assert(nested(RAS_NESTED_DEPTH, 4) ==
           leaf(RAS_NESTED_DEPTH + 4) + RAS_NESTED_DEPTH);
    if (!setjmp(escape)) {
        unwind(RAS_UNWIND_DEPTH);
    }
    assert(nested(50, 7) == leaf(57) + 50);
    assert(sigaltstack(&alt, NULL) == 0);
    sigemptyset(&action.sa_mask);
    assert(sigaction(SIGUSR1, &action, NULL) == 0);
    assert(sigaction(SIGUSR2, &action, NULL) == 0);
    assert(raise(SIGUSR1) == 0 && deliveries == 2);
#ifdef LATX_HRAS_STRESS
    test_async_signals();
#endif
    assert(getcontext(&other_context) == 0);
    other_context.uc_stack.ss_sp = malloc(64 * 1024);
    other_context.uc_stack.ss_size = 64 * 1024;
    makecontext(&other_context, other_stack, 0);
    assert(swapcontext(&main_context, &other_context) == 0);
    for (uintptr_t i = 0; i < 4; ++i) {
        assert(pthread_create(&threads[i], NULL, worker, (void *)i) == 0);
    }
    for (unsigned i = 0; i < 4; ++i) {
        void *result;
        assert(pthread_join(threads[i], &result) == 0);
        assert(result == (void *)(uintptr_t)i);
    }
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        worker(NULL);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child && status == 0);
    test_guest_stack();
    test_smc();
    test_parallel_smc();
#if UINTPTR_MAX > UINT32_MAX
    if (!getenv("LATX_HRAS_TEST_SKIP_PAGE_END")) {
        test_page_end();
    }
#endif
    puts("PASS: deep nesting, indirect, longjmp, signals/altstack, "
         "async signals, stack switch, threads, fork");
    puts("PASS: guest return PC, RET immediate, SMC/single+parallel");
#if UINTPTR_MAX > UINT32_MAX
    puts("PASS: page-end CALL");
#endif
    return 0;
}
