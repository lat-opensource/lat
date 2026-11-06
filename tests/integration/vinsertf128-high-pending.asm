; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

%macro CHECK 2
    vmovdqu [actual], %1
    lea rsi, [actual]
    lea rdi, [%2]
    mov ecx, 4
%%next:
    mov rax, [rsi]
    cmp rax, [rdi]
    jne fail
    add rsi, 8
    add rdi, 8
    loop %%next
%endmacro

%macro INSERT 2
    vmovdqu ymm0, [input_b]
    vmovdqu ymm1, [input_a]
    vmovdqu ymm2, [input_b]
    vmovdqa xmm1, xmm1
    vmovdqa xmm0, xmm0
    %if %2 = 0
        vinsertf128 ymm0, ymm1, xmm2, %1
    %elif %2 = 1
        vmovdqa xmm0, xmm1
        vinsertf128 ymm0, ymm0, xmm2, %1
    %elif %2 = 2
        vinsertf128 ymm0, ymm1, xmm0, %1
    %elif %2 = 3
        vinsertf128 ymm0, ymm1, xmm1, %1
    %elif %2 = 4
        vmovdqa xmm0, xmm1
        vinsertf128 ymm0, ymm0, xmm0, %1
    %elif %2 = 5
        vinsertf128 ymm0, ymm1, [input_b], %1
    %else
        vmovdqa xmm0, xmm1
        vinsertf128 ymm0, ymm0, [input_b], %1
    %endif
    %if %2 = 3 || %2 = 4
        %if %1 & 1
            CHECK ymm0, expected_aa_high
        %else
            CHECK ymm0, expected_a_zero
        %endif
    %else
        %if %1 & 1
            CHECK ymm0, expected_ab_high
        %else
            CHECK ymm0, expected_b_zero
        %endif
    %endif
    CHECK ymm1, expected_a_zero
%endmacro

; %1 selects distinct, src1 alias or src2 alias. Read exactly 16 bytes.
%macro MEMORY 2
    vmovdqu ymm0, [input_b]
    vmovdqu ymm1, [input_a]
    lea rdi, [r13 - %2]
    vmovdqa xmm0, xmm0
    vmovdqa xmm1, xmm1
    %if %1 = 1
        vinsertf128 ymm0, ymm0, [rdi], 255
        CHECK ymm0, expected_bb_high
    %else
        vinsertf128 ymm0, ymm1, [rdi], 255
        CHECK ymm0, expected_ab_high
    %endif
    CHECK ymm1, expected_a_zero
%endmacro

section .text
_start:
    stmxcsr [original_mxcsr]
%assign imm 0
%rep 256
%assign alias 0
%rep 7
    INSERT imm, alias
%assign alias alias + 1
%endrep
%assign imm imm + 1
%endrep
    stmxcsr [actual_mxcsr]
    mov eax, [original_mxcsr]
    cmp eax, [actual_mxcsr]
    jne fail

    mov eax, 9
    xor edi, edi
    mov esi, 131072
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
    syscall
    cmp rax, -4095
    jae fail
    mov r12, rax
    lea r13, [rax + 65536]
    mov rax, [input_b]
    mov [r13 - 16], rax
    mov rax, [input_b + 8]
    mov [r13 - 8], rax
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    xor edx, edx
    syscall
    test eax, eax
    jne fail
    MEMORY 0, 16
    MEMORY 1, 16

    mov eax, 13
    mov edi, 11
    lea rsi, [action]
    xor edx, edx
    mov r10d, 8
    syscall
    test eax, eax
    jne fail
%assign alias 0
%rep 2
    ; Reprotect before each cross-page load, then recreate deferred clears.
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    mov edx, 3
    syscall
    mov rax, [input_b]
    mov [r13 - 15], rax
    mov rax, [input_b + 8]
    mov [r13 - 7], rax
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    xor edx, edx
    syscall
    test eax, eax
    jne fail
    MEMORY alias, 15
%assign alias alias + 1
%endrep
    cmp dword [faults], 2
    jne fail
    mov eax, 11
    mov rdi, r12
    mov esi, 131072
    syscall
    xor edi, edi
    jmp exit

handler:
    cmp edi, 11
    jne fail
    cmp [rsi + 16], r13
    jne fail
    ; x86-64 ucontext holds fpregs at 224; XSAVE YMM_Hi128 starts at 576.
    mov r8, [rdx + 224]
    mov rax, [input_b]
    cmp [r8 + 160], rax
    jne fail
    mov rax, [input_b + 8]
    cmp [r8 + 168], rax
    jne fail
    mov rax, [input_a]
    cmp [r8 + 176], rax
    jne fail
    mov rax, [input_a + 8]
    cmp [r8 + 184], rax
    jne fail
    cmp qword [r8 + 576], 0
    jne fail
    cmp qword [r8 + 584], 0
    jne fail
    cmp qword [r8 + 592], 0
    jne fail
    cmp qword [r8 + 600], 0
    jne fail
    inc dword [faults]
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    mov edx, 3
    syscall
    test eax, eax
    jne fail
    ret
restorer:
    mov eax, 15
    syscall
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
input_a: dq 0x7ff0000000000001, 0x8000000000000000
         dq 0x123456789abcdef0, 0xfedcba9876543210
input_b: dq 0xfff800000000cafe, 0x0123456789abcdef
         dq 0xaaaaaaaa55555555, 0x9999999966666666
expected_a_zero: dq 0x7ff0000000000001, 0x8000000000000000, 0, 0
expected_b_zero: dq 0xfff800000000cafe, 0x0123456789abcdef, 0, 0
expected_ab_high: dq 0x7ff0000000000001, 0x8000000000000000
                  dq 0xfff800000000cafe, 0x0123456789abcdef
expected_aa_high: times 2 dq 0x7ff0000000000001, 0x8000000000000000
expected_bb_high: times 2 dq 0xfff800000000cafe, 0x0123456789abcdef
action: dq handler, 0x04000004, restorer, 0

section .bss align=32
actual: resb 32
original_mxcsr: resd 1
actual_mxcsr: resd 1
faults: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
