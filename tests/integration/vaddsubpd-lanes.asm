; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

section .text

%macro CHECK 2
    mov edi, %2
    vmovdqu [actual], ymm0
    mov rax, [actual]
    cmp rax, [%1]
    jne fail
    mov rax, [actual + 8]
    cmp rax, [%1 + 8]
    jne fail
    mov rax, [actual + 16]
    or rax, [actual + 24]
    jne fail
%endmacro

%macro ALIASES 0
    vmovdqu ymm0, [junk]
    vmovdqu ymm1, [input_a]
    vmovdqu ymm2, [input_b]
    vaddsubpd xmm0, xmm1, xmm2
    CHECK expected_ab, 1
    vmovdqu ymm0, [input_a]
    vaddsubpd xmm0, xmm0, xmm2
    CHECK expected_ab, 2
    vmovdqu ymm0, [input_b]
    vaddsubpd xmm0, xmm1, xmm0
    CHECK expected_ab, 3
    vaddsubpd xmm0, xmm1, xmm1
    CHECK rbx, 4
    vmovdqu ymm0, [input_a]
    vaddsubpd xmm0, xmm0, xmm0
    CHECK rbx, 5
    vaddsubpd xmm0, xmm1, [input_b]
    CHECK expected_ab, 6
    vmovdqu ymm0, [input_a]
    vaddsubpd xmm0, xmm0, [input_b]
    CHECK expected_ab, 7
%endmacro

_start:
    mov ecx, 4
.rounding:
    lea rbx, [expected_aa]
    cmp ecx, 2
    jne .expected_ready
    lea rbx, [expected_aa_down]
.expected_ready:
    mov eax, ecx
    dec eax
    shl eax, 13
    or eax, 0x1f80
    mov [mxcsr], eax
    ldmxcsr [mxcsr]
    ALIASES
    ; Unused YMM lanes contain signaling NaNs and must not raise invalid.
    stmxcsr [mxcsr]
    mov edi, 8
    test dword [mxcsr], 1
    jnz fail
    dec ecx
    jnz .rounding
    xor edi, edi
fail:
    mov eax, 60
    syscall

section .rodata align=32
input_a: dq 6.0, -4.0, 0x7ff0000000000001, 0x7ff0000000000001
input_b: dq 2.0, 1.0, 0x7ff0000000000001, 0x7ff0000000000001
junk: times 4 dq 0xdeadbeefdeadbeef
expected_ab: dq 4.0, -3.0
expected_aa: dq 0.0, -8.0
expected_aa_down: dq 0x8000000000000000, -8.0

section .bss align=32
actual: resb 32
mxcsr: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
