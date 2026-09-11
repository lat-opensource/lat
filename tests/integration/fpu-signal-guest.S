/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Freestanding x86_64 guest for signal-frame observation and sigreturn.
 * Self-signalling through kill exercises delivery at a syscall boundary,
 * not arbitrary interruption of translated code or a host libm helper.
 * Report: uint32_t handler_seen followed by a case-specific uint32_t value.
 */

#ifndef TEST_CASE
#error TEST_CASE must select a signal FPU test
#endif
#if TEST_CASE < 1 || TEST_CASE > 3
#error unsupported TEST_CASE
#endif

.equ __NR_write, 1
.equ __NR_rt_sigaction, 13
.equ __NR_rt_sigreturn, 15
.equ __NR_getpid, 39
.equ __NR_kill, 62
.equ __NR_exit, 60
.equ SIGUSR1, 10
.equ SA_SIGINFO, 4
.equ SA_RESTORER, 0x04000000
.equ MXCSR_STATUS_MASK, 0x0000003f
.equ REPORT_SIZE, 8

/* struct target_ucontext.tuc_mcontext is target_sigcontext_64; its
 * fpstate pointer lives at offset 184, and tuc_mcontext starts at 40. */
.equ UC_MCONTEXT_FPSTATE_OFF, 224
/* struct target_fpstate_fxsave.mxcsr is at offset 24. */
.equ FPSTATE_MXCSR_OFF, 24

.section .text
.global _start
.type _start, @function
_start:
    fninit
    fnclex
    ldmxcsr clean_mxcsr(%rip)

    /* rt_sigaction(SIGUSR1, &act, NULL, 8) */
    leaq sig_handler(%rip), %rax
    movq %rax, act_handler(%rip)
    movq $(SA_SIGINFO | SA_RESTORER), act_flags(%rip)
    leaq sig_restorer(%rip), %rax
    movq %rax, act_restorer(%rip)
    movl $__NR_rt_sigaction, %eax
    movl $SIGUSR1, %edi
    leaq act(%rip), %rsi
    xorl %edx, %edx
    movl $8, %r10d
    syscall
    testq %rax, %rax
    js syscall_failed

    /* Masked SSE 0/0: sets IE sticky in the native FCSR, no trap. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0

    /* kill(getpid(), SIGUSR1).  The syscall forces a TB exit, saving the
     * native FCSR into env->fcsr, but env->mxcsr is still clean. */
    movl $__NR_getpid, %eax
    syscall
    testq %rax, %rax
    js syscall_failed
    movq %rax, %rdi
    movl $__NR_kill, %eax
    movl $SIGUSR1, %esi
    syscall
    testq %rax, %rax
    js syscall_failed

#if TEST_CASE == 2
    /* The frame requests ZE only; handler-generated IE must not survive. */
    stmxcsr report_value(%rip)
    andl $MXCSR_STATUS_MASK, report_value(%rip)
#elif TEST_CASE == 3
    /* The frame requests RU, while the handler last used RD. */
    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0
    movss %xmm0, report_value(%rip)
#endif

    /* write(1, &handler_seen, REPORT_SIZE) */
    movl $__NR_write, %eax
    movl $1, %edi
    leaq handler_seen(%rip), %rsi
    movl $REPORT_SIZE, %edx
    syscall
    cmpq $REPORT_SIZE, %rax
    jne syscall_failed

    xorl %edi, %edi
    movl $__NR_exit, %eax
    syscall

syscall_failed:
    movl $1, %edi
    movl $__NR_exit, %eax
    syscall
.size _start, .-_start

/*
 * handler(int signo, siginfo_t *info, void *ucontext)
 *
 * Observe MXCSR through the ucontext's fpstate, the same path a real
 * handler uses.  This bypasses the STMXCSR translator merge, so only a
 * signal-frame FCSR flush can make the pending IE visible here.
 */
.type sig_handler, @function
sig_handler:
    movl $1, handler_seen(%rip)
    movq UC_MCONTEXT_FPSTATE_OFF(%rdx), %rax
    testq %rax, %rax
    jz syscall_failed
#if TEST_CASE == 1
    movl FPSTATE_MXCSR_OFF(%rax), %ecx
    andl $MXCSR_STATUS_MASK, %ecx
    movl %ecx, report_value(%rip)
#elif TEST_CASE == 2
    /* Saved frame and live handler state are deliberately different. */
    movl $0x00001f84, FPSTATE_MXCSR_OFF(%rax)
    ldmxcsr clean_mxcsr(%rip)
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
#else
    movl $0x00005f80, FPSTATE_MXCSR_OFF(%rax)
    ldmxcsr down_mxcsr(%rip)
    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0
#endif
    ret
.size sig_handler, .-sig_handler

.type sig_restorer, @function
sig_restorer:
    movl $__NR_rt_sigreturn, %eax
    syscall
    ud2
.size sig_restorer, .-sig_restorer

.section .rodata
.align 4
clean_mxcsr:
    .long 0x00001f80
down_mxcsr:
    .long 0x00003f80
one:
    .long 0x3f800000
half_ulp:
    .long 0x33800000

.section .data
.align 8
act:
act_handler:
    .quad 0
act_flags:
    .quad 0
act_restorer:
    .quad 0
act_mask:
    .quad 0

.section .bss
.align 4
handler_seen:
    .zero 4
report_value:
    .zero 4

.section .note.GNU-stack,"",@progbits
