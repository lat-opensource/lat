/* SPDX-License-Identifier: GPL-2.0-or-later */
typedef unsigned long guest_ulong_t;
typedef long guest_slong_t;

#define __NR_close          6
#define __NR_munmap        91
#define __NR_ftruncate     93
#define __NR_mprotect     125
#define __NR_msync        144
#define __NR_mremap       163
#define __NR_mmap2        192
#define __NR_exit_group   252
#define __NR_memfd_create 356

#define EFAULT 14

#define PROT_READ  1
#define PROT_WRITE 2

#define MAP_SHARED    1
#define MAP_PRIVATE   2
#define MAP_FIXED    16
#define MAP_ANONYMOUS 32

#define MREMAP_MAYMOVE 1
#define MREMAP_FIXED   2
#define MS_SYNC         4

static inline guest_slong_t syscall1(guest_slong_t nr, guest_slong_t a1)
{
    guest_slong_t ret;

    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a1)
                     : "memory");
    return ret;
}

static inline guest_slong_t syscall2(guest_slong_t nr, guest_slong_t a1,
                                     guest_slong_t a2)
{
    guest_slong_t ret;

    __asm__ volatile("int $0x80" : "=a"(ret)
                     : "a"(nr), "b"(a1), "c"(a2) : "memory");
    return ret;
}

static inline guest_slong_t syscall3(guest_slong_t nr, guest_slong_t a1,
                                     guest_slong_t a2, guest_slong_t a3)
{
    guest_slong_t ret;

    __asm__ volatile("int $0x80" : "=a"(ret)
                     : "a"(nr), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

static inline guest_slong_t syscall5(guest_slong_t nr, guest_slong_t a1,
                                     guest_slong_t a2, guest_slong_t a3,
                                     guest_slong_t a4, guest_slong_t a5)
{
    guest_slong_t ret;

    __asm__ volatile("int $0x80" : "=a"(ret)
                     : "a"(nr), "b"(a1), "c"(a2), "d"(a3), "S"(a4),
                       "D"(a5) : "memory");
    return ret;
}

static inline guest_slong_t syscall6(guest_slong_t nr, guest_slong_t a1,
                                     guest_slong_t a2, guest_slong_t a3,
                                     guest_slong_t a4, guest_slong_t a5,
                                     guest_slong_t a6)
{
    register guest_slong_t ebp __asm__("ebp") = a6;
    guest_slong_t ret;

    __asm__ volatile("int $0x80" : "=a"(ret)
                     : "a"(nr), "b"(a1), "c"(a2), "d"(a3), "S"(a4),
                       "D"(a5), "r"(ebp) : "memory");
    return ret;
}

static guest_slong_t do_mmap(guest_ulong_t addr, guest_ulong_t len,
                             guest_slong_t prot, guest_slong_t flags,
                             guest_slong_t fd)
{
    return syscall6(__NR_mmap2, addr, len, prot, flags, fd, 0);
}

static int test_shared_alias(void)
{
    guest_slong_t source;
    guest_slong_t alias;

    source = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1);
    if (source < 0) {
        return 30;
    }
    alias = syscall5(__NR_mremap, source, 0, 16384, MREMAP_MAYMOVE, 0);
    if (alias < 0) {
        return 30;
    }
    *(guest_ulong_t *)source = 0x10203040UL;
    *(guest_ulong_t *)(alias + 12288) = 0x50607080UL;
    if (*(guest_ulong_t *)alias != 0x10203040UL ||
        *(guest_ulong_t *)(source + 12288) != 0x50607080UL ||
        syscall3(__NR_mprotect, alias, 16384, PROT_READ) != 0 ||
        syscall3(__NR_msync, alias, 16384, MS_SYNC) != 0 ||
        syscall2(__NR_munmap, source, 16384) != 0 ||
        *(guest_ulong_t *)alias != 0x10203040UL) {
        return 30;
    }
    syscall2(__NR_munmap, alias, 16384);
    return 0;
}

static int test_partial_failure(guest_ulong_t old_size,
                                guest_ulong_t new_size)
{
    guest_slong_t source;
    guest_slong_t target;

    source = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1);
    target = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1);
    if (source < 0 || target < 0) {
        return 31;
    }
    *(guest_ulong_t *)source = 0x11223344UL;
    *(guest_ulong_t *)target = 0x55667788UL;
    if (syscall5(__NR_mremap, source, old_size, new_size,
                 MREMAP_MAYMOVE | MREMAP_FIXED, target) != -EFAULT ||
        *(guest_ulong_t *)source != 0x11223344UL ||
        *(guest_ulong_t *)target != 0x55667788UL) {
        return 31;
    }
    syscall2(__NR_munmap, source, 16384);
    syscall2(__NR_munmap, target, 16384);
    return 0;
}

static int test_shadow_failure(void)
{
    static const char name[] = "lat-i386-shadow";
    guest_slong_t map;
    guest_slong_t fd;

    map = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1);
    fd = syscall2(__NR_memfd_create, (guest_slong_t)name, 0);
    if (map < 0 || fd < 0 || syscall2(__NR_ftruncate, fd, 4096) != 0 ||
        do_mmap(map + 4096, 4096, PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_FIXED, fd) != map + 4096) {
        return 32;
    }
    *(guest_ulong_t *)map = 0x89abcdefUL;
    *(guest_ulong_t *)(map + 4096) = 0x76543210UL;
    if (syscall5(__NR_mremap, map + 4096, 0, 4096,
                 MREMAP_MAYMOVE, 0) != -EFAULT ||
        *(guest_ulong_t *)map != 0x89abcdefUL ||
        *(guest_ulong_t *)(map + 4096) != 0x76543210UL ||
        syscall3(__NR_mprotect, map + 4096, 4096, PROT_READ) != 0 ||
        syscall3(__NR_msync, map + 4096, 4096, MS_SYNC) != 0 ||
        syscall2(__NR_munmap, map, 16384) != 0) {
        return 32;
    }
    syscall1(__NR_close, fd);
    return 0;
}

int test_main(guest_ulong_t *stack)
{
    guest_ulong_t argc = stack[0];
    char **argv = (char **)&stack[1];
    int ret;

    ret = test_shared_alias();
    if (ret || argc < 2 || !argv[1] || argv[1][0] != 'h') {
        return ret;
    }
    ret = test_partial_failure(8193, 8193);
    if (!ret) {
        ret = test_partial_failure(8193, 12289);
    }
    if (!ret) {
        ret = test_partial_failure(12289, 8193);
    }
    if (!ret) {
        ret = test_shadow_failure();
    }
    return ret;
}

__asm__(".section .text\n"
        ".global _start\n"
        ".type _start,@function\n"
        "_start:\n"
        "mov %esp,%eax\n"
        "push %eax\n"
        "call test_main\n"
        "mov %eax,%ebx\n"
        "mov $252,%eax\n"
        "int $0x80\n"
        "ud2\n"
        ".size _start,.-_start\n"
        ".section .note.GNU-stack,\"\",@progbits\n");
