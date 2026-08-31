; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
BITS 64
default rel
global _start
section .text

%macro CONVERT 4
    vmovdqu ymm0, [poison]
    vmovdqu ymm1, [input]
%if %4 = 0
    vmovdqu ymm0, [input]
%if %3 = 256
    %1 xmm0, ymm0
%else
    %1 xmm0, xmm0
%endif
%elif %4 = 1
%if %3 = 256
    %1 xmm0, ymm1
%else
    %1 xmm0, xmm1
%endif
%else
%if %3 = 256
    %1 xmm0, yword [input]
%else
    %1 xmm0, oword [input]
%endif
%endif
    vmovdqu [output], ymm0
%if %2
    mov rax, 0x400000003f800000
%else
    mov rax, 0x0000000200000001
%endif
    cmp [output], rax
    jne fail
%if %3 = 128
    xor eax, eax
%elif %2
    mov rax, 0x4080000040400000
%else
    mov rax, 0x0000000400000003
%endif
    cmp [output + 8], rax
    jne fail
    cmp qword [output + 16], 0
    jne fail
    cmp qword [output + 24], 0
    jne fail
%endmacro

_start:
; Repeat translations so a broken release build flushes its buffered diagnostic.
%rep 32
%assign kind 0
%rep 3
    CONVERT vcvtpd2ps, 1, 128, kind
    CONVERT vcvtpd2ps, 1, 256, kind
    CONVERT vcvtpd2dq, 0, 128, kind
    CONVERT vcvtpd2dq, 0, 256, kind
    CONVERT vcvttpd2dq, 0, 128, kind
    CONVERT vcvttpd2dq, 0, 256, kind
%assign kind kind + 1
%endrep
%endrep
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
input: dq 1.0, 2.0, 3.0, 4.0
poison: times 4 dq -1
section .bss align=32
output: resb 32
section .note.GNU-stack noalloc noexec nowrite progbits
