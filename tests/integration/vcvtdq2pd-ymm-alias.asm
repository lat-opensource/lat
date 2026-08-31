; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

section .text

%macro CHECK_RESULT 1
    vmovdqu [actual], ymm0
    mov rax, [actual]
    cmp rax, [%1]
    jne fail
    mov rax, [actual + 8]
    cmp rax, [%1 + 8]
    jne fail
    mov rax, [actual + 16]
    cmp rax, [%1 + 16]
    jne fail
    mov rax, [actual + 24]
    cmp rax, [%1 + 24]
    jne fail
%endmacro

_start:
    vmovdqu xmm1, [input]
    vpxor ymm0, ymm0, ymm0
    vcvtdq2pd ymm0, xmm1
    CHECK_RESULT expected

    vmovdqu ymm1, [alias_input]
    vcvtdq2pd ymm1, xmm1
    vmovdqu ymm0, ymm1
    CHECK_RESULT expected

    vpxor ymm0, ymm0, ymm0
    vcvtdq2pd ymm0, [input]
    CHECK_RESULT expected

    vmovdqu ymm2, [alias_input]
    vcvtdq2pd ymm2, [input]
    vmovdqu ymm0, ymm2
    CHECK_RESULT expected

    xor edi, edi
fail:
    mov eax, 60
    syscall

section .rodata align=32
input:
    dd 0, 0x80000000, 0x7fffffff, -1
    dd 1, 2, 3, 4
alias_input:
    dd 0, 0x80000000, 0x7fffffff, -1
    dq 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef
expected:
    dq 0x0000000000000000, 0xc1e0000000000000
    dq 0x41dfffffffc00000, 0xbff0000000000000

section .bss align=32
actual: resb 32

section .note.GNU-stack noalloc noexec nowrite progbits
