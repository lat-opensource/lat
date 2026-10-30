; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

section .text

%macro CASE 3
    %assign narrow 0
    %ifidni %1, xmm
        %assign narrow 1
    %endif
    vmovdqu ymm0, [junk]
    vmovdqu ymm1, [input_a]
    vmovdqu ymm2, [input_b]
    %if %3 == 7
        vmovdqu xmm1, [input_a]
        vmovdqu xmm2, [input_b]
    %endif
    %if %3 == 0 || %3 == 7
        vshufpd %1 %+ 0, %1 %+ 1, %1 %+ 2, %2
    %elif %3 == 1
        vshufpd %1 %+ 1, %1 %+ 1, %1 %+ 2, %2
        vmovdqu ymm0, ymm1
    %elif %3 == 2
        vshufpd %1 %+ 2, %1 %+ 1, %1 %+ 2, %2
        vmovdqu ymm0, ymm2
    %elif %3 == 3
        vshufpd %1 %+ 0, %1 %+ 1, %1 %+ 1, %2
    %elif %3 == 4
        vshufpd %1 %+ 1, %1 %+ 1, %1 %+ 1, %2
        vmovdqu ymm0, ymm1
    %elif %3 == 5
        vshufpd %1 %+ 0, %1 %+ 1, [input_b], %2
    %else
        vshufpd %1 %+ 1, %1 %+ 1, [input_b], %2
        vmovdqu ymm0, ymm1
    %endif
    vmovdqu [actual], ymm0
    %assign half 0
    %rep 2
        %assign lane 0
        %rep 2
            %assign index ((%2 >> (half * 2 + lane)) & 1)
            %if (narrow && half == 1) || (%3 == 7 && half == 1)
                xor eax, eax
            %elif lane == 0 || %3 == 3 || %3 == 4
                mov rax, [input_a + half * 16 + index * 8]
            %else
                mov rax, [input_b + half * 16 + index * 8]
            %endif
            cmp [actual + half * 16 + lane * 8], rax
            jne fail
            %assign lane lane + 1
        %endrep
        %assign half half + 1
    %endrep
%endmacro

_start:
    mov edi, 1
    %assign immediate 0
    %rep 256
        %assign alias 0
        %rep 8
            CASE ymm, immediate, alias
            CASE xmm, immediate, alias
            %assign alias alias + 1
        %endrep
        %assign immediate immediate + 1
    %endrep
    xor edi, edi
fail:
    mov eax, 60
    syscall

section .rodata align=32
input_a: dq 0x7ff0000000000001, 0x8000000000000000
         dq 0x0123456789abcdef, 0x1020304050607080
input_b: dq 0x7ff8123456789abc, 0xffffffffffffffff
         dq 0xfedcba9876543210, 0x8877665544332211
junk: times 4 dq 0xdeadbeefdeadbeef

section .bss align=32
actual: resb 32

section .note.GNU-stack noalloc noexec nowrite progbits
