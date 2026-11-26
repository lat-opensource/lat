/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Thirty long members make the encoded fault-search metadata exceed the
 * fifty-one-member static scratch size even though the member count does not.
 */
        .text
        .globl _start
        .macro long_block
        .rept 200
        addl %ecx, %eax
        .endr
        jmp 1f
        nop
1:
        .endm
_start:
        movl $1, %ecx
        .rept 30
        long_block
        .endr
        xorl %edi, %edi
#ifdef __x86_64__
        movl $60, %eax
        syscall
#else
        movl $1, %eax
        int $0x80
#endif
        .section .note.GNU-stack,"",@progbits
