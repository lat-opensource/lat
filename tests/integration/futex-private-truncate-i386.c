/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Freestanding i386 regression for private futexes on truncated file maps. */
typedef unsigned int u32;

extern int syscall6(int nr, u32 a, u32 b, u32 c, u32 d, u32 e, u32 f);

__asm__(
    ".text\n.globl syscall6\nsyscall6:\n"
    "push %ebp; push %edi; push %esi; push %ebx;\n"
    "mov 20(%esp), %eax; mov 24(%esp), %ebx;\n"
    "mov 28(%esp), %ecx; mov 32(%esp), %edx;\n"
    "mov 36(%esp), %esi; mov 40(%esp), %edi;\n"
    "mov 44(%esp), %ebp; int $0x80;\n"
    "pop %ebx; pop %esi; pop %edi; pop %ebp; ret;\n");

#define PTR(p) ((u32)(p))

#define NR_OPEN          5
#define NR_CLOSE         6
#define NR_UNLINK        10
#define NR_MUNMAP        91
#define NR_FTRUNCATE     93
#define NR_MMAP2         192
#define NR_EXIT          1
#define NR_FUTEX         240
#define NR_FUTEX_TIME64  422

#define O_CREAT  0100
#define O_EXCL   0200
#define O_RDWR   2

#define PROT_READ    1
#define PROT_WRITE   2
#define MAP_PRIVATE  2

#define FUTEX_WAIT_PRIVATE         128
#define FUTEX_WAIT_BITSET_PRIVATE  137
#define EFAULT 14

void _start(void)
{
    static const char path[] = "futex-truncate.data";
    static const unsigned long long zero[2];
    static const int syscalls[] = { NR_FUTEX, NR_FUTEX_TIME64 };
    static const int ops[] = { FUTEX_WAIT_PRIVATE,
                               FUTEX_WAIT_BITSET_PRIVATE };
    const u32 length = 65536;
    int failed = 0;
    int fd;
    int ret;
    u32 address;
    u32 i;
    u32 j;

    fd = syscall6(NR_OPEN, PTR(path), O_CREAT | O_EXCL | O_RDWR, 0600,
                  0, 0, 0);
    if (fd < 0) {
        failed = 1;
        goto out;
    }
    if (syscall6(NR_UNLINK, PTR(path), 0, 0, 0, 0, 0) ||
        syscall6(NR_FTRUNCATE, fd, length, 0, 0, 0, 0)) {
        failed = 1;
        goto close;
    }
    address = syscall6(NR_MMAP2, 0, length, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE, fd, 0);
    if (address >= (u32)-4095 ||
        syscall6(NR_FTRUNCATE, fd, 0, 0, 0, 0, 0)) {
        failed = 1;
        goto close;
    }

    /* Only the kernel accesses address: it must reject the truncated page. */
    for (i = 0; i < sizeof(syscalls) / sizeof(syscalls[0]); i++) {
        for (j = 0; j < sizeof(ops) / sizeof(ops[0]); j++) {
            ret = syscall6(syscalls[i], address, ops[j], 0, PTR(zero), 0,
                           0xffffffff);
            failed |= ret != -EFAULT;
        }
    }
    syscall6(NR_MUNMAP, address, length, 0, 0, 0, 0);

close:
    syscall6(NR_CLOSE, fd, 0, 0, 0, 0, 0);
out:
    syscall6(NR_EXIT, failed, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}
