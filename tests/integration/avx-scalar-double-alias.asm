; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

section .text

%macro CHECK_RESULT 2
    mov edi, %2
    vmovdqu [actual], ymm0
    mov rax, [actual]
    cmp rax, [%1]
    jne fail
    mov rax, [actual + 8]
    cmp rax, [input_a + 8]
    jne fail
    mov rax, [actual + 16]
    or rax, [actual + 24]
    jne fail
%endmacro

%macro CHECK_ALIASES 3
    vmovdqu ymm0, [junk]
    vmovdqu ymm1, [input_a]
    vmovdqu ymm2, [input_b]
    %1 xmm0, xmm1, xmm2
    CHECK_RESULT %2, 1

    vmovdqu ymm0, [input_a]
    %1 xmm0, xmm0, xmm2
    CHECK_RESULT %2, 2

    vmovdqu ymm0, [input_b]
    %1 xmm0, xmm1, xmm0
    CHECK_RESULT %2, 3

    vmovdqu ymm0, [junk]
    %1 xmm0, xmm1, xmm1
    CHECK_RESULT %3, 4

    vmovdqu ymm0, [input_a]
    %1 xmm0, xmm0, xmm0
    CHECK_RESULT %3, 5

    vmovdqu ymm0, [junk]
    %1 xmm0, xmm1, [input_b]
    CHECK_RESULT %2, 6

    vmovdqu ymm0, [input_a]
    %1 xmm0, xmm0, [input_b]
    CHECK_RESULT %2, 7
%endmacro

_start:
    mov edi, 1
    CHECK_ALIASES vaddsd, sum_ab, sum_aa
    CHECK_ALIASES vsubsd, diff_ab, diff_aa
    CHECK_ALIASES vmulsd, product_ab, product_aa
    CHECK_ALIASES vdivsd, quotient_ab, quotient_aa
    xor edi, edi
fail:
    mov eax, 60
    syscall

section .rodata align=32
input_a: dq 6.0, 0x1122334455667788, -1, -1
input_b: dq 2.0, 0x8877665544332211, -1, -1
junk: times 4 dq 0xdeadbeefdeadbeef
sum_ab: dq 8.0
sum_aa: dq 12.0
diff_ab: dq 4.0
diff_aa: dq 0.0
product_ab: dq 12.0
product_aa: dq 36.0
quotient_ab: dq 3.0
quotient_aa: dq 1.0

section .bss align=32
actual: resb 32

section .note.GNU-stack noalloc noexec nowrite progbits
