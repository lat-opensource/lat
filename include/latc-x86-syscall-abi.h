#ifndef LATC_X86_SYSCALL_ABI_H
#define LATC_X86_SYSCALL_ABI_H

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

/* Linux x86-64 kernel ABI layouts shared with linux-user/syscall.c. */
typedef struct LatcX86Rlimit64 {
    uint64_t rlim_cur;
    uint64_t rlim_max;
} LatcX86Rlimit64;

typedef struct LatcX86Stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t pad0;
    uint64_t st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks;
    uint64_t st_atime_sec;
    uint64_t st_atime_nsec;
    uint64_t st_mtime_sec;
    uint64_t st_mtime_nsec;
    uint64_t st_ctime_sec;
    uint64_t st_ctime_nsec;
    int64_t unused[3];
} LatcX86Stat;

typedef struct LatcX86Sigaction {
    uint64_t handler;
    uint64_t flags;
    uint64_t restorer;
    uint64_t mask;
} LatcX86Sigaction;

typedef struct LatcX86Sysinfo {
    int64_t uptime;
    uint64_t loads[3];
    uint64_t totalram;
    uint64_t freeram;
    uint64_t sharedram;
    uint64_t bufferram;
    uint64_t totalswap;
    uint64_t freeswap;
    uint16_t procs;
    uint16_t pad;
    uint32_t align_pad;
    uint64_t totalhigh;
    uint64_t freehigh;
    uint32_t mem_unit;
    uint32_t tail_pad;
} LatcX86Sysinfo;

_Static_assert(sizeof(LatcX86Rlimit64) == 16,
               "unexpected x86-64 rlimit64 layout");
_Static_assert(sizeof(LatcX86Stat) == 144,
               "unexpected x86-64 stat layout");
_Static_assert(sizeof(LatcX86Sigaction) == 32,
               "unexpected x86-64 sigaction layout");
_Static_assert(sizeof(LatcX86Sysinfo) == 112,
               "unexpected x86-64 sysinfo layout");

static inline uint64_t latc_x86_syscall_result(long result, int host_errno)
{
    return result < 0 ? (uint64_t)-(int64_t)host_errno : (uint64_t)result;
}

static inline int latc_x86_target_to_host_resource(int code)
{
    switch (code) {
    case 0: return RLIMIT_CPU;
    case 1: return RLIMIT_FSIZE;
    case 2: return RLIMIT_DATA;
    case 3: return RLIMIT_STACK;
    case 4: return RLIMIT_CORE;
    case 5: return RLIMIT_RSS;
    case 6: return RLIMIT_NPROC;
    case 7: return RLIMIT_NOFILE;
    case 8: return RLIMIT_MEMLOCK;
    case 9: return RLIMIT_AS;
    case 10: return RLIMIT_LOCKS;
    case 11: return RLIMIT_SIGPENDING;
    case 12: return RLIMIT_MSGQUEUE;
    case 13: return RLIMIT_NICE;
    case 14: return RLIMIT_RTPRIO;
    default: return code;
    }
}

static inline void latc_x86_host_to_target_stat(LatcX86Stat *target,
                                                 const struct stat *host)
{
    memset(target, 0, sizeof(*target));
    target->st_dev = host->st_dev;
    target->st_ino = host->st_ino;
    target->st_nlink = host->st_nlink;
    target->st_mode = host->st_mode;
    target->st_uid = host->st_uid;
    target->st_gid = host->st_gid;
    target->st_rdev = host->st_rdev;
    target->st_size = host->st_size;
    target->st_blksize = host->st_blksize;
    target->st_blocks = host->st_blocks;
    target->st_atime_sec = host->st_atim.tv_sec;
    target->st_atime_nsec = host->st_atim.tv_nsec;
    target->st_mtime_sec = host->st_mtim.tv_sec;
    target->st_mtime_nsec = host->st_mtim.tv_nsec;
    target->st_ctime_sec = host->st_ctim.tv_sec;
    target->st_ctime_nsec = host->st_ctim.tv_nsec;
}

static inline void latc_x86_host_to_target_sysinfo(LatcX86Sysinfo *target,
                                                    const struct sysinfo *host)
{
    memset(target, 0, sizeof(*target));
    target->uptime = host->uptime;
    target->loads[0] = host->loads[0];
    target->loads[1] = host->loads[1];
    target->loads[2] = host->loads[2];
    target->totalram = host->totalram;
    target->freeram = host->freeram;
    target->sharedram = host->sharedram;
    target->bufferram = host->bufferram;
    target->totalswap = host->totalswap;
    target->freeswap = host->freeswap;
    target->procs = host->procs;
    target->totalhigh = host->totalhigh;
    target->freehigh = host->freehigh;
    target->mem_unit = host->mem_unit;
}

#endif
