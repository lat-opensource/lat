.equ __NR_rt_sigaction, 13
.equ __NR_getpid, 39
.equ __NR_fork, 57
.equ __NR_exit, 60
.equ __NR_wait4, 61
.equ __NR_prctl, 157

.equ SIGSYS, 31
.equ EFAULT, 14
.equ EINVAL, 22
.equ SA_SIGINFO, 4
.equ SA_RESTORER, 0x04000000
.equ PR_SET_SYSCALL_USER_DISPATCH, 59
.equ PR_SYS_DISPATCH_OFF, 0
.equ PR_SYS_DISPATCH_ON, 1
.equ SYSCALL_DISPATCH_FILTER_ALLOW, 0
.equ SYSCALL_DISPATCH_FILTER_BLOCK, 1
.equ SYS_USER_DISPATCH, 2
.equ AUDIT_ARCH_X86_64, 0xc000003e

.section .text
.global _start
.type _start, @function
_start:
    /* Unknown prctl commands must be rejected in the guest. */
    mov $__NR_prctl, %eax
    mov $0x7fffffff, %edi
    xor %esi, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-EINVAL, %rax
    jne unknown_prctl_failed

    /* Match the kernel's parameter validation before enabling dispatch. */
    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %edi
    mov $-1, %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-EINVAL, %rax
    jne bad_param_failed

    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %edi
    mov $PR_SYS_DISPATCH_OFF, %esi
    mov $1, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-EINVAL, %rax
    jne bad_param_failed

    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %edi
    mov $PR_SYS_DISPATCH_OFF, %esi
    xor %edx, %edx
    mov $1, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-EINVAL, %rax
    jne bad_param_failed

    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %edi
    mov $PR_SYS_DISPATCH_OFF, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    lea selector(%rip), %r8
    syscall
    cmp $-EINVAL, %rax
    jne bad_param_failed

    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %edi
    mov $PR_SYS_DISPATCH_ON, %esi
    mov $1, %edx
    xor %r10d, %r10d
    lea selector(%rip), %r8
    syscall
    cmp $-EINVAL, %rax
    jne bad_param_failed

    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %edi
    mov $PR_SYS_DISPATCH_ON, %esi
    xor %edx, %edx
    mov $1, %r10d
    mov $-1, %r8
    syscall
    cmp $-EFAULT, %rax
    jne bad_param_failed

    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %edi
    mov $PR_SYS_DISPATCH_ON, %esi
    mov $1, %edx
    mov $-1, %r10
    lea selector(%rip), %r8
    syscall
    cmp $-EINVAL, %rax
    jne bad_param_failed

    mov $__NR_rt_sigaction, %eax
    mov $SIGSYS, %edi
    lea action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js setup_failed

    /* A syscall issued inside the configured range is always allowed. */
    movb $SYSCALL_DISPATCH_FILTER_BLOCK, selector(%rip)
    call allowed_getpid
    test %rax, %rax
    jle allowed_range_failed
    movb $SYSCALL_DISPATCH_FILTER_ALLOW, selector(%rip)

    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %edi
    mov $PR_SYS_DISPATCH_ON, %esi
    lea dispatch_allowed(%rip), %rdx
    mov $(dispatch_allowed_end - dispatch_allowed), %r10d
    lea selector(%rip), %r8
    syscall
    test %rax, %rax
    js setup_failed

    /* Linux clears syscall user dispatch in a fork child. */
    mov $__NR_fork, %eax
    syscall
    test %rax, %rax
    jz fork_child
    js fork_failed

    mov %eax, %edi
    lea child_status(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $__NR_wait4, %eax
    syscall
    test %rax, %rax
    js fork_failed
    cmpl $0, child_status(%rip)
    jne fork_failed
    jmp test_blocked_syscall

fork_child:
    movb $1, test_stage(%rip)
    movb $SYSCALL_DISPATCH_FILTER_BLOCK, selector(%rip)
    mov $__NR_getpid, %eax
    syscall
    xor %edi, %edi
    jmp exit_now

test_blocked_syscall:
    movb $2, test_stage(%rip)
    movb $0, signal_seen(%rip)
    movb $SYSCALL_DISPATCH_FILTER_BLOCK, selector(%rip)

    /* This syscall is outside dispatch_allowed and must raise guest SIGSYS. */
blocked_syscall:
    mov $__NR_getpid, %eax
    syscall
blocked_syscall_after:
    cmp $__NR_getpid, %rax
    jne bad_return
    cmpb $1, signal_seen(%rip)
    jne bad_siginfo

    /* Turning dispatch off makes the selector irrelevant. */
    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %edi
    mov $PR_SYS_DISPATCH_OFF, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js setup_failed
    movb $SYSCALL_DISPATCH_FILTER_BLOCK, selector(%rip)
    mov $__NR_getpid, %eax
    syscall
    test %rax, %rax
    jle disable_failed

    xor %edi, %edi
    jmp exit_now

bad_return:
    mov $10, %edi
    jmp exit_now

unknown_prctl_failed:
    mov $13, %edi
    jmp exit_now

setup_failed:
    mov $11, %edi
    jmp exit_now

fork_failed:
    mov $14, %edi
    jmp exit_now

bad_param_failed:
    mov $15, %edi
    jmp exit_now

allowed_range_failed:
    mov $16, %edi
    jmp exit_now

disable_failed:
    mov $17, %edi
    jmp exit_now

.size _start, .-_start

.p2align 4
dispatch_allowed:
    cmpb $1, test_stage(%rip)
    je fork_dispatch_inherited
    cmp $SIGSYS, %edi
    jne bad_siginfo
    cmpl $0, 4(%rsi)
    jne bad_siginfo
    cmpl $SYS_USER_DISPATCH, 8(%rsi)
    jne bad_siginfo
    lea blocked_syscall_after(%rip), %rax
    cmpq %rax, 16(%rsi)
    jne bad_siginfo
    cmpl $__NR_getpid, 24(%rsi)
    jne bad_siginfo
    cmpl $AUDIT_ARCH_X86_64, 28(%rsi)
    jne bad_siginfo
    movb $1, signal_seen(%rip)
    movb $SYSCALL_DISPATCH_FILTER_ALLOW, selector(%rip)
    ret

bad_siginfo:
    mov $12, %edi
    jmp exit_now

fork_dispatch_inherited:
    mov $14, %edi
    jmp exit_now

allowed_getpid:
    mov $__NR_getpid, %eax
    syscall
    ret

exit_now:
    mov $__NR_exit, %eax
    syscall
    ud2

sigreturn:
    mov $15, %eax
    syscall
    ud2
dispatch_allowed_end:

.section .rodata
.p2align 3
action:
    .quad dispatch_allowed
    .quad SA_SIGINFO | SA_RESTORER
    .quad sigreturn
    .quad 0

.section .data
selector:
    .byte 0
test_stage:
    .byte 0
signal_seen:
    .byte 0

.section .bss
.p2align 2
child_status:
    .skip 4

.section .note.GNU-stack,"",@progbits
