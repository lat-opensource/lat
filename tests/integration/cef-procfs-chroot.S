.equ __NR_close, 3
.equ __NR_stat, 4
.equ __NR_fstat, 5
.equ __NR_lstat, 6
.equ __NR_sched_yield, 24
.equ __NR_dup, 32
.equ __NR_dup2, 33
.equ __NR_clone, 56
.equ __NR_exit, 60
.equ __NR_fchdir, 81
.equ __NR_chroot, 161
.equ __NR_futex, 202
.equ __NR_openat, 257
.equ __NR_newfstatat, 262
.equ __NR_unshare, 272
.equ __NR_statx, 332
.equ AT_FDCWD, -100
.equ AT_EMPTY_PATH, 0x1000
.equ O_RDONLY, 0
.equ O_DIRECTORY, 0x10000
.equ STATX_BASIC_STATS, 0x7ff
.equ CLONE_VM, 0x00000100
.equ CLONE_FS, 0x00000200
.equ CLONE_FILES, 0x00000400
.equ CLONE_SIGHAND, 0x00000800
.equ CLONE_THREAD, 0x00010000
.equ CLONE_SYSVSEM, 0x00040000
.equ CLONE_CHILD_CLEARTID, 0x00200000
.equ CLONE_CHILD_SETTID, 0x01000000
.equ CLONE_THREAD_FLAGS, (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND)
.equ CLONE_TID_FLAGS, (CLONE_THREAD | CLONE_SYSVSEM | CLONE_CHILD_CLEARTID)
.equ FUTEX_WAIT, 0
.equ CLONE_NEWUSER, 0x10000000
.equ RACE_ITERATIONS, 10000

.section .rodata
proc_root:
    .asciz "/proc"
host_root:
    .asciz "/"
absolute_task:
    .asciz "/proc/self/task"
chroot_path:
    .asciz "/proc/self/fdinfo"
relative_task:
    .asciz "self/task"
self_path:
    .asciz "self"
task_path:
    .asciz "task"
dot_path:
    .asciz "."
empty_path:
    .asciz ""

.section .bss
.align 8
statbuf:
    .space 144
.align 8
statxbuf:
    .space 256
other_task_path:
    .space 64
self_fd:
    .long 0
.align 16
race_stack:
    .space 65536
.align 4
race_child_tid:
    .long 0
race_ready:
    .long 0
race_stop:
    .long 0
race_error:
    .long 0
race_result:
    .long 0
race_seen:
    .long 0
race_path_seen:
    .long 0
.align 8
race_path:
    .space 8
race_fd:
    .long 0
self_nlink:
    .quad 0
root_fd:
    .long 0
chroot_fd:
    .long 0
root_race_seen:
    .long 0

.section .text
.global _start
.type _start, @function
_start:
    mov 16(%rsp), %rsi
    lea other_task_path(%rip), %rdi
.Lcopy_other_pid:
    lodsb
    test %al, %al
    je .Lappend_task
    stosb
    jmp .Lcopy_other_pid
.Lappend_task:
    movb $'/', (%rdi)
    movb $'t', 1(%rdi)
    movb $'a', 2(%rdi)
    movb $'s', 3(%rdi)
    movb $'k', 4(%rdi)
    movb $0, 5(%rdi)

    xor %r15d, %r15d
    mov 24(%rsp), %rsi
.Lparse_other_nlink:
    movzbl (%rsi), %eax
    test %al, %al
    je .Lother_nlink_ready
    sub $'0', %eax
    imul $10, %r15
    add %rax, %r15
    inc %rsi
    jmp .Lparse_other_nlink
.Lother_nlink_ready:
    cmp $4, %r15
    jb other_task_initial_count_failed

    mov $__NR_openat, %eax
    mov $AT_FDCWD, %edi
    lea proc_root(%rip), %rsi
    mov $(O_RDONLY | O_DIRECTORY), %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js open_proc_failed
    mov %eax, %r12d

    mov $__NR_openat, %eax
    mov $AT_FDCWD, %edi
    lea host_root(%rip), %rsi
    mov $(O_RDONLY | O_DIRECTORY), %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js open_root_failed
    mov %eax, root_fd(%rip)

    mov $__NR_openat, %eax
    mov $AT_FDCWD, %edi
    lea chroot_path(%rip), %rsi
    mov $(O_RDONLY | O_DIRECTORY), %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js open_chroot_failed
    mov %eax, chroot_fd(%rip)

    mov $__NR_openat, %eax
    mov %r12d, %edi
    lea relative_task(%rip), %rsi
    mov $(O_RDONLY | O_DIRECTORY), %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js open_task_failed
    mov %eax, %r13d

    mov $__NR_openat, %eax
    mov %r12d, %edi
    lea self_path(%rip), %rsi
    mov $(O_RDONLY | O_DIRECTORY), %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js open_self_failed
    mov %eax, self_fd(%rip)

    mov $__NR_openat, %eax
    mov %r12d, %edi
    lea other_task_path(%rip), %rsi
    mov $(O_RDONLY | O_DIRECTORY), %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js open_other_task_failed
    mov %eax, %r14d

    mov $__NR_openat, %eax
    mov $AT_FDCWD, %edi
    mov 32(%rsp), %rsi
    mov $(O_RDONLY | O_DIRECTORY), %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js open_ordinary_root_failed
    mov %eax, %ebx

    mov $__NR_openat, %eax
    mov %ebx, %edi
    lea relative_task(%rip), %rsi
    mov $(O_RDONLY | O_DIRECTORY), %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js open_ordinary_task_failed
    mov %eax, %ebp

    /* Starting the namespace sandbox also starts LATX's host RCU helper. */
    mov $__NR_unshare, %eax
    mov $CLONE_NEWUSER, %edi
    syscall
    test %rax, %rax
    js unshare_failed

    /* Chromium shares this chroot through CLONE_FS before probing procfd. */
    mov $__NR_chroot, %eax
    lea chroot_path(%rip), %rdi
    syscall
    test %rax, %rax
    js chroot_failed

    mov $__NR_newfstatat, %eax
    mov %r12d, %edi
    lea relative_task(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js task_stat_failed

    /* One guest thread must be reported as '.' + '..' + one task. */
    cmpq $3, statbuf+16(%rip)
    jne task_count_failed

    mov $__NR_fstat, %eax
    mov %r13d, %edi
    lea statbuf(%rip), %rsi
    syscall
    test %rax, %rax
    js task_fstat_failed
    cmpq $3, statbuf+16(%rip)
    jne task_fstat_count_failed

    mov $__NR_newfstatat, %eax
    mov %r13d, %edi
    lea empty_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    mov $AT_EMPTY_PATH, %r10d
    syscall
    test %rax, %rax
    js task_empty_fstatat_failed
    cmpq $3, statbuf+16(%rip)
    jne task_empty_fstatat_count_failed

    mov $__NR_statx, %eax
    mov %r13d, %edi
    lea empty_path(%rip), %rsi
    mov $AT_EMPTY_PATH, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js task_empty_statx_failed
    cmpl $3, statxbuf+16(%rip)
    jne task_empty_statx_count_failed

    mov $__NR_statx, %eax
    mov %r12d, %edi
    lea relative_task(%rip), %rsi
    xor %edx, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js task_relative_statx_failed
    cmpl $3, statxbuf+16(%rip)
    jne task_relative_statx_count_failed

    mov $__NR_fstat, %eax
    mov %r14d, %edi
    lea statbuf(%rip), %rsi
    syscall
    test %rax, %rax
    js other_task_fstat_failed
    cmp statbuf+16(%rip), %r15
    jne other_task_fstat_rewritten

    mov $__NR_newfstatat, %eax
    mov %r14d, %edi
    lea empty_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    mov $AT_EMPTY_PATH, %r10d
    syscall
    test %rax, %rax
    js other_task_empty_fstatat_failed
    cmp statbuf+16(%rip), %r15
    jne other_task_empty_fstatat_rewritten

    mov $__NR_statx, %eax
    mov %r14d, %edi
    lea empty_path(%rip), %rsi
    mov $AT_EMPTY_PATH, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js other_task_empty_statx_failed
    cmp statxbuf+16(%rip), %r15d
    jne other_task_empty_statx_rewritten

    mov $__NR_statx, %eax
    mov %r12d, %edi
    lea other_task_path(%rip), %rsi
    xor %edx, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js other_task_relative_statx_failed
    cmp statxbuf+16(%rip), %r15d
    jne other_task_relative_statx_rewritten

    mov $__NR_fchdir, %eax
    mov %r12d, %edi
    syscall
    test %rax, %rax
    js fchdir_proc_failed

    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea relative_task(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js cwd_task_fstatat_failed
    cmpq $3, statbuf+16(%rip)
    jne cwd_task_fstatat_count_failed

    mov $__NR_statx, %eax
    mov $AT_FDCWD, %edi
    lea relative_task(%rip), %rsi
    xor %edx, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js cwd_task_statx_failed
    cmpl $3, statxbuf+16(%rip)
    jne cwd_task_statx_count_failed

    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea other_task_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js cwd_other_task_fstatat_failed
    cmp statbuf+16(%rip), %r15
    jne cwd_other_task_fstatat_rewritten

    mov $__NR_statx, %eax
    mov $AT_FDCWD, %edi
    lea other_task_path(%rip), %rsi
    xor %edx, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js cwd_other_task_statx_failed
    cmp statxbuf+16(%rip), %r15d
    jne cwd_other_task_statx_rewritten

    mov $__NR_fchdir, %eax
    mov self_fd(%rip), %edi
    syscall
    test %rax, %rax
    js fchdir_self_failed

    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea task_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js cwd_self_task_fstatat_failed
    cmpq $3, statbuf+16(%rip)
    jne cwd_self_task_fstatat_count_failed

    mov $__NR_statx, %eax
    mov $AT_FDCWD, %edi
    lea task_path(%rip), %rsi
    xor %edx, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js cwd_self_task_statx_failed
    cmpl $3, statxbuf+16(%rip)
    jne cwd_self_task_statx_count_failed

    mov $__NR_fchdir, %eax
    mov %r13d, %edi
    syscall
    test %rax, %rax
    js fchdir_task_failed

    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea dot_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js cwd_dot_task_fstatat_failed
    cmpq $3, statbuf+16(%rip)
    jne cwd_dot_task_fstatat_count_failed

    mov $__NR_statx, %eax
    mov $AT_FDCWD, %edi
    lea dot_path(%rip), %rsi
    xor %edx, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js cwd_dot_task_statx_failed
    cmpl $3, statxbuf+16(%rip)
    jne cwd_dot_task_statx_count_failed

    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea empty_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    mov $AT_EMPTY_PATH, %r10d
    syscall
    test %rax, %rax
    js cwd_empty_task_fstatat_failed
    cmpq $3, statbuf+16(%rip)
    jne cwd_empty_task_fstatat_count_failed

    mov $__NR_statx, %eax
    mov $AT_FDCWD, %edi
    lea empty_path(%rip), %rsi
    mov $AT_EMPTY_PATH, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js cwd_empty_task_statx_failed
    cmpl $3, statxbuf+16(%rip)
    jne cwd_empty_task_statx_count_failed

    mov $__NR_fchdir, %eax
    mov %r14d, %edi
    syscall
    test %rax, %rax
    js fchdir_other_task_failed

    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea empty_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    mov $AT_EMPTY_PATH, %r10d
    syscall
    test %rax, %rax
    js cwd_empty_other_task_fstatat_failed
    cmp statbuf+16(%rip), %r15
    jne cwd_empty_other_task_fstatat_rewritten

    mov $__NR_statx, %eax
    mov $AT_FDCWD, %edi
    lea empty_path(%rip), %rsi
    mov $AT_EMPTY_PATH, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js cwd_empty_other_task_statx_failed
    cmp statxbuf+16(%rip), %r15d
    jne cwd_empty_other_task_statx_rewritten

    mov $__NR_fchdir, %eax
    mov %ebx, %edi
    syscall
    test %rax, %rax
    js fchdir_ordinary_root_failed

    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea relative_task(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js cwd_ordinary_task_fstatat_failed
    cmpq $2, statbuf+16(%rip)
    jne cwd_ordinary_task_fstatat_rewritten

    mov $__NR_statx, %eax
    mov $AT_FDCWD, %edi
    lea relative_task(%rip), %rsi
    xor %edx, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js cwd_ordinary_task_statx_failed
    cmpl $2, statxbuf+16(%rip)
    jne cwd_ordinary_task_statx_rewritten

    mov $__NR_fchdir, %eax
    mov %ebp, %edi
    syscall
    test %rax, %rax
    js fchdir_ordinary_task_failed

    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea empty_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    mov $AT_EMPTY_PATH, %r10d
    syscall
    test %rax, %rax
    js cwd_empty_ordinary_task_fstatat_failed
    cmpq $2, statbuf+16(%rip)
    jne cwd_empty_ordinary_task_fstatat_rewritten

    mov $__NR_statx, %eax
    mov $AT_FDCWD, %edi
    lea empty_path(%rip), %rsi
    mov $AT_EMPTY_PATH, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js cwd_empty_ordinary_task_statx_failed
    cmpl $2, statxbuf+16(%rip)
    jne cwd_empty_ordinary_task_statx_rewritten

    mov $__NR_fstat, %eax
    mov self_fd(%rip), %edi
    lea statbuf(%rip), %rsi
    syscall
    test %rax, %rax
    js race_self_fstat_failed
    mov statbuf+16(%rip), %rax
    mov %rax, self_nlink(%rip)

    mov $__NR_dup, %eax
    mov self_fd(%rip), %edi
    syscall
    test %rax, %rax
    js race_dup_failed
    mov %eax, race_fd(%rip)

    /* Race cwd, dirfd, and pathname changes against the stat lookups. */
    mov $__NR_clone, %eax
    mov $(CLONE_THREAD_FLAGS | CLONE_TID_FLAGS | CLONE_CHILD_SETTID), %edi
    lea race_stack+65536(%rip), %rsi
    xor %edx, %edx
    lea race_child_tid(%rip), %r10
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js race_clone_failed
    jz cwd_racer

.Lwait_racer_ready:
    cmpl $1, race_ready(%rip)
    jne .Lwait_racer_ready

    mov $RACE_ITERATIONS, %r9d
.Lrace_stat_loop:
    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea task_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js race_fstatat_failed
    cmpq $2, statbuf+16(%rip)
    jne .Lrace_fstatat_proc
    orl $1, race_seen(%rip)
    jmp .Lrace_statx
.Lrace_fstatat_proc:
    cmpq $4, statbuf+16(%rip)
    jne race_fstatat_count_failed
    orl $2, race_seen(%rip)

.Lrace_statx:
    mov $__NR_statx, %eax
    mov $AT_FDCWD, %edi
    lea task_path(%rip), %rsi
    xor %edx, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js race_statx_failed
    cmpl $2, statxbuf+16(%rip)
    jne .Lrace_statx_proc
    orl $1, race_seen(%rip)
    jmp .Lrace_dirfd_fstatat
.Lrace_statx_proc:
    cmpl $4, statxbuf+16(%rip)
    jne race_statx_count_failed
    orl $2, race_seen(%rip)

.Lrace_dirfd_fstatat:
    mov $__NR_newfstatat, %eax
    mov race_fd(%rip), %edi
    lea task_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js race_dirfd_fstatat_failed
    cmpq $2, statbuf+16(%rip)
    je .Lrace_dirfd_statx
    cmpq $4, statbuf+16(%rip)
    jne race_dirfd_fstatat_count_failed

.Lrace_dirfd_statx:
    mov $__NR_statx, %eax
    mov race_fd(%rip), %edi
    lea task_path(%rip), %rsi
    xor %edx, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js race_dirfd_statx_failed
    cmpl $2, statxbuf+16(%rip)
    je .Lrace_path_fstatat
    cmpl $4, statxbuf+16(%rip)
    jne race_dirfd_statx_count_failed

.Lrace_path_fstatat:
    mov $__NR_newfstatat, %eax
    mov self_fd(%rip), %edi
    lea race_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js race_path_fstatat_failed
    cmpq $4, statbuf+16(%rip)
    jne .Lrace_path_fstatat_self
    orl $1, race_path_seen(%rip)
    jmp .Lrace_path_statx
.Lrace_path_fstatat_self:
    mov self_nlink(%rip), %rax
    cmp %rax, statbuf+16(%rip)
    jne race_path_fstatat_count_failed
    orl $2, race_path_seen(%rip)

.Lrace_path_statx:
    mov $__NR_statx, %eax
    mov self_fd(%rip), %edi
    lea race_path(%rip), %rsi
    xor %edx, %edx
    mov $STATX_BASIC_STATS, %r10d
    lea statxbuf(%rip), %r8
    syscall
    test %rax, %rax
    js race_path_statx_failed
    cmpl $4, statxbuf+16(%rip)
    jne .Lrace_path_statx_self
    orl $1, race_path_seen(%rip)
    jmp .Lrace_next
.Lrace_path_statx_self:
    mov self_nlink(%rip), %eax
    cmp %eax, statxbuf+16(%rip)
    jne race_path_statx_count_failed
    orl $2, race_path_seen(%rip)

.Lrace_next:
    mov $__NR_sched_yield, %eax
    syscall
    dec %r9d
    jne .Lrace_stat_loop
    movl $1, race_stop(%rip)

.Lwait_racer_stopped:
    mov race_child_tid(%rip), %edx
    test %edx, %edx
    jz .Lracer_stopped
    mov $__NR_futex, %eax
    lea race_child_tid(%rip), %rdi
    mov $FUTEX_WAIT, %esi
    xor %r10d, %r10d
    xor %r8d, %r8d
    xor %r9d, %r9d
    syscall
    jmp .Lwait_racer_stopped

.Lracer_stopped:
    cmpl $1, race_error(%rip)
    je race_fchdir_failed
    cmpl $2, race_error(%rip)
    je race_dup2_failed
    cmpl $3, race_seen(%rip)
    jne race_coverage_failed
    cmpl $3, race_path_seen(%rip)
    jne race_path_coverage_failed

    movl $0, race_child_tid(%rip)
    movl $0, race_ready(%rip)
    movl $0, race_stop(%rip)
    movl $0, race_error(%rip)

    /* Race absolute stat lookups against repeated root changes. */
    mov $__NR_clone, %eax
    mov $(CLONE_THREAD_FLAGS | CLONE_TID_FLAGS | CLONE_CHILD_SETTID), %edi
    lea race_stack+65536(%rip), %rsi
    xor %edx, %edx
    lea race_child_tid(%rip), %r10
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js root_race_clone_failed
    jz root_racer

.Lwait_root_racer_ready:
    cmpl $1, race_ready(%rip)
    jne .Lwait_root_racer_ready

    mov $RACE_ITERATIONS, %r9d
.Lroot_race_loop:
    mov $__NR_stat, %eax
    lea absolute_task(%rip), %rdi
    lea statbuf(%rip), %rsi
    syscall
    test %rax, %rax
    js .Lroot_race_stat_error
    cmpq $4, statbuf+16(%rip)
    jne root_race_stat_count_failed
    orl $1, root_race_seen(%rip)
    jmp .Lroot_race_lstat
.Lroot_race_stat_error:
    cmp $-2, %rax
    jne root_race_stat_failed
    orl $2, root_race_seen(%rip)

.Lroot_race_lstat:
    mov $__NR_lstat, %eax
    lea absolute_task(%rip), %rdi
    lea statbuf(%rip), %rsi
    syscall
    test %rax, %rax
    js .Lroot_race_lstat_error
    cmpq $4, statbuf+16(%rip)
    jne root_race_lstat_count_failed
    orl $1, root_race_seen(%rip)
    jmp .Lroot_race_next
.Lroot_race_lstat_error:
    cmp $-2, %rax
    jne root_race_lstat_failed
    orl $2, root_race_seen(%rip)

.Lroot_race_next:
    mov $__NR_sched_yield, %eax
    syscall
    dec %r9d
    jne .Lroot_race_loop
    movl $1, race_stop(%rip)

.Lwait_root_racer_stopped:
    mov race_child_tid(%rip), %edx
    test %edx, %edx
    jz .Lroot_racer_stopped
    mov $__NR_futex, %eax
    lea race_child_tid(%rip), %rdi
    mov $FUTEX_WAIT, %esi
    xor %r10d, %r10d
    xor %r8d, %r8d
    xor %r9d, %r9d
    syscall
    jmp .Lwait_root_racer_stopped

.Lroot_racer_stopped:
    cmpl $0, race_error(%rip)
    jne root_race_chroot_failed
    cmpl $3, root_race_seen(%rip)
    jne root_race_coverage_failed

    mov $__NR_close, %eax
    mov race_fd(%rip), %edi
    syscall

    mov $__NR_close, %eax
    mov chroot_fd(%rip), %edi
    syscall

    mov $__NR_close, %eax
    mov root_fd(%rip), %edi
    syscall

    mov $__NR_close, %eax
    mov %ebp, %edi
    syscall

    mov $__NR_close, %eax
    mov %ebx, %edi
    syscall

    mov $__NR_close, %eax
    mov self_fd(%rip), %edi
    syscall

    mov $__NR_close, %eax
    mov %r14d, %edi
    syscall

    mov $__NR_close, %eax
    mov %r13d, %edi
    syscall

    mov $__NR_close, %eax
    mov %r12d, %edi
    syscall
    xor %edi, %edi
    jmp exit

cwd_racer:
    mov $__NR_fchdir, %eax
    mov self_fd(%rip), %edi
    syscall
    test %rax, %rax
    js .Lcwd_racer_start_failed
    mov $__NR_dup2, %eax
    mov self_fd(%rip), %edi
    mov race_fd(%rip), %esi
    syscall
    test %rax, %rax
    js .Lcwd_racer_dup_failed
    movabs $0x002f2e2f6b736174, %rax
    mov %rax, race_path(%rip)
    movl $1, race_ready(%rip)
.Lcwd_racer_loop:
    cmpl $1, race_stop(%rip)
    je .Lcwd_racer_exit
    mov $__NR_fchdir, %eax
    mov self_fd(%rip), %edi
    syscall
    test %rax, %rax
    js .Lcwd_racer_failed
    mov $__NR_dup2, %eax
    mov self_fd(%rip), %edi
    mov race_fd(%rip), %esi
    syscall
    test %rax, %rax
    js .Lcwd_racer_dup_failed
    movb $'/', race_path+6(%rip)
    mov $__NR_sched_yield, %eax
    syscall
    mov $__NR_fchdir, %eax
    mov %ebx, %edi
    syscall
    test %rax, %rax
    js .Lcwd_racer_failed
    mov $__NR_dup2, %eax
    mov %ebx, %edi
    mov race_fd(%rip), %esi
    syscall
    test %rax, %rax
    js .Lcwd_racer_dup_failed
    movb $'.', race_path+6(%rip)
    mov $__NR_sched_yield, %eax
    syscall
    jmp .Lcwd_racer_loop

.Lcwd_racer_failed:
    movl $1, race_error(%rip)
.Lcwd_racer_exit:
    xor %edi, %edi
    jmp exit

.Lcwd_racer_start_failed:
    movl $1, race_error(%rip)
    movl $1, race_ready(%rip)
    jmp .Lcwd_racer_exit

.Lcwd_racer_dup_failed:
    movl $2, race_error(%rip)
    movl $1, race_ready(%rip)
    jmp .Lcwd_racer_exit

root_racer:
    call restore_host_root
    test %rax, %rax
    js .Lroot_racer_start_failed
    movl $1, race_ready(%rip)
.Lroot_racer_loop:
    cmpl $1, race_stop(%rip)
    je .Lroot_racer_exit
    call restore_host_root
    test %rax, %rax
    js .Lroot_racer_failed
    mov $__NR_sched_yield, %eax
    syscall
    mov $__NR_fchdir, %eax
    mov chroot_fd(%rip), %edi
    syscall
    test %rax, %rax
    js .Lroot_racer_failed
    mov $__NR_chroot, %eax
    lea dot_path(%rip), %rdi
    syscall
    test %rax, %rax
    js .Lroot_racer_failed
    mov $__NR_sched_yield, %eax
    syscall
    jmp .Lroot_racer_loop

.Lroot_racer_failed:
    movl $1, race_error(%rip)
.Lroot_racer_exit:
    call restore_host_root
    xor %edi, %edi
    jmp exit

.Lroot_racer_start_failed:
    movl $1, race_error(%rip)
    movl $1, race_ready(%rip)
    jmp .Lroot_racer_exit

restore_host_root:
    mov $__NR_fchdir, %eax
    mov root_fd(%rip), %edi
    syscall
    test %rax, %rax
    js .Lrestore_host_root_done
    mov $__NR_chroot, %eax
    lea dot_path(%rip), %rdi
    syscall
.Lrestore_host_root_done:
    ret

.Lstop_racer_after_error:
    mov %edi, race_result(%rip)
    movl $1, race_stop(%rip)
.Lwait_racer_after_error:
    mov race_child_tid(%rip), %edx
    test %edx, %edx
    jz .Lreturn_race_error
    mov $__NR_futex, %eax
    lea race_child_tid(%rip), %rdi
    mov $FUTEX_WAIT, %esi
    xor %r10d, %r10d
    xor %r8d, %r8d
    xor %r9d, %r9d
    syscall
    jmp .Lwait_racer_after_error
.Lreturn_race_error:
    mov race_result(%rip), %edi
    jmp exit

open_proc_failed:
    mov $30, %edi
    jmp exit
unshare_failed:
    mov $31, %edi
    jmp exit
chroot_failed:
    mov $32, %edi
    jmp exit
task_stat_failed:
    mov $33, %edi
    jmp exit
task_count_failed:
    mov $34, %edi
    jmp exit
open_task_failed:
    mov $35, %edi
    jmp exit
task_fstat_failed:
    mov $36, %edi
    jmp exit
task_fstat_count_failed:
    mov $37, %edi
    jmp exit
task_empty_fstatat_failed:
    mov $38, %edi
    jmp exit
task_empty_fstatat_count_failed:
    mov $39, %edi
    jmp exit
task_empty_statx_failed:
    mov $40, %edi
    jmp exit
task_empty_statx_count_failed:
    mov $41, %edi
    jmp exit
open_other_task_failed:
    mov $42, %edi
    jmp exit
other_task_initial_count_failed:
    mov $44, %edi
    jmp exit
other_task_fstat_failed:
    mov $45, %edi
    jmp exit
other_task_fstat_rewritten:
    mov $46, %edi
    jmp exit
other_task_empty_fstatat_failed:
    mov $47, %edi
    jmp exit
other_task_empty_fstatat_rewritten:
    mov $48, %edi
    jmp exit
other_task_empty_statx_failed:
    mov $49, %edi
    jmp exit
other_task_empty_statx_rewritten:
    mov $50, %edi
    jmp exit
task_relative_statx_failed:
    mov $51, %edi
    jmp exit
task_relative_statx_count_failed:
    mov $52, %edi
    jmp exit
other_task_relative_statx_failed:
    mov $53, %edi
    jmp exit
other_task_relative_statx_rewritten:
    mov $54, %edi
    jmp exit
open_ordinary_root_failed:
    mov $55, %edi
    jmp exit
open_ordinary_task_failed:
    mov $56, %edi
    jmp exit
fchdir_proc_failed:
    mov $57, %edi
    jmp exit
cwd_task_fstatat_failed:
    mov $58, %edi
    jmp exit
cwd_task_fstatat_count_failed:
    mov $59, %edi
    jmp exit
cwd_task_statx_failed:
    mov $60, %edi
    jmp exit
cwd_task_statx_count_failed:
    mov $61, %edi
    jmp exit
cwd_other_task_fstatat_failed:
    mov $62, %edi
    jmp exit
cwd_other_task_fstatat_rewritten:
    mov $63, %edi
    jmp exit
cwd_other_task_statx_failed:
    mov $64, %edi
    jmp exit
cwd_other_task_statx_rewritten:
    mov $65, %edi
    jmp exit
fchdir_task_failed:
    mov $66, %edi
    jmp exit
cwd_empty_task_fstatat_failed:
    mov $67, %edi
    jmp exit
cwd_empty_task_fstatat_count_failed:
    mov $68, %edi
    jmp exit
cwd_empty_task_statx_failed:
    mov $69, %edi
    jmp exit
cwd_empty_task_statx_count_failed:
    mov $70, %edi
    jmp exit
fchdir_other_task_failed:
    mov $71, %edi
    jmp exit
cwd_empty_other_task_fstatat_failed:
    mov $72, %edi
    jmp exit
cwd_empty_other_task_fstatat_rewritten:
    mov $73, %edi
    jmp exit
cwd_empty_other_task_statx_failed:
    mov $74, %edi
    jmp exit
cwd_empty_other_task_statx_rewritten:
    mov $75, %edi
    jmp exit
fchdir_ordinary_root_failed:
    mov $76, %edi
    jmp exit
cwd_ordinary_task_fstatat_failed:
    mov $77, %edi
    jmp exit
cwd_ordinary_task_fstatat_rewritten:
    mov $78, %edi
    jmp exit
cwd_ordinary_task_statx_failed:
    mov $79, %edi
    jmp exit
cwd_ordinary_task_statx_rewritten:
    mov $80, %edi
    jmp exit
fchdir_ordinary_task_failed:
    mov $81, %edi
    jmp exit
cwd_empty_ordinary_task_fstatat_failed:
    mov $82, %edi
    jmp exit
cwd_empty_ordinary_task_fstatat_rewritten:
    mov $83, %edi
    jmp exit
cwd_empty_ordinary_task_statx_failed:
    mov $84, %edi
    jmp exit
cwd_empty_ordinary_task_statx_rewritten:
    mov $85, %edi
    jmp exit
open_self_failed:
    mov $86, %edi
    jmp exit
fchdir_self_failed:
    mov $87, %edi
    jmp exit
cwd_self_task_fstatat_failed:
    mov $88, %edi
    jmp exit
cwd_self_task_fstatat_count_failed:
    mov $89, %edi
    jmp exit
cwd_self_task_statx_failed:
    mov $90, %edi
    jmp exit
cwd_self_task_statx_count_failed:
    mov $91, %edi
    jmp exit
cwd_dot_task_fstatat_failed:
    mov $92, %edi
    jmp exit
cwd_dot_task_fstatat_count_failed:
    mov $93, %edi
    jmp exit
cwd_dot_task_statx_failed:
    mov $94, %edi
    jmp exit
cwd_dot_task_statx_count_failed:
    mov $95, %edi
    jmp exit
race_clone_failed:
    mov $96, %edi
    jmp exit
race_fstatat_failed:
    mov $97, %edi
    jmp .Lstop_racer_after_error
race_fstatat_count_failed:
    mov $98, %edi
    jmp .Lstop_racer_after_error
race_statx_failed:
    mov $99, %edi
    jmp .Lstop_racer_after_error
race_statx_count_failed:
    mov $100, %edi
    jmp .Lstop_racer_after_error
race_fchdir_failed:
    mov $101, %edi
    jmp exit
race_coverage_failed:
    mov $102, %edi
    jmp exit
race_dirfd_fstatat_failed:
    mov $103, %edi
    jmp .Lstop_racer_after_error
race_dirfd_fstatat_count_failed:
    mov $104, %edi
    jmp .Lstop_racer_after_error
race_dirfd_statx_failed:
    mov $105, %edi
    jmp .Lstop_racer_after_error
race_dirfd_statx_count_failed:
    mov $106, %edi
    jmp .Lstop_racer_after_error
race_dup_failed:
    mov $107, %edi
    jmp exit
race_dup2_failed:
    mov $108, %edi
    jmp exit
race_self_fstat_failed:
    mov $109, %edi
    jmp exit
race_path_fstatat_failed:
    mov $110, %edi
    jmp .Lstop_racer_after_error
race_path_fstatat_count_failed:
    mov $111, %edi
    jmp .Lstop_racer_after_error
race_path_statx_failed:
    mov $112, %edi
    jmp .Lstop_racer_after_error
race_path_statx_count_failed:
    mov $113, %edi
    jmp .Lstop_racer_after_error
race_path_coverage_failed:
    mov $114, %edi
    jmp exit
open_root_failed:
    mov $115, %edi
    jmp exit
open_chroot_failed:
    mov $116, %edi
    jmp exit
root_race_clone_failed:
    mov $117, %edi
    jmp exit
root_race_stat_failed:
    mov $118, %edi
    jmp .Lstop_racer_after_error
root_race_stat_count_failed:
    mov $119, %edi
    jmp .Lstop_racer_after_error
root_race_lstat_failed:
    mov $120, %edi
    jmp .Lstop_racer_after_error
root_race_lstat_count_failed:
    mov $121, %edi
    jmp .Lstop_racer_after_error
root_race_chroot_failed:
    mov $122, %edi
    jmp exit
root_race_coverage_failed:
    mov $123, %edi

exit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
