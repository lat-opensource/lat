; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

%macro CHECK_FLAG 4
%if %4 & (1 << %1)
    mov eax, ebp
    shr eax, %3
    and eax, 1
    cmp %2, al
    jne fail_flags
%endif
%endmacro

%macro CASE 4
    vmovdqu ymm0, [rsi]
    vmovdqu ymm1, [rsi + 32]
    mov ebp, [rsi + 64]
%if %2 = 1
    mov ebp, [rsi + 68]
%endif
    mov rax, 0x123456789abcde00
    mov r8, rax
%if %2 = 0
    %1 xmm0, xmm1
%elif %2 = 1
    %1 xmm0, xmm0
%else
    %1 xmm0, [rsi + 32]
%endif
%if %3 = 0
    seta al
    mov rdx, rax
%elif %3 = 1
    seta ah
    mov rdx, rax
%else
    seta r8b
    mov rdx, r8
%endif
%if %4 & 1
    setc r9b
%endif
%if %4 & 2
    setp r10b
%endif
%if %4 & 4
    lahf
    movzx ebx, ah
    mov r14d, ebx
%endif
%if %4 & 8
    setz r11b
%endif
%if %4 & 16
    sets r12b
%endif
%if %4 & 32
    seto r13b
%endif
    cmp ecx, ecx
    pushfq
    pop rcx
    mov ecx, 0
    mov eax, ebp
    and eax, 0x41
    sete cl
%if %3 = 1
    shl ecx, 8
    mov rbx, 0x123456789abc0000
%else
    mov rbx, 0x123456789abcde00
%endif
    or rbx, rcx
    cmp rdx, rbx
    jne fail_result
    CHECK_FLAG 0, r9b, 0, %4
    CHECK_FLAG 1, r10b, 2, %4
%if %4 & 4
    mov eax, ebp
    or eax, 2
    cmp r14b, al
    jne fail_flags
%endif
    CHECK_FLAG 3, r11b, 6, %4
    CHECK_FLAG 4, r12b, 7, %4
    CHECK_FLAG 5, r13b, 11, %4
    vmovdqu [actual], ymm0
    vmovdqu [actual + 32], ymm1
%assign q 0
%rep 8
    mov rax, [actual + q * 8]
    cmp rax, [rsi + q * 8]
    jne fail_vector
%assign q q + 1
%endrep
%endmacro

%macro STACK_BYTE 5
    mov rdi, %3
    mov rdx, rdi
    and rdx, -256
    %1 xmm0, %4
    seta %2
    mov rcx, %3
    mov %3, rdi
    cmp eax, eax
    pushfq
    pop rax
    or rdx, %5
    cmp rcx, rdx
    jne fail_result
%endmacro

section .text
_start:
    ldmxcsr [clean]
    lea rsi, [pairs]
    mov r15d, (pairs_end - pairs) / 96
next_pair:
    call suite
    add rsi, 96
    dec r15d
    jnz next_pair
    ; The random-test harness reserves RSP and RBP.
    movsd xmm0, [pairs]
    movsd xmm1, [pairs + 32]
    STACK_BYTE ucomisd, spl, rsp, xmm1, 1
    STACK_BYTE ucomisd, bpl, rbp, xmm0, 0
    STACK_BYTE vucomisd, spl, rsp, xmm0, 0
    STACK_BYTE vucomisd, bpl, rbp, xmm1, 1
    xor edi, edi
    jmp exit
suite:
%assign mask 0
%rep 64
%assign form 0
%rep 3
%assign dest 0
%rep 3
    CASE ucomisd, form, dest, mask
    CASE vucomisd, form, dest, mask
%assign dest dest + 1
%endrep
%assign form form + 1
%endrep
%assign mask mask + 1
%endrep
    ret
fail_result: mov edi, 1
    jmp exit
fail_flags: mov edi, 2
    jmp exit
fail_vector: mov edi, 3
exit:
    mov eax, 60
    syscall

%macro PAIR 4
    dq %1, 0x0123456789abcdef, 0xffeeddccbbaa9988, 0x1122334455667788
    dq %2, 0xfedcba9876543210, 0xaabbccddeeff0011, 0x8877665544332211
    dd %3, %4
    times 24 db 0
%endmacro
section .rodata align=32
clean: dd 0x1f80
align 32
pairs:
    PAIR 0x4000000000000000, 0x3ff0000000000000, 0, 0x40
    PAIR 0x3ff0000000000000, 0x4000000000000000, 1, 0x40
    PAIR 0x3ff0000000000000, 0x3ff0000000000000, 0x40, 0x40
    PAIR 0, 0x8000000000000000, 0x40, 0x40
    PAIR 0x7ff0000000000000, 0x4000000000000000, 0, 0x40
    PAIR 0xfff0000000000000, 0xbff0000000000000, 1, 0x40
    PAIR 0x7ff8000000000001, 0x3ff0000000000000, 0x45, 0x45
    PAIR 0x3ff0000000000000, 0xfff800000000cafe, 0x45, 0x40
    PAIR 0x7ff0000000000001, 0x3ff0000000000000, 0x45, 0x45
    PAIR 0x3ff0000000000000, 0xfff0000000000001, 0x45, 0x40
    PAIR 1, 0, 0, 0x40
    PAIR 0x8000000000000001, 0, 1, 0x40
pairs_end:
section .bss align=32
actual: resb 64
section .note.GNU-stack noalloc noexec nowrite progbits
