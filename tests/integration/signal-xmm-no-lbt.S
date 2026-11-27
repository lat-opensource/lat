.equ __NR_rt_sigaction, 13
.equ __NR_rt_sigreturn, 15
.equ __NR_exit, 60

.equ SIGTRAP, 5
.equ SA_RESTORER, 0x04000000

.section .text
.global _start
.type _start, @function
_start:
    sub $32, %rsp
    lea .Lhandler(%rip), %rax
    mov %rax, 0(%rsp)
    movq $SA_RESTORER, 8(%rsp)
    lea .Lrestorer(%rip), %rax
    mov %rax, 16(%rsp)
    movq $0, 24(%rsp)

    mov $__NR_rt_sigaction, %eax
    mov $SIGTRAP, %edi
    mov %rsp, %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js .Lfail

    movabs $0x1122334455667788, %rax
    movq %rax, %xmm0
    movabs $0x8877665544332211, %rax
    pinsrq $1, %rax, %xmm0
    int3

    movq %xmm0, %rax
    movabs $0x1122334455667788, %rcx
    cmp %rcx, %rax
    jne .Lfail
    pextrq $1, %xmm0, %rax
    movabs $0x8877665544332211, %rcx
    cmp %rcx, %rax
    jne .Lfail

    xor %edi, %edi
    jmp .Lexit

.Lhandler:
    pxor %xmm0, %xmm0
    ret

.Lrestorer:
    mov $__NR_rt_sigreturn, %eax
    syscall

.Lfail:
    mov $1, %edi

.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start
