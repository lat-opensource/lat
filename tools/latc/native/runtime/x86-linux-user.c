#define _GNU_SOURCE

#include "x86-linux-user.h"
#include "latc-x86-syscall-abi.h"
#include "latx-x86-env-offsets.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

typedef struct HostRlimit64 {
    uint64_t rlim_cur;
    uint64_t rlim_max;
} HostRlimit64;

static unsigned char *guest_brk_base;
static unsigned char *guest_brk_current;
static size_t guest_brk_capacity;
static uint64_t guest_clear_tid;
static LatcX86Sigaction guest_sigactions[65];

static uint64_t *reg(unsigned char *env, size_t offset)
{
    return (void *)(env + offset);
}

static long guest_rt_sigaction(uint64_t signal_number, uint64_t action_address,
                               uint64_t old_action_address,
                               uint64_t sigset_size)
{
    if (sigset_size != sizeof(uint64_t) || signal_number < 1 ||
        signal_number > 64 || signal_number == SIGKILL ||
        signal_number == SIGSTOP) {
        errno = EINVAL;
        return -1;
    }
    LatcX86Sigaction *saved = &guest_sigactions[signal_number];
    if (old_action_address) {
        memcpy((void *)(uintptr_t)old_action_address, saved, sizeof(*saved));
    }
    if (action_address) {
        memcpy(saved, (const void *)(uintptr_t)action_address, sizeof(*saved));
    }
    return 0;
}

static long guest_readlinkat(int dirfd, const char *path, void *buffer,
                             size_t size)
{
    if (!strcmp(path, "/proc/self/exe")) {
        static const char guest_exe[] = "/latc-guest";
        size_t length = sizeof(guest_exe) - 1;
        if (length > size) length = size;
        memcpy(buffer, guest_exe, length);
        return (long)length;
    }
    return readlinkat(dirfd, path, buffer, size);
}

static uint64_t guest_brk(uint64_t requested)
{
    if (!guest_brk_base) {
        guest_brk_capacity = 16 * 1024 * 1024;
        guest_brk_base = mmap(NULL, guest_brk_capacity, PROT_NONE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (guest_brk_base == MAP_FAILED) {
            guest_brk_base = NULL;
            guest_brk_capacity = 0;
            return 0;
        }
        guest_brk_current = guest_brk_base;
    }
    if (!requested) return (uint64_t)(uintptr_t)guest_brk_current;
    if (requested < (uint64_t)(uintptr_t)guest_brk_base ||
        requested > (uint64_t)(uintptr_t)guest_brk_base + guest_brk_capacity) {
        return (uint64_t)(uintptr_t)guest_brk_current;
    }
    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t end = (requested + (uint64_t)page_size - 1) &
                    ~((uint64_t)page_size - 1);
    if (end > (uintptr_t)guest_brk_base &&
        mprotect(guest_brk_base, end - (uintptr_t)guest_brk_base,
                 PROT_READ | PROT_WRITE)) {
        return (uint64_t)(uintptr_t)guest_brk_current;
    }
    guest_brk_current = (void *)(uintptr_t)requested;
    return requested;
}

void lat_x86_linux_user_syscall(unsigned char *env)
{
    uint64_t number = *reg(env, LATC_X86_ENV_RAX_OFFSET);
    uint64_t arg1 = *reg(env, LATC_X86_ENV_RDI_OFFSET);
    uint64_t arg2 = *reg(env, LATC_X86_ENV_RSI_OFFSET);
    uint64_t arg3 = *reg(env, LATC_X86_ENV_RDX_OFFSET);
    uint64_t arg4 = *reg(env, LATC_X86_ENV_R10_OFFSET);
    uint64_t arg5 = *reg(env, LATC_X86_ENV_R8_OFFSET);
    uint64_t arg6 = *reg(env, LATC_X86_ENV_R9_OFFSET);
    long result;

    switch (number) {
    case 0:
        result = read((int)arg1, (void *)(uintptr_t)arg2, (size_t)arg3);
        break;
    case 1:
        result = write((int)arg1, (const void *)(uintptr_t)arg2, (size_t)arg3);
        break;
    case 3:
        result = close((int)arg1);
        break;
    case 5: {
        struct stat host;
        result = fstat((int)arg1, &host);
        if (result >= 0) {
            latc_x86_host_to_target_stat((void *)(uintptr_t)arg2, &host);
        }
        break;
    }
    case 8:
        result = lseek((int)arg1, (off_t)(int64_t)arg2, (int)arg3);
        break;
    case 9: {
        void *mapped = mmap((void *)(uintptr_t)arg1, (size_t)arg2, (int)arg3,
                            (int)arg4, (int)arg5, (off_t)arg6);
        *reg(env, LATC_X86_ENV_RAX_OFFSET) = mapped == MAP_FAILED ?
            latc_x86_syscall_result(-1, errno) :
            (uint64_t)(uintptr_t)mapped;
        return;
    }
    case 10:
        result = mprotect((void *)(uintptr_t)arg1, (size_t)arg2, (int)arg3);
        break;
    case 11:
        result = munmap((void *)(uintptr_t)arg1, (size_t)arg2);
        break;
    case 12:
        *reg(env, LATC_X86_ENV_RAX_OFFSET) = guest_brk(arg1);
        return;
    case 13:
        result = guest_rt_sigaction(arg1, arg2, arg3, arg4);
        break;
    case 16:
        result = ioctl((int)arg1, (unsigned long)arg2,
                       (void *)(uintptr_t)arg3);
        break;
    case 39:
        result = getpid();
        break;
    case 60:
        _exit((int)arg1);
    case 72:
        result = fcntl((int)arg1, (int)arg2, arg3);
        break;
    case 82:
        result = rename((const char *)(uintptr_t)arg1,
                        (const char *)(uintptr_t)arg2);
        break;
    case 87:
        result = unlink((const char *)(uintptr_t)arg1);
        break;
    case 89:
        result = guest_readlinkat(AT_FDCWD,
                                  (const void *)(uintptr_t)arg1,
                                  (void *)(uintptr_t)arg2, (size_t)arg3);
        break;
    case 98:
        result = getrusage((int)arg1, (struct rusage *)(uintptr_t)arg2);
        break;
    case 99: {
        struct sysinfo host;
        result = sysinfo(&host);
        if (result >= 0 && arg1) {
            latc_x86_host_to_target_sysinfo((void *)(uintptr_t)arg1, &host);
        }
        break;
    }
    case 100:
        result = times((struct tms *)(uintptr_t)arg1);
        break;
    case 102:
        result = getuid();
        break;
    case 104:
        result = getgid();
        break;
    case 107:
        result = geteuid();
        break;
    case 108:
        result = getegid();
        break;
    case 110:
        result = getppid();
        break;
    case 158:
        switch ((uint32_t)arg1) {
        case 0x1001: *reg(env, LATC_X86_ENV_GS_BASE_OFFSET) = arg2;
                     result = 0; break;
        case 0x1002: *reg(env, LATC_X86_ENV_FS_BASE_OFFSET) = arg2;
                     result = 0; break;
        case 0x1003: *(uint64_t *)(uintptr_t)arg2 =
                         *reg(env, LATC_X86_ENV_FS_BASE_OFFSET);
                     result = 0; break;
        case 0x1004: *(uint64_t *)(uintptr_t)arg2 =
                         *reg(env, LATC_X86_ENV_GS_BASE_OFFSET);
                     result = 0; break;
        default: errno = EINVAL; result = -1; break;
        }
        break;
    case 201: {
        time_t now = time(NULL);
        result = now;
        if (now != (time_t)-1 && arg1) {
            *(int64_t *)(uintptr_t)arg1 = (int64_t)now;
        }
        break;
    }
    case 202:
        result = syscall(SYS_futex, (uint32_t *)(uintptr_t)arg1, (int)arg2,
                         (uint32_t)arg3, (void *)(uintptr_t)arg4,
                         (uint32_t *)(uintptr_t)arg5, (uint32_t)arg6);
        break;
    case 218:
        guest_clear_tid = arg1;
        result = syscall(SYS_gettid);
        break;
    case 228:
        result = clock_gettime((clockid_t)arg1,
                               (struct timespec *)(uintptr_t)arg2);
        break;
    case 231:
        _exit((int)arg1);
    case 257:
        result = openat((int)arg1, (const char *)(uintptr_t)arg2,
                        (int)arg3, (mode_t)arg4);
        break;
    case 262: {
        struct stat host;
        result = fstatat((int)arg1, (const char *)(uintptr_t)arg2, &host,
                         (int)arg4);
        if (result >= 0) {
            latc_x86_host_to_target_stat((void *)(uintptr_t)arg3, &host);
        }
        break;
    }
    case 267:
        result = guest_readlinkat((int)arg1,
                                  (const void *)(uintptr_t)arg2,
                                  (void *)(uintptr_t)arg3, (size_t)arg4);
        break;
    case 273:
        errno = ENOSYS;
        result = -1;
        break;
    case 302: {
        int resource = latc_x86_target_to_host_resource((int)arg2);
        HostRlimit64 new_limit;
        HostRlimit64 old_limit;
        HostRlimit64 *new_pointer = NULL;
        if (arg3) {
            const LatcX86Rlimit64 *target = (const void *)(uintptr_t)arg3;
            new_limit.rlim_cur = target->rlim_cur;
            new_limit.rlim_max = target->rlim_max;
            new_pointer = &new_limit;
        }
        result = syscall(SYS_prlimit64, (pid_t)arg1, resource, new_pointer,
                         arg4 ? &old_limit : NULL);
        if (result >= 0 && arg4) {
            LatcX86Rlimit64 *target = (void *)(uintptr_t)arg4;
            target->rlim_cur = old_limit.rlim_cur;
            target->rlim_max = old_limit.rlim_max;
        }
        break;
    }
    case 318:
        result = getrandom((void *)(uintptr_t)arg1, (size_t)arg2,
                           (unsigned)arg3);
        break;
    case 334:
        (void)guest_clear_tid;
        errno = ENOSYS;
        result = -1;
        break;
    default:
        dprintf(STDERR_FILENO,
                "latc: unsupported x86 syscall %llu args=%#llx,%#llx,%#llx,"
                "%#llx,%#llx,%#llx\n",
                (unsigned long long)number,
                (unsigned long long)arg1, (unsigned long long)arg2,
                (unsigned long long)arg3, (unsigned long long)arg4,
                (unsigned long long)arg5, (unsigned long long)arg6);
        _exit(127);
    }
    *reg(env, LATC_X86_ENV_RAX_OFFSET) =
        latc_x86_syscall_result(result, errno);
}
