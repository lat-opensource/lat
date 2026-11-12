/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * More than 1024 connected TBs fit within the independent page/IR1 budgets.
 */
        .text
        .globl _start
_start:
        xorl %eax, %eax
        movl $callee, %edx
#ifdef __x86_64__
        call *%rdx
#else
        call *%edx
#endif
        .rept 1100
        incl %eax
        jmp 1f
        /* Keep each target distinct from the JMP fallthrough address. */
        int3
1:
        .endr
        cmpl $1101, %eax
        setne %bl
        movzbl %bl, %ebx
#ifdef __x86_64__
        movl %ebx, %edi
        movl $60, %eax
        syscall
#else
        movl $1, %eax
        int $0x80
#endif
callee:
        incl %eax
        ret
        .section .note.GNU-stack,"",@progbits
