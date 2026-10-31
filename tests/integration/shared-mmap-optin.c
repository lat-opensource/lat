/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Freestanding x86 guests: keep the mapping test independent of a guest libc. */
typedef unsigned long ulong;
#ifdef __i386__
#define NR_exit 1
#define NR_fork 2
#define NR_read 3
#define NR_write 4
#define NR_close 6
#define NR_openat 295
#define NR_wait 7
#define NR_dup2 63
#define NR_munmap 91
#define NR_truncate 93
#define NR_mprotect 125
#define NR_msync 144
#define NR_pread 180
#define NR_pwrite 181
#define NR_memfd 356
static long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long bp __asm__("ebp") = f;
    long r;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e),
                       "r"(bp) : "memory");
    return r;
}
static long map(ulong addr, ulong len, int prot, int flags, int fd, ulong off)
{
    ulong a[] = {addr, len, prot, flags, fd, off};
    return sc(90, (long)a, 0, 0, 0, 0, 0);
}
__asm__(".global _start\n_start:\nxor %ebp,%ebp\nmov %esp,%eax\n"
        "and $-16,%esp\nsub $12,%esp\npush %eax\ncall test_start\n");
#else
#define NR_exit 60
#define NR_fork 57
#define NR_read 0
#define NR_write 1
#define NR_close 3
#define NR_openat 257
#define NR_wait 61
#define NR_dup2 33
#define NR_munmap 11
#define NR_truncate 77
#define NR_mprotect 10
#define NR_msync 26
#define NR_pread 17
#define NR_pwrite 18
#define NR_memfd 319
static long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    long r;
    __asm__ volatile("syscall" : "=a"(r)
                     : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10),
                       "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return r;
}
static long map(ulong addr, ulong len, int prot, int flags, int fd, ulong off)
{
    return sc(9, addr, len, prot, flags, fd, off);
}
__asm__(".global _start\n_start:\nxor %ebp,%ebp\nmov %rsp,%rdi\n"
        "and $-16,%rsp\ncall test_start\n");
#endif

#define BASE 0x10070000UL
#define PAGE 4096UL
#define VALUE 0x12345678U
#define CHECK(x, code) do { if (!(x)) return (code); } while (0)

static int run(char kind)
{
    ulong address = BASE + PAGE, length = 6 * PAGE, offset = 0;
    int fd = sc(NR_memfd, (long)"shared-mmap-optin", 0, 0, 0, 0, 0);
    unsigned value = 0;
    long p;

    CHECK(fd >= 0 && sc(NR_truncate, fd, 16 * PAGE, 0, 0, 0, 0) == 0, 2);
    if (kind == 'b') {
        offset = PAGE; /* Fresh, congruent fragments exercise metadata setup. */
    } else if (kind == 'c') {
        address = BASE;
        length = 8 * PAGE;
    }
#ifndef __i386__
    /* The existing x86_64 misaligned path expects a reserved guest VMA. */
    if (kind != 'b' && kind != 'c') {
        CHECK(map(BASE, 8 * PAGE, 3, 0x32, -1, 0) == BASE, 3);
    }
#endif
    if (kind == 'd') {
        CHECK(sc(NR_dup2, fd, 0, 0, 0, 0, 0) == 0, 4);
        sc(NR_close, fd, 0, 0, 0, 0, 0);
        fd = 0;
    }
    p = map(address, length, kind == 'e' ? 7 : 3, 0x11, fd, offset);
    CHECK((ulong)p == address, 5);
    *(volatile unsigned *)p = VALUE;
    CHECK(sc(NR_msync, p, length, 4, 0, 0, 0) == 0, 6);
    CHECK(sc(NR_pread, fd, (long)&value, sizeof(value), offset, 0, 0) ==
          sizeof(value), 7);
    CHECK(value == VALUE, 1);
    *(volatile unsigned *)(p + length - 4) = VALUE + 1;
    CHECK(sc(NR_pread, fd, (long)&value, sizeof(value),
             offset + length - 4, 0, 0) == sizeof(value) && value == VALUE + 1, 8);

    if (kind == 'd') {
        CHECK(map(p + PAGE, 2 * PAGE, 3, 0x11, fd, 2 * PAGE) == p + PAGE, 15);
        *(volatile unsigned *)(p + 2 * PAGE) = VALUE + 3;
        CHECK(sc(NR_pread, fd, (long)&value, sizeof(value), 3 * PAGE, 0, 0) ==
              sizeof(value) && value == VALUE + 3, 16);
    }
    if (kind == 'e') {
        volatile unsigned char *code = (void *)p;
        code[0] = 0xb8; /* mov eax, 42; ret */
        *(volatile unsigned *)(p + 1) = 42;
        code[5] = 0xc3;
        CHECK(((int (*)(void))p)() == 42, 9);
    } else if (kind == 'f') {
        int status = 0;
        long child = sc(NR_fork, 0, 0, 0, 0, 0, 0);
        CHECK(child >= 0, 10);
        if (!child) {
            *(volatile unsigned *)p = VALUE + 2;
            sc(NR_exit, 0, 0, 0, 0, 0, 0);
            __builtin_unreachable();
        }
        CHECK(sc(NR_wait, child, (long)&status, 0, 0, 0, 0) == child && !status, 11);
        CHECK(*(volatile unsigned *)p == VALUE + 2, 12);
        CHECK(sc(NR_pread, fd, (long)&value, sizeof(value), offset, 0, 0) ==
              sizeof(value) && value == VALUE + 2, 13);
    }
    CHECK(sc(NR_munmap, p, length, 0, 0, 0, 0) == 0, 14);
    sc(NR_close, fd, 0, 0, 0, 0, 0);
    return 0;
}

__attribute__((noreturn, used)) void test_start(ulong *stack)
{
    char kind = stack[0] > 1 ? ((char *)stack[2])[0] : 'a';
    int ret = kind >= 'a' && kind <= 'f' ? run(kind) : 99;
    sc(NR_write, 1, (long)(ret ? "FAIL\n" : "PASS\n"), 5, 0, 0, 0);
    sc(NR_exit, ret, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}
