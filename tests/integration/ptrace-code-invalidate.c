/* SPDX-License-Identifier: GPL-2.0-only */
/* Real ptrace writes must invalidate already translated guest code. */
typedef unsigned long ulong;
static long call6(long n, long a, long b, long c, long d, long e, long f)
{
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    long ret;
    __asm__ volatile("syscall" : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c),
                       "r"(r10), "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");
    return ret;
}
#define SC(n,a,b,c,d) call6(n,(long)(a),(long)(b),(long)(c),(long)(d),0,0)
static void quit(int status)
{
    SC(60, status, 0, 0, 0);
    __builtin_unreachable();
}
static int check(unsigned char *code, int multiple, int detach)
{
    long pid = SC(57, 0, 0, 0, 0);
    int status;
    if (pid < 0) {
        return 10;
    }
    if (!pid) {
        int (*first)(void) = (void *)code;
        int (*second)(void) = (void *)(code + 65536);
        long ret;
        if (first() != 100 || second() != 101) {
            quit(11);
        }
        ret = SC(101, 0, 0, 0, 0); /* PTRACE_TRACEME */
        if (ret < 0) {
            quit(ret == -1 ? 77 : 12);
        }
        SC(62, SC(39, 0, 0, 0, 0), 19, 0, 0); /* SIGSTOP */
        quit(first() != 200 || second() != (multiple ? 201 : 101) ? 13 : 0);
    }
    if (SC(61, pid, &status, 0, 0) != pid) {
        return 14;
    }
    if (status == (77 << 8)) {
        return 77;
    }
    if ((status & 255) != 127 || (status >> 8) != 19) {
        return 15;
    }
    for (int i = 0; i < (multiple ? 2 : 1); i++) {
        ulong value = 0;
        unsigned char *address = code + i * 65536;
        /* The raw x86 ptrace syscall returns PEEK data through argument four. */
        if (SC(101, 2, pid, address, &value) < 0) {
            return 16;
        }
        value = (value & ~0xffffffff00UL) | ((ulong)(200 + i) << 8);
        if (SC(101, 5, pid, address, value) < 0) {
            return 17;
        }
    }
    if (SC(101, detach ? 17 : 7, pid, 0, 0) < 0) {
        return 18;
    }
    for (;;) {
        if (SC(61, pid, &status, 0, 0) != pid) {
            return 19;
        }
        if ((status & 255) != 127) {
            return status == 0 ? 0 : 20;
        }
        /* Deliver LATU's queued code-change signal, rather than suppress it. */
        if (SC(101, 7, pid, 0, status >> 8) < 0) {
            return 21;
        }
    }
}
static int run(void)
{
    long mapping = call6(9, 0, 131072, 3, 0x22, -1, 0);
    unsigned char *code;
    if ((ulong)mapping >= (ulong)-4095) {
        return 2;
    }
    code = (void *)mapping;
    for (int i = 0; i < 2; i++) {
        unsigned char *p = code + i * 65536;
        p[0] = 0xb8;
        p[1] = 100 + i;
        p[2] = p[3] = p[4] = 0;
        p[5] = 0xc3;
    }
    if (SC(10, code, 131072, 5, 0)) {
        return 3;
    }
    for (int multiple = 0; multiple < 2; multiple++) {
        for (int detach = 0; detach < 2; detach++) {
            int ret = check(code, multiple, detach);
            if (ret) {
                return ret;
            }
        }
    }
    return 0;
}
void _start(void)
{
    quit(run());
}
