; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

%macro CHECK 0
    vmovdqu [actual], ymm0
    mov rax, [rdi]
    lea rsi, [actual]
    mov ecx, 4
%%loop:
    cmp [rsi], rax
    jne fail
    add rsi, 8
    loop %%loop
%endmacro

%macro LOAD_DISP 1
    lea rbx, [data + 64 - %1]
    lea rdi, [rbx + %1]
    vbroadcastsd ymm0, [rbx + %1]
    CHECK
%endmacro

section .text
_start:
    stmxcsr [original_mxcsr]
    vmovdqu ymm1, [sentinel]
    LOAD_DISP -4096
    LOAD_DISP -2048
    LOAD_DISP -2040
    LOAD_DISP -9
    LOAD_DISP -8
    LOAD_DISP -1
    LOAD_DISP 0
    LOAD_DISP 1
    LOAD_DISP 8
    LOAD_DISP 2040
    LOAD_DISP 2047
    LOAD_DISP 4096

%assign offset 0
%rep 8
    lea rbx, [data + offset]
    lea rdi, [rbx + 8]
    vxorps xmm0, xmm0, xmm0
    vbroadcastsd ymm0, [rbx + 8]
    CHECK
%assign offset offset + 1
%endrep

    ; A register source must still work when its upper half is pending zero.
    vmovdqu ymm0, [sentinel]
    vmovdqa xmm0, [data]
    vbroadcastsd ymm0, xmm0
    lea rdi, [data]
    CHECK
    vbroadcastsd ymm0, xmm1
    lea rdi, [sentinel]
    CHECK
    vmovdqu [actual], ymm1
    lea rsi, [actual]
    lea rdi, [sentinel]
    mov ecx, 4
.preserved:
    mov rax, [rsi]
    cmp rax, [rdi]
    jne fail
    add rsi, 8
    add rdi, 8
    loop .preserved
    stmxcsr [actual_mxcsr]
    mov eax, [original_mxcsr]
    cmp eax, [actual_mxcsr]
    jne fail

    ; Leave the last 8 bytes accessible, followed by a protected page.
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
    mov rax, 0x7ff0000000000001
    mov [r13 - 8], rax
    mov rax, 0xfedcba9876543210
    mov [r13], rax
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    xor edx, edx
    syscall
    test eax, eax
    jne fail

    lea rdi, [r13 - 8]
    vbroadcastsd ymm0, [rdi]
    CHECK

    ; A genuine cross-page fault must be restartable with the right address.
    mov eax, 13
    mov edi, 11
    lea rsi, [action]
    xor edx, edx
    mov r10d, 8
    syscall
    test eax, eax
    jne fail
    lea rdi, [r13 - 7]
    vbroadcastsd ymm0, [rdi]
    CHECK
    cmp dword [faults], 1
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
sentinel: dq 0x0123456789abcdef, 0xfedcba9876543210
          dq 0x7ff0000000000001, 0x8000000000000000
data: times 16 dq 0x7ff0000000000001, 0x8000000000000000
action: dq handler, 0x04000004, restorer, 0

section .bss align=32
actual: resb 32
original_mxcsr: resd 1
actual_mxcsr: resd 1
faults: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
