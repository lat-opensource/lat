; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
default rel
section .text
global _start

_start:
    mov r14d, 1
    lea r12, [bsr_cases]
    mov r13d, (bsr_cases_end - bsr_cases) / 16
bsr_loop:
    mov r11, [r12]
    mov rcx, -1
    bsr ecx, r11d
    not ecx
    add ecx, 15
    cmp ecx, ecx
    cmp rcx, [r12 + 8]
    jne fail
    add r12, 16
    dec r13d
    jne bsr_loop

    mov r14d, 2
    lea r12, [clamp_cases]
    mov r13d, (clamp_cases_end - clamp_cases) / 8
clamp_loop:
    mov rdi, [r12]
    call clamp_reference
    mov [expected], rax
    mov [expected + 8], rcx
    mov [expected + 16], r8
    call clamp_folded
    cmp rax, [expected]
    jne fail
    cmp rcx, [expected + 8]
    jne fail
    cmp r8, [expected + 16]
    jne fail
    cmp rdi, [r12]
    jne fail
    add r12, 8
    dec r13d
    jne clamp_loop

    ; Source/scratch alias must keep the original sequence.
    mov r14d, 3
    mov edi, 123
    mov eax, 255
    sub eax, edi
    shr eax, 23
    xor edi, edi
    test rdi, rdi
    sete dil
    or eax, edi
    or eax, edi
    movzx eax, al
    cmp eax, eax
    cmp rax, 1
    jne fail

    xor edi, edi
    jmp exit
fail:
    mov edi, r14d
exit:
    mov eax, 60
    syscall

clamp_folded:
    mov eax, 255
    sub eax, edi
    shr eax, 23
    xor ecx, ecx
    test rdi, rdi
    sete cl
    or eax, edi
    or eax, ecx
    movzx r8d, al
    cmp eax, eax
    ret

clamp_reference:
    mov eax, 255
    nop
    sub eax, edi
    nop
    shr eax, 23
    nop
    xor ecx, ecx
    nop
    test rdi, rdi
    nop
    sete cl
    nop
    or eax, edi
    nop
    or eax, ecx
    nop
    movzx r8d, al
    cmp eax, eax
    ret

section .rodata
align 8
bsr_cases:
    dq 1, 14
    dq 2, 13
    dq 0x8000, 0xffffffff
    dq 0x80000000, 0xffffffef
    dq 0xdeadbeef00000001, 14
    dq 0xdeadbeef80000000, 0xffffffef
bsr_cases_end:
clamp_cases:
    dq 0, 1, 254, 255, 256, 257, 0xffffffff
    dq 0x80000000, 0xdeadbeef00000000, 0xdeadbeef00000001
    dq 0xdeadbeef00000100, 0xdeadbeefffffffff
clamp_cases_end:

section .bss
align 8
expected: resq 3
