/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Freestanding x86_64 tests for prctl semantics that cross subsystem
 * boundaries.  Each invocation runs one irreversible process-state case.
 */

typedef unsigned long guest_ulong_t;
typedef long guest_slong_t;

#define __NR_read               0
#define __NR_write              1
#define __NR_close              3
#define __NR_execve            59
#define __NR_mmap               9
#define __NR_mprotect          10
#define __NR_munmap            11
#define __NR_brk               12
#define __NR_shmget            29
#define __NR_shmat             30
#define __NR_shmctl            31
#define __NR_shmdt             67
#define __NR_ftruncate         77
#define __NR_fork              57
#define __NR_wait4             61
#define __NR_mremap            25
#define __NR_msync             26
#define __NR_personality      135
#define __NR_prctl            157
#define __NR_set_tid_address  218
#define __NR_timer_create     222
#define __NR_timer_delete     226
#define __NR_exit_group       231
#define __NR_openat           257
#define __NR_memfd_create     319
#define __NR_readlinkat       267
#define __NR_execveat         322
#define __NR_readlink          89
#define __NR_rename            82
#define __NR_unlink            87

#define EACCES 13
#define EBADF   9
#define EBUSY  16
#define EFAULT 14
#define EINVAL 22
#define ENOMEM 12
#define ENAMETOOLONG 36
#define EOPNOTSUPP 95
#define EPERM   1

#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    1
#define MAP_PRIVATE   2
#define MAP_FIXED    16
#define MAP_ANONYMOUS 32

#define MREMAP_MAYMOVE   1
#define MREMAP_FIXED     2
#define MREMAP_DONTUNMAP 4
#define MS_SYNC           4

#define AT_FDCWD (-100)
#define AT_EMPTY_PATH 0x1000
#define O_DIRECTORY 00200000

#define IPC_PRIVATE 0
#define IPC_CREAT   01000
#define IPC_RMID    0
#define SHM_EXEC    0100000

#define READ_IMPLIES_EXEC 0x0400000

#define PR_SET_PDEATHSIG             1
#define PR_GET_PDEATHSIG             2
#define PR_GET_DUMPABLE              3
#define PR_SET_DUMPABLE              4
#define PR_SET_KEEPCAPS              8
#define PR_GET_KEEPCAPS              7
#define PR_SET_TIMING               14
#define PR_GET_TIMING               13
#define PR_SET_NAME                 15
#define PR_GET_NAME                 16
#define PR_CAPBSET_READ             23
#define PR_GET_TSC                  25
#define PR_SET_TSC                  26
#define PR_TSC_SIGSEGV               2
#define PR_SET_TIMERSLACK           29
#define PR_GET_TIMERSLACK           30
#define PR_TASK_PERF_EVENTS_DISABLE 31
#define PR_TASK_PERF_EVENTS_ENABLE  32
#define PR_MCE_KILL                 33
#define PR_MCE_KILL_GET             34
#define PR_SET_MM                   35
#define PR_SET_MM_START_CODE         1
#define PR_SET_MM_END_CODE           2
#define PR_SET_MM_START_DATA         3
#define PR_SET_MM_END_DATA           4
#define PR_SET_MM_START_STACK        5
#define PR_SET_MM_START_BRK          6
#define PR_SET_MM_BRK                7
#define PR_SET_MM_ARG_START          8
#define PR_SET_MM_ARG_END            9
#define PR_SET_MM_ENV_START         10
#define PR_SET_MM_ENV_END           11
#define PR_SET_MM_AUXV              12
#define PR_SET_MM_EXE_FILE          13
#define PR_SET_MM_MAP               14
#define PR_SET_MM_MAP_SIZE          15
#define PR_MCE_KILL_CLEAR            0
#define PR_MCE_KILL_SET              1
#define PR_MCE_KILL_DEFAULT          2
#define PR_MCE_KILL_EARLY            1
#define PR_MCE_KILL_LATE             0
#define PR_GET_TID_ADDRESS          40
#define PR_SET_CHILD_SUBREAPER      36
#define PR_GET_CHILD_SUBREAPER      37
#define PR_SET_NO_NEW_PRIVS         38
#define PR_GET_NO_NEW_PRIVS         39
#define PR_SET_THP_DISABLE          41
#define PR_GET_THP_DISABLE          42
#define PR_THP_DISABLE_EXCEPT_ADVISED 2
#define PR_SCHED_CORE               62
#define PR_SCHED_CORE_GET            0
#define PR_SCHED_CORE_SCOPE_THREAD   0
#define PR_SET_MDWE                 65
#define PR_GET_MDWE                 66
#define PR_MDWE_REFUSE_EXEC_GAIN     1
#define PR_MDWE_NO_INHERIT           2
#define PR_SET_MEMORY_MERGE         67
#define PR_GET_MEMORY_MERGE         68
#define PR_TIMER_CREATE_RESTORE_IDS 77
#define PR_TIMER_CREATE_RESTORE_IDS_OFF 0
#define PR_TIMER_CREATE_RESTORE_IDS_ON  1
#define PR_TIMER_CREATE_RESTORE_IDS_GET 2
#define PR_FUTEX_HASH               78
#define PR_FUTEX_HASH_SET_SLOTS      1
#define PR_FUTEX_HASH_GET_SLOTS      2
#define PR_RSEQ_SLICE_EXTENSION     79
#define PR_RSEQ_SLICE_EXTENSION_GET  1
#define PR_RSEQ_SLICE_EXTENSION_SET  2
#define PR_RSEQ_SLICE_EXT_ENABLE      1
#define PR_GET_CFI                   80
#define PR_SET_CFI                   81
#define PR_CFI_BRANCH_LANDING_PADS    0
#define PR_CFI_ENABLE                 1
#define PR_SET_VMA          0x53564d41
#define PR_SET_VMA_ANON_NAME         0
#define PR_GET_AUXV         0x41555856

#define CAP_CHOWN 0

typedef struct PrctlMmMap {
    guest_ulong_t start_code;
    guest_ulong_t end_code;
    guest_ulong_t start_data;
    guest_ulong_t end_data;
    guest_ulong_t start_brk;
    guest_ulong_t brk;
    guest_ulong_t start_stack;
    guest_ulong_t arg_start;
    guest_ulong_t arg_end;
    guest_ulong_t env_start;
    guest_ulong_t env_end;
    guest_ulong_t auxv;
    unsigned int auxv_size;
    unsigned int exe_fd;
} PrctlMmMap;

static inline guest_slong_t syscall1(guest_slong_t nr, guest_slong_t a1)
{
    guest_slong_t ret;

    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(a1)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline guest_slong_t syscall2(guest_slong_t nr, guest_slong_t a1,
                                     guest_slong_t a2)
{
    guest_slong_t ret;

    __asm__ volatile("syscall" : "=a"(ret)
                     : "a"(nr), "D"(a1), "S"(a2)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline guest_slong_t syscall3(guest_slong_t nr, guest_slong_t a1,
                                     guest_slong_t a2, guest_slong_t a3)
{
    guest_slong_t ret;

    __asm__ volatile("syscall" : "=a"(ret)
                     : "a"(nr), "D"(a1), "S"(a2), "d"(a3)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline guest_slong_t syscall4(guest_slong_t nr, guest_slong_t a1,
                                     guest_slong_t a2, guest_slong_t a3,
                                     guest_slong_t a4)
{
    register guest_slong_t r10 __asm__("r10") = a4;
    guest_slong_t ret;

    __asm__ volatile("syscall" : "=a"(ret)
                     : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline guest_slong_t syscall5(guest_slong_t nr, guest_slong_t a1,
                                     guest_slong_t a2, guest_slong_t a3,
                                     guest_slong_t a4, guest_slong_t a5)
{
    register guest_slong_t r10 __asm__("r10") = a4;
    register guest_slong_t r8 __asm__("r8") = a5;
    guest_slong_t ret;

    __asm__ volatile("syscall" : "=a"(ret)
                     : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10),
                       "r"(r8)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline guest_slong_t syscall6(guest_slong_t nr, guest_slong_t a1,
                                     guest_slong_t a2, guest_slong_t a3,
                                     guest_slong_t a4, guest_slong_t a5,
                                     guest_slong_t a6)
{
    register guest_slong_t r10 __asm__("r10") = a4;
    register guest_slong_t r8 __asm__("r8") = a5;
    register guest_slong_t r9 __asm__("r9") = a6;
    guest_slong_t ret;

    __asm__ volatile("syscall" : "=a"(ret)
                     : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10),
                       "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");
    return ret;
}

static guest_slong_t do_prctl(guest_slong_t option, guest_slong_t arg2,
                              guest_slong_t arg3, guest_slong_t arg4,
                              guest_slong_t arg5)
{
    return syscall5(__NR_prctl, option, arg2, arg3, arg4, arg5);
}

static guest_slong_t do_mmap(void *addr, guest_ulong_t len,
                             guest_slong_t prot, guest_slong_t flags,
                             guest_slong_t fd, guest_ulong_t offset)
{
    return syscall6(__NR_mmap, (guest_slong_t)addr, len, prot, flags, fd,
                    offset);
}

static int bytes_equal(const char *a, const char *b, guest_ulong_t len)
{
    guest_ulong_t i;

    for (i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static guest_ulong_t string_length(const char *value)
{
    guest_ulong_t len = 0;

    while (value[len]) {
        len++;
    }
    return len;
}

static int contains(const char *buf, guest_ulong_t len, const char *needle,
                    guest_ulong_t needle_len)
{
    guest_ulong_t i;

    if (needle_len > len) {
        return 0;
    }
    for (i = 0; i <= len - needle_len; i++) {
        if (bytes_equal(buf + i, needle, needle_len)) {
            return 1;
        }
    }
    return 0;
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return -1;
}

static int named_range(const char *buf, guest_ulong_t len, const char *marker,
                       guest_ulong_t marker_len, guest_ulong_t *start,
                       guest_ulong_t *end)
{
    guest_ulong_t marker_pos;
    guest_ulong_t line;
    guest_ulong_t pos;
    int digit;

    for (marker_pos = 0; marker_pos + marker_len <= len; marker_pos++) {
        if (bytes_equal(buf + marker_pos, marker, marker_len)) {
            break;
        }
    }
    if (marker_pos + marker_len > len) {
        return 0;
    }
    line = marker_pos;
    while (line && buf[line - 1] != '\n') {
        line--;
    }
    *start = 0;
    for (pos = line; pos < marker_pos && buf[pos] != '-'; pos++) {
        digit = hex_value(buf[pos]);
        if (digit < 0) {
            return 0;
        }
        *start = (*start << 4) | digit;
    }
    if (pos >= marker_pos || buf[pos++] != '-') {
        return 0;
    }
    *end = 0;
    for (; pos < marker_pos && buf[pos] != ' '; pos++) {
        digit = hex_value(buf[pos]);
        if (digit < 0) {
            return 0;
        }
        *end = (*end << 4) | digit;
    }
    return pos < marker_pos && buf[pos] == ' ';
}

static guest_slong_t read_maps(char *buffer, guest_ulong_t size)
{
    static const char maps_path[] = "/proc/self/maps";
    guest_slong_t fd = syscall4(__NR_openat, AT_FDCWD,
                                (guest_slong_t)maps_path, 0, 0);
    guest_slong_t len;

    if (fd < 0) {
        return fd;
    }
    len = syscall3(__NR_read, fd, (guest_slong_t)buffer, size);
    syscall1(__NR_close, fd);
    return len;
}

static guest_slong_t read_file(const char *path, char *buffer,
                               guest_ulong_t size)
{
    guest_slong_t fd = syscall4(__NR_openat, AT_FDCWD,
                                (guest_slong_t)path, 0, 0);
    guest_slong_t len;

    if (fd < 0) {
        return fd;
    }
    len = syscall3(__NR_read, fd, (guest_slong_t)buffer, size);
    syscall1(__NR_close, fd);
    return len;
}

static int test_shared_mremap_4k_case(guest_ulong_t old_request,
                                      guest_ulong_t new_request)
{
    guest_ulong_t old_size = (old_request + 4095) & -4096UL;
    guest_ulong_t new_size = (new_request + 4095) & -4096UL;
    guest_ulong_t common_size = old_size < new_size ? old_size : new_size;
    guest_slong_t source;
    guest_slong_t old_alias;
    guest_slong_t target;
    guest_slong_t new_alias;
    guest_ulong_t offset;

    source = do_mmap(0, old_size, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (source < 0) {
        return 1;
    }
    old_alias = syscall5(__NR_mremap, source, 0, old_size,
                         MREMAP_MAYMOVE, 0);
    target = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (old_alias < 0 || target < 0) {
        return 1;
    }
    for (offset = 0; offset < old_size; offset += 4096) {
        *(guest_ulong_t *)(source + offset) =
            0x1020304050607000UL + offset;
    }
    if (new_size < 16384) {
        *(guest_ulong_t *)(target + new_size) = 0xaabbccddeeff0011UL;
    }
    if (syscall5(__NR_mremap, source, old_request, new_request,
                 MREMAP_MAYMOVE | MREMAP_FIXED, target) != target ||
        syscall3(__NR_mprotect, source, old_size, PROT_READ) != -ENOMEM) {
        return 1;
    }
    for (offset = 0; offset < common_size; offset += 4096) {
        guest_ulong_t expected = 0x1020304050607000UL + offset;

        if (*(guest_ulong_t *)(target + offset) != expected ||
            *(guest_ulong_t *)(old_alias + offset) != expected) {
            return 1;
        }
    }
    if (new_size > old_size &&
        *(guest_ulong_t *)(target + old_size) != 0) {
        return 1;
    }
    if (new_size < 16384 &&
        *(guest_ulong_t *)(target + new_size) != 0xaabbccddeeff0011UL) {
        return 1;
    }
    *(guest_ulong_t *)target = 0x8877665544332211UL;
    if (*(guest_ulong_t *)old_alias != 0x8877665544332211UL) {
        return 1;
    }
    new_alias = syscall5(__NR_mremap, target, 0, new_size,
                         MREMAP_MAYMOVE, 0);
    if (new_alias < 0) {
        return 1;
    }
    *(guest_ulong_t *)(new_alias + new_size - 4096) =
        0xfedcba9876543210UL;
    if (*(guest_ulong_t *)(target + new_size - 4096) !=
            0xfedcba9876543210UL) {
        return 1;
    }
    syscall2(__NR_munmap, old_alias, old_size);
    syscall2(__NR_munmap, new_alias, new_size);
    syscall2(__NR_munmap, target, 16384);
    return 0;
}

static int test_process_controls(void)
{
    static const char exe_path[] = "/proc/self/exe";
    static const char set_name[16] = "lat-prctl-proc";
    char get_name[16] = { 0 };
    int value = -1;
    int old_dumpable;
    int old_keepcaps;
    int old_subreaper;
    int old_thp;
    int timer_id;
    guest_slong_t old_slack;
    guest_ulong_t tid_address = 0;
    guest_ulong_t clear_tid = 0;
    guest_ulong_t cookie = 0;
    guest_slong_t ret;

    if (do_prctl(0x7fffffff, 0, 0, 0, 0) != -EINVAL) {
        return 40;
    }
    if (do_prctl(PR_GET_PDEATHSIG, 0, 0, 0, 0) != -EFAULT ||
        do_prctl(PR_SET_PDEATHSIG, 10, 0, 0, 0) != 0 ||
        do_prctl(PR_GET_PDEATHSIG, (guest_slong_t)&value, 0, 0, 0) != 0 ||
        value != 10 || do_prctl(PR_SET_PDEATHSIG, 0, 0, 0, 0) != 0) {
        return 41;
    }
    if (do_prctl(PR_SET_NAME, (guest_slong_t)set_name, 0, 0, 0) != 0 ||
        do_prctl(PR_GET_NAME, (guest_slong_t)get_name, 0, 0, 0) != 0 ||
        !bytes_equal(set_name, get_name, sizeof(set_name))) {
        return 42;
    }

    old_dumpable = do_prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
    if (old_dumpable < 0 || do_prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0 ||
        do_prctl(PR_GET_DUMPABLE, 0, 0, 0, 0) != 0 ||
        do_prctl(PR_SET_DUMPABLE, old_dumpable, 0, 0, 0) != 0) {
        return 43;
    }

    old_keepcaps = do_prctl(PR_GET_KEEPCAPS, 0, 0, 0, 0);
    if (old_keepcaps < 0 || do_prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) != 0 ||
        do_prctl(PR_GET_KEEPCAPS, 0, 0, 0, 0) != 1 ||
        do_prctl(PR_SET_KEEPCAPS, old_keepcaps, 0, 0, 0) != 0) {
        return 44;
    }

    if (do_prctl(PR_GET_TIMING, 0, 0, 0, 0) != 0 ||
        do_prctl(PR_SET_TIMING, 0, 0, 0, 0) != 0 ||
        do_prctl(PR_SET_TIMING, 1, 0, 0, 0) != -EINVAL) {
        return 45;
    }

    old_slack = do_prctl(PR_GET_TIMERSLACK, 0, 0, 0, 0);
    if (old_slack <= 0 ||
        do_prctl(PR_SET_TIMERSLACK, 1234567, 0, 0, 0) != 0 ||
        do_prctl(PR_GET_TIMERSLACK, 0, 0, 0, 0) != 1234567 ||
        do_prctl(PR_SET_TIMERSLACK, old_slack, 0, 0, 0) != 0) {
        return 46;
    }

    ret = do_prctl(PR_MCE_KILL_GET, 0, 0, 0, 0);
    if (ret < 0 || do_prctl(PR_MCE_KILL, PR_MCE_KILL_SET,
                            PR_MCE_KILL_LATE, 0, 0) != 0 ||
        do_prctl(PR_MCE_KILL_GET, 0, 0, 0, 0) != PR_MCE_KILL_LATE ||
        do_prctl(PR_MCE_KILL, PR_MCE_KILL_CLEAR, 0, 0, 0) != 0 ||
        do_prctl(PR_MCE_KILL_GET, 0, 0, 0, 0) != PR_MCE_KILL_DEFAULT) {
        return 47;
    }

    old_subreaper = -1;
    if (do_prctl(PR_GET_CHILD_SUBREAPER,
                 (guest_slong_t)&old_subreaper, 0, 0, 0) ||
        old_subreaper < 0 ||
        do_prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) != 0) {
        return 48;
    }
    value = -1;
    if (do_prctl(PR_GET_CHILD_SUBREAPER, (guest_slong_t)&value, 0, 0, 0) != 0 ||
        value != 1 ||
        do_prctl(PR_SET_CHILD_SUBREAPER, old_subreaper, 0, 0, 0) != 0) {
        return 48;
    }

    if (syscall1(__NR_set_tid_address, (guest_slong_t)&clear_tid) <= 0 ||
        do_prctl(PR_GET_TID_ADDRESS,
                 (guest_slong_t)&tid_address, 0, 0, 0) != 0 ||
        tid_address != (guest_ulong_t)&clear_tid) {
        return 49;
    }

    old_thp = do_prctl(PR_GET_THP_DISABLE, 0, 0, 0, 0);
    if (old_thp < 0 || do_prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0) != 0 ||
        do_prctl(PR_GET_THP_DISABLE, 0, 0, 0, 0) != 1 ||
        do_prctl(PR_SET_THP_DISABLE, !!old_thp, old_thp & ~1, 0, 0) != 0) {
        return 50;
    }
    ret = do_prctl(PR_SET_THP_DISABLE, 1,
                   PR_THP_DISABLE_EXCEPT_ADVISED, 0, 0);
    if (ret == 0) {
        if (do_prctl(PR_GET_THP_DISABLE, 0, 0, 0, 0) != 3 ||
            do_prctl(PR_SET_THP_DISABLE, !!old_thp, old_thp & ~1,
                     0, 0) != 0) {
            return 50;
        }
    } else if (ret != -EINVAL) {
        return 50;
    }

    if (do_prctl(PR_GET_MEMORY_MERGE, 0, 0, 0, 0) != -EINVAL ||
        do_prctl(PR_SET_MEMORY_MERGE, 1, 0, 0, 0) != -EINVAL) {
        return 51;
    }

    ret = do_prctl(PR_SCHED_CORE, PR_SCHED_CORE_GET, 0,
                   PR_SCHED_CORE_SCOPE_THREAD, (guest_slong_t)&cookie);
    if (ret != 0 && ret != -EINVAL) {
        return 52;
    }
    ret = do_prctl(PR_CAPBSET_READ, CAP_CHOWN, 0, 0, 0);
    if (ret != 0 && ret != 1) {
        return 53;
    }
    if (do_prctl(PR_CAPBSET_READ, 999, 0, 0, 0) != -EINVAL ||
        do_prctl(PR_TASK_PERF_EVENTS_DISABLE, 0, 0, 0, 0) != 0 ||
        do_prctl(PR_TASK_PERF_EVENTS_ENABLE, 0, 0, 0, 0) != 0) {
        return 53;
    }

    if (do_prctl(PR_SET_NO_NEW_PRIVS, 1, 1, 0, 0) != -EINVAL ||
        do_prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0 ||
        do_prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) != 1) {
        return 55;
    }

    timer_id = 123;
    if (do_prctl(PR_TIMER_CREATE_RESTORE_IDS,
                 PR_TIMER_CREATE_RESTORE_IDS_GET, 0, 0, 0) != 0 ||
        do_prctl(PR_TIMER_CREATE_RESTORE_IDS,
                 PR_TIMER_CREATE_RESTORE_IDS_ON, 0, 0, 0) != 0 ||
        do_prctl(PR_TIMER_CREATE_RESTORE_IDS,
                 PR_TIMER_CREATE_RESTORE_IDS_GET, 0, 0, 0) != 1 ||
        syscall3(__NR_timer_create, 1, 0, (guest_slong_t)&timer_id) != 0 ||
        timer_id != 123) {
        return 56;
    }
    timer_id = 123;
    if (syscall3(__NR_timer_create, 1, 0, (guest_slong_t)&timer_id) != -EBUSY ||
        syscall1(__NR_timer_delete, 123) != 0) {
        return 56;
    }
    timer_id = -1;
    if (syscall3(__NR_timer_create, 1, 0,
                 (guest_slong_t)&timer_id) != -EINVAL ||
        do_prctl(PR_TIMER_CREATE_RESTORE_IDS,
                 PR_TIMER_CREATE_RESTORE_IDS_OFF, 0, 0, 0) != 0) {
        return 56;
    }
    timer_id = -1;
    if (syscall3(__NR_timer_create, 1, 0, (guest_slong_t)&timer_id) != 0 ||
        timer_id < 0 || syscall1(__NR_timer_delete, timer_id) != 0 ||
        do_prctl(PR_TIMER_CREATE_RESTORE_IDS, 99, 0, 0, 0) != -EINVAL ||
        do_prctl(PR_TIMER_CREATE_RESTORE_IDS,
                 PR_TIMER_CREATE_RESTORE_IDS_GET, 1, 0, 0) != -EINVAL) {
        return 56;
    }

    if (do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_GET_SLOTS,
                 123, 456, 789) != 0 ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_SET_SLOTS,
                 1, 0, 0) != -EINVAL ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_SET_SLOTS,
                 3, 0, 0) != -EINVAL ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_SET_SLOTS,
                 16, 1, 0) != -EINVAL ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_SET_SLOTS,
                 16, 0, 0) != 0 ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_GET_SLOTS,
                 0, 0, 0) != 16 ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_SET_SLOTS,
                 32, 0, 0) != 0 ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_GET_SLOTS,
                 0, 0, 0) != 32 ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_SET_SLOTS,
                 0, 0, 0) != 0 ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_SET_SLOTS,
                 16, 0, 0) != -EBUSY) {
        return 57;
    }

    value = 0x12345678;
    if (do_prctl(PR_RSEQ_SLICE_EXTENSION,
                 PR_RSEQ_SLICE_EXTENSION_GET, 0, 0, 0) != -EOPNOTSUPP ||
        do_prctl(PR_RSEQ_SLICE_EXTENSION,
                 PR_RSEQ_SLICE_EXTENSION_GET, 0, 1, 0) != -EINVAL ||
        do_prctl(PR_RSEQ_SLICE_EXTENSION,
                 PR_RSEQ_SLICE_EXTENSION_SET,
                 PR_RSEQ_SLICE_EXT_ENABLE, 0, 0) != -EOPNOTSUPP ||
        do_prctl(PR_GET_CFI, PR_CFI_BRANCH_LANDING_PADS,
                 (guest_slong_t)&value, 0, 0) != -EINVAL ||
        do_prctl(PR_SET_CFI, PR_CFI_BRANCH_LANDING_PADS,
                 PR_CFI_ENABLE, 0, 0) != -EINVAL) {
        return 58;
    }

    if (syscall3(__NR_readlink, (guest_slong_t)exe_path, -1, 0) != -EINVAL ||
        syscall3(__NR_readlink, (guest_slong_t)exe_path, -1, -1) != -EINVAL ||
        syscall4(__NR_readlinkat, AT_FDCWD, (guest_slong_t)exe_path, -1, 0) !=
            -EINVAL) {
        return 59;
    }
    return 0;
}

static int test_vma_name(int large_host_page)
{
    static const char name[] = "lat-vma-inner";
    static const char marker[] = "[anon:lat-vma-inner]";
    static const char shared_name[] = "lat-vma-shared";
    static const char shared_marker[] = "[anon_shmem:lat-vma-shared]";
    static const char sysv_name[] = "lat-vma-sysv";
    static const char sysv_marker[] = "[anon_shmem:lat-vma-sysv]";
    static const char invalid_name[] = "lat[vma";
    static const char exe_path[] = "/proc/self/exe";
    static const char memfd_name[] = "lat-mremap-shadow";
    char long_name[80];
    char buffer[32768];
    guest_slong_t map;
    guest_slong_t shared_map;
    guest_slong_t sysv_map;
    guest_slong_t sysv_remap;
    guest_slong_t file_map;
    guest_slong_t shmid;
    guest_slong_t second_shmid;
    guest_slong_t remap_source;
    guest_slong_t remap_target;
    guest_slong_t remap_copy;
    guest_slong_t fd;
    guest_slong_t len;
    guest_ulong_t named_start;
    guest_ulong_t named_end;
    guest_ulong_t i;

    map = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map < 0) {
        return 60;
    }
    if (do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, map + 4096, 4096,
                 (guest_slong_t)name) != 0) {
        return 61;
    }
    len = read_maps(buffer, sizeof(buffer));
    if (len <= 0 ||
        !named_range(buffer, len, marker, sizeof(marker) - 1,
                     &named_start, &named_end) ||
        named_start != (guest_ulong_t)map + 4096 ||
        named_end != (guest_ulong_t)map + 8192) {
        if (len > 0) {
            syscall3(__NR_write, 2, (guest_slong_t)buffer, len);
        }
        return 63;
    }
    if (do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, map + 4096, 4096, 0)) {
        return 64;
    }
    len = read_maps(buffer, sizeof(buffer));
    if (len <= 0 || contains(buffer, len, marker, sizeof(marker) - 1)) {
        return 65;
    }

    for (i = 0; i < sizeof(long_name); i++) {
        long_name[i] = 'a';
    }
    if (do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, map, 4096,
                 (guest_slong_t)invalid_name) != -EINVAL ||
        do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, map, 4096,
                 (guest_slong_t)long_name) != -ENAMETOOLONG) {
        return 66;
    }

    fd = syscall4(__NR_openat, AT_FDCWD, (guest_slong_t)exe_path, 0, 0);
    if (fd < 0) {
        return 67;
    }
    file_map = do_mmap(0, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
    syscall1(__NR_close, fd);
    if (file_map < 0 ||
        do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, file_map, 4096,
                 (guest_slong_t)name) != -EBADF) {
        return 67;
    }
    syscall2(__NR_munmap, file_map, 4096);

    shared_map = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_map < 0 ||
        do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, shared_map + 4096,
                 4096, (guest_slong_t)shared_name) != 0) {
        return 68;
    }
    len = read_maps(buffer, sizeof(buffer));
    if (len <= 0 ||
        !named_range(buffer, len, shared_marker,
                     sizeof(shared_marker) - 1, &named_start, &named_end) ||
        named_start != (guest_ulong_t)shared_map + 4096 ||
        named_end != (guest_ulong_t)shared_map + 8192) {
        if (len > 0) {
            syscall3(__NR_write, 2, (guest_slong_t)buffer, len);
        }
        return 68;
    }
    syscall2(__NR_munmap, shared_map, 16384);

    shmid = syscall3(__NR_shmget, IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    if (shmid < 0) {
        return 69;
    }
    sysv_map = syscall3(__NR_shmat, shmid, 0, 0);
    if (sysv_map < 0 ||
        do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, sysv_map, 4096,
                 (guest_slong_t)sysv_name) != 0) {
        syscall3(__NR_shmctl, shmid, IPC_RMID, 0);
        return 69;
    }
    len = read_maps(buffer, sizeof(buffer));
    if (len <= 0 ||
        !named_range(buffer, len, sysv_marker, sizeof(sysv_marker) - 1,
                     &named_start, &named_end) ||
        named_start != (guest_ulong_t)sysv_map ||
        named_end != (guest_ulong_t)sysv_map + 4096) {
        syscall1(__NR_shmdt, sysv_map);
        syscall3(__NR_shmctl, shmid, IPC_RMID, 0);
        return 69;
    }
    if (syscall1(__NR_shmdt, sysv_map) != 0 ||
        syscall3(__NR_shmctl, shmid, IPC_RMID, 0) != 0) {
        return 69;
    }

    second_shmid = syscall3(__NR_shmget, IPC_PRIVATE, 4096,
                            IPC_CREAT | 0600);
    if (second_shmid < 0) {
        return 69;
    }
    sysv_remap = syscall3(__NR_shmat, second_shmid, sysv_map, 0);
    if (sysv_remap != sysv_map) {
        syscall3(__NR_shmctl, second_shmid, IPC_RMID, 0);
        return 69;
    }
    len = read_maps(buffer, sizeof(buffer));
    if (len <= 0 || contains(buffer, len, sysv_marker,
                             sizeof(sysv_marker) - 1)) {
        syscall1(__NR_shmdt, sysv_remap);
        syscall3(__NR_shmctl, second_shmid, IPC_RMID, 0);
        return 69;
    }
    syscall1(__NR_shmdt, sysv_remap);
    syscall3(__NR_shmctl, second_shmid, IPC_RMID, 0);

    if (do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, map + 4096, 4096,
                 (guest_slong_t)name) != 0 ||
        syscall2(__NR_munmap, map + 4096, 4096) != 0 ||
        do_mmap((void *)(map + 4096), 4096, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) !=
            map + 4096) {
        return 69;
    }
    len = read_maps(buffer, sizeof(buffer));
    if (len <= 0 || contains(buffer, len, marker, sizeof(marker) - 1)) {
        return 69;
    }
    syscall2(__NR_munmap, map, 16384);

    remap_source = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    remap_target = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (remap_source < 0 || remap_target < 0 ||
        do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, remap_source, 12288,
                 (guest_slong_t)name) != 0 ||
        syscall2(__NR_munmap, remap_target, 16384) != 0 ||
        syscall5(__NR_mremap, remap_source, 8193, 8193,
                 MREMAP_MAYMOVE | MREMAP_FIXED, remap_target) !=
            remap_target) {
        return 69;
    }
    len = read_maps(buffer, sizeof(buffer));
    if (len <= 0 ||
        !named_range(buffer, len, marker, sizeof(marker) - 1,
                     &named_start, &named_end) ||
        named_start != (guest_ulong_t)remap_target ||
        named_end != (guest_ulong_t)remap_target + 12288) {
        return 69;
    }
    syscall2(__NR_munmap, remap_target, 12288);

    remap_source = do_mmap(0, 12288, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    remap_target = do_mmap(0, 12288, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (remap_source < 0 || remap_target < 0) {
        return 69;
    }
    *(guest_ulong_t *)remap_source = 0x123456789abcdef0UL;
    if (syscall3(__NR_mprotect, remap_source, 12288, 0) != 0 ||
        syscall2(__NR_munmap, remap_target, 12288) != 0 ||
        syscall5(__NR_mremap, remap_source, 8193, 8193,
                 MREMAP_MAYMOVE | MREMAP_FIXED, remap_target) !=
            remap_target ||
        syscall3(__NR_mprotect, remap_target, 12288, PROT_READ) != 0 ||
        *(guest_ulong_t *)remap_target != 0x123456789abcdef0UL ||
        syscall3(__NR_mprotect, remap_source, 4096, PROT_READ) != -ENOMEM) {
        return 69;
    }
    syscall2(__NR_munmap, remap_target, 12288);

    remap_source = do_mmap(0, 12288, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    remap_target = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (remap_source < 0 || remap_target < 0) {
        return 69;
    }
    *(guest_ulong_t *)remap_source = 0x1122334455667788UL;
    *(guest_ulong_t *)(remap_target + 12288) = 0x8877665544332211UL;
    if (syscall5(__NR_mremap, remap_source, 8193, 8193,
                 MREMAP_MAYMOVE | MREMAP_FIXED, remap_target) !=
            remap_target ||
        *(guest_ulong_t *)remap_target != 0x1122334455667788UL ||
        *(guest_ulong_t *)(remap_target + 12288) != 0x8877665544332211UL) {
        return 69;
    }
    syscall2(__NR_munmap, remap_target, 16384);

    remap_source = do_mmap(0, 12288, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    remap_target = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (remap_source < 0 || remap_target < 0 ||
        syscall3(__NR_mprotect, remap_source + 4096, 4096, PROT_READ) != 0) {
        return 69;
    }
    *(guest_ulong_t *)remap_target = 0xaabbccddeeff0011UL;
    *(guest_ulong_t *)(remap_target + 12288) = 0x1100ffeeddccbbaaUL;
    if (syscall5(__NR_mremap, remap_source, 8193, 8193,
                 MREMAP_MAYMOVE | MREMAP_FIXED, remap_target) != -EFAULT ||
        *(guest_ulong_t *)remap_target != 0xaabbccddeeff0011UL ||
        *(guest_ulong_t *)(remap_target + 12288) != 0x1100ffeeddccbbaaUL) {
        return 69;
    }
    syscall2(__NR_munmap, remap_source, 12288);
    syscall2(__NR_munmap, remap_target, 16384);

    shared_map = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_map < 0 ||
        do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, shared_map, 16384,
                 (guest_slong_t)shared_name) != 0) {
        return 69;
    }
    remap_copy = syscall5(__NR_mremap, shared_map, 0, 16384,
                          MREMAP_MAYMOVE, 0);
    if (remap_copy < 0 ||
        do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, shared_map, 16384, 0)) {
        return 69;
    }
    *(guest_ulong_t *)shared_map = 0x123456789abcdef0UL;
    if (*(guest_ulong_t *)remap_copy != 0x123456789abcdef0UL) {
        return 69;
    }
    *(guest_ulong_t *)(remap_copy + 12288) = 0xfedcba9876543210UL;
    if (*(guest_ulong_t *)(shared_map + 12288) != 0xfedcba9876543210UL ||
        syscall3(__NR_mprotect, remap_copy, 16384, PROT_READ) != 0 ||
        syscall3(__NR_msync, remap_copy, 16384, MS_SYNC) != 0) {
        return 69;
    }
    len = read_maps(buffer, sizeof(buffer));
    if (len <= 0 ||
        !named_range(buffer, len, shared_marker,
                     sizeof(shared_marker) - 1, &named_start, &named_end) ||
        named_start != (guest_ulong_t)remap_copy ||
        named_end != (guest_ulong_t)remap_copy + 16384) {
        return 69;
    }
    syscall2(__NR_munmap, shared_map, 16384);
    if (*(guest_ulong_t *)remap_copy != 0x123456789abcdef0UL ||
        *(guest_ulong_t *)(remap_copy + 12288) != 0xfedcba9876543210UL) {
        return 69;
    }
    syscall2(__NR_munmap, remap_copy, 16384);

    /* Partial shared moves cannot be expressed safely on a 16K host page. */
    shared_map = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    remap_copy = syscall5(__NR_mremap, shared_map, 0, 16384,
                          MREMAP_MAYMOVE, 0);
    remap_target = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (shared_map < 0 || remap_copy < 0 || remap_target < 0 ||
        do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, shared_map, 16384,
                 (guest_slong_t)shared_name) != 0) {
        return 69;
    }
    *(guest_ulong_t *)remap_copy = 0x1020304050607080UL;
    *(guest_ulong_t *)(remap_copy + 4096) = 0x1122334455667788UL;
    *(guest_ulong_t *)(remap_copy + 8192) = 0x8877665544332211UL;
    *(guest_ulong_t *)(remap_copy + 12288) = 0x0102030405060708UL;
    *(guest_ulong_t *)(remap_target + 12288) = 0x8070605040302010UL;
    *(guest_ulong_t *)remap_target = 0xaabbccddeeff0011UL;
    if (syscall5(__NR_mremap, shared_map, 8193, 8193,
                 MREMAP_MAYMOVE | MREMAP_FIXED, remap_target + 1) != -EINVAL ||
        (large_host_page &&
         (syscall5(__NR_mremap, shared_map, 8193, 8193,
                   MREMAP_MAYMOVE | MREMAP_FIXED, remap_target) != -EFAULT ||
          syscall5(__NR_mremap, shared_map, 8193, 12289,
                   MREMAP_MAYMOVE | MREMAP_FIXED, remap_target) != -EFAULT ||
          syscall5(__NR_mremap, shared_map, 12289, 8193,
                   MREMAP_MAYMOVE | MREMAP_FIXED, remap_target) != -EFAULT)) ||
        *(guest_ulong_t *)remap_copy != 0x1020304050607080UL ||
        *(guest_ulong_t *)(remap_copy + 4096) != 0x1122334455667788UL ||
        *(guest_ulong_t *)(remap_copy + 8192) != 0x8877665544332211UL ||
        *(guest_ulong_t *)(remap_copy + 12288) != 0x0102030405060708UL ||
        *(guest_ulong_t *)remap_target != 0xaabbccddeeff0011UL ||
        *(guest_ulong_t *)(remap_target + 12288) !=
            0x8070605040302010UL) {
        return 69;
    }
    *(guest_ulong_t *)remap_copy = 0x8070605040302010UL;
    len = read_maps(buffer, sizeof(buffer));
    if (*(guest_ulong_t *)shared_map != 0x8070605040302010UL ||
        syscall3(__NR_mprotect, shared_map, 16384, PROT_READ) != 0 ||
        syscall3(__NR_msync, shared_map, 16384, MS_SYNC) != 0 ||
        syscall3(__NR_mprotect, shared_map, 16384,
                 PROT_READ | PROT_WRITE) != 0 ||
        len <= 0 ||
        !named_range(buffer, len, shared_marker,
                     sizeof(shared_marker) - 1, &named_start, &named_end) ||
        named_start != (guest_ulong_t)shared_map ||
        named_end != (guest_ulong_t)shared_map + 16384) {
        return 69;
    }
    syscall2(__NR_munmap, remap_target, 16384);
    syscall2(__NR_munmap, shared_map, 16384);
    syscall2(__NR_munmap, remap_copy, 16384);

    if (!large_host_page &&
        (test_shared_mremap_4k_case(8193, 8193) ||
         test_shared_mremap_4k_case(8193, 12289) ||
         test_shared_mremap_4k_case(12289, 8193))) {
        return 69;
    }

    if (large_host_page) {
        shared_map = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        remap_target = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (shared_map < 0 || remap_target < 0 ||
            syscall3(__NR_mprotect, shared_map + 4096, 4096,
                     PROT_READ) != 0) {
            return 69;
        }
        *(guest_ulong_t *)shared_map = 0xaabbccddeeff0011UL;
        *(guest_ulong_t *)remap_target = 0x1100ffeeddccbbaaUL;
        if (syscall5(__NR_mremap, shared_map, 0, 16384,
                     MREMAP_MAYMOVE | MREMAP_FIXED, remap_target) !=
                -EFAULT ||
            *(guest_ulong_t *)shared_map != 0xaabbccddeeff0011UL ||
            *(guest_ulong_t *)remap_target != 0x1100ffeeddccbbaaUL) {
            return 69;
        }
        syscall2(__NR_munmap, shared_map, 16384);
        syscall2(__NR_munmap, remap_target, 16384);
    }

    /* A shared file fragment uses a shadow page on a 16K host. */
    map = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    fd = syscall2(__NR_memfd_create, (guest_slong_t)memfd_name, 0);
    if (map < 0 || fd < 0 || syscall2(__NR_ftruncate, fd, 4096) != 0 ||
        do_mmap((void *)(map + 4096), 4096, PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_FIXED, fd, 0) != map + 4096) {
        return 69;
    }
    *(guest_ulong_t *)map = 0x0123456789abcdefUL;
    *(guest_ulong_t *)(map + 4096) = 0xfedcba9876543210UL;
    remap_copy = syscall5(__NR_mremap, map + 4096, 0, 4096,
                          MREMAP_MAYMOVE, 0);
    if ((large_host_page && remap_copy != -EFAULT) ||
        (!large_host_page && remap_copy < 0) ||
        *(guest_ulong_t *)map != 0x0123456789abcdefUL ||
        *(guest_ulong_t *)(map + 4096) != 0xfedcba9876543210UL ||
        syscall3(__NR_mprotect, map + 4096, 4096, PROT_READ) != 0 ||
        syscall3(__NR_msync, map + 4096, 4096, MS_SYNC) != 0 ||
        syscall3(__NR_mprotect, map + 4096, 4096,
                 PROT_READ | PROT_WRITE) != 0) {
        return 69;
    }
    if (remap_copy >= 0) {
        *(guest_ulong_t *)remap_copy = 0x1122334455667788UL;
        if (*(guest_ulong_t *)(map + 4096) != 0x1122334455667788UL) {
            return 69;
        }
        syscall2(__NR_munmap, remap_copy, 4096);
    }
    syscall2(__NR_munmap, map, 16384);
    syscall1(__NR_close, fd);

    remap_source = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (remap_source < 0 ||
        do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, remap_source, 16384,
                 (guest_slong_t)name) != 0) {
        return 69;
    }
    remap_copy = syscall5(__NR_mremap, remap_source, 16384, 16384,
                          MREMAP_MAYMOVE | MREMAP_DONTUNMAP, 0);
    if (remap_copy < 0 ||
        do_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, remap_source, 16384, 0)) {
        return 69;
    }
    len = read_maps(buffer, sizeof(buffer));
    if (len <= 0 ||
        !named_range(buffer, len, marker, sizeof(marker) - 1,
                     &named_start, &named_end) ||
        named_start != (guest_ulong_t)remap_copy ||
        named_end != (guest_ulong_t)remap_copy + 16384) {
        return 69;
    }
    syscall2(__NR_munmap, remap_source, 16384);
    syscall2(__NR_munmap, remap_copy, 16384);
    return 0;
}

static int enable_mdwe(void)
{
    return do_prctl(PR_SET_MDWE, PR_MDWE_REFUSE_EXEC_GAIN, 0, 0, 0) == 0 &&
           do_prctl(PR_GET_MDWE, 0, 0, 0, 0) == PR_MDWE_REFUSE_EXEC_GAIN;
}

static int test_mdwe_basic(void)
{
    guest_slong_t map;

    if (!enable_mdwe()) {
        return 70;
    }
    map = do_mmap(0, 4096, PROT_READ | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map < 0 || syscall3(__NR_mprotect, map, 4096,
                            PROT_READ | PROT_EXEC) != 0) {
        return 71;
    }
    syscall2(__NR_munmap, map, 4096);

    map = do_mmap(0, 4096, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map < 0 || syscall3(__NR_mprotect, map, 4096,
                            PROT_READ | PROT_EXEC) != -EACCES) {
        return 72;
    }
    syscall2(__NR_munmap, map, 4096);

    if (do_mmap(0, 4096, PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != -EACCES) {
        return 73;
    }

    map = do_mmap(0, 4096, PROT_READ,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map < 0 || do_mmap((void *)map, 4096, PROT_READ | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                           -1, 0) != map) {
        return 74;
    }
    syscall2(__NR_munmap, map, 4096);

    if (do_prctl(PR_SET_MDWE, PR_MDWE_REFUSE_EXEC_GAIN, 0, 0, 0) != 0 ||
        do_prctl(PR_SET_MDWE, 0, 0, 0, 0) != -EPERM) {
        return 75;
    }
    return 0;
}

static int test_mdwe_personality(void)
{
    guest_slong_t old_personality;
    guest_slong_t map;

    old_personality = syscall1(__NR_personality, 0xffffffffUL);
    if (old_personality < 0 ||
        syscall1(__NR_personality, old_personality | READ_IMPLIES_EXEC) < 0) {
        return 80;
    }
    if (!enable_mdwe()) {
        return 81;
    }
    map = do_mmap(0, 4096, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    syscall1(__NR_personality, old_personality);
    if (map != -EACCES) {
        return 82;
    }
    return 0;
}

static int test_mdwe_shmat(void)
{
    guest_slong_t shmid;
    guest_slong_t map;

    shmid = syscall3(__NR_shmget, IPC_PRIVATE, 4096, IPC_CREAT | 0700);
    if (shmid < 0) {
        return 90;
    }
    if (!enable_mdwe()) {
        syscall3(__NR_shmctl, shmid, IPC_RMID, 0);
        return 91;
    }
    map = syscall3(__NR_shmat, shmid, 0, SHM_EXEC);
    syscall3(__NR_shmctl, shmid, IPC_RMID, 0);
    if (map != -EACCES) {
        if (map >= 0) {
            syscall1(__NR_shmdt, map);
        }
        return 92;
    }
    return 0;
}

static int test_fork_state(int no_inherit)
{
    int mdwe = PR_MDWE_REFUSE_EXEC_GAIN |
               (no_inherit ? PR_MDWE_NO_INHERIT : 0);
    int expected_child_mdwe = no_inherit ? 0 : mdwe;
    int tsc_mode = 0;
    int status = -1;
    guest_ulong_t clear_tid = 0;
    guest_ulong_t tid_address = 1;
    guest_slong_t pid;

    if (do_prctl(PR_SET_MDWE, mdwe, 0, 0, 0) != 0 ||
        do_prctl(PR_SET_TSC, PR_TSC_SIGSEGV, 0, 0, 0) != 0 ||
        do_prctl(PR_TIMER_CREATE_RESTORE_IDS,
                 PR_TIMER_CREATE_RESTORE_IDS_ON, 0, 0, 0) != 0 ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_SET_SLOTS,
                 32, 0, 0) != 0 ||
        syscall1(__NR_set_tid_address, (guest_slong_t)&clear_tid) <= 0) {
        return 110;
    }
    pid = syscall1(__NR_fork, 0);
    if (pid < 0) {
        return 111;
    }
    if (pid == 0) {
        int result = 0;

        if (do_prctl(PR_GET_MDWE, 0, 0, 0, 0) != expected_child_mdwe ||
            do_prctl(PR_GET_TSC, (guest_slong_t)&tsc_mode, 0, 0, 0) != 0 ||
            tsc_mode != PR_TSC_SIGSEGV ||
            do_prctl(PR_TIMER_CREATE_RESTORE_IDS,
                     PR_TIMER_CREATE_RESTORE_IDS_GET, 0, 0, 0) != 0 ||
            do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_GET_SLOTS,
                     0, 0, 0) != 0 ||
            do_prctl(PR_GET_TID_ADDRESS,
                     (guest_slong_t)&tid_address, 0, 0, 0) ||
            tid_address != 0) {
            result = 112;
        }
        syscall1(__NR_exit_group, result);
    }
    if (syscall4(__NR_wait4, pid, (guest_slong_t)&status, 0, 0) != pid ||
        status) {
        return 113;
    }
    if (do_prctl(PR_GET_MDWE, 0, 0, 0, 0) != mdwe ||
        do_prctl(PR_GET_TSC, (guest_slong_t)&tsc_mode, 0, 0, 0) != 0 ||
        tsc_mode != PR_TSC_SIGSEGV ||
        do_prctl(PR_TIMER_CREATE_RESTORE_IDS,
                 PR_TIMER_CREATE_RESTORE_IDS_GET, 0, 0, 0) != 1 ||
        do_prctl(PR_FUTEX_HASH, PR_FUTEX_HASH_GET_SLOTS,
                 0, 0, 0) != 32 ||
        do_prctl(PR_GET_TID_ADDRESS,
                 (guest_slong_t)&tid_address, 0, 0, 0) ||
        tid_address != (guest_ulong_t)&clear_tid) {
        return 114;
    }
    return 0;
}

static int test_set_mm_privileged(const char *self_path)
{
    static const char cmdline[] = "lat-prctl-mm";
    static const char cmdline_path[] = "/proc/self/cmdline";
    static const char environ[] = "E=1";
    static const char environ_path[] = "/proc/self/environ";
    static const char proctitle[] = "lat-prctl-title";
    static const char auxv_path[] = "/proc/self/auxv";
    static const char exe_path[] = "/proc/self/exe";
    guest_ulong_t replacement_auxv[4] = { 123, 456, 0, 0 };
    guest_ulong_t second_auxv[4] = { 789, 321, 0, 0 };
    guest_ulong_t auxv_buffer[8] = { 0 };
    PrctlMmMap mm;
    char buffer[512];
    char renamed_path[256];
    guest_slong_t map;
    guest_slong_t fd;
    guest_slong_t len;
    guest_ulong_t self_len;
    guest_ulong_t i;

    map = do_mmap(0, 16384, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map < 0) {
        return 120;
    }
    for (i = 0; i < sizeof(cmdline); i++) {
        ((char *)map)[i] = cmdline[i];
    }
    for (i = 0; i < 4; i++) {
        ((char *)map)[128 + i] = "E=1\0"[i];
    }
    mm.start_code = map;
    mm.end_code = map + 64;
    mm.start_data = map + 128;
    mm.end_data = map + 256;
    mm.start_brk = map + 4096;
    mm.brk = map + 8192;
    mm.start_stack = map + 12288;
    mm.arg_start = map;
    mm.arg_end = map + sizeof(cmdline);
    mm.env_start = map + 128;
    mm.env_end = map + 132;
    mm.auxv = (guest_ulong_t)replacement_auxv;
    mm.auxv_size = sizeof(replacement_auxv);
    mm.exe_fd = ~0U;

    if (do_prctl(PR_SET_MM, PR_SET_MM_MAP, (guest_slong_t)&mm, sizeof(mm), 0)) {
        return 121;
    }
    fd = syscall4(__NR_openat, AT_FDCWD, (guest_slong_t)cmdline_path, 0, 0);
    if (fd < 0) {
        return 122;
    }
    len = syscall3(__NR_read, fd, (guest_slong_t)buffer, sizeof(buffer));
    syscall1(__NR_close, fd);
    if (len != sizeof(cmdline) ||
        !bytes_equal(buffer, cmdline, sizeof(cmdline)) ||
        syscall1(__NR_brk, 0) != map + 8192) {
        return 122;
    }
    len = read_file(environ_path, buffer, sizeof(buffer));
    if (len != sizeof(environ) ||
        !bytes_equal(buffer, environ, sizeof(environ))) {
        return 122;
    }

    for (i = 0; i < sizeof(proctitle); i++) {
        ((char *)map)[512 + i] = proctitle[i];
    }
    mm.arg_start = map + 512;
    mm.arg_end = map + 516;
    mm.env_start = mm.arg_end;
    mm.env_end = map + 512 + sizeof(proctitle);
    mm.auxv_size = 0;
    if (do_prctl(PR_SET_MM, PR_SET_MM_MAP, (guest_slong_t)&mm, sizeof(mm), 0)) {
        return 122;
    }
    len = read_file(cmdline_path, buffer, sizeof(buffer));
    if (len != sizeof(proctitle) ||
        !bytes_equal(buffer, proctitle, sizeof(proctitle))) {
        return 122;
    }

    if (do_prctl(PR_GET_AUXV, (guest_slong_t)auxv_buffer,
                 sizeof(replacement_auxv), 0, 0) !=
            54 * sizeof(guest_ulong_t) ||
        !bytes_equal((char *)auxv_buffer, (char *)replacement_auxv,
                     sizeof(replacement_auxv))) {
        return 123;
    }
    fd = syscall4(__NR_openat, AT_FDCWD, (guest_slong_t)auxv_path, 0, 0);
    if (fd < 0) {
        return 123;
    }
    len = syscall3(__NR_read, fd, (guest_slong_t)auxv_buffer,
                   sizeof(auxv_buffer));
    syscall1(__NR_close, fd);
    if (len != sizeof(replacement_auxv) ||
        !bytes_equal((char *)auxv_buffer, (char *)replacement_auxv,
                     sizeof(replacement_auxv))) {
        return 123;
    }

    if (do_prctl(PR_SET_MM, PR_SET_MM_START_CODE, map, 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, PR_SET_MM_END_CODE, map + 64, 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, PR_SET_MM_START_DATA, map + 128, 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, PR_SET_MM_END_DATA, map + 256, 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, PR_SET_MM_START_BRK, map + 4096, 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, PR_SET_MM_BRK, map + 8192, 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, PR_SET_MM_START_STACK, map + 12288,
                 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, PR_SET_MM_ARG_START, map, 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, PR_SET_MM_ARG_END, map + 4, 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, PR_SET_MM_ENV_START, map + 128,
                 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, PR_SET_MM_ENV_END, map + 132,
                 0, 0) != -EPERM) {
        return 124;
    }
    fd = syscall4(__NR_openat, AT_FDCWD, (guest_slong_t)cmdline_path, 0, 0);
    if (fd < 0) {
        return 125;
    }
    len = syscall3(__NR_read, fd, (guest_slong_t)buffer, sizeof(buffer));
    syscall1(__NR_close, fd);
    if (len != sizeof(proctitle) ||
        !bytes_equal(buffer, proctitle, sizeof(proctitle))) {
        return 125;
    }

    if (do_prctl(PR_SET_MM, PR_SET_MM_AUXV, (guest_slong_t)second_auxv,
                 sizeof(second_auxv), 0) != -EPERM ||
        do_prctl(PR_GET_AUXV, (guest_slong_t)auxv_buffer,
                 sizeof(replacement_auxv), 0, 0) < 0 ||
        !bytes_equal((char *)auxv_buffer, (char *)replacement_auxv,
                     sizeof(replacement_auxv))) {
        return 126;
    }
    if (do_prctl(PR_SET_MM, PR_SET_MM_START_STACK,
                 map + 0x100000, 0, 0) != -EPERM ||
        do_prctl(PR_SET_MM, 99, map, 0, 0) != -EPERM) {
        return 127;
    }

    fd = syscall4(__NR_openat, AT_FDCWD, (guest_slong_t)exe_path, 0, 0);
    if (fd < 0 ||
        do_prctl(PR_SET_MM, PR_SET_MM_EXE_FILE, fd, 0, 0) != -EPERM) {
        return 128;
    }
    syscall1(__NR_close, fd);
    fd = syscall4(__NR_openat, AT_FDCWD, (guest_slong_t)exe_path, 0, 0);
    if (fd < 0) {
        return 128;
    }
    syscall1(__NR_close, fd);

    if (!self_path) {
        return 129;
    }
    self_len = string_length(self_path);
    fd = syscall4(__NR_openat, AT_FDCWD, (guest_slong_t)self_path, 0, 0);
    if (fd < 0) {
        return 129;
    }
    mm.exe_fd = fd;
    if (do_prctl(PR_SET_MM, PR_SET_MM_MAP, (guest_slong_t)&mm, sizeof(mm), 0)) {
        syscall1(__NR_close, fd);
        return 129;
    }
    syscall1(__NR_close, fd);
    len = syscall3(__NR_readlink, (guest_slong_t)exe_path,
                   (guest_slong_t)buffer, sizeof(buffer));
    if (len != (guest_slong_t)self_len ||
        !bytes_equal(buffer, self_path, self_len)) {
        return 129;
    }
    for (i = 0; self_path[i] && i + 9 < sizeof(renamed_path); i++) {
        renamed_path[i] = self_path[i];
    }
    if (self_path[i] || i + 9 >= sizeof(renamed_path)) {
        return 129;
    }
    renamed_path[i++] = '.';
    renamed_path[i++] = 'r';
    renamed_path[i++] = 'e';
    renamed_path[i++] = 'n';
    renamed_path[i++] = 'a';
    renamed_path[i++] = 'm';
    renamed_path[i++] = 'e';
    renamed_path[i++] = 'd';
    renamed_path[i] = 0;
    if (syscall2(__NR_rename, (guest_slong_t)self_path,
                 (guest_slong_t)renamed_path) != 0) {
        return 129;
    }
    len = syscall3(__NR_readlink, (guest_slong_t)exe_path,
                   (guest_slong_t)buffer, sizeof(buffer));
    if (len != (guest_slong_t)i || !bytes_equal(buffer, renamed_path, i)) {
        return 129;
    }
    if (syscall1(__NR_unlink, (guest_slong_t)renamed_path) != 0) {
        return 129;
    }
    len = syscall3(__NR_readlink, (guest_slong_t)exe_path,
                   (guest_slong_t)buffer, sizeof(buffer));
    if (len != (guest_slong_t)(i + 10) ||
        !bytes_equal(buffer, renamed_path, i) ||
        !bytes_equal(buffer + i, " (deleted)", 10)) {
        return 129;
    }
    return 0;
}

static int test_native_exec_env(const char *helper_path, int at_empty_path)
{
    static char empty_path[] = "";
    static char mdwe_env[] = "_LATX_GUEST_MDWE=forged";
    static char tsc_env[] = "_LATX_GUEST_TSC=forged";
    static char keep_env[] = "KEEP=1";
    char *argv[] = { (char *)helper_path, 0 };
    char *envp[] = { mdwe_env, tsc_env, keep_env, 0 };

    if (!helper_path ||
        do_prctl(PR_SET_MDWE, PR_MDWE_REFUSE_EXEC_GAIN, 0, 0, 0) ||
        do_prctl(PR_SET_TSC, PR_TSC_SIGSEGV, 0, 0, 0)) {
        return 130;
    }
    if (at_empty_path) {
        guest_slong_t fd = syscall4(__NR_openat, AT_FDCWD,
                                    (guest_slong_t)helper_path, 0, 0);

        if (fd < 0) {
            return 131;
        }
        syscall5(__NR_execveat, fd, (guest_slong_t)empty_path,
                 (guest_slong_t)argv, (guest_slong_t)envp, AT_EMPTY_PATH);
    } else {
        syscall3(__NR_execve, (guest_slong_t)helper_path, (guest_slong_t)argv,
                 (guest_slong_t)envp);
    }
    return 131;
}

static int test_script_exec(const char *dir_path)
{
    static char script_name[] = "state-script";
    static char stage_name[] = "k";
    char *argv[] = { script_name, stage_name, 0 };
    guest_slong_t dirfd;

    if (!dir_path ||
        do_prctl(PR_SET_MDWE, PR_MDWE_REFUSE_EXEC_GAIN, 0, 0, 0) ||
        do_prctl(PR_SET_TSC, PR_TSC_SIGSEGV, 0, 0, 0)) {
        return 132;
    }
    dirfd = syscall4(__NR_openat, AT_FDCWD, (guest_slong_t)dir_path,
                     O_DIRECTORY, 0);
    if (dirfd < 0) {
        return 132;
    }
    syscall5(__NR_execveat, dirfd, (guest_slong_t)script_name,
             (guest_slong_t)argv, 0, 0);
    return 132;
}

static int test_script_inherit(void)
{
    int tsc_mode = 0;

    return do_prctl(PR_GET_MDWE, 0, 0, 0, 0) ==
               PR_MDWE_REFUSE_EXEC_GAIN &&
           do_prctl(PR_GET_TSC, (guest_slong_t)&tsc_mode, 0, 0, 0) == 0 &&
           tsc_mode == PR_TSC_SIGSEGV ? 0 : 133;
}

static int test_mdwe_inherit(void)
{
    return do_prctl(PR_GET_MDWE, 0, 0, 0, 0) ==
           PR_MDWE_REFUSE_EXEC_GAIN ? 0 : 134;
}

static int test_exec_race(const char *target_path, int use_execveat)
{
    static char mode[] = "l";
    static char keep_env[] = "KEEP=1";
    char *argv[] = { (char *)target_path, mode, 0 };
    char *envp[] = { keep_env, 0 };

    if (!target_path ||
        do_prctl(PR_SET_MDWE, PR_MDWE_REFUSE_EXEC_GAIN, 0, 0, 0)) {
        return 135;
    }
    if (use_execveat) {
        syscall5(__NR_execveat, AT_FDCWD, (guest_slong_t)target_path,
                 (guest_slong_t)argv, (guest_slong_t)envp, 0);
    } else {
        syscall3(__NR_execve, (guest_slong_t)target_path,
                 (guest_slong_t)argv, (guest_slong_t)envp);
    }
    return 135;
}

int test_main(guest_ulong_t *stack)
{
    guest_ulong_t argc = stack[0];
    char **argv = (char **)&stack[1];
    char mode;

    if (argc < 2 || argc > 3 || !argv[1]) {
        return 100;
    }
    if (argc == 3 && argv[2] && argv[2][0] == 'k') {
        return test_script_inherit();
    }
    if (argc == 3 && argv[2] && argv[2][0] == 'l') {
        return test_mdwe_inherit();
    }
    mode = argv[1][0];
    if (mode == 'p') {
        return test_process_controls();
    }
    if (mode == 'v') {
        return test_vma_name(argc == 3 && argv[2][0] == 'h');
    }
    if (mode == 'm') {
        return test_mdwe_basic();
    }
    if (mode == 'r') {
        return test_mdwe_personality();
    }
    if (mode == 's') {
        return test_mdwe_shmat();
    }
    if (mode == 'f') {
        return test_fork_state(1);
    }
    if (mode == 'i') {
        return test_fork_state(0);
    }
    if (mode == 'x') {
        return test_set_mm_privileged(argc == 3 ? argv[2] : 0);
    }
    if (mode == 'e') {
        return test_native_exec_env(argc == 3 ? argv[2] : 0, 0);
    }
    if (mode == 'a') {
        return test_native_exec_env(argc == 3 ? argv[2] : 0, 1);
    }
    if (mode == 'd') {
        return test_script_exec(argc == 3 ? argv[2] : 0);
    }
    if (mode == 'k') {
        return test_script_inherit();
    }
    if (mode == 'l') {
        return test_mdwe_inherit();
    }
    if (mode == 't') {
        return test_exec_race(argc == 3 ? argv[2] : 0, 0);
    }
    if (mode == 'u') {
        return test_exec_race(argc == 3 ? argv[2] : 0, 1);
    }
    return 101;
}

__asm__(".section .text\n"
        ".global _start\n"
        ".type _start,@function\n"
        "_start:\n"
        "mov %rsp,%rdi\n"
        "andq $-16,%rsp\n"
        "call test_main\n"
        "mov %eax,%edi\n"
        "mov $231,%eax\n"
        "syscall\n"
        "ud2\n"
        ".size _start,.-_start\n"
        ".section .note.GNU-stack,\"\",@progbits\n");
