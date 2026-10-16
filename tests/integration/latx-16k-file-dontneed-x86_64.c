/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * On hosts whose page size is larger than the guest page size, a private
 * file mapping whose file offset is not host-page aligned cannot be mapped
 * file-backed directly.  LATX emulates it with an anonymous host mapping that
 * is filled by pread.  MADV_DONTNEED on such a mapping must not discard the
 * copied file contents: the guest is entitled to the same reload-from-file
 * behaviour as a native private file mapping.
 */
#define NR_read 0
#define NR_write 1
#define NR_open 2
#define NR_close 3
#define NR_lseek 8
#define NR_mmap 9
#define NR_madvise 28
#define NR_ftruncate 77
#define NR_unlink 87
#define NR_exit 231

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_PRIVATE 2
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MADV_DONTNEED 4

#define PAGE_SIZE 4096
#define MAP_LEN (4 * PAGE_SIZE)
#define FILE_OFF PAGE_SIZE
#define FILE_LEN (8 * PAGE_SIZE)

static long call6(long nr, long a, long b, long c, long d, long e, long f)
{
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    long ret;

    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(nr), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8),
                       "r"(r9)
                     : "rcx", "r11", "memory");
    return ret;
}

static long call3(long nr, long a, long b, long c)
{
    return call6(nr, a, b, c, 0, 0, 0);
}

static unsigned char pattern[FILE_LEN];
static unsigned char expected[MAP_LEN];

__attribute__((used)) int test_main(long *stack)
{
    char **argv = (char **)(stack + 1);
    const char *path = argv[1];
    long fd, res, va, p, off, n, i;

    if (stack[0] != 2) {
        return 9;
    }

    for (i = 0; i < FILE_LEN; i++) {
        pattern[i] = (unsigned char)(i * 7 + 3);
    }

    fd = call3(NR_open, (long)path, 0102 /* O_RDWR | O_CREAT */, 0600);
    if (fd < 0) {
        return 10;
    }
    if (call3(NR_ftruncate, fd, FILE_LEN, 0) < 0) {
        return 11;
    }
    for (off = 0; off < FILE_LEN; off += n) {
        n = call3(NR_write, fd, (long)pattern + off, FILE_LEN - off);
        if (n <= 0) {
            return 12;
        }
    }

    /* The bytes the fixed mapping must expose, independent of page size. */
    if (call3(NR_lseek, fd, FILE_OFF, 0 /* SEEK_SET */) != FILE_OFF) {
        return 17;
    }
    for (off = 0, n = 0; off < MAP_LEN; off += n) {
        n = call3(NR_read, fd, (long)expected + off, MAP_LEN - off);
        if (n <= 0) {
            return 13;
        }
    }

    /*
     * Reserve a range and pick a host-page-aligned address inside it, then
     * replace it with the file mapping at a 4K-aligned (but not necessarily
     * host-page-aligned) file offset.
     */
    res = call6(NR_mmap, 0, 2 * MAP_LEN, PROT_NONE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (res < 0) {
        return 14;
    }
    va = (res + 0x3fff) & ~0x3fffl;
    p = call6(NR_mmap, va, MAP_LEN, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_FIXED, fd, FILE_OFF);
    if (p != va) {
        return 15;
    }

    for (i = 0; i < MAP_LEN; i++) {
        if (((unsigned char *)va)[i] != expected[i]) {
            return 20;
        }
    }

    if (call3(NR_madvise, va, MAP_LEN, MADV_DONTNEED) < 0) {
        return 16;
    }

    for (i = 0; i < MAP_LEN; i++) {
        if (((unsigned char *)va)[i] != expected[i]) {
            return 21;
        }
    }

    call3(NR_close, fd, 0, 0);
    call3(NR_unlink, (long)path, 0, 0);
    return 0;
}

__asm__(".text\n.global _start\n_start:\nmov %rsp,%rdi\nandq $-16,%rsp\n"
        "call test_main\nmov %eax,%edi\nmov $231,%eax\nsyscall\nud2\n"
        ".section .note.GNU-stack,\"\",@progbits\n");
