/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "syscall-tunnel.h"

#ifdef CONFIG_LATX_SYSCALL_TUNNEL
const bool syscall_optimize_confirm[] = {
    false,      /*  sys_restart_syscall         0 */
    false,      /*  sys_exit                    1 */
    false,      /*  sys_ni_syscall              2 sys_fork */
    false,      /*  sys_read                    3 */
    false,      /*  sys_write                   4 */
    false,      /*  sys_open                    5 */
    false,      /*  sys_close                   6 */
    false,      /*  sys_ni_syscall              7 sys_waitpid */
    false,      /*  sys_creat                   8 */
    false,      /*  sys_link                    9 */
    false,      /*  sys_unlink                  10 */
    false,      /*  sys_execve                  11 */
    false,      /*  sys_chdir                   12 */
    false,      /*  sys_ni_syscall              13 sys_time */
    false,      /*  sys_mknod                   14 */
    false,      /*  sys_chmod                   15 */
    false,      /*  sys_lchown16                16 */
    false,      /*  sys_ni_syscall              17 break */
    false,      /*  sys_ni_syscall              18 sys_stat */
    false,      /*  sys_lseek                   19 */
    false,       /*  sys_getpid                 20 */
    false,      /*  sys_mount                   21 */
    false,      /*  sys_ni_syscall              22 sys_oldumount */
    false,      /*  sys_setuid16                23 */
    false,      /*  sys_getuid16                24 */
    false,      /*  sys_ni_syscall              25 sys_stime */
    false,      /*  sys_ptrace                  26 */
    false,      /*  sys_alarm                   27 */
    false,      /*  sys_ni_syscall              28 sys_fstat */
    false,      /*  sys_ni_syscall              29 sys_pause */
    false,      /*  sys_ni_syscall              30 sys_utime */
    false,      /*  sys_ni_syscall              31 stty */
    false,      /*  sys_ni_syscall              32 gtty */
    false,      /*  sys_access                  33 */
    false,      /*  sys_ni_syscall              34 sys_nice */
    false,      /*  sys_ni_syscall              35 ftime */
    false,      /*  sys_sync                    36 */
    false,      /*  sys_kill                    37 */
    false,      /*  sys_rename                  38 */
    false,      /*  sys_mkdir                   39 */
    false,      /*  sys_rmdir                   40 */
    false,      /*  sys_dup                     41 */
    false,      /*  sys_pipe                    42 */
    false,      /*  sys_times                   43 */
    false,      /*  sys_ni_syscall              44 prof */
    false,      /*  sys_brk                     45 */
    false,      /*  sys_setgid16                46 */
    false,      /*  sys_getgid16                47 */
    false,      /*  sys_ni_syscall              48 sys_signal */
    false,      /*  sys_geteuid16               49 */
    false,      /*  sys_getegid16               50 */
    false,      /*  sys_acct                    51 */
    false,      /*  sys_umount                  52 */
    false,      /*  sys_ni_syscall              53 lock */
    false,      /*  sys_ioctl                   54 */
    false,      /*  sys_fcntl                   55 */
    false,      /*  sys_ni_syscall              56 mxp */
    false,      /*  sys_setpgid                 57 */
    false,      /*  sys_ni_syscall              58 ulimit */
    false,      /*  sys_ni_syscall              59 sys_olduname */
    false,      /*  sys_umask                   60 */
    false,      /*  sys_chroot                  61 */
    false,      /*  sys_ustat                   62 */
    false,      /*  sys_dup2                    63 */
    false,      /*  sys_getppid                 64 */
    false,      /*  sys_getpgrp                 65 */
    false,      /*  sys_setsid                  66 */
    false,      /*  sys_ni_syscall              67 sys_sigaction */
    false,      /*  sys_sgetmask                68 */
    false,      /*  sys_ssetmask                69 */
    false,      /*  sys_setreuid16              70 */
    false,      /*  sys_setregid16              71 */
    false,      /*  sys_ni_syscall              72 sys_sigsuspend */
    false,      /*  sys_ni_syscall              73 sys_sigpending */
    false,      /*  sys_sethostname             74 */
    false,      /*  sys_setrlimit               75 */
    false,      /*  sys_ni_syscall              76 sys_old_getrlimit */
    false,      /*  sys_getrusage               77 */
    false,      /*  sys_gettimeofday            78 */
    false,      /*  sys_settimeofday            79 */
    false,      /*  sys_getgroups16             80 */
    false,      /*  sys_setgroups16             81 */
    false,      /*  sys_ni_syscall              82 sys_old_select */
    false,      /*  sys_symlink                 83 */
    false,      /*  sys_ni_syscall              84 sys_lstat */
    false,      /*  sys_readlink                85 */
    false,      /*  sys_uselib                  86 */
    false,      /*  sys_swapon                  87 */
    false,      /*  sys_reboot                  88 */
    false,      /*  sys_ni_syscall              89 sys_old_readdir */
    false,      /*  sys_ni_syscall              90 sys_old_mmap */
    false,      /*  sys_munmap                  91 */
    false,      /*  sys_truncate                92 */
    false,      /*  sys_ftruncate               93 */
    false,      /*  sys_fchmod                  94 */
    false,      /*  sys_fchown16                95 */
    false,      /*  sys_getpriority             96 */
    false,      /*  sys_setpriority             97 */
    false,      /*  sys_ni_syscall              98 profil */
    false,      /*  sys_statfs                  99 */
    false,      /*  sys_fstatfs                 100 */
    false,      /*  sys_ni_syscall              101 sys_ioperm */
    false,      /*  sys_socketcall              102 */
    false,      /*  sys_syslog                  103 */
    false,      /*  sys_setitimer               104 */
    false,      /*  sys_getitimer               105 */
    false,      /*  sys_newstat                 106 */
    false,      /*  sys_newlstat                107 */
    false,      /*  sys_newfstat                108 */
    false,      /*  sys_ni_syscall              109 sys_uname */
    false,      /*  sys_ni_syscall              110 sys_iopl */
    false,      /*  sys_vhangup                 111 */
    false,      /*  sys_ni_syscall              112 idel */
    false,      /*  sys_vm86old                 113 */
    false,      /*  sys_wait4                   114 */
    false,      /*  sys_swapoff                 115 */
    false,      /*  sys_sysinfo                 116 */
    false,      /*  sys_ipc                     117 */
    false,      /*  sys_fsync                   118 */
    false,      /*  sys_ni_syscall              119 sys_sigreturn */
    false,      /*  sys_clone                   120 */
    false,      /*  sys_setdomainname           121 */
    false,      /*  sys_newuname                122 */
    false,      /*  sys_modify_ldt              123 */
    false,      /*  sys_adjtimex                124 */
    false,      /*  sys_mprotect                125 */
    false,      /*  sys_ni_syscall              126 sys_sigprocmask */
    false,      /*  sys_ni_syscall              127 create_module */
    false,      /*  sys_init_module             128 */
    false,      /*  sys_delete_module           129 */
    false,      /*  sys_ni_syscall              130 get_kernel_syms */
    false,      /*  sys_quotactl                131 */
    false,      /*  sys_getpgid                 132 */
    false,      /*  sys_fchdir                  133 */
    false,      /*  sys_bdflush                 134 */
    false,      /*  sys_sysfs                   135 */
    false,      /*  sys_personality             136 */
    false,      /*  sys_ni_syscall              137 afs_syscall */
    false,      /*  sys_setfsuid16              138 */
    false,      /*  sys_setfsgid16              139 */
    false,      /*  sys_ni_syscall              140 sys_llseek */
    false,      /*  sys_getdents                141 */
    false,      /*  sys_select                  142 */
    false,      /*  sys_flock                   143 */
    false,      /*  sys_msync                   144 */
    false,      /*  sys_readv                   145 */
    false,      /*  sys_writev                  146 */
    false,      /*  sys_getsid                  147 */
    false,      /*  sys_fdatasync               148 */
    false,      /*  sys_sysctl                  149 */
    false,      /*  sys_mlock                   150 */
    false,      /*  sys_munlock                 151 */
    false,      /*  sys_mlockall                152 */
    false,      /*  sys_munlockall              153 */
    false,      /*  sys_sched_setparam          154 */
    false,      /*  sys_sched_getparam          155 */
    false,      /*  sys_sched_setscheduler      156 */
    false,      /*  sys_sched_getscheduler      157 */
    false,      /*  sys_sched_yield             158 */
    false,      /*  sys_sched_get_priority_max  159 */
    false,      /*  sys_sched_get_priority_min  160 */
    false,      /*  sys_sched_rr_get_interval   161 */
    false,      /*  sys_nanosleep               162 */
    false,      /*  sys_mremap                  163 */
    false,      /*  sys_setresuid16             164 */
    false,      /*  sys_getresuid16             165 */
    false,      /*  sys_vm86                    166 */
    false,      /*  sys_ni_syscall              167 query_module */
    false,      /*  sys_poll                    168 */
    false,      /*  sys_ni_syscall              169 nfsservctl */
    false,      /*  sys_setresgid16             170 */
    false,      /*  sys_getresgid16             171 */
    false,      /*  sys_prctl                   172 */
    false,      /*  sys_rt_sigreturn            173 */
    false,      /*  sys_rt_sigaction            174 */
    false,      /*  sys_rt_sigprocmask          175 */
    false,      /*  sys_rt_sigpending           176 */
    false,      /*  sys_rt_sigtimedwait         177 */
    false,      /*  sys_rt_sigqueueinfo         178 */
    false,      /*  sys_rt_sigsuspend           179 */
    false,      /*  sys_pread64                 180 */
    false,      /*  sys_pwrite64                181 */
    false,      /*  sys_chown16                 182 */
    false,      /*  sys_getcwd                  183 */
    false,      /*  sys_capget                  184 */
    false,      /*  sys_capset                  185 */
    false,      /*  sys_sigaltstack             186 */
    false,      /*  sys_sendfile                187 */
    false,      /*  sys_ni_syscall              188 getpmsg */
    false,      /*  sys_ni_syscall              189 putpmsg */
    false,      /*  sys_ni_syscall              190 sys_vfork */
    false,      /*  sys_getrlimit               191 */
    false,      /*  sys_mmap_pgoff              192 */
    false,      /*  sys_ni_syscall              193 sys_truncate64 */
    false,      /*  sys_ni_syscall              194 sys_ftruncate64 */
    false,      /*  sys_ni_syscall              195 sys_stat64 */
    false,      /*  sys_ni_syscall              196 sys_lstat64 */
    false,      /*  sys_ni_syscall              197 sys_fstat64 */
    false,      /*  sys_lchown                  198 */
    false,      /*  sys_getuid                  199 */
    false,      /*  sys_getgid                  200 */
    false,      /*  sys_geteuid                 201 */
    false,      /*  sys_getegid                 202 */
    false,      /*  sys_setreuid                203 */
    false,      /*  sys_setregid                204 */
    false,      /*  sys_getgroups               205 */
    false,      /*  sys_setgroups               206 */
    false,      /*  sys_fchown                  207 */
    false,      /*  sys_setresuid               208 */
    false,      /*  sys_getresuid               209 */
    false,      /*  sys_setresgid               210 */
    false,      /*  sys_getresgid               211 */
    false,      /*  sys_chown                   212 */
    false,      /*  sys_setuid                  213 */
    false,      /*  sys_setgid                  214 */
    false,      /*  sys_setfsuid                215 */
    false,      /*  sys_setfsgid                216 */
    false,      /*  sys_pivot_root              217 */
    false,      /*  sys_mincore                 218 */
    false,      /*  sys_madvise                 219 */
    false,      /*  sys_getdents64              220 */
    false,      /*  sys_ni_syscall              221 sys_fcntl64 */
    false,      /*  sys_ni_syscall              222 is unused */
    false,      /*  sys_ni_syscall              223 is unused */
    false,      /*  sys_gettid                  224 */
    false,      /*  sys_readahead               225 */
    false,      /*  sys_setxattr                226 */
    false,      /*  sys_lsetxattr               227 */
    false,      /*  sys_fsetxattr               228 */
    false,      /*  sys_getxattr                229 */
    false,      /*  sys_lgetxattr               230 */
    false,      /*  sys_fgetxattr               231 */
    false,      /*  sys_listxattr               232 */
    false,      /*  sys_llistxattr              233 */
    false,      /*  sys_flistxattr              234 */
    false,      /*  sys_removexattr             235 */
    false,      /*  sys_lremovexattr            236 */
    false,      /*  sys_fremovexattr            237 */
    false,      /*  sys_tkill                   238 */
    false,      /*  sys_sendfile64              239 */
    false,      /*  sys_futex                   240 */
    false,      /*  sys_sched_setaffinity       241 */
    false,      /*  sys_sched_getaffinity       242 */
    false,      /*  sys_ni_syscall              243 sys_set_thread_area */
    false,      /*  sys_ni_syscall              244 sys_get_thread_area */
    false,      /*  sys_io_setup                245 */
    false,      /*  sys_io_destroy              246 */
    false,      /*  sys_io_getevents            247 */
    false,      /*  sys_io_submit               248 */
    false,      /*  sys_io_cancel               249 */
    false,      /*  sys_fadvise64               250 */
    false,      /*  sys_ni_syscall              251 is available for reuse*/
    false,      /*  sys_exit_group              252 */
    false,      /*  sys_lookup_dcookie          253 */
    false,      /*  sys_epoll_create            254 */
    false,      /*  sys_epoll_ctl               255 */
    false,      /*  sys_epoll_wait              256 */
    false,      /*  sys_remap_file_pages        257 */
    false,      /*  sys_set_tid_address         258 */
    false,      /*  sys_timer_create            259 */
    false,      /*  sys_timer_settime           260 */
    false,      /*  sys_timer_gettime           261 */
    false,      /*  sys_timer_getoverrun        262 */
    false,      /*  sys_timer_delete            263 */
    false,      /*  sys_clock_settime           264 */
    false,      /*  sys_clock_gettime           265 */
    false,      /*  sys_clock_getres            266 */
    false,      /*  sys_clock_nanosleep         267 */
    false,      /*  sys_statfs64                268 */
    false,      /*  sys_fstatfs64               269 */
    false,      /*  sys_tgkill                  270 */
    false,      /*  sys_utimes                  271 */
    false,      /*  sys_fadvise64_64            272 */
    false,      /*  sys_ni_syscall              273 vserver */
    false,      /*  sys_mbind                   274 */
    false,      /*  sys_get_mempolicy           275 */
    false,      /*  sys_set_mempolicy           276 */
    false,      /*  sys_mq_open                 277 */
    false,      /*  sys_mq_unlink               278 */
    false,      /*  sys_mq_timedsend            279 */
    false,      /*  sys_mq_timedreceive         280 */
    false,      /*  sys_mq_notify               281 */
    false,      /*  sys_mq_getsetattr           282 */
    false,      /*  sys_kexec_load              283 */
    false,      /*  sys_waitid                  284 */
    false,      /*  sys_ni_syscall              285 sys_setaltroot */
    false,      /*  sys_add_key                 286 */
    false,      /*  sys_request_key             287 */
    false,      /*  sys_keyctl                  288 */
    false,      /*  sys_ioprio_set              289 */
    false,      /*  sys_ioprio_get              290 */
    false,      /*  sys_inotify_init            291 */
    false,      /*  sys_inotify_add_watch       292 */
    false,      /*  sys_inotify_rm_watch        293 */
    false,      /*  sys_migrate_pages           294 */
    false,      /*  sys_openat                  295 */
    false,      /*  sys_mkdirat                 296 */
    false,      /*  sys_mknodat                 297 */
    false,      /*  sys_fchownat                298 */
    false,      /*  sys_futimesat               299 */
    false,      /*  sys_ni_syscall              300 sys_fstatat64 */
    false,      /*  sys_unlinkat                301 */
    false,      /*  sys_renameat                302 */
    false,      /*  sys_linkat                  303 */
    false,      /*  sys_symlinkat               304 */
    false,      /*  sys_readlinkat              305 */
    false,      /*  sys_fchmodat                306 */
    false,      /*  sys_faccessat               307 */
    false,      /*  sys_pselect6                308 */
    false,      /*  sys_ppoll                   309 */
    false,      /*  sys_unshare                 310 */
    false,      /*  sys_set_robust_list         311 */
    false,      /*  sys_get_robust_list         312 */
    false,      /*  sys_splice                  313 */
    false,      /*  sys_sync_file_range         314 */
    false,      /*  sys_tee                     315 */
    false,      /*  sys_vmsplice                316 */
    false,      /*  sys_move_pages              317 */
    false,      /*  sys_getcpu                  318 */
    false,      /*  sys_epoll_pwait             319 */
    false,      /*  sys_utimensat               320 */
    false,      /*  sys_signalfd                321 */
    false,      /*  sys_timerfd_create          322 */
    false,      /*  sys_eventfd                 323 */
    false,      /*  sys_fallocate               324 */
    false,      /*  sys_timerfd_settime         325 */
    false,      /*  sys_timerfd_gettime         326 */
    false,      /*  sys_signalfd4               327 */
    false,      /*  sys_eventfd2                328 */
    false,      /*  sys_epoll_create1           329 */
    false,      /*  sys_dup3                    330 */
    false,      /*  sys_pipe2                   331 */
    false,      /*  sys_inotify_init1           332 */
    false,      /*  sys_preadv                  333 */
    false,      /*  sys_pwritev                 334 */
    false,      /*  sys_rt_tgsigqueueinfo       335 */
    false,      /*  sys_perf_event_open         336 */
    false,      /*  sys_recvmmsg                337 */
    false,      /*  sys_fanotify_init           338 */
    false,      /*  sys_fanotify_mark           339 */
    false,      /*  sys_prlimit64               340 */
    false,      /*  sys_name_to_handle_at       341 */
    false,      /*  sys_open_by_handle_at       342 */
    false,      /*  sys_clock_adjtime           343 */
    false,      /*  sys_syncfs                  344 */
    false,      /*  sys_sendmmsg                345 */
    false,      /*  sys_setns                   346 */
    false,      /*  sys_process_vm_readv        347 */
    false,      /*  sys_process_vm_writev       348 */
    false,      /*  sys_kcmp                    349 */
    false,      /*  sys_finit_module            350 */
    false,      /*  sys_sched_setattr           351 */
    false,      /*  sys_sched_getattr           352 */
    false,      /*  sys_renameat2               353 */
    false,      /*  sys_seccomp                 354 */
    false,      /*  sys_getrandom               355 */
    false,      /*  sys_memfd_create            356 */
    false,      /*  sys_bpf                     357 */
    false,      /*  sys_execveat                358 */
    false,      /*  sys_socket                  359 */
    false,      /*  sys_socketpair              360 */
    false,      /*  sys_bind                    361 */
    false,      /*  sys_connect                 362 */
    false,      /*  sys_listen                  363 */
    false,      /*  sys_accept4                 364 */
    false,      /*  sys_getsockopt              365 */
    false,      /*  sys_setsockopt              366 */
    false,      /*  sys_getsockname             367 */
    false,      /*  sys_getpeername             368 */
    false,      /*  sys_sendto                  369 */
    false,      /*  sys_sendmsg                 370 */
    false,      /*  sys_recvfrom                371 */
    false,      /*  sys_recvmsg                 372 */
    false,      /*  sys_shutdown                373 */
    false,      /*  sys_userfaultfd             374 */
    false,      /*  sys_membarrier              375 */
    false,      /*  sys_mlock2                  376 */
    false,      /*  sys_copy_file_range         377 */
    false,      /*  sys_preadv2                 378 */
    false,      /*  sys_pwritev2                379 */
    false,      /*  sys_pkey_mprotect           380 */
    false,      /*  sys_pkey_alloc              381 */
    false,      /*  sys_pkey_free               382 */
    false,      /*  sys_statx                   383 */
    false,      /*  sys_ni_syscall              384 sys_arch_prctl */
    false,      /*  sys_io_pgetevents           385 */
    false       /*  sys_rseq                    386 */
};

bool syscall_is_optimized(int64_t sys_num)
{
    return syscall_optimize_confirm[sys_num];
}

#endif
