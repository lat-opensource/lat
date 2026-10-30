; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

; Compare fixed-direction scalar rounds with the unchanged MXCSR-controlled
; translation, using the same rounding direction and initial state. This checks
; equivalence, including existing exception-state behavior, rather than assuming
; that every exception-state detail already matches native x86.
;
; A preceding legacy ROUNDSD optionally seeds native inexact. Keep sources
; distinct and verify all of YMM13-15, MXCSR, and the x87 control/status words.
; DUMP emits complete records for comparisons between translator builds.
%macro RUN 4 ; immediate, single, alias, output
    fninit
    fldcw [control]
    ldmxcsr [initial_mxcsr]
    cmp dword [preexisting], 0
    je %%ready
    roundsd xmm12, [fractional], 3
%%ready:
    %if %2
        mov eax, 0x3fa00000
        %if %3 = 3 || %3 = 4
            mov eax, [r15 + 8]
        %endif
        mov rdx, 0xaabbccdd00000000
        or rax, rdx
    %else
        mov rax, 0x3ff4000000000000
        %if %3 = 3 || %3 = 4
            mov rax, [r15]
        %endif
    %endif
    mov [source1], rax
    %if %2
        mov eax, [r15 + 8]
        mov rdx, 0x1234567800000000
        or rax, rdx
    %else
        mov rax, [r15]
    %endif
    mov [source2], rax
    vmovdqu ymm13, [junk]
    vmovdqu ymm14, [source1]
    vmovdqu ymm15, [source2]
    %if %2
        %define OP vroundss
    %else
        %define OP vroundsd
    %endif
    %if %3 = 0
        OP xmm13, xmm14, xmm15, %1
    %elif %3 = 1
        OP xmm14, xmm14, xmm15, %1
    %elif %3 = 2
        OP xmm15, xmm14, xmm15, %1
    %elif %3 = 3
        OP xmm13, xmm14, xmm14, %1
    %elif %3 = 4
        OP xmm14, xmm14, xmm14, %1
    %elif %3 = 5
        OP xmm13, xmm14, [source2], %1
    %else
        OP xmm14, xmm14, [source2], %1
    %endif
    %undef OP
    stmxcsr [%4 + 96]
    vmovdqu [%4], ymm13
    vmovdqu [%4 + 32], ymm14
    vmovdqu [%4 + 64], ymm15
    fnstcw [%4 + 100]
    fnstsw [%4 + 102]
%endmacro

%macro TEST 4 ; fixed direction, suppress precision, single, alias
    mov dword [test_imm], %1 | (%2 << 3)
    mov dword [test_single], %3
    mov dword [test_alias], %4
    RUN 4 | (%2 << 3), %3, %4, reference
    RUN %1 | (%2 << 3), %3, %4, actual
    lea rsi, [reference]
    lea rdi, [actual]
    mov ecx, 13
    repe cmpsq
    jne fail
%ifdef DUMP
    mov eax, 1
    mov edi, 1
    lea rsi, [actual]
    mov edx, 104
    syscall
    cmp eax, 104
    jne fail
%endif
%endmacro

section .text
_start:
    cld
    xor r12d, r12d
.x87_rounding:
    lea rax, [controls]
    mov ax, [rax + r12 * 2]
    mov [control], ax
    xor r13d, r13d
.denormal:
    xor r14d, r14d
.sticky:
    mov dword [preexisting], 0
.preexisting:
    lea r15, [cases]
.case:
    %assign mode 0
    %rep 4
        mov eax, 0x1f80 | (mode << 13)
        lea rdx, [denormal_modes]
        or eax, [rdx + r13 * 4]
        lea rdx, [sticky_flags]
        or eax, [rdx + r14 * 4]
        mov [initial_mxcsr], eax
        %assign single 0
        %rep 2
            %assign suppress 0
            %rep 2
                %assign alias 0
                %rep 7
                    TEST mode, suppress, single, alias
                    %assign alias alias + 1
                %endrep
                %assign suppress suppress + 1
            %endrep
            %assign single single + 1
        %endrep
        %assign mode mode + 1
    %endrep
    add r15, 16
    lea rax, [cases_end]
    cmp r15, rax
    jb .case
    inc dword [preexisting]
    cmp dword [preexisting], 2
    jb .preexisting
    inc r14d
    cmp r14d, 4
    jb .sticky
    inc r13d
    cmp r13d, 4
    jb .denormal
    inc r12d
    cmp r12d, 4
    jb .x87_rounding
    fninit
    xor edi, edi
    jmp exit
fail:
    mov eax, 1
    mov edi, 2
    lea rsi, [failure_text]
    mov edx, failure_text_end - failure_text
    syscall
    mov rax, [r15]
    call print_hex
    mov eax, [r15 + 8]
    call print_hex
    mov eax, [test_imm]
    call print_hex
    mov eax, [test_single]
    call print_hex
    mov eax, [test_alias]
    call print_hex
    mov eax, [initial_mxcsr]
    call print_hex
    movzx eax, word [control]
    call print_hex
    mov eax, [preexisting]
    call print_hex
    mov edi, 1
exit:
    mov eax, 60
    syscall
print_hex:
    mov r8, rax
    lea rsi, [hex_buffer]
    lea rdx, [digits]
    mov ecx, 16
.hex:
    rol r8, 4
    mov eax, r8d
    and eax, 15
    mov al, [rdx + rax]
    mov [rsi], al
    inc rsi
    loop .hex
    mov eax, 1
    mov edi, 2
    lea rsi, [hex_buffer]
    mov edx, 17
    syscall
    ret

section .rodata align=32
fractional: dq 0x3ff8000000000000
controls: dw 0x037f, 0x077f, 0x0b7f, 0x0f7f
denormal_modes: dd 0, 0x40, 0x8000, 0x8040
sticky_flags: dd 0, 1, 0x20, 0x3f
junk: dq -1, -2, -3, -4
digits: db "0123456789abcdef"
failure_text: db "fixed-round equivalence failure: double input, single input, imm, single, alias, MXCSR, x87 control, preexisting", 10
failure_text_end:
cases:
    dq 0x0000000000000000
    dd 0x00000000, 0
    dq 0x8000000000000000
    dd 0x80000000, 0
    dq 0x3fe0000000000000
    dd 0x3f000000, 0
    dq 0xbfe0000000000000
    dd 0xbf000000, 0
    dq 0x3ff8000000000000
    dd 0x3fc00000, 0
    dq 0xbff8000000000000
    dd 0xbfc00000, 0
    dq 0x4004000000000000
    dd 0x40200000, 0
    dq 0xc004000000000000
    dd 0xc0200000, 0
    dq 0x432fffffffffffff
    dd 0x4affffff, 0
    dq 0x4330000000000000
    dd 0x4b000000, 0
    dq 0x4340000000000000
    dd 0x4b800000, 0
    dq 0x7fefffffffffffff
    dd 0x7f7fffff, 0
    dq 0xffefffffffffffff
    dd 0xff7fffff, 0
    dq 0x7ff0000000000000
    dd 0x7f800000, 0
    dq 0xfff0000000000000
    dd 0xff800000, 0
    dq 0x7ff8000000001234
    dd 0x7fc01234, 0
    dq 0xfff8000000001234
    dd 0xffc01234, 0
    dq 0x7ff0000000001234
    dd 0x7f801234, 0
    dq 0xfff0000000001234
    dd 0xff801234, 0
    dq 0x0000000000000001
    dd 0x00000001, 0
    dq 0x8000000000000001
    dd 0x80000001, 0
    dq 0x000fffffffffffff
    dd 0x007fffff, 0
    dq 0x0010000000000000
    dd 0x00800000, 0
    dq 0x8010000000000000
    dd 0x80800000, 0
    dq 0xa3ef75086d442e56
    dd 0xe42ba35c, 0
    dq 0xb93a6efeef301356
    dd 0xc06bc2a2, 0
    dq 0xf12da26c84774fc1
    dd 0xef347576, 0
    dq 0xb7528fb835ac4416
    dd 0x5953fb04, 0
    dq 0x784187f1389d6510
    dd 0x6c576b63, 0
    dq 0x5b64c897137705dd
    dd 0x708836d0, 0
    dq 0x7f9e9bc8af63ee5b
    dd 0xd06f1c7b, 0
    dq 0x418c56e5255154a6
    dd 0xd6bf15f3, 0
    dq 0x936a3b207995db07
    dd 0x3386361b, 0
    dq 0x7ddcfcb2a91c278f
    dd 0x91136a72, 0
    dq 0x7d866f997223d98f
    dd 0xa17c8f5e, 0
    dq 0xcc2762b3e269c2f5
    dd 0x5079d6dc, 0
    dq 0xfc5c856f8149fbff
    dd 0xb94b1d80, 0
    dq 0x81c54a6124345e8e
    dd 0x0152c3d6, 0
    dq 0x13dad9b04d2e5203
    dd 0x7f6972a5, 0
    dq 0xdcb1b2ec92d9e4d5
    dd 0x47db29a9, 0
    dq 0x23228eb71d1f20fd
    dd 0x3d6592e5, 0
    dq 0x8a184731c16c8bd2
    dd 0x8cffa780, 0
    dq 0x89cbc4717b731ebc
    dd 0x750c5855, 0
    dq 0xc06f33af90965b38
    dd 0xe01a40da, 0
    dq 0xa95e1f1f02abed3c
    dd 0x4a5bef45, 0
    dq 0x20a17f39d50c2923
    dd 0x78fab4b8, 0
    dq 0xd9d70f6631b984b7
    dd 0xd02e764f, 0
    dq 0x2385c334499a82bf
    dd 0x4f4fdda8, 0
    dq 0xe24189f416b59e20
    dd 0xf5e6690e, 0
    dq 0x56e5882cd6497c07
    dd 0x4e4d7e26, 0
    dq 0xcf5793bf2424e1f5
    dd 0x59b851b3, 0
    dq 0x0d257998d4c65554
    dd 0xd627e8d6, 0
    dq 0xa910902ca024df6d
    dd 0x916bdadb, 0
    dq 0xd7d7c178257bf64f
    dd 0x0a54f53c, 0
    dq 0x038817c05fc9d72e
    dd 0xc585eaef, 0
    dq 0xf66e168013b40af1
    dd 0x77d23916, 0
cases_end:

section .data align=32
source1: dq 0, 0x123456789abcdef0, 0x1111222233334444, 0x5555666677778888
source2: dq 0, 0xfedcba9876543210, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000
hex_buffer: db "0000000000000000", 10

section .bss align=32
reference: resb 104
actual: resb 104
initial_mxcsr: resd 1
preexisting: resd 1
test_imm: resd 1
test_single: resd 1
test_alias: resd 1
control: resw 1
section .note.GNU-stack noalloc noexec nowrite progbits
