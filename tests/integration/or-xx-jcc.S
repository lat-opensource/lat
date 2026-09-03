/* SPDX-License-Identifier: GPL-2.0-only */
    .equ __NR_rt_sigaction, 13
    .equ __NR_rt_sigreturn, 15
    .equ SIGSEGV, 11
    .equ SA_SIGINFO, 0x00000004
    .equ SA_RESTORER, 0x04000000
    .equ UCONTEXT_RIP_OFFSET, 168
    .equ UCONTEXT_EFL_OFFSET, 176

    .data
    .p2align 3
segv_action:
    .quad segv_handler
    .quad (SA_SIGINFO | SA_RESTORER)
    .quad signal_restorer
    .quad 0
expected_eflags:
    .quad 0
resume_rip:
    .quad 0
signal_count:
    .quad 0

    .text
    .globl _start
_start:
    cmpq $2, (%rsp)
    je fault_case
    cmpq $3, (%rsp)
    je .Lsignal_cases_ok

    movl $__NR_rt_sigaction, %eax
    movl $SIGSEGV, %edi
    leaq segv_action(%rip), %rsi
    xorl %edx, %edx
    movl $8, %r10d
    syscall
    testq %rax, %rax
    js install_failed

    xorl %r15d, %r15d

    /* Recover flags from the saved 8-bit OR result, not the overwritten AL. */
    xorl %eax, %eax
    movq $0x44, expected_eflags(%rip)
    leaq .Lfault8_resume(%rip), %r10
    movq %r10, resume_rip(%rip)
    movb $0x80, %dl
    addb %dl, %dl
    orb $0, %al
    movb $0x7f, %al
    movl (%r15), %r11d
.Lfault8_resume:
    je .Lfault8_ok
    movl $20, %edi
    jmp exit_guest
.Lfault8_ok:

    /* Recover PF and SF from a 16-bit negative result. */
    movw $0x8000, %ax
    movq $0x84, expected_eflags(%rip)
    leaq .Lfault16_resume(%rip), %r10
    movq %r10, resume_rip(%rip)
    movb $0x80, %dl
    addb %dl, %dl
    orw $0, %ax
    movw $0, %ax
    movl (%r15), %r11d
.Lfault16_resume:
    js .Lfault16_ok
    movl $21, %edi
    jmp exit_guest
.Lfault16_ok:

    /* Recover clear CF/PF/ZF/SF/OF from a 32-bit result of one. */
    movl $1, %eax
    movq $0, expected_eflags(%rip)
    leaq .Lfault32_resume(%rip), %r10
    movq %r10, resume_rip(%rip)
    movb $0x80, %dl
    addb %dl, %dl
    orl $0, %eax
    movl $-1, %eax
    movl (%r15), %r11d
.Lfault32_resume:
    jne .Lfault32_ok
    movl $22, %edi
    jmp exit_guest
.Lfault32_ok:

    /* Recover PF and SF from a 64-bit negative result. */
    movq $-1, %rax
    movq $0x84, expected_eflags(%rip)
    leaq .Lfault64_resume(%rip), %r10
    movq %r10, resume_rip(%rip)
    movb $0x80, %dl
    addb %dl, %dl
    orq $0, %rax
    movq $0, %rax
    movl (%r15), %r11d
.Lfault64_resume:
    js .Lfault64_ok
    movl $23, %edi
    jmp exit_guest
.Lfault64_ok:
    cmpq $4, signal_count(%rip)
    je .Lsignal_cases_ok
    movl $24, %edi
    jmp exit_guest
.Lsignal_cases_ok:

    /* One neutral instruction, 8-bit zero result. */
    movl $1, %edi
    xorl %eax, %eax
    orb $0, %al
    nop
    je .Lzero_ok
    jmp exit_guest
.Lzero_ok:

    /* The fixed pattern register survives an overwrite of the guest GPR. */
    movl $2, %edi
    movw $1, %ax
    orw $0, %ax
    movw $0, %ax
    jne .Loverwrite_ok
    jmp exit_guest
.Loverwrite_ok:

    /* LEA is non-faulting and may consume ordinary itemps. */
    movl $3, %edi
    movl $0x80000000, %eax
    orl $0, %eax
    leaq 8(%r10,%r11,2), %r12
    js .Lsign_ok
    jmp exit_guest
.Lsign_ok:

    /* Multiple intervening instructions and JNS. */
    movl $4, %edi
    movq $1, %rax
    orq $0, %rax
    movq $2, %r10
    movq %r10, %r11
    leaq 1(%r11), %r12
    nop
    jns .Ldistance4_ok
    jmp exit_guest
.Ldistance4_ok:

    /* Register-only extensions are safe neutral instructions. */
    movl $5, %edi
    movb $1, %r10b
    movl $1, %eax
    orl $0, %eax
    movzbl %r10b, %r11d
    movslq %r11d, %r12
    jne .Lextensions_ok
    jmp exit_guest
.Lextensions_ok:

    /* The fixed pattern register survives a 64-bit destination overwrite. */
    movl $6, %edi
    movq $1, %rax
    orq $0, %rax
    movq $0, %rax
    jne .Lborrow_clobber_ok
    jmp exit_guest
.Lborrow_clobber_ok:

    /* Flags needed by the successor must still be materialized. */
    movl $7, %edi
    movb $0x81, %al
    orb $0, %al
    nop
    js .Lflags_live
    jmp exit_guest
.Lflags_live:
    pushfq
    popq %r10
    andq $0x8c5, %r10
    cmpq $0x84, %r10
    jne exit_guest

    /* A memory intermediate preserves the guest-visible result. */
    movl $8, %edi
    movl $1, %eax
    orl $0, %eax
    movl mem32(%rip), %r10d
    jne .Lmemory_reject_ok
    jmp exit_guest
.Lmemory_reject_ok:

    /* More than four intervening instructions. */
    movl $9, %edi
    movl $1, %eax
    orl $0, %eax
    nop
    nop
    nop
    nop
    nop
    jne .Ldistance_reject_ok
    jmp exit_guest
.Ldistance_reject_ok:

    /* A flag clobber must prevent the OR pattern from reaching this Jcc. */
    movl $10, %edi
    xorl %eax, %eax
    orl $0, %eax
    addl $1, %r10d
    jne .Lclobber_reject_ok
    jmp exit_guest
.Lclobber_reject_ok:

    /* PUSH can fault and therefore cannot be crossed. */
    movl $11, %edi
    movl $1, %eax
    orl $0, %eax
    pushq %rax
    jne .Lpush_reject_ok
    jmp exit_guest
.Lpush_reject_ok:
    addq $8, %rsp

    /* Hot path with ordinary-itemp pressure between OR and Jcc. */
    movl $200000, %ecx
    xorl %r8d, %r8d
.Lhot_loop:
    orb $0, %r8b
    leaq 1(%r10,%r11,2), %r12
    movzbl %r12b, %r13d
    movslq %r13d, %r14
    nop
    jne .Lhot_taken
    xorb $1, %r8b
    jmp .Lhot_count
.Lhot_taken:
    xorb $1, %r8b
.Lhot_count:
    decl %ecx
    jne .Lhot_loop

    xorl %edi, %edi
exit_guest:
    movl $60, %eax
    syscall

fault_case:
    xorl %eax, %eax
    movl $1, %r10d
    orl $0, %r10d
    movl (%rax), %r11d
    jne .Lfault_unreachable
.Lfault_unreachable:
    movl $12, %edi
    jmp exit_guest

install_failed:
    movl $13, %edi
    jmp exit_guest

    .type segv_handler, @function
segv_handler:
    cmpl $SIGSEGV, %edi
    jne handler_wrong_signal
    movq UCONTEXT_EFL_OFFSET(%rdx), %rax
    andq $0x8c5, %rax
    cmpq expected_eflags(%rip), %rax
    jne handler_wrong_eflags
    incq signal_count(%rip)
    movq resume_rip(%rip), %rax
    movq %rax, UCONTEXT_RIP_OFFSET(%rdx)
    ret
    .size segv_handler, .-segv_handler

handler_wrong_signal:
    movl $30, %edi
    jmp exit_guest

handler_wrong_eflags:
    movl $31, %edi
    jmp exit_guest

    .type signal_restorer, @function
signal_restorer:
    movl $__NR_rt_sigreturn, %eax
    syscall
    .size signal_restorer, .-signal_restorer

    .data
    .p2align 2
mem32:
    .long 0
