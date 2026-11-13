; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

section .text

%macro CHECK 2
    vmovdqu [actual], ymm0
    mov edi, %2
    mov rax, [%1]
    cmp rax, [actual]
    jne fail
    mov rax, [%1 + 8]
    cmp rax, [actual + 8]
    jne fail
    mov rax, [%1 + 16]
    cmp rax, [actual + 16]
    jne fail
    mov rax, [%1 + 24]
    cmp rax, [actual + 24]
    jne fail
%endmacro

%macro ALIASES 1
    lea rbx, [expected_ab_low]
    lea rdx, [expected_aa_low]
    test byte [immediate], 1
    jz %%expected
    lea rbx, [expected_ab_high]
    lea rdx, [expected_aa_high]
%%expected:
    vmovdqu ymm0, [junk]
    vmovdqu ymm1, [input_a]
    vmovdqu ymm2, [input_b]
    vinsertf128 ymm0, ymm1, xmm2, %1
    CHECK rbx, 1
    vmovdqu ymm0, [input_a]
    vinsertf128 ymm0, ymm0, xmm2, %1
    CHECK rbx, 2
    vmovdqu ymm0, [input_b]
    vinsertf128 ymm0, ymm1, xmm0, %1
    CHECK rbx, 3
    vinsertf128 ymm0, ymm1, xmm1, %1
    CHECK rdx, 4
    vmovdqu ymm0, [input_a]
    vinsertf128 ymm0, ymm0, xmm0, %1
    CHECK rdx, 5
    vinsertf128 ymm0, ymm1, [input_b], %1
    CHECK rbx, 6
    vmovdqu ymm0, [input_a]
    vinsertf128 ymm0, ymm0, [input_b], %1
    CHECK rbx, 7
%endmacro

_start:
%assign imm 0
%rep 256
    mov byte [immediate], imm
    ALIASES imm
%assign imm imm+1
%endrep
    ; A pending VEX upper-half clear must be applied before reading src1.
    vmovdqu ymm0, [input_a]
    vmovdqu ymm2, [input_b]
    vpxor xmm0, xmm0, xmm0
    vinsertf128 ymm0, ymm0, xmm2, 0
    CHECK expected_pending, 8
    xor edi, edi
fail:
    mov eax, 60
    syscall

section .rodata align=32
input_a: dq 0x7ff0000000000001, 0x8000000000000000
         dq 0x123456789abcdef0, 0xfedcba9876543210
input_b: dq 0xfff800000000cafe, 0x0123456789abcdef
         dq 0xaaaaaaaa55555555, 0x9999999966666666
junk: times 4 dq 0xdeadbeefdeadbeef
expected_ab_low: dq 0xfff800000000cafe, 0x0123456789abcdef
                 dq 0x123456789abcdef0, 0xfedcba9876543210
expected_ab_high: dq 0x7ff0000000000001, 0x8000000000000000
                  dq 0xfff800000000cafe, 0x0123456789abcdef
expected_aa_low: dq 0x7ff0000000000001, 0x8000000000000000
                 dq 0x123456789abcdef0, 0xfedcba9876543210
expected_aa_high: times 2 dq 0x7ff0000000000001, 0x8000000000000000
expected_pending: dq 0xfff800000000cafe, 0x0123456789abcdef, 0, 0

section .bss align=32
actual: resb 32
immediate: resb 1

section .note.GNU-stack noalloc noexec nowrite progbits
