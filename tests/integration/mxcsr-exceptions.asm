; SPDX-License-Identifier: GPL-2.0-only
; Standalone SSE/VEX test, buildable on LoongArch with NASM and ld.lld.
%ifidn __OUTPUT_FORMAT__, elf64
bits 64
default rel
%else
bits 32
%endif
global _start

%macro CHECK_FLAGS 2
    %1 [output]
    cmp dword [output], 0x1f80 | %2
    jne fail
%endmacro

%macro RESET 1
    %1 [clean]
%endmacro

%macro SWEEP 5
    xor ebx, ebx
%%flags:
    mov eax, ebx
    or eax, 0x1f80
    mov [input], eax
    %1 [input]
    %2 [output]
    cmp eax, [output]
    jne fail
    inc ebx
    cmp ebx, 64
    jne %%flags

    RESET %1
    movss xmm0, [one]
    movss xmm1, [zero]
    %3
    CHECK_FLAGS %2, 4
    RESET %1
    CHECK_FLAGS %2, 0

    movss xmm0, [zero]
    movss xmm1, [zero]
    %3
    CHECK_FLAGS %2, 1
    RESET %1
    CHECK_FLAGS %2, 0

    movss xmm0, [max_float]
    movss xmm1, [max_float]
    %4
    CHECK_FLAGS %2, 0x28
    RESET %1
    CHECK_FLAGS %2, 0

    movss xmm0, [min_normal]
    movss xmm1, [min_normal]
    %4
    CHECK_FLAGS %2, 0x30
    RESET %1
    CHECK_FLAGS %2, 0

    movss xmm0, [one]
    movss xmm1, [half_ulp]
    %5
    CHECK_FLAGS %2, 0x20
    RESET %1
    CHECK_FLAGS %2, 0
%endmacro

section .text
_start:
    SWEEP ldmxcsr, stmxcsr, {divss xmm0, xmm1}, {mulss xmm0, xmm1}, {addss xmm0, xmm1}
    SWEEP vldmxcsr, vstmxcsr, {vdivss xmm0, xmm0, xmm1}, {vmulss xmm0, xmm0, xmm1}, {vaddss xmm0, xmm0, xmm1}
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
%if __BITS__ == 64
    mov eax, 60
    syscall
%else
    mov ebx, edi
    mov eax, 1
    int 0x80
%endif

section .rodata align=16
clean: dd 0x1f80
one: dd 0x3f800000
zero: dd 0
max_float: dd 0x7f7fffff
min_normal: dd 0x00800000
half_ulp: dd 0x33800000

section .bss align=16
input: resd 1
output: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
