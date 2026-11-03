/* SPDX-License-Identifier: GPL-2.0-only */
#define _GNU_SOURCE
#undef NDEBUG
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "entry-signal.h"
static volatile sig_atomic_t done;
static volatile sig_atomic_t delivered;
static void handler(int sig)
{
    delivered = sig;
    done = 1;
}
#if UINTPTR_MAX > UINT32_MAX
__asm__(".text\n.balign 64\n.globl entry_spin, entry_leaf\n"
        "entry_spin: push %rbx; mov %rdi,%rbx;"
        "1: call entry_leaf; subl $1,(%rbx); jne 1b; pop %rbx; ret\n"
        ".balign 64\nentry_leaf: ret\n");
#else
__asm__(".text\n.balign 64\n.globl entry_spin, entry_leaf\n"
        "entry_spin: push %ebx; mov 8(%esp),%ebx;"
        "1: call entry_leaf; subl $1,(%ebx); jne 1b; pop %ebx; ret\n"
        ".balign 64\nentry_leaf: ret\n");
#endif
int main(int argc, char **argv)
{
    (void)argv;

    struct sigaction action = { .sa_handler = handler };

    sigemptyset(&action.sa_mask);
    assert(sigaction(SIGUSR1, &action, NULL) == 0);
    done = 100;
    /* Force entry_spin through the dispatcher, even with AOT links. */
    assert(getpid() > 0);
    entry_spin(&done);
    assert(done == 0);
    assert(argc > 1 || delivered == SIGUSR1);
    puts("entry-signal PASS");
    return 0;
}
