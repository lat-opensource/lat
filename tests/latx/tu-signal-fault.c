#define _GNU_SOURCE
#undef NDEBUG
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

#include "tu-signal-fault.h"
static volatile sig_atomic_t faults;

static void handler(int sig, siginfo_t *info, void *opaque)
{
    ucontext_t *uc = opaque;
#if UINTPTR_MAX > UINT32_MAX
    uintptr_t ip = uc->uc_mcontext.gregs[REG_RIP];
#else
    uintptr_t ip = uc->uc_mcontext.gregs[REG_EIP];
#endif

    if (sig != SIGSEGV || info->si_addr != (void *)1 ||
        ip != (uintptr_t)probe_fault) {
        dprintf(2, "fault mismatch sig=%d addr=%p ip=%p expected=%p\n",
                sig, info->si_addr, (void *)ip, probe_fault);
        _Exit(11);
    }
    faults++;
#if UINTPTR_MAX > UINT32_MAX
    uc->uc_mcontext.gregs[REG_RIP] = (uintptr_t)probe_after;
#else
    uc->uc_mcontext.gregs[REG_EIP] = (uintptr_t)probe_after;
#endif
}

int main(void)
{
    struct sigaction sa = { .sa_sigaction = handler, .sa_flags = SA_SIGINFO };

    sigemptyset(&sa.sa_mask);
    assert(sigaction(SIGSEGV, &sa, 0) == 0);
    for (int i = 0; i < 24; i++) {
        assert(cross_entry(200000) == 3);
    }
    assert(faults == 24);
    printf("signal-fault PASS faults=%d\n", faults);
    return 0;
}
