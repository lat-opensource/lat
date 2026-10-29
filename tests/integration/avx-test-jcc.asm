; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

; Check both the branch outcome and flags visible in its successor.
%macro CHECK 7
    vmovdqu ymm0, [%3]
    vmovdqu ymm1, [%4]
    %1 %2 %+ 0, %2 %+ 1
    %5 %%taken
    mov r9d, 0
    jmp %%flags
%%taken:
    mov r9d, 1
%%flags:
    pushfq
    pop rax
    and eax, 0x8d5
    cmp eax, %6
    jne fail
    cmp r9d, (%7) & 1
    jne fail
%endmacro

%macro RELATION 5
    CHECK %1, %2, %3, %4, je, %5, (%5 & 0x40) != 0
    CHECK %1, %2, %3, %4, jne, %5, (%5 & 0x40) = 0
    CHECK %1, %2, %3, %4, jb, %5, (%5 & 1) != 0
    CHECK %1, %2, %3, %4, jae, %5, (%5 & 1) = 0
    CHECK %1, %2, %3, %4, jbe, %5, (%5 & 0x41) != 0
    CHECK %1, %2, %3, %4, ja, %5, (%5 & 0x41) = 0
%endmacro

%macro SUITE 2
    RELATION %1, %2, zeros, zeros, 0x41
    RELATION %1, %2, zeros, ones, 0x40
    RELATION %1, %2, ones, ones, 1
    RELATION %1, %2, half, ones, 0
%endmacro

section .text
_start:
    mov ecx, 128
again:
    SUITE vptest, xmm
    SUITE vptest, ymm
    SUITE vtestps, xmm
    SUITE vtestps, ymm
    SUITE vtestpd, xmm
    SUITE vtestpd, ymm
    dec ecx
    jnz again
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
zeros: times 4 dq 0
ones: times 4 dq -1
half: dq -1, 0, -1, 0
section .note.GNU-stack noalloc noexec nowrite progbits
