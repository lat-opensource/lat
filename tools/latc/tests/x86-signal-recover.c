#define _GNU_SOURCE

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

extern int signal_boundary_site(void);
extern char signal_boundary_fault[], signal_boundary_resume[];
extern int signal_internal_site(void);
extern char signal_internal_fault[], signal_internal_resume[];
extern int signal_end_site(void);
extern char signal_end_fault[], signal_end_resume[];
extern int signal_helper_site(void);
extern char signal_helper_fault[], signal_helper_resume[];

enum SignalCase {
    SIGNAL_BOUNDARY = 1,
    SIGNAL_INTERNAL,
    SIGNAL_END,
    SIGNAL_HELPER,
};

enum SignalGpr {
    SIGNAL_RAX,
    SIGNAL_RBX,
    SIGNAL_RCX,
    SIGNAL_RDX,
    SIGNAL_RSI,
    SIGNAL_RDI,
    SIGNAL_RBP,
    SIGNAL_R8,
    SIGNAL_R9,
    SIGNAL_R10,
    SIGNAL_R11,
    SIGNAL_R12,
    SIGNAL_R13,
    SIGNAL_R14,
    SIGNAL_R15,
    SIGNAL_GPR_COUNT,
};

__attribute__((visibility("hidden"))) const uint64_t
signal_expected_gprs[SIGNAL_GPR_COUNT] = {
    UINT64_C(0x0102030405060708),
    UINT64_C(0x1112131415161718),
    UINT64_C(0),
    UINT64_C(0x2122232425262728),
    UINT64_C(0x3132333435363738),
    UINT64_C(0x4142434445464748),
    UINT64_C(0x5152535455565758),
    UINT64_C(0x6162636465666768),
    UINT64_C(0x7172737475767778),
    UINT64_C(0x8182838485868788),
    UINT64_C(0x9192939495969798),
    UINT64_C(0xa1a2a3a4a5a6a7a8),
    UINT64_C(0xb1b2b3b4b5b6b7b8),
    UINT64_C(0xc1c2c3c4c5c6c7c8),
    UINT64_C(0xd1d2d3d4d5d6d7d8),
};

__attribute__((aligned(16), visibility("hidden"))) const uint32_t
signal_expected_xmm[4] = {
    UINT32_C(0x10213243), UINT32_C(0x54657687),
    UINT32_C(0x98a9bacb), UINT32_C(0xdcedfe0f),
};

#define SIGNAL_RFLAGS_MASK UINT64_C(0xcd5)
#define SIGNAL_POST_RFLAGS_MASK UINT64_C(0x1)
#define SIGNAL_POST_R15 UINT64_C(0xe1e2e3e4e5e6e7e8)

static const uint32_t signal_post_xmm[4] = {
    UINT32_C(0x0f1e2d3c), UINT32_C(0x4b5a6978),
    UINT32_C(0x8796a5b4), UINT32_C(0xc3d2e1f0),
};

static const int signal_greg_indices[SIGNAL_GPR_COUNT] = {
    REG_RAX, REG_RBX, REG_RCX, REG_RDX, REG_RSI, REG_RDI, REG_RBP,
    REG_R8, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15,
};

static volatile sig_atomic_t current_case;
static volatile sig_atomic_t nested_seen;
static volatile sig_atomic_t recovered_cases;
static uintptr_t expected_pc;
static uintptr_t resume_pc;
static uintptr_t altstack_begin;
static uintptr_t altstack_end;

static int on_altstack(const void *address)
{
    uintptr_t value = (uintptr_t)address;
    return value >= altstack_begin && value < altstack_end;
}

static void fail_from_handler(int code)
{
    _Exit(code);
}

static int signal_frame_state_matches(const ucontext_t *context)
{
    for (int index = 0; index < SIGNAL_GPR_COUNT; index++) {
        if (index == SIGNAL_R10) {
            continue;
        }
        if ((uint64_t)context->uc_mcontext.gregs[
                signal_greg_indices[index]] != signal_expected_gprs[index]) {
            return 0;
        }
    }
    if ((uintptr_t)context->uc_mcontext.gregs[REG_RSP] !=
            (uintptr_t)context->uc_mcontext.gregs[REG_R10] ||
        ((uint64_t)context->uc_mcontext.gregs[REG_EFL] &
            SIGNAL_RFLAGS_MASK) != SIGNAL_RFLAGS_MASK ||
        !context->uc_mcontext.fpregs) {
        return 0;
    }
    for (int reg = 0; reg < 16; reg++) {
        for (int word = 0; word < 4; word++) {
            if (context->uc_mcontext.fpregs->_xmm[reg].element[word] !=
                signal_expected_xmm[word]) {
                return 0;
            }
        }
    }
    return sigismember(&context->uc_sigmask, SIGUSR2) == 1 &&
           sigismember(&context->uc_sigmask, SIGUSR1) == 0;
}

static void handle_nested(int signal, siginfo_t *info, void *opaque)
{
    volatile unsigned char stack_byte = 0;
    ucontext_t *context = opaque;

    if (signal != SIGUSR1 || info->si_signo != SIGUSR1 || !context ||
        !on_altstack((const void *)&stack_byte) ||
        sigismember(&context->uc_sigmask, SIGUSR2) != 1 ||
        sigismember(&context->uc_sigmask, SIGFPE) != 1) {
        fail_from_handler(20);
    }
    nested_seen++;
}

static void handle_fault(int signal, siginfo_t *info, void *opaque)
{
    volatile unsigned char stack_byte = 0;
    ucontext_t *context = opaque;
    int expected_signal = current_case == SIGNAL_HELPER ? SIGILL : SIGFPE;

    if (signal != expected_signal || info->si_signo != expected_signal ||
        !context || (uintptr_t)context->uc_mcontext.gregs[REG_RIP] !=
                        expected_pc ||
        !signal_frame_state_matches(context) ||
        !on_altstack((const void *)&stack_byte)) {
        fail_from_handler(21 + current_case);
    }
    if (current_case == SIGNAL_INTERNAL) {
        long tid = syscall(SYS_gettid);
        if (tid <= 0 || syscall(SYS_tgkill, getpid(), tid, SIGUSR1) ||
            nested_seen != 1) {
            fail_from_handler(27);
        }
    }
    context->uc_mcontext.gregs[REG_R15] = (greg_t)SIGNAL_POST_R15;
    context->uc_mcontext.gregs[REG_EFL] =
        (greg_t)(((uint64_t)context->uc_mcontext.gregs[REG_EFL] &
                  ~SIGNAL_RFLAGS_MASK) | SIGNAL_POST_RFLAGS_MASK);
    for (int word = 0; word < 4; word++) {
        context->uc_mcontext.fpregs->_xmm[15].element[word] =
            signal_post_xmm[word];
    }
    context->uc_mcontext.gregs[REG_RIP] = (greg_t)resume_pc;
    recovered_cases++;
}

static int run_case(enum SignalCase test, uintptr_t fault, uintptr_t resume,
                    int (*site)(void))
{
    current_case = test;
    expected_pc = fault;
    resume_pc = resume;
    int result = site();
    if (result) {
        fprintf(stderr, "signal case %d sigreturn state failure %d\n",
                test, result);
        return -1;
    }
    sigset_t mask;
    return sigprocmask(SIG_SETMASK, NULL, &mask) ||
           sigismember(&mask, SIGUSR2) != 1 ||
           sigismember(&mask, SIGFPE) != 0 ||
           sigismember(&mask, SIGILL) != 0 ||
           sigismember(&mask, SIGUSR1) != 0;
}

int main(void)
{
    size_t stack_size = (size_t)SIGSTKSZ * 4;
    void *stack = malloc(stack_size);
    if (!stack) {
        return 2;
    }
    altstack_begin = (uintptr_t)stack;
    altstack_end = altstack_begin + stack_size;
    stack_t alternate = {
        .ss_sp = stack,
        .ss_size = stack_size,
    };
    struct sigaction fault_action = {
        .sa_sigaction = handle_fault,
        .sa_flags = SA_SIGINFO | SA_ONSTACK,
    };
    struct sigaction nested_action = {
        .sa_sigaction = handle_nested,
        .sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER,
    };
    sigset_t blocked;
    sigset_t original_mask;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR2);
    sigemptyset(&fault_action.sa_mask);
    sigemptyset(&nested_action.sa_mask);
    if (sigprocmask(SIG_BLOCK, &blocked, &original_mask) ||
        sigaltstack(&alternate, NULL) ||
        sigaction(SIGFPE, &fault_action, NULL) ||
        sigaction(SIGILL, &fault_action, NULL) ||
        sigaction(SIGUSR1, &nested_action, NULL)) {
        return 3;
    }
    if (run_case(SIGNAL_BOUNDARY, (uintptr_t)signal_boundary_fault,
                 (uintptr_t)signal_boundary_resume, signal_boundary_site) ||
        run_case(SIGNAL_INTERNAL, (uintptr_t)signal_internal_fault,
                 (uintptr_t)signal_internal_resume, signal_internal_site) ||
        run_case(SIGNAL_END, (uintptr_t)signal_end_fault,
                 (uintptr_t)signal_end_resume, signal_end_site) ||
        run_case(SIGNAL_HELPER, (uintptr_t)signal_helper_fault,
                 (uintptr_t)signal_helper_resume, signal_helper_site) ||
        recovered_cases != 4 || nested_seen != 1) {
        return 4;
    }
    stack_t observed;
    if (sigaltstack(NULL, &observed) || observed.ss_sp != alternate.ss_sp ||
        observed.ss_size != alternate.ss_size ||
        (observed.ss_flags & (SS_DISABLE | SS_ONSTACK))) {
        return 5;
    }
    puts("AOT v2 signal recovery boundary=1 internal=1 end=1 helper=1 "
         "gprs=1 rflags=1 vector=1 mask=1 nested=1 altstack=1 sigreturn=1");
    return 0;
}
