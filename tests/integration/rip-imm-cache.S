/* SPDX-License-Identifier: GPL-2.0-only */

    .equ __NR_rt_sigaction, 13
    .equ __NR_rt_sigreturn, 15
    .equ __NR_mmap, 9
    .equ __NR_munmap, 11
    .equ __NR_getpid, 39
    .equ __NR_fork, 57
    .equ __NR_execve, 59
    .equ __NR_exit, 60
    .equ __NR_wait4, 61
    .equ __NR_kill, 62
    .equ PROT_RWX, 7
    .equ MAP_PRIVATE_ANONYMOUS, 0x22
    .equ SIGUSR1, 10
    .equ SA_RESTORER, 0x04000000

    .section .text
    .balign 8
negative_data:
    .quad 0x11

    .globl _start
    .type _start, @function
_start:
    cmpq $1, (%rsp)
    jne .Lcore_only
    mov 8(%rsp), %rax
    mov %rax, saved_argv0(%rip)
    movb $1, run_lifecycle(%rip)

.Lcore_only:
    call .Lrun_core
    test %eax, %eax
    jne .Lexit

    call .Ltest_signal
    test %eax, %eax
    jne .Lexit

    cmpb $0, run_lifecycle(%rip)
    je .Lexit

    mov $__NR_fork, %eax
    syscall
    test %eax, %eax
    js .Lfail
    jz .Lfork_child

    mov %eax, %edi
    lea child_status(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $__NR_wait4, %eax
    syscall
    test %eax, %eax
    js .Lfail
    cmpl $0, child_status(%rip)
    jne .Lfail

    mov saved_argv0(%rip), %rdi
    mov %rdi, exec_argv(%rip)
    lea exec_marker(%rip), %rax
    mov %rax, exec_argv + 8(%rip)
    lea exec_argv(%rip), %rsi
    xor %edx, %edx
    mov $__NR_execve, %eax
    syscall
    jmp .Lfail

.Lfork_child:
    call .Lrun_core
    mov %eax, %edi
    mov $__NR_exit, %eax
    syscall

.Lrun_core:
    mov negative_data(%rip), %r8
    mov positive_data(%rip), %r9
    add positive_data(%rip), %r9
    xor distant_data(%rip), %r8
    add %r9, %r8

    /* Keep GOTPCREL references unrelaxed in the PIE/shared variants. */
    lea positive_data@GOTPCREL(%rip), %r11
    lea positive_data@GOTPCREL(%rip), %r12
    cmp %r11, %r12
    jne .Lcore_fail

    /* Exercise both common and raw helper-call paths with a live RIP cache. */
    pxor %xmm0, %xmm0
    pcmpistri $0, %xmm0, %xmm0
    fld1
    fsin
    fstp %st(0)
    mov positive_data(%rip), %r10
    cmp $0x22, %r10
    jne .Lcore_fail
    mov %fs:positive_data(%rip), %r10
    cmp $0x22, %r10
    jne .Lcore_fail
    cmp $0x99, %r8
    jne .Lcore_fail

    call .Lcross_page
    test %eax, %eax
    jne .Lcore_fail

    call .Ltest_smc
    test %eax, %eax
    jne .Lcore_fail

    xor %eax, %eax
    ret

.Lcore_fail:
    mov $42, %eax
    ret

.Ltest_signal:
    lea .Lsignal_handler(%rip), %rax
    mov %rax, signal_action(%rip)
    lea .Lsignal_restorer(%rip), %rax
    mov %rax, signal_action + 16(%rip)
    mov $__NR_rt_sigaction, %eax
    mov $SIGUSR1, %edi
    lea signal_action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %eax, %eax
    js .Lsignal_fail

    mov $__NR_getpid, %eax
    syscall
    mov %eax, %edi
    mov $SIGUSR1, %esi
    mov $__NR_kill, %eax
    syscall
    test %eax, %eax
    js .Lsignal_fail
    cmpb $1, signal_seen(%rip)
    jne .Lsignal_fail

    xor %eax, %eax
    ret

.Lsignal_fail:
    mov $42, %eax
    ret

.Lsignal_handler:
    mov positive_data(%rip), %rax
    cmp positive_data(%rip), %rax
    jne .Lsignal_handler_fail
    movb $1, signal_seen(%rip)
    ret

.Lsignal_handler_fail:
    movb $42, signal_seen(%rip)
    ret

.Lsignal_restorer:
    mov $__NR_rt_sigreturn, %eax
    syscall

.Ltest_smc:
    mov $__NR_mmap, %eax
    xor %edi, %edi
    mov $4096, %esi
    mov $PROT_RWX, %edx
    mov $MAP_PRIVATE_ANONYMOUS, %r10d
    mov $-1, %r8d
    xor %r9d, %r9d
    syscall
    test %rax, %rax
    js .Lsmc_fail
    mov %rax, %r13

    movabs $0x0d8b0000000a058b, %rax
    mov %rax, 0(%r13)
    movabs $0x90c3c80100000004, %rax
    mov %rax, 8(%r13)
    movabs $0x0000002200000011, %rax
    mov %rax, 16(%r13)
    call *%r13
    cmp $0x22, %eax
    jne .Lsmc_unmap_fail

    /* Retarget both RIP-relative loads from data[0] to data[1]. */
    movb $0x0e, 2(%r13)
    movb $0x08, 8(%r13)
    call *%r13
    mov %eax, %r12d

    mov $__NR_munmap, %eax
    mov %r13, %rdi
    mov $4096, %esi
    syscall
    test %eax, %eax
    js .Lsmc_fail
    cmp $0x44, %r12d
    jne .Lsmc_fail
    xor %eax, %eax
    ret

.Lsmc_unmap_fail:
    mov $__NR_munmap, %eax
    mov %r13, %rdi
    mov $4096, %esi
    syscall
.Lsmc_fail:
    mov $42, %eax
    ret

.Lfail:
    mov $42, %eax
.Lexit:
    mov %eax, %edi
    mov $__NR_exit, %eax
    syscall
    .size _start, .-_start

    /* The first load straddles a 4 KiB guest page boundary. */
    .balign 4096
    .space 4092, 0x90
.Lcross_page:
    mov positive_data(%rip), %eax
    cmp $0x22, %eax
    jne .Lcross_page_fail
    mov positive_data(%rip), %ecx
    cmp $0x22, %ecx
    jne .Lcross_page_fail
    xor %eax, %eax
    ret

.Lcross_page_fail:
    mov $42, %eax
    ret

    .section .data
    .balign 8
positive_data:
    .quad 0x22
    .space 4096
distant_data:
    .quad 0x44

    .balign 8
signal_action:
    .quad 0
    .quad SA_RESTORER
    .quad 0
    .quad 0
saved_argv0:
    .quad 0
exec_argv:
    .quad 0, 0, 0
child_status:
    .long 0
signal_seen:
    .byte 0
run_lifecycle:
    .byte 0
exec_marker:
    .asciz "exec"

    .section .note.GNU-stack,"",@progbits
