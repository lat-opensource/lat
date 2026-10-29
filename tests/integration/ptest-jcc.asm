; SPDX-License-Identifier: GPL-2.0-only
%ifidn __OUTPUT_FORMAT__, elf32
bits 32
%define PUSHF pushfd
%define POPF popfd
%define WORDREG eax
%else
bits 64
default rel
%define PUSHF pushfq
%define POPF popfq
%define WORDREG rax
%endif
global _start

%macro CHECK 6
    movdqu xmm0, [%1]
    movdqu xmm1, [%2]
    push 0xad7
    POPF
%if %6
    ptest xmm0, [%2]
%else
    ptest xmm0, xmm1
%endif
    %3 %%taken
    mov edx, 0
    jmp %%flags
%%taken:
    mov edx, 1
%%flags:
    PUSHF
    pop WORDREG
    and eax, 0x8d5
    cmp eax, %4
    jne fail
    cmp edx, (%5) & 1
    jne fail
%endmacro

%macro RELATION 4
    CHECK %1, %2, je, %3, (%3 & 0x40) != 0, %4
    CHECK %1, %2, jne, %3, (%3 & 0x40) = 0, %4
    CHECK %1, %2, jb, %3, (%3 & 1) != 0, %4
    CHECK %1, %2, jae, %3, (%3 & 1) = 0, %4
    CHECK %1, %2, jbe, %3, (%3 & 0x41) != 0, %4
    CHECK %1, %2, ja, %3, (%3 & 0x41) = 0, %4
    ; These conditions must remain on the ordinary Jcc path.
    CHECK %1, %2, jp, %3, 0, %4
    CHECK %1, %2, jo, %3, 0, %4
    CHECK %1, %2, js, %3, 0, %4
%endmacro

; One PTEST/Jcc site reaches both successors, including hot TU translation.
%macro ALTERNATING 1
    mov ecx, 1024
%%again:
    movdqu xmm0, [zeros]
    movdqu xmm1, [ones]
    test ecx, 1
    jz %%ready
    movdqu xmm0, [ones]
%%ready:
%if %1
    ptest xmm0, [ones]
%else
    ptest xmm0, xmm1
%endif
    je %%zero
    PUSHF
    pop WORDREG
    and eax, 0x8d5
    cmp eax, 1
    jne fail
    test ecx, 1
    jz fail
    jmp %%next
%%zero:
    PUSHF
    pop WORDREG
    and eax, 0x8d5
    cmp eax, 0x40
    jne fail
    test ecx, 1
    jnz fail
%%next:
    dec ecx
    jnz %%again
%endmacro

section .text
_start:
    mov ecx, 256
again:
%assign memory 0
%rep 2
    RELATION zeros, zeros, 0x41, memory
    RELATION zeros, ones, 0x40, memory
    RELATION ones, ones, 1, memory
    RELATION low, ones, 0, memory
    RELATION high, ones, 0, memory
    RELATION low, high, 0x40, memory
%assign memory memory + 1
%endrep
    dec ecx
    jnz again
    ALTERNATING 0
    ALTERNATING 1
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
%ifidn __OUTPUT_FORMAT__, elf32
    mov ebx, edi
    mov eax, 1
    int 0x80
%else
    mov eax, 60
    syscall
%endif
    ud2

section .rodata align=16
zeros: dq 0, 0
ones: dq -1, -1
low: dq 1, 0
high: dq 0, 0x8000000000000000
section .note.GNU-stack noalloc noexec nowrite progbits
