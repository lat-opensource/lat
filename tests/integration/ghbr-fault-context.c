/* SPDX-License-Identifier: GPL-2.0-only */
#define _GNU_SOURCE
#include <inttypes.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <ucontext.h>

static sigjmp_buf fault_return;
static volatile uint64_t observed_rax; /* Interrupted state from the handler. */
static volatile uintptr_t observed_pc; /* Interrupted state from the handler. */
static volatile uintptr_t observed_addr; /* Fault address from the handler. */
static volatile sig_atomic_t observed_signal;
static uintptr_t expected_pc;

static void fault_handler(int signal, siginfo_t *info, void *context)
{
    const ucontext_t *uc = context;

    observed_rax = uc->uc_mcontext.gregs[REG_RAX];
    observed_pc = uc->uc_mcontext.gregs[REG_RIP];
    observed_addr = (uintptr_t)info->si_addr;
    observed_signal = signal;
    siglongjmp(fault_return, 1);
}

static __attribute__((noinline)) void load_fault(void)
{
    __asm__ volatile(
        "leaq 1f(%%rip), %%r11\n\t"
        "movq %%r11, %[pc]\n\t"
        "xorl %%edi, %%edi\n\t"
        "movabsq $0x1234567887654321, %%rcx\n\t"
        "movl %%ecx, %%eax\n\t"
        "1: movl (%%rdi), %%eax"
        : [pc] "=m"(expected_pc)
        : : "rax", "rcx", "rdi", "r11", "memory", "cc");
}

static __attribute__((noinline)) void store_fault(void)
{
    __asm__ volatile(
        "leaq 1f(%%rip), %%r11\n\t"
        "movq %%r11, %[pc]\n\t"
        "xorl %%edi, %%edi\n\t"
        "movabsq $0x1234567887654321, %%rcx\n\t"
        "movl %%ecx, %%eax\n\t"
        "1: movl $1, (%%rdi)\n\t"
        "movl $0, %%eax"
        : [pc] "=m"(expected_pc)
        : : "rax", "rcx", "rdi", "r11", "memory", "cc");
}

static __attribute__((noinline)) void cmov_fault(void)
{
    __asm__ volatile(
        "leaq 1f(%%rip), %%r11\n\t"
        "movq %%r11, %[pc]\n\t"
        "xorl %%edi, %%edi\n\t"
        "movabsq $0x1234567887654321, %%rcx\n\t"
        "movl %%ecx, %%eax\n\t"
        "xorl %%edx, %%edx\n\t"
        "1: cmovnel (%%rdi), %%eax"
        : [pc] "=m"(expected_pc)
        : : "rax", "rcx", "rdx", "rdi", "r11", "memory", "cc");
}

static __attribute__((noinline)) void div_fault(void)
{
    __asm__ volatile(
        "leaq 1f(%%rip), %%r11\n\t"
        "movq %%r11, %[pc]\n\t"
        "movabsq $0x1234567887654321, %%rcx\n\t"
        "movl %%ecx, %%eax\n\t"
        "xorl %%edx, %%edx\n\t"
        "xorl %%edi, %%edi\n\t"
        "1: divl %%edi"
        : [pc] "=m"(expected_pc)
        : : "rax", "rcx", "rdx", "rdi", "r11", "memory", "cc");
}

static __attribute__((noinline)) void lods_fault(void)
{
    __asm__ volatile(
        "leaq 1f(%%rip), %%r11\n\t"
        "movq %%r11, %[pc]\n\t"
        "xorl %%esi, %%esi\n\t"
        "movabsq $0x1234567887654321, %%rcx\n\t"
        "movl %%ecx, %%eax\n\t"
        "1: lodsl\n\t"
        "movl $0, %%eax"
        : [pc] "=m"(expected_pc)
        : : "rax", "rcx", "rsi", "r11", "memory", "cc");
}

int main(void)
{
    static const struct {
        const char *name;
        void (*run)(void);
        int signal;
    } cases[] = {
        { "load", load_fault, SIGSEGV },
        { "store", store_fault, SIGSEGV },
        { "cmov-false", cmov_fault, SIGSEGV },
        { "div32", div_fault, SIGFPE },
        { "lods", lods_fault, SIGSEGV },
    };
    struct sigaction action = { .sa_sigaction = fault_handler,
                                .sa_flags = SA_SIGINFO };
    volatile int failed = 0; /* Preserve failures across siglongjmp. */

    sigemptyset(&action.sa_mask);
    if (sigaction(SIGSEGV, &action, NULL) ||
        sigaction(SIGFPE, &action, NULL)) {
        perror("sigaction");
        return 1;
    }
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        observed_signal = 0;
        if (!sigsetjmp(fault_return, 1)) {
            cases[i].run();
            fprintf(stderr, "FAIL: %s did not fault\n", cases[i].name);
            failed = 1;
            continue;
        }
        printf("%s: signal=%d rax=%016" PRIx64 "\n",
               cases[i].name, observed_signal, observed_rax);
        if (observed_signal != cases[i].signal ||
            observed_pc != expected_pc ||
            observed_rax != UINT64_C(0x87654321) ||
            (cases[i].signal == SIGSEGV && observed_addr != 0)) {
            fprintf(stderr, "FAIL: %s signal context\n", cases[i].name);
            failed = 1;
        }
    }
    return failed;
}
