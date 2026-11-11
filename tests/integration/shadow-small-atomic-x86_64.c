/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Exercise AMADD/AMSWAP on a shared 4K fragment of a 16K host page. */
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_lseek 8
#define SYS_mmap 9
#define SYS_mprotect 10
#define SYS_pread64 17
#define SYS_fork 57
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_ftruncate 77
#define SYS_unlink 87
static long syscall6(long n, long a, long b, long c, long d, long e, long f)
{
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    long ret;
    __asm__ volatile("syscall" : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10),
                       "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return ret;
}
#define SC(n, a, b, c) \
    syscall6(SYS_##n, (long)(a), (long)(b), (long)(c), 0, 0, 0)
#define CHECK(x, n) do { if (!(x)) { return n; } } while (0)
#define FLAGS "pushfq; popq %0"
#define PREP "cmpl $1,%3; stc; "
static int operations(unsigned char *p, int half)
{
    unsigned long flags;
    unsigned int seed = 0;
    unsigned int x;
#define TEST(T, S) do { \
    *(T *)p = (T)0x7e; \
    __asm__ volatile(PREP "lock add" S " $1,%1; " FLAGS \
                     : "=r"(flags), "+m"(*(T *)p) : "m"(*p), "r"(seed) \
                     : "cc", "memory"); \
    CHECK(*(T *)p == 0x7f && (flags & 0x8d5) == 0, 20); \
    __asm__ volatile("lock sub" S " $1,%0" \
                     : "+m"(*(T *)p) : : "cc", "memory"); \
    CHECK(*(T *)p == 0x7e, 21); \
    __asm__ volatile("lock inc" S " %0" \
                     : "+m"(*(T *)p) : : "cc", "memory"); \
    CHECK(*(T *)p == 0x7f, 22); \
    __asm__ volatile("lock dec" S " %0" \
                     : "+m"(*(T *)p) : : "cc", "memory"); \
    CHECK(*(T *)p == 0x7e, 23); \
    T v = 2; \
    __asm__ volatile("lock xadd" S " %0,%1" \
                     : "+q"(v), "+m"(*(T *)p) : : "cc", "memory"); \
    CHECK(v == 0x7e && *(T *)p == 0x80, 24); \
    v = 0x31; \
    __asm__ volatile("xchg" S " %0,%1" \
                     : "+q"(v), "+m"(*(T *)p) : : "memory"); \
    CHECK(v == 0x80 && *(T *)p == 0x31, 25); \
    *(T *)p = (T)-1; \
    v = 1; \
    __asm__ volatile("lock xadd" S " %1,%2; " FLAGS \
                     : "=r"(flags), "+q"(v), "+m"(*(T *)p) \
                     : : "cc", "memory"); \
    CHECK(v == (T)-1 && *(T *)p == 0 && (flags & 0x8d5) == 0x55, 27); \
    v = 0x31; \
    __asm__ volatile("xchg" S " %0,%1" \
                     : "+q"(v), "+m"(*(T *)p) : : "memory"); \
    CHECK(v == 0 && *(T *)p == 0x31, 28); \
    x = sizeof(T); \
} while (0)
    if (half) {
        TEST(unsigned short, "w");
    } else {
        TEST(unsigned char, "b");
    }
    CHECK(p[-1] == 0x5a && p[x] == 0x5a, 26);
    return 0;
}
static int contention(unsigned char *p, int half)
{
    long children[4];
    unsigned long seen[256] = {0};
    /* A byte counter's returned old values must each occur exactly once. */
    volatile unsigned char *ready = p + 900;
    /* Volatile polling observes updates from the forked processes. */
    volatile unsigned char *start = p + 904;
    *start = 0;
    for (int i = 0; i < 4; i++) {
        ready[i] = 0;
    }
    *p = 0;
    if (half) {
        *(unsigned short *)p = 0;
    }
    for (int i = 0; i < 4; i++) {
        children[i] = SC(fork, 0, 0, 0);
        CHECK(children[i] >= 0, 30);
        if (!children[i]) {
            ready[i] = 1;
            while (!*start) {
                __asm__ volatile("pause" : : : "memory");
            }
            for (int k = 0; k < 64; k++) {
                unsigned int old;
                if (half) {
                    unsigned short v = 1;
                    __asm__ volatile("lock xaddw %0,%1"
                                     : "+r"(v), "+m"(*(unsigned short *)p)
                                     : : "cc", "memory");
                    old = v;
                } else {
                    unsigned char v = 1;
                    __asm__ volatile("lock xaddb %0,%1"
                                     : "+q"(v), "+m"(*p) : : "cc", "memory");
                    old = v;
                }
                /* Each child has a distinct result area in the same file. */
                ((unsigned short *)(p + 128))[i * 64 + k] = old;
            }
            SC(exit, 0, 0, 0);
            __builtin_unreachable();
        }
    }
    for (int i = 0; i < 4; i++) {
        while (!ready[i]) {
            __asm__ volatile("pause" : : : "memory");
        }
    }
    *start = 1;
    for (int i = 0; i < 4; i++) {
        int status = -1;
        CHECK(SC(wait4, children[i], &status, 0) == children[i] && !status, 31);
    }
    CHECK(half ? *(unsigned short *)p == 256 : *p == 0, 32);
    for (int i = 0; i < 256; i++) {
        unsigned int old = ((unsigned short *)(p + 128))[i];
        CHECK(old < 256 && !seen[old], 33);
        seen[old] = 1;
    }
    return 0;
}
static int denied_access(unsigned char *p, int crossing)
{
    unsigned char before = p[4095];
    long child = SC(fork, 0, 0, 0);
    CHECK(child >= 0, 40);
    if (!child) {
        long page = (long)p + (crossing ? 4096 : 0);
        if (SC(mprotect, page, 4096, crossing ? 0 : 1)) {
            SC(exit, 41, 0, 0);
        }
        unsigned short *q = (void *)(p + (crossing ? 4095 : 18));
        __asm__ volatile("lock addw $1,%0" : "+m"(*q) : : "cc", "memory");
        SC(exit, 42, 0, 0);
        __builtin_unreachable();
    }
    int status = -1;
    CHECK(SC(wait4, child, &status, 0) == child, 43);
    CHECK((status & 0x7f) == 11, 44);
    CHECK(p[4095] == before, 45);
    return 0;
}
__attribute__((used)) int test_main(long *sp)
{
    CHECK(sp[0] == 2, 9);
    const char *path = (const char *)sp[2];
    long fd = SC(open, path, 0102, 0600);
    CHECK(fd >= 0 && SC(ftruncate, fd, 16384, 0) == 0, 10);
    long base = syscall6(SYS_mmap, 0, 65536, 3, 0x22, -1, 0);
    CHECK(base > 0, 11);
    long addr = ((base + 16383) & ~16383L) + 4096;
    long mapped = syscall6(SYS_mmap, addr, 4096, 3, 0x11, fd, 0);
    CHECK(mapped == addr, 12);
    unsigned char *p = (void *)addr;
    int end_offset = 23;
    for (int h = 0; h < 2; h++) {
        for (int off = 16; off < end_offset; off += h ? 2 : 1) {
            for (int j = 0; j < 40; j++) {
                p[j] = 0x5a;
            }
            int ret = operations(p + off, h);
            if (ret) {
                return ret;
            }
            unsigned char data[40];
            CHECK(syscall6(SYS_pread64, fd, (long)data, 40, 0, 0, 0) == 40, 13);
            for (int j = 0; j < 40; j++) {
                CHECK(data[j] == p[j], 14);
            }
        }
        int ret = contention(p + 32, h);
        if (ret) {
            return ret;
        }
    }
    for (int crossing = 0; crossing < 1; crossing++) {
        int ret = denied_access(p, crossing);
        if (ret) {
            return ret;
        }
    }
    SC(close, fd, 0, 0);
    SC(unlink, path, 0, 0);
    return 0;
}
__asm__(".global _start\n"
        "_start:\n"
        "mov %rsp,%rdi\n"
        "and $-16,%rsp\n"
        "call test_main\n"
        "mov %eax,%edi\n"
        "mov $60,%eax\n"
        "syscall\n");
