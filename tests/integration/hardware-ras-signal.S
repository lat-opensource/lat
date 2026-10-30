# SPDX-License-Identifier: GPL-2.0-only
        .text
        .globl _start
_start:
        call callee
continuation:
        /* Exercise a second CALL/RET path before the exit syscall. */
        push $tail_exit
        jmp callee
tail_exit:
        cmpl $7, %eax
        jne fail
        xorl %ebx, %ebx
        jmp quit
fail:
        movl $1, %ebx
quit:
#ifdef __x86_64__
        movl %ebx, %edi
        movl $60, %eax
        syscall
#else
        movl $1, %eax
        int $0x80
#endif
        .org 0x4000
callee:
        movl $7, %eax
        ret
        .section .note.GNU-stack,"",@progbits
