/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Verify that replacing the guest executable identity does not allocate the
 * translator's saved descriptor over guest stdin, stdout, or stderr.
 */

typedef unsigned long long u64;
typedef unsigned int u32;

#if defined(__x86_64__)
#define NR_close 3
#define NR_dup 32
#define NR_openat 257
#define NR_prctl 157
#define NR_exit_group 231
#else
#define NR_close 6
#define NR_dup 41
#define NR_openat 295
#define NR_prctl 172
#define NR_exit_group 252
#endif

#define AT_FDCWD (-100)
#define O_RDWR 2
#define PR_SET_MM 35
#define PR_SET_MM_EXE_FILE 13
#define PR_SET_MM_MAP 14

struct prctl_mm_map {
    u64 start_code;
    u64 end_code;
    u64 start_data;
    u64 end_data;
    u64 start_brk;
    u64 brk;
    u64 start_stack;
    u64 arg_start;
    u64 arg_end;
    u64 env_start;
    u64 env_end;
    u64 auxv;
    u32 auxv_size;
    u32 exe_fd;
};

static long syscall1(long nr, long arg1)
{
    long ret;

#if defined(__x86_64__)
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(arg1)
                     : "rcx", "r11", "memory");
#else
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(arg1)
                     : "memory");
#endif
    return ret;
}

static long syscall5(long nr, long arg1, long arg2, long arg3, long arg4,
                     long arg5)
{
    long ret;

#if defined(__x86_64__)
    register long r10 __asm__("r10") = arg4;
    register long r8 __asm__("r8") = arg5;
    __asm__ volatile("syscall" : "=a"(ret)
                     : "a"(nr), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10),
                       "r"(r8)
                     : "rcx", "r11", "memory");
#else
    __asm__ volatile("int $0x80" : "=a"(ret)
                     : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3),
                       "S"(arg4), "D"(arg5)
                     : "memory");
#endif
    return ret;
}

static int check_standard_fds(void)
{
    static const char null_path[] = "/dev/null";
    long fd = syscall5(NR_openat, AT_FDCWD, (long)null_path, O_RDWR, 0, 0);
    long out;
    long err;

    if (fd != 0) {
        return 20;
    }
    out = syscall1(NR_dup, fd);
    if (out != 1) {
        return 21;
    }
    err = syscall1(NR_dup, fd);
    return err == 2 ? 0 : 22;
}

static int set_mm_exe_file(long fd)
{
    return syscall5(NR_prctl, PR_SET_MM, PR_SET_MM_EXE_FILE, fd, 0, 0) == 0 ?
           0 : 10;
}

static int set_mm_map(long fd)
{
    struct prctl_mm_map map = { 0 };
    u64 code = (u64)(unsigned long)set_mm_map & ~(u64)4095;

    map.start_code = code;
    map.end_code = code + 4096;
    map.start_data = code + 8192;
    map.end_data = map.start_data + 4096;
    map.start_brk = code + 16384;
    map.brk = code + 20480;
    map.start_stack = code + 24576;
    map.arg_start = map.start_stack;
    map.arg_end = map.arg_start + 4;
    map.env_start = map.arg_end;
    map.env_end = map.env_start + 4;
    map.exe_fd = (u32)fd;

    return syscall5(NR_prctl, PR_SET_MM, PR_SET_MM_MAP, (long)&map,
                    sizeof(map), 0) == 0 ? 0 : 11;
}

int test_main(unsigned long *stack)
{
    unsigned long argc = stack[0];
    char **argv = (char **)&stack[1];
    long fd;
    int ret;

    if (argc != 3 || !argv[1] || !argv[2]) {
        return 1;
    }
    fd = syscall5(NR_openat, AT_FDCWD, (long)argv[2], O_RDWR, 0, 0);
    if (fd < 3) {
        return 2;
    }
    syscall1(NR_close, 0);
    syscall1(NR_close, 1);
    syscall1(NR_close, 2);

    if (argv[1][0] == 'f') {
        ret = set_mm_exe_file(fd);
    } else if (argv[1][0] == 'm') {
        ret = set_mm_map(fd);
    } else {
        return 3;
    }
    if (ret) {
        return ret;
    }
    return check_standard_fds();
}

#if defined(__x86_64__)
__asm__(".text\n.global _start\n_start:\nmov %rsp,%rdi\nandq $-16,%rsp\n"
        "call test_main\nmov %eax,%edi\nmov $231,%eax\nsyscall\nud2\n"
        ".pushsection .note.GNU-stack,\"\",@progbits\n"
        ".popsection\n");
#else
__asm__(".text\n.global _start\n_start:\nmov %esp,%eax\nandl $-16,%esp\n"
        "subl $12,%esp\npush %eax\ncall test_main\nmov %eax,%ebx\n"
        "mov $252,%eax\nint $0x80\nud2\n");
#endif
