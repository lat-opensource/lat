.equ __NR_exit, 1
.equ __NR_getpid, 20
.equ __NR_kill, 37
.equ __NR_rt_sigreturn, 173
.equ __NR_rt_sigaction, 174

.equ SIGUSR1, 10
.equ SA_SIGINFO, 4
.equ SA_RESTORER, 0x04000000

.macro check_down fail
    fldl 8(%esp)
    fcos
    fstps 4(%esp)
    cmpl $0x3f7fffff, 4(%esp)
    jne \fail
.endm

.macro check_sse_up fail
    movss 16(%esp), %xmm0
    addss 20(%esp), %xmm0
    movss %xmm0, 4(%esp)
    cmpl $0x3f800001, 4(%esp)
    jne \fail
.endm

.section .text
.global _start
.type _start, @function
_start:
    and $-16, %esp
    sub $64, %esp

    movw $0x077f, 0(%esp)
    movl $0, 8(%esp)
    movl $0x3d700000, 12(%esp)
    movl $0x3f800000, 16(%esp)
    movl $0x33800000, 20(%esp)
    movl $0x5f80, 24(%esp)

    mov $__NR_rt_sigaction, %eax
    mov $SIGUSR1, %ebx
    mov $signal_action, %ecx
    xor %edx, %edx
    mov $8, %esi
    int $0x80
    test %eax, %eax
    js .Lfail_sigaction

    fldcw 0(%esp)
    ldmxcsr 24(%esp)
    mov $__NR_getpid, %eax
    int $0x80
    mov %eax, %ebx
    mov $SIGUSR1, %ecx
    mov $__NR_kill, %eax
    int $0x80
    test %eax, %eax
    js .Lfail_signal

    check_sse_up .Lfail_mxcsr
    check_down .Lfail_x87
    xor %ebx, %ebx
    jmp .Lexit
.Lfail_sigaction:
    mov $1, %ebx
    jmp .Lexit
.Lfail_signal:
    mov $2, %ebx
    jmp .Lexit
.Lfail_x87:
    mov $3, %ebx
    jmp .Lexit
.Lfail_mxcsr:
    mov $4, %ebx
.Lexit:
    mov $__NR_exit, %eax
    int $0x80
.size _start, .-_start

.type signal_handler, @function
signal_handler:
    fldcw signal_nearest_cw
    ldmxcsr signal_down_mxcsr
    ret
.size signal_handler, .-signal_handler

.type signal_restorer, @function
signal_restorer:
    mov $__NR_rt_sigreturn, %eax
    int $0x80
    ud2
.size signal_restorer, .-signal_restorer

.section .data
.align 8
signal_action:
    .long signal_handler
    .long SA_SIGINFO | SA_RESTORER
    .long signal_restorer
    .quad 0
signal_nearest_cw:
    .short 0x037f
.align 4
signal_down_mxcsr:
    .long 0x3f80

.section .note.GNU-stack,"",@progbits
