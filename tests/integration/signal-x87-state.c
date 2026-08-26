/* SPDX-License-Identifier: GPL-2.0-only */
#define TEST_X87_ENTRY 1
#define TEST_X87_ROUNDING 2
#define TEST_MMX_RESTORE 3
#define TEST_SSE_ENTRY 4
#define TEST_AVX_ENTRY 5

#ifndef TEST_CASE
#error TEST_CASE must select an x87 signal regression
#endif

#define TARGET_SIGUSR1 10
#define TARGET_SA_RESTORER 0x04000000

#ifdef __x86_64__
#define TARGET_NR_RT_SIGACTION 13
#define TARGET_NR_RT_SIGRETURN 15
#define TARGET_NR_GETPID 39
#define TARGET_NR_EXIT 60
#define TARGET_NR_GETTID 186
#define TARGET_NR_TGKILL 234
#define SAVE_FP "fxsave64"
#define SIGNAL_RETURN "mov $15, %rax; syscall"
#else
#define TARGET_NR_RT_SIGACTION 174
#define TARGET_NR_RT_SIGRETURN 173
#define TARGET_NR_GETPID 20
#define TARGET_NR_EXIT 1
#define TARGET_NR_GETTID 224
#define TARGET_NR_TGKILL 270
#define SAVE_FP "fxsave"
#ifdef TEST_LEGACY_SIGNAL
#define SIGNAL_RETURN "pop %eax; mov $119, %eax; int $0x80"
#else
#define SIGNAL_RETURN "mov $173, %eax; int $0x80"
#endif
#endif

#define STRINGIFY_(value) #value
#define STRINGIFY(value) STRINGIFY_(value)

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

struct target_sigaction {
    void (*handler)(int);
    unsigned long flags;
    void (*restorer)(void);
    uint64_t mask;
};

struct fxsave_area {
    uint16_t fcw;
    uint16_t fsw;
    uint8_t ftw;
    uint8_t reserved1;
    uint16_t fop;
    uint64_t fip;
    uint64_t fdp;
    uint32_t mxcsr;
    uint32_t mxcsr_mask;
    uint8_t st_space[8][16];
    uint8_t xmm_space[16][16];
    uint8_t reserved2[96];
} __attribute__((packed, aligned(64)));

/* Written by the signal handler and observed after signal delivery. */
static volatile unsigned long handler_ran;
static struct fxsave_area handler_state __attribute__((aligned(64)));
static const uint64_t fp_input = 0x3ffe666666666666ULL; /* 1.9 */
static const uint64_t x87_value = 0x405ee00000000000ULL; /* 123.5 */
static const uint64_t mmx_value = 0x0123456789abcdefULL;
static const uint64_t vector_value[4] __attribute__((aligned(32))) = {
    0x0123456789abcdefULL, 0xfedcba9876543210ULL,
    0x1122334455667788ULL, 0x8877665544332211ULL,
};
static uint64_t handler_vector[4] __attribute__((aligned(32)));
static uint64_t restored_vector[4] __attribute__((aligned(32)));
static uint32_t handler_mxcsr;

static inline long target_syscall4(long nr, long arg1, long arg2,
                                   long arg3, long arg4)
{
    long ret;

#ifdef __x86_64__
    register long r10 __asm__("r10") = arg4;

    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory");
#else
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4)
        : "memory");
#endif
    return ret;
}

static inline long target_syscall1(long nr, long arg1)
{
    return target_syscall4(nr, arg1, 0, 0, 0);
}

static inline long target_syscall0(long nr)
{
    return target_syscall4(nr, 0, 0, 0, 0);
}

static void __attribute__((naked)) signal_restorer(void)
{
    __asm__ volatile(SIGNAL_RETURN);
}

static void signal_handler(int sig)
{
    (void)sig;

#if TEST_CASE == TEST_SSE_ENTRY || TEST_CASE == TEST_AVX_ENTRY
    /* Compiler-generated FP/vector instructions are disabled for this file. */
    __asm__ volatile("stmxcsr %0" : "=m"(handler_mxcsr));
#if TEST_CASE == TEST_AVX_ENTRY
    __asm__ volatile("vmovdqu %%ymm0, %0" : "=m"(handler_vector));
#else
    __asm__ volatile("movdqu %%xmm0, %0" : "=m"(handler_vector));
#endif
    __asm__ volatile("fninit" : : : "memory");
#elif TEST_CASE == TEST_X87_ENTRY
    /* This must be the first floating-point instruction in the handler. */
    __asm__ volatile(
        SAVE_FP " %0\n\t"
        "fninit"
        : "=m"(handler_state)
        :
        : "memory");
#else
    __asm__ volatile("fninit" : : : "memory");
#endif
    handler_ran++;
}

static long install_handler(void)
{
    struct target_sigaction action;

    action.handler = signal_handler;
    action.flags = TARGET_SA_RESTORER;
#ifndef TEST_LEGACY_SIGNAL
    action.flags |= 4; /* SA_SIGINFO: rt signal frame. */
#endif
    action.restorer = signal_restorer;
    action.mask = 0;
    return target_syscall4(TARGET_NR_RT_SIGACTION, TARGET_SIGUSR1,
                           (long)&action, 0, sizeof(action.mask));
}

static long send_signal(void)
{
    long pid = target_syscall0(TARGET_NR_GETPID);
    long tid = target_syscall0(TARGET_NR_GETTID);

    return target_syscall4(TARGET_NR_TGKILL, pid, tid, TARGET_SIGUSR1, 0);
}

#if TEST_CASE == TEST_X87_ENTRY
static int run_test(void)
{
    uint64_t restored = 0;

    __asm__ volatile(
        "fninit\n\t"
        "fldl %0"
        :
        : "m"(x87_value)
        : "st", "memory");
    if (send_signal() < 0 || handler_ran != 1) {
        return 11;
    }
    __asm__ volatile(
        "fstpl %0\n\t"
        "fninit"
        : "=m"(restored)
        :
        : "st", "memory");

    if (handler_state.fcw != 0x037f || handler_state.fsw != 0 ||
        handler_state.ftw != 0) {
        return 21;
    }
    return restored == x87_value ? 0 : 22;
}
#elif TEST_CASE == TEST_X87_ROUNDING
static int run_test(void)
{
    uint16_t control = 0x077f;
    uint16_t restored_control = 0;
    int rounded = 0;

    __asm__ volatile(
        "fninit\n\t"
        "fldcw %0"
        :
        : "m"(control)
        : "memory");
    if (send_signal() < 0 || handler_ran != 1) {
        return 11;
    }
    __asm__ volatile(
        "fnstcw %0\n\t"
        "fldl %2\n\t"
        "fistpl %1\n\t"
        "fninit"
        : "=m"(restored_control), "=m"(rounded)
        : "m"(fp_input)
        : "st", "memory");

    if (restored_control != control) {
        return 31;
    }
    return rounded == 1 ? 0 : 32;
}
#elif TEST_CASE == TEST_MMX_RESTORE
static int run_test(void)
{
    long pid = target_syscall0(TARGET_NR_GETPID);
    long tid = target_syscall0(TARGET_NR_GETTID);
    uint64_t restored = 0;

    __asm__ volatile(
        "movq %0, %%mm0"
        :
        : "m"(mmx_value)
        : "mm0", "memory");
    if (target_syscall4(TARGET_NR_TGKILL, pid, tid, TARGET_SIGUSR1, 0) < 0 ||
        handler_ran != 1) {
        return 11;
    }
    __asm__ volatile(
        "movq %%mm0, %0\n\t"
        "emms"
        : "=m"(restored)
        :
        : "mm0", "memory");

    return restored == mmx_value ? 0 : 41;
}
#elif TEST_CASE == TEST_SSE_ENTRY || TEST_CASE == TEST_AVX_ENTRY
static int run_test(void)
{
    uint32_t mxcsr = 0x3f80;
    uint32_t restored_mxcsr;
    int i;

#if TEST_CASE == TEST_AVX_ENTRY
    unsigned int a, b, c, d;

    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(1), "c"(0));
    if ((c & 0x18000000) != 0x18000000) {
        return 77;
    }
    __asm__ volatile("xgetbv" : "=a"(a), "=d"(d) : "c"(0));
    if ((a & 6) != 6) {
        return 77;
    }
#endif
    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
#if TEST_CASE == TEST_AVX_ENTRY
    __asm__ volatile("vmovdqu %0, %%ymm0" : : "m"(vector_value));
#else
    __asm__ volatile("movdqu %0, %%xmm0" : : "m"(vector_value));
#endif
    if (send_signal() < 0 || handler_ran != 1) {
        return 11;
    }
    __asm__ volatile("stmxcsr %0" : "=m"(restored_mxcsr));
#if TEST_CASE == TEST_AVX_ENTRY
    __asm__ volatile("vmovdqu %%ymm0, %0" : "=m"(restored_vector));
#else
    __asm__ volatile("movdqu %%xmm0, %0" : "=m"(restored_vector));
#endif
    if (handler_mxcsr != 0x1f80) {
        return 51;
    }
    if (restored_mxcsr != mxcsr) {
        return 52;
    }
    for (i = 0; i < (TEST_CASE == TEST_AVX_ENTRY ? 4 : 2); i++) {
        if (handler_vector[i]) {
            return 53;
        }
        if (restored_vector[i] != vector_value[i]) {
            return 54;
        }
    }
    return 0;
}
#else
#error unknown TEST_CASE
#endif

void _start(void)
{
    int ret;

    if (install_handler() < 0) {
        ret = 10;
    } else {
        ret = run_test();
    }
    target_syscall1(TARGET_NR_EXIT, ret);
    for (;;) {
        __asm__ volatile("");
    }
}
