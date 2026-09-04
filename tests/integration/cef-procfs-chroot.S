.equ __NR_close, 3
.equ __NR_fstat, 5
.equ __NR_exit, 60
.equ __NR_chroot, 161
.equ __NR_openat, 257
.equ __NR_newfstatat, 262
.equ __NR_unshare, 272
.equ __NR_statx, 332
.equ AT_FDCWD, -100
.equ AT_EMPTY_PATH, 0x1000
.equ O_RDONLY, 0
.equ O_DIRECTORY, 0x10000
.equ STATX_BASIC_STATS, 0x7ff
.equ CLONE_NEWUSER, 0x10000000

.section .rodata
proc_root:
    .asciz "/proc"
chroot_path:
    .asciz "/proc/self/fdinfo"
relative_task:
    .asciz "self/task"
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
    lea other_task_path(%rip), %rsi
    mov $(O_RDONLY | O_DIRECTORY), %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js open_other_task_failed
    mov %eax, %r14d

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

exit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
