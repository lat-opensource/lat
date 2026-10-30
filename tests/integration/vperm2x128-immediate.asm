; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

section .text

%macro CASE 3
    vmovdqu ymm0, [junk]
    vmovdqu ymm1, [input_a]
    vmovdqu ymm2, [input_b]
    %assign source_low 0
    %assign source_high 0
    %if %3 == 0
        %1 ymm0, ymm1, ymm2, %2
    %elif %3 == 1
        %1 ymm1, ymm1, ymm2, %2
        vmovdqu ymm0, ymm1
    %elif %3 == 2
        %1 ymm2, ymm1, ymm2, %2
        vmovdqu ymm0, ymm2
    %elif %3 == 3
        %assign source_low 2
        %assign source_high 2
        %1 ymm0, ymm1, ymm1, %2
    %elif %3 == 4
        %assign source_low 2
        %assign source_high 2
        %1 ymm1, ymm1, ymm1, %2
        vmovdqu ymm0, ymm1
    %elif %3 == 5
        %1 ymm0, ymm1, [input_b], %2
    %elif %3 == 6
        %1 ymm1, ymm1, [input_b], %2
        vmovdqu ymm0, ymm1
    %else
        ; The low half is read after a VEX.128 write made upper zero pending.
        %assign source_low 2
        %assign source_high 2
        vmovdqu xmm1, [input_a]
        %1 ymm0, ymm1, ymm1, %2
    %endif
    vmovdqu [actual], ymm0
    %assign half 0
    %rep 2
        %assign select ((%2 >> (half * 4)) & 3)
        %assign clear ((%2 >> (half * 4 + 3)) & 1)
        %if half == 0
            %assign same source_low
        %else
            %assign same source_high
        %endif
        %assign lane 0
        %rep 2
            %if clear || (%3 == 7 && (select & 1))
                xor eax, eax
            %elif select < 2 || same
                mov rax, [input_a + (select & 1) * 16 + lane * 8]
            %else
                mov rax, [input_b + (select & 1) * 16 + lane * 8]
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
            CASE vperm2f128, immediate, alias
            CASE vperm2i128, immediate, alias
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
