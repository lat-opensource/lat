; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

section .text

%macro CASE 4
    vmovdqu ymm%1, [pattern_a]
    vmovdqu ymm%2, [pattern_b]
    vmovdqu ymm%3, [pattern_c]
%if %4
    vxorps xmm%2, xmm%2, xmm%2
%endif
    vmovdqu [saved1], ymm%2
    vmovdqu [saved2], ymm%3
    mov rax, [saved2]
    mov [expected], rax
    mov rax, [saved1 + 8]
    mov [expected + 8], rax
    mov qword [expected + 16], 0
    mov qword [expected + 24], 0
%if %4
    vxorps xmm%2, xmm%2, xmm%2
%endif
    stmxcsr [mxcsr_before]
    vmovsd xmm%1, xmm%2, xmm%3
    vmovdqu [actual], ymm%1
    stmxcsr [mxcsr_after]
    inc edi
    mov eax, [mxcsr_before]
    cmp eax, [mxcsr_after]
    jne fail
%assign lane 0
%rep 4
    mov rax, [actual + lane * 8]
    cmp rax, [expected + lane * 8]
    jne fail
%assign lane lane + 1
%endrep
%if %2 != %1
    vmovdqu [actual], ymm%2
%assign lane 0
%rep 4
    mov rax, [actual + lane * 8]
    cmp rax, [saved1 + lane * 8]
    jne fail
%assign lane lane + 1
%endrep
%endif
%if %3 != %1
    vmovdqu [actual], ymm%3
%assign lane 0
%rep 4
    mov rax, [actual + lane * 8]
    cmp rax, [saved2 + lane * 8]
    jne fail
%assign lane lane + 1
%endrep
%endif
%endmacro

_start:
    xor edi, edi
%assign rc 0
%rep 4
    mov dword [mxcsr_before], 0x1f80 | (rc << 13)
    ldmxcsr [mxcsr_before]
%assign d 0
%rep 16
%assign s1 (d + 1) % 16
%assign s2 (d + 2) % 16
    CASE d, s1, s2, 0
    CASE d, d, s2, 0
    CASE d, s1, d, 0
    CASE d, s1, s1, 0
    CASE d, d, d, 0
    CASE d, s1, s2, 1
    CASE d, d, s2, 1
    CASE d, s1, d, 1
    CASE d, s1, s1, 1
    CASE d, d, d, 1
%assign d d + 1
%endrep
%assign rc rc + 1
%endrep
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
pattern_a: dq 0x8000000000000000, 0x7ff0000000000001, -1, 0x123456789abcdef0
pattern_b: dq 0x7ff8000000000055, 0x0123456789abcdef, 0x7777777777777777, -1
pattern_c: dq 0x7ff0000000000001, 0x5555555555555555, -1, -1

section .bss align=32
actual: resb 32
expected: resb 32
saved1: resb 32
saved2: resb 32
mxcsr_before: resd 1
mxcsr_after: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
