/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Exercise glibc closefrom's close-and-rewind procfs fallback. */
#if defined(__x86_64__)
#define NR_open 2
#define NR_close 3
#define NR_lseek 8
#define NR_readlink 89
#define NR_fcntl 72
#define NR_getdents 78
#define NR_getdents64 217
#define NR_exit 231
#define NR_fork 57
#else
#define NR_open 5
#define NR_close 6
#define NR_lseek 19
#define NR_readlink 85
#define NR_fcntl 55
#define NR_getdents 141
#define NR_getdents64 220
#define NR_exit 252
#define NR_fork 2
#endif

static long call3(long nr, long a, long b, long c)
{
    long ret;
#if defined(__x86_64__)
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(a), "S"(b),
                     "d"(c) : "rcx", "r11", "memory");
#else
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a), "c"(b),
                     "d"(c) : "memory");
#endif
    return ret;
}

static long wait_child(long pid, int *status)
{
#if defined(__x86_64__)
    register long r10 __asm__("r10") = 0;
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(61), "D"(pid),
                     "S"((long)status), "d"(0), "r"(r10)
                     : "rcx", "r11", "memory");
    return ret;
#else
    return call3(7, pid, (long)status, 0);
#endif
}

static long open_dir(const char *path)
{
    return call3(NR_open, (long)path, 00200000, 0);
}

static int number(const char *s)
{
    int n = 0;
    if (!*s) {
        return -1;
    }
    for (; *s; s++) {
        if (*s < '0' || *s > '9' || n > 10000000) {
            return -1;
        }
        n = n * 10 + *s - '0';
    }
    return n;
}

__attribute__((used)) int test_main(long *stack)
{
    char **argv = (char **)(stack + 1);
    unsigned char buf[1024];
    char before[4096], after[4096];
    long nr, size, dir, payload, n, before_len, after_len;
    int name_off, reclen_off, rounds = 0, found = 0;
    int ordinary, expected = 4;

    if (stack[0] != 5) {
        return 10;
    }
    nr = argv[1][0] == '6' ? NR_getdents64 : NR_getdents;
    name_off = nr == NR_getdents64 ? 19 : 2 * sizeof(long) + 2;
    reclen_off = nr == NR_getdents64 ? 16 : 2 * sizeof(long);
    size = argv[2][0] == 's' ? 24 : sizeof(buf);
    ordinary = argv[4][0] == 'd';
    before_len = call3(NR_readlink, (long)"/proc/self/exe",
                       (long)before, sizeof(before));
    payload = call3(NR_open, (long)"/dev/null", 0, 0);
    dir = open_dir(argv[3]);
    if (dir < 0 || payload < 0 || before_len <= 0) {
        return 11;
    }
    if (argv[4][0] == 'p') {
        char path[] = "/proc/self/fd/00";
        long pid;
        int status;

        expected = -1;
        for (int fd = 3; fd < 64; fd++) {
            long len, i;
            if (fd < 10) {
                path[14] = '0' + fd;
                path[15] = 0;
            } else {
                path[14] = '0' + fd / 10;
                path[15] = '0' + fd % 10;
                path[16] = 0;
            }
            len = call3(NR_readlink, (long)path, (long)after, sizeof(after));
            if (len != before_len) {
                continue;
            }
            i = 0;
            while (i < len && before[i] == after[i]) {
                i++;
            }
            if (i == len) {
                expected = fd;
                break;
            }
        }
        if (expected < 0) {
            return 21;
        }
        pid = call3(NR_fork, 0, 0, 0);
        if (pid < 0) {
            return 22;
        }
        if (pid > 0) {
            return wait_child(pid, &status) == pid && status == 0 ? 0 : 23;
        }
        /*
         * The inherited directory still names our parent's fd table.
         * Our private descriptor number must not be hidden from that table.
         */
        ordinary = 1;
    }
    if (argv[4][0] == 'e') {
        if (call3(nr, -1, (long)buf, sizeof(buf)) != -9 ||
            call3(nr, dir, (long)buf, 1) != -22 ||
            call3(nr, dir, 1, sizeof(buf)) != -14) {
            return 20;
        }
        return 0;
    }
    while ((n = call3(nr, dir, (long)buf, size)) > 0) {
        long offset = 0;
        int closed = 0;
        if (++rounds > 128) {
            /* Deterministic failure rather than an unbounded test hang. */
            return 12;
        }
        while (offset < n) {
            unsigned char *de = buf + offset;
            unsigned reclen = de[reclen_off] | de[reclen_off + 1] << 8;
            int fd;
            if (reclen <= name_off || reclen > n - offset) {
                return 13;
            }
            fd = number((const char *)de + name_off);
            if (ordinary) {
                found |= fd == expected;
            } else if (fd >= 3 && fd != dir) {
                call3(NR_close, fd, 0, 0);
                closed = 1;
            }
            offset += reclen;
        }
        /* glibc __closefrom_fallback restarts after closing any fd. */
        if (closed && call3(NR_lseek, dir, 0, 0) < 0) {
            return 14;
        }
    }
    call3(NR_close, dir, 0, 0);
    if (n < 0) {
        return 15;
    }
    if (ordinary) {
        return found ? 0 : 16;
    }
    if (call3(NR_fcntl, payload, 1, 0) != -9) {
        return 17;
    }
    after_len = call3(NR_readlink, (long)"/proc/self/exe",
                      (long)after, sizeof(after));
    if (after_len != before_len) {
        return 18;
    }
    for (long i = 0; i < before_len; i++) {
        if (before[i] != after[i]) {
            return 19;
        }
    }
    return 0;
}

#if defined(__x86_64__)
__asm__(".text\n.global _start\n_start:\nmov %rsp,%rdi\nandq $-16,%rsp\n"
        "call test_main\nmov %eax,%edi\nmov $231,%eax\nsyscall\nud2\n"
        ".section .note.GNU-stack,\"\",@progbits\n");
#else
__asm__(".text\n.global _start\n_start:\nmov %esp,%eax\nandl $-16,%esp\n"
        "subl $12,%esp\npush %eax\ncall test_main\nmov %eax,%ebx\n"
        "mov $252,%eax\nint $0x80\nud2\n"
        ".section .note.GNU-stack,\"\",@progbits\n");
#endif
