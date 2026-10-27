/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * x86-64 XSAVE64/XRSTOR64 must save and restore the non-volatile XMM
 * registers. Wine's syscall dispatcher relies on this across win32u calls;
 * a missing XSAVE64 translation corrupts dialog sizes and other state held
 * in XMM6-XMM15.
 */
.intel_syntax noprefix
.global _start
.section .text
_start:
    lea rbx, [area]
    add rbx, 63
    and rbx, -64

    movdqu xmm6, [pat6]
    movdqu xmm7, [pat7]

    mov eax, 3
    xor edx, edx
    xsave64 [rbx]

    mov rax, [rbx + 0x100]
    cmp rax, [pat6]
    jne fail
    mov rax, [rbx + 0x108]
    cmp rax, [pat6 + 8]
    jne fail
    mov rax, [rbx + 0x110]
    cmp rax, [pat7]
    jne fail
    mov rax, [rbx + 0x118]
    cmp rax, [pat7 + 8]
    jne fail

    movdqu xmm6, [pat8]
    movdqu xmm7, [pat9]

    mov eax, 3
    xor edx, edx
    xrstor64 [rbx]

    movdqu [actual], xmm6
    movdqu [actual + 16], xmm7
    mov rax, [actual]
    cmp rax, [pat6]
    jne fail
    mov rax, [actual + 8]
    cmp rax, [pat6 + 8]
    jne fail
    mov rax, [actual + 16]
    cmp rax, [pat7]
    jne fail
    mov rax, [actual + 24]
    cmp rax, [pat7 + 8]
    jne fail

    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

.section .rodata
.p2align 4
pat6: .quad 0x1111111111111111, 0x2222222222222222
pat7: .quad 0x3333333333333333, 0x4444444444444444
pat8: .quad 0x5555555555555555, 0x6666666666666666
pat9: .quad 0x7777777777777777, 0x8888888888888888

.section .bss
.p2align 6
area: .skip 2048
actual: .skip 32
