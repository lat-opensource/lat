.equ __NR_rt_sigaction, 13
.equ __NR_rt_sigreturn, 15
.equ __NR_exit, 60
.equ SIGTRAP, 5
.equ SA_SIGINFO, 0x00000004
.equ SA_RESTORER, 0x04000000
/* offsetof(ucontext_t, uc_mcontext.gregs[REG_RIP]) on x86_64. */
.equ UCONTEXT_RIP_OFFSET, 168

.section .data
.align 8
trap_action:
    .quad trap_handler
    .quad (SA_SIGINFO | SA_RESTORER)
    .quad signal_restorer
    .quad 0

.section .bss
.align 4
trap_seen:
    .long 0

.section .text
.global _start
.type _start, @function
_start:
    mov $__NR_rt_sigaction, %eax
    mov $SIGTRAP, %edi
    lea trap_action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js install_failed

    int3
after_int3:
    cmpl $1, trap_seen(%rip)
    jne signal_not_delivered
    xor %edi, %edi
    jmp exit

install_failed:
    mov $10, %edi
    jmp exit

.type trap_handler, @function
trap_handler:
    cmp $SIGTRAP, %edi
    jne wrong_signal
    lea after_int3(%rip), %rax
    cmp %rax, UCONTEXT_RIP_OFFSET(%rdx)
    jne wrong_rip
    movl $1, trap_seen(%rip)
    ret
.size trap_handler, .-trap_handler

wrong_signal:
    mov $11, %edi
    jmp exit

wrong_rip:
    mov $12, %edi
    jmp exit

signal_not_delivered:
    mov $13, %edi

exit:
    mov $__NR_exit, %eax
    syscall

.type signal_restorer, @function
signal_restorer:
    mov $__NR_rt_sigreturn, %eax
    syscall
.size signal_restorer, .-signal_restorer

.section .note.GNU-stack,"",@progbits
