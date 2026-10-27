; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
; 135 boundary combinations and 128 deterministic full-register cases.
default rel
section .text
global _start

_start:
    lea rdi, [cases]
    mov esi, (cases_end - cases) / 48
loop:
    mov r11, [rdi + 8]
    mov rdx, [rdi + 16]
    movzx r9d, word [rdi]
    mov r10d, 0xffff8000
    sub r10d, r9d
    mov ecx, r10d
    sub ecx, r11d
    cmp cx, 5
    cmovb ecx, edx
    movzx r11d, cx
    cmp r11d, 0x7fff
    cmp r9, [rdi]
    jne fail
    cmp r10, [rdi + 24]
    jne fail
    cmp rcx, [rdi + 32]
    jne fail
    cmp r11, [rdi + 40]
    jne fail
    add rdi, 48
    dec esi
    jne loop
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata
align 8
%macro CASE 3
    %assign diff (0xffff8000 - (%1)) & 0xffffffff
    %assign value (diff - (%2)) & 0xffffffff
    %if (value & 0xffff) < 5
        %assign value (%3) & 0xffffffff
    %endif
    dq %1, 0xdeadbeef00000000 | (%2), 0xabcdef0100000000 | (%3)
    dq diff, value, value & 0xffff
%endmacro

cases:
%assign input_index 0
%rep 5
    %if input_index == 0
        %assign input_value 0
    %elif input_index == 1
        %assign input_value 1
    %elif input_index == 2
        %assign input_value 0x7fff
    %elif input_index == 3
        %assign input_value 0x8000
    %else
        %assign input_value 0xffff
    %endif
    %assign prev_index 0
    %rep 9
        %if prev_index == 0
            %assign prev_value 0
        %elif prev_index == 1
            %assign prev_value 1
        %elif prev_index < 7
            %assign prev_value 0x7ffa + prev_index
        %elif prev_index == 7
            %assign prev_value 0xffff8000
        %else
            %assign prev_value 0xffffffff
        %endif
        CASE input_value, prev_value, 0x2a
        CASE input_value, prev_value, 0x12348000
        CASE input_value, prev_value, 0xffffffff
        %assign prev_index prev_index + 1
    %endrep
    %assign input_index input_index + 1
%endrep

%assign seed 438
%rep 128
    %assign seed (seed * 1664525 + 1013904223) & 0xffffffff
    %assign input_value seed & 0xffff
    %assign seed (seed * 1664525 + 1013904223) & 0xffffffff
    %assign prev_value seed
    %assign seed (seed * 1664525 + 1013904223) & 0xffffffff
    CASE input_value, prev_value, seed
%endrep
cases_end:
