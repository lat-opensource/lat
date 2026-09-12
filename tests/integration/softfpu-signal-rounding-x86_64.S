.equ __NR_rt_sigaction, 13
.equ __NR_rt_sigreturn, 15
.equ __NR_getpid, 39
.equ __NR_kill, 62
.equ __NR_exit, 60

.equ SIGUSR1, 10
.equ SA_SIGINFO, 4
.equ SA_RESTORER, 0x04000000

.macro check_down fail
    fldl 8(%rsp)
    fcos
    fstps 4(%rsp)
    cmpl $0x3f7fffff, 4(%rsp)
    jne \fail
.endm

.macro check_sse_up fail
    movss 16(%rsp), %xmm0
    addss 20(%rsp), %xmm0
    movss %xmm0, 4(%rsp)
    cmpl $0x3f800001, 4(%rsp)
    jne \fail
.endm

.section .text
.global _start
.type _start, @function
_start:
    and $-16, %rsp
    sub $64, %rsp

    movw $0x077f, 0(%rsp)
    movl $0, 8(%rsp)
    movl $0x3d700000, 12(%rsp)
    movl $0x3f800000, 16(%rsp)
    movl $0x33800000, 20(%rsp)
    movl $0x5f80, 24(%rsp)

    mov $__NR_rt_sigaction, %eax
    mov $SIGUSR1, %edi
    lea signal_action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js .Lfail_sigaction

    fldcw 0(%rsp)
    ldmxcsr 24(%rsp)
    mov $__NR_getpid, %eax
    syscall
    mov %eax, %edi
    mov $SIGUSR1, %esi
    mov $__NR_kill, %eax
    syscall
    test %rax, %rax
    js .Lfail_signal

    check_sse_up .Lfail_mxcsr
    check_down .Lfail_x87
    xor %edi, %edi
    jmp .Lexit
.Lfail_sigaction:
    mov $1, %edi
    jmp .Lexit
.Lfail_signal:
    mov $2, %edi
    jmp .Lexit
.Lfail_x87:
    mov $3, %edi
    jmp .Lexit
.Lfail_mxcsr:
    mov $4, %edi
.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.type signal_handler, @function
signal_handler:
    fldcw signal_nearest_cw(%rip)
    ldmxcsr signal_down_mxcsr(%rip)
    ret
.size signal_handler, .-signal_handler

.type signal_restorer, @function
signal_restorer:
    mov $__NR_rt_sigreturn, %eax
    syscall
    ud2
.size signal_restorer, .-signal_restorer

.section .data
.align 8
signal_action:
    .quad signal_handler
    .quad SA_SIGINFO | SA_RESTORER
    .quad signal_restorer
    .quad 0
signal_nearest_cw:
    .short 0x037f
.align 4
signal_down_mxcsr:
    .long 0x3f80

.section .note.GNU-stack,"",@progbits
