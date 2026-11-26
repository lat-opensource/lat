/* SPDX-License-Identifier: GPL-2.0-only */
/* No guest libc needed; build with -m32/-m64 -nostdlib -static -no-pie. */
        .text
        .globl _start
_start:
        movl $2000, %esi
loop:
        movl %esi, %edx
        andl $1, %edx
#ifdef __x86_64__
        leaq targets(%rip), %rbx
        movq (%rbx,%rdx,8), %rax
#else
        movl targets(,%edx,4), %eax
#endif
        call invoke
        leal 7(,%edx,2), %ebx
        cmpl %ebx, %eax
        jne fail
        decl %esi
        jnz loop
        xorl %ebx, %ebx
        jmp exit
fail:
        movl $1, %ebx
exit:
#ifdef __x86_64__
        movl %ebx, %edi
        movl $60, %eax
        syscall
#else
        movl $1, %eax
        int $0x80
#endif
        .p2align 12
        .globl invoke, invoke_end
invoke:
        testl %edx, %edx
        jz invoke_zero
        nop
invoke_zero:
        /* More than 64 CALLIN members exercise the complete TU member list. */
        movl %eax, %edi
        .rept 70
#ifdef __x86_64__
        movq %rdi, %rax
        call *%rax
#else
        movl %edi, %eax
        call *%eax
#endif
        addl $1, %eax
        .endr
        ret
invoke_end:
        .p2align 12
f0:
        movl $6, %eax
        ret
f1:
        movl $8, %eax
        ret
        .section .rodata
        .p2align 3
targets:
#ifdef __x86_64__
        .quad f0, f1
#else
        .long f0, f1
#endif
        .section .note.GNU-stack,"",@progbits
