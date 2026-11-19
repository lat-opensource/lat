; SPDX-License-Identifier: GPL-2.0-only
%ifidn __OUTPUT_FORMAT__, elf32
bits 32
%define PUSHF pushfd
%define POPF popfd
%else
bits 64
default rel
%define PUSHF pushfq
%define POPF popfq
%endif
global _start

; Keep captured AL/AH intact while observing the retained CMP.
%macro CASE 5
    movdqu xmm0, [%1]
    movdqu xmm1, [%2]
    push 0xad7
    POPF
%if %5
    ptest xmm0, [%2]
%else
    ptest xmm0, xmm1
%endif
%if %4 & 1
    setc bl
%endif
%if %4 & 2
    setp bh
%endif
%if %4 & 4
    lahf
%endif
%if %4 & 8
    setz dl
%endif
%if %4 & 16
    sets dh
%endif
%if %4 & 32
    seto al
%endif
    cmp ecx, ecx
    PUSHF
%ifidn __OUTPUT_FORMAT__, elf32
    pop edi
%else
    pop rdi
%endif
    and edi, 0x8d5
    cmp edi, 0x44
    jne fail
%if %4 & 1
    cmp bl, (%3 & 1) != 0
    jne fail
%endif
%if %4 & 2
    cmp bh, 0
    jne fail
%endif
%if %4 & 4
    and ah, 0xd5
    cmp ah, %3
    jne fail
%endif
%if %4 & 8
    cmp dl, (%3 & 0x40) != 0
    jne fail
%endif
%if %4 & 16
    cmp dh, 0
    jne fail
%endif
%if %4 & 32
    cmp al, 0
    jne fail
%endif
    movdqu xmm2, [%1]
    pcmpeqb xmm2, xmm0
    pmovmskb esi, xmm2
    cmp esi, 0xffff
    jne fail
    movdqu xmm2, [%2]
    pcmpeqb xmm2, xmm1
    pmovmskb esi, xmm2
    cmp esi, 0xffff
    jne fail
%endmacro

section .text
_start:
%assign mask 0
%rep 64
%assign memory 0
%rep 2
    CASE zeros, zeros, 0x41, mask, memory
    CASE zeros, ones, 0x40, mask, memory
    CASE ones, ones, 1, mask, memory
    CASE low, ones, 0, mask, memory
    CASE high, ones, 0, mask, memory
    CASE low, high, 0x40, mask, memory
    CASE high, high, 1, mask, memory
%assign memory memory + 1
%endrep
%assign mask mask + 1
%endrep
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
