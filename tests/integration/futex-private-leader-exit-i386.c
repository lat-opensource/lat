/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Freestanding i386 guest: the leader exits before either worker starts. */
typedef unsigned int u32;

int syscall6(int nr, u32 a, u32 b, u32 c, u32 d, u32 e, u32 f);
int start_thread(void (*entry)(void), void *stack);

__asm__(
    ".text\n.globl syscall6\nsyscall6:\n"
    "push %ebp; push %edi; push %esi; push %ebx;\n"
    "mov 20(%esp), %eax; mov 24(%esp), %ebx;\n"
    "mov 28(%esp), %ecx; mov 32(%esp), %edx;\n"
    "mov 36(%esp), %esi; mov 40(%esp), %edi;\n"
    "mov 44(%esp), %ebp; int $0x80;\n"
    "pop %ebx; pop %esi; pop %edi; pop %ebp; ret;\n"
    ".globl start_thread\nstart_thread:\n"
    "push %ebp; push %edi; push %esi; push %ebx;\n"
    "mov 20(%esp), %eax; mov 24(%esp), %ecx;\n"
    "sub $16, %ecx; mov %eax, (%ecx);\n"
    "mov $0x50f00, %ebx; xor %edx, %edx;\n"
    "xor %esi, %esi; xor %edi, %edi; mov $120, %eax; int $0x80;\n"
    "test %eax, %eax; jz 1f;\n"
    "pop %ebx; pop %esi; pop %edi; pop %ebp; ret;\n"
    "1: call *(%esp); ud2;\n");

#define PTR(p) ((u32)(p))

static unsigned char stacks[2][65536] __attribute__((aligned(16)));
static unsigned char *page;
static u32 started;
static u32 done;
static u32 calls;

static void finish(int status)
{
    syscall6(252, status, 0, 0, 0, 0, 0); /* exit_group */
    __builtin_unreachable();
}

static void mover(void)
{
    char byte;
    u32 i;

    /* The host runner sends this only after the leader becomes a zombie. */
    if (syscall6(3, 0, PTR(&byte), 1, 0, 0, 0) != 1) {
        finish(11);
    }
    __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
    for (i = 0; i < 256; i++) {
        u32 low = 0, high = 0;
        u32 completed;

        /* Cross an eight-byte boundary, on the futex's host page. */
        __asm__ volatile("lock cmpxchg8b %0"
                         : "+m"(*(unsigned long long *)(page + 129)),
                           "+a"(low), "+d"(high)
                         : "b"(0), "c"(0) : "cc", "memory");
        /* Let a waiter finish recovery before taking mmap_lock again. */
        completed = __atomic_load_n(&calls, __ATOMIC_ACQUIRE);
        while (__atomic_load_n(&calls, __ATOMIC_ACQUIRE) == completed) {
            syscall6(158, 0, 0, 0, 0, 0, 0);
        }
    }
    __atomic_store_n(&done, 1, __ATOMIC_RELEASE);
    syscall6(1, 0, 0, 0, 0, 0, 0); /* exit only this thread */
    __builtin_unreachable();
}

static void waiter(void)
{
    static const unsigned long long zero[2];
    static const int numbers[] = {240, 240, 422, 422};
    static const int ops[] = {128, 137, 128, 137};
    static const char pass[] =
        "PASS: all four private waits retried after leader exit\n";
    u32 which = 0;

    while (!__atomic_load_n(&started, __ATOMIC_ACQUIRE)) {
        syscall6(158, 0, 0, 0, 0, 0, 0); /* sched_yield */
    }
    do {
        int ret = syscall6(numbers[which], PTR(page + 64), ops[which], 0,
                           PTR(zero), 0, 0xffffffff);

        __atomic_add_fetch(&calls, 1, __ATOMIC_RELEASE);
        if (ret == -11) { /* EAGAIN: the unchanged zero word cannot mismatch. */
            if (++which == 4) {
                syscall6(4, 1, PTR(pass), sizeof(pass) - 1, 0, 0, 0);
                finish(0);
            }
        } else if (ret == -14) {
            finish(20); /* transient EFAULT escaped to the guest */
        } else if (ret != -110) { /* ETIMEDOUT on an accessible zero word */
            finish(21);
        }
    } while (!__atomic_load_n(&done, __ATOMIC_ACQUIRE));

    finish(22); /* Fail closed if any recovery path was not exercised. */
}

void _start(void)
{
    static const char ready[] = "READY\n";
    u32 addr = syscall6(192, 0, 65536, 3, 0x22, -1, 0); /* mmap2 */

    if (addr >= (u32)-4095) {
        finish(10);
    }
    page = (unsigned char *)addr;
    /* The preload shim widens only moves of this tagged guest page. */
    ((u32 *)page)[0] = 0x46555458;
    ((u32 *)page)[1] = 0x4c454144;
    if (start_thread(mover, stacks[0] + sizeof(stacks[0])) < 0 ||
        start_thread(waiter, stacks[1] + sizeof(stacks[1])) < 0) {
        finish(12);
    }
    syscall6(4, 1, PTR(ready), sizeof(ready) - 1, 0, 0, 0);
    syscall6(1, 0, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}
