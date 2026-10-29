; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

%macro CHECK_YMM 2
    vmovdqu [actual + %2], %1
    cmp qword [actual + %2], -1
    jne fail
    cmp qword [actual + %2 + 8], -1
    jne fail
    cmp qword [actual + %2 + 16], 0
    jne fail
    cmp qword [actual + %2 + 24], 0
    jne fail
%endmacro

; Both successors read two registers whose upper halves were deferred.
; Repeated execution makes both successors eligible for internal TU jumps.
%macro CHECK_EXIT 3
    mov r15d, 300000
    xor r14d, r14d
%%again:
    xor r14d, 1
    mov r13d, r14d
    shl r13d, 1
    movaps xmm0, [rbx + r13 * 8]
    movaps xmm1, [one]
    vpxor xmm10, xmm11, xmm12
    vpxor xmm9, xmm11, xmm12
%ifidni %1, cmp
    cmp r14d, 0
%elifidni %1, test
    test r14d, r14d
%elifidni %1, bt
    bt r14d, 0
%elifidni %1, sub
    sub r13d, 0
%elifidni %1, shr
    shr r13d, 1
%elifidni %1, and
    mov r13d, r14d
    and r13d, 1
%else
    %1 xmm0, xmm1
%endif
%if %3
    ; A separated compare must still clear a new pending value at the Jcc.
    vpxor xmm8, xmm11, xmm12
%endif
    %2 %%taken
%%not_taken:
    CHECK_YMM ymm10, 0
    CHECK_YMM ymm9, 32
%if %3
    CHECK_YMM ymm8, 64
%endif
    jmp %%join
%%taken:
    CHECK_YMM ymm10, 0
    CHECK_YMM ymm9, 32
%if %3
    CHECK_YMM ymm8, 64
%endif
%%join:
    dec r15d
    jne %%again
%endmacro

section .text
_start:
    vmovdqu ymm11, [ones]
    vpxor ymm12, ymm12, ymm12
    ldmxcsr [clean]
    lea rbx, [values]
    CHECK_EXIT cmp, jne, 0
    CHECK_EXIT test, jne, 0
    CHECK_EXIT bt, jb, 0
    CHECK_EXIT sub, jne, 0
    CHECK_EXIT shr, jne, 0
    CHECK_EXIT and, jne, 0
    CHECK_EXIT cmp, jne, 1
    CHECK_EXIT test, jne, 1
    CHECK_EXIT bt, jb, 1
    CHECK_EXIT comiss, jne, 0
    CHECK_EXIT ucomiss, jne, 0
    CHECK_EXIT comisd, jne, 0
    CHECK_EXIT ucomisd, jne, 0
    CHECK_EXIT vcomiss, jne, 0
    CHECK_EXIT vucomiss, jne, 0
    CHECK_EXIT vcomisd, jne, 0
    CHECK_EXIT vucomisd, jne, 0
    CHECK_EXIT comiss, jne, 1
    CHECK_EXIT ucomiss, jne, 1
    CHECK_EXIT comisd, jne, 1
    CHECK_EXIT ucomisd, jne, 1
    CHECK_EXIT ptest, jne, 0
    CHECK_EXIT ptest, jne, 1
    CHECK_EXIT vcomiss, jne, 1
    CHECK_EXIT vucomiss, jne, 1
    CHECK_EXIT vcomisd, jne, 1
    CHECK_EXIT vucomisd, jne, 1
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
ones: dq -1, -1, -1, -1
; The low dword and full qword both represent finite, ordered values.
values: dq 0x3ff000003f800000, 0, 0x4000000040000000, 0
one: dq 0x3ff000003f800000, 0
clean: dd 0x1f80

section .bss align=32
actual: resb 96

section .note.GNU-stack noalloc noexec nowrite progbits
