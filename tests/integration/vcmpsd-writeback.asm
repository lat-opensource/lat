; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only

bits 64
default rel
global _start

%define LESS      0x96969696
%define EQUAL     0xa5a5a5a5
%define GREATER   0xf0f0f0f0
%define UNORDERED 0x87788778

%macro VERIFY 2
    vmovdqu [output], %1
    bt %2, (imm & 31)
    sbb rdx, rdx
    cmp [output], rdx
    jne fail
    mov rax, [r8 + 8]
    cmp [output + 8], rax
    jne fail
    mov rax, [output + 16]
    or rax, [output + 24]
    jne fail
%endmacro

%macro COMPARE 3
    ldmxcsr [masked]
    vmovdqu ymm0, [r8]
    vmovdqu ymm1, [r8]
    vmovdqu ymm2, [r9]
    vcmpsd %1, %2, %3, imm
    stmxcsr [actual_mxcsr]
%endmacro

%macro SAME_MXCSR 0
    mov eax, [actual_mxcsr]
    cmp eax, [reference_mxcsr]
    jne fail
%endmacro

%macro SAVE_MXCSR 0
    mov eax, [actual_mxcsr]
    mov [reference_mxcsr], eax
%endmacro

%macro CHECK_FINITE_MXCSR 0
    ; A high-lane SNaN must not raise invalid for finite low lanes.
    cmp r11d, UNORDERED
    je %%unordered
    cmp eax, [masked]
    jne fail
%%unordered:
%endmacro

section .text
_start:
    ; Put memory operands at the last eight bytes before an inaccessible page.
    xor edi, edi
    mov esi, 8192
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
    mov eax, 9
    syscall
    test rax, rax
    js fail
    lea r13, [rax + 4088]
    lea rdi, [rax + 4096]
    mov esi, 4096
    xor edx, edx
    mov eax, 10
    syscall
    test eax, eax
    jnz fail
    lea r14, [pairs]
    lea r15, [pairs_end]
.pair:
    mov r8, [r14]
    mov r9, [r14 + 8]
    mov r11d, [r14 + 16]
    mov r12d, [r14 + 20]
    mov rax, [r9]
    mov [r13], rax
    %assign imm 0
    %rep 256
        ; Aliasing src1 retains the original temporary-result path.
        COMPARE xmm0, xmm0, xmm2
        VERIFY ymm0, r11d
        SAVE_MXCSR
        CHECK_FINITE_MXCSR
        COMPARE xmm0, xmm1, xmm2
        VERIFY ymm0, r11d
        SAME_MXCSR
        COMPARE xmm2, xmm1, xmm2
        VERIFY ymm2, r11d
        SAME_MXCSR
        COMPARE xmm0, xmm0, [r13]
        VERIFY ymm0, r11d
        SAME_MXCSR
        COMPARE xmm0, xmm1, [r13]
        VERIFY ymm0, r11d
        SAME_MXCSR

        COMPARE xmm1, xmm1, xmm1
        VERIFY ymm1, r12d
        SAVE_MXCSR
        COMPARE xmm0, xmm1, xmm1
        VERIFY ymm0, r12d
        SAME_MXCSR

        ldmxcsr [masked]
        vmovdqu ymm8, [r8]
        vmovdqu ymm9, [r8]
        vmovdqu ymm10, [r9]
        vcmpsd xmm8, xmm9, xmm10, imm
        VERIFY ymm8, r11d
        %assign imm imm + 1
    %endrep
    add r14, 24
    cmp r14, r15
    jb .pair
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
one:  dq 1.0, 0x7ff0000000000123, -1, -1
two:  dq 2.0, 0x123456789abcdef0, -1, -1
qnan: dq 0x7ff8000000000001, 0x7ff0000000000456, -1, -1
snan: dq 0x7ff0000000000001, 0xfedcba9876543210, -1, -1
zero: dq 0, 0x7ff0000000000123, -1, -1
negative_zero: dq 0x8000000000000000, 0x0123456789abcdef, -1, -1
infinity: dq 0x7ff0000000000000, 0x7ff0000000000123, -1, -1
negative_infinity: dq 0xfff0000000000000, 0x7ff0000000000123, -1, -1
largest: dq 0x7fefffffffffffff, 0x7ff0000000000123, -1, -1
pairs:
    dq one, two
    dd LESS, EQUAL
    dq one, one
    dd EQUAL, EQUAL
    dq two, one
    dd GREATER, EQUAL
    dq qnan, one
    dd UNORDERED, UNORDERED
    dq one, snan
    dd UNORDERED, EQUAL
    dq snan, qnan
    dd UNORDERED, UNORDERED
    dq zero, negative_zero
    dd EQUAL, EQUAL
    dq negative_infinity, one
    dd LESS, EQUAL
    dq infinity, largest
    dd GREATER, EQUAL
    dq infinity, infinity
    dd EQUAL, EQUAL
pairs_end:
masked: dd 0x1f80

section .bss align=32
output: resb 32
actual_mxcsr: resd 1
reference_mxcsr: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
