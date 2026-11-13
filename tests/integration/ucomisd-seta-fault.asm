; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

%macro ARM 2
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    xor edx, edx
    syscall
    test eax, eax
    jne fail_setup
    lea rax, [%1]
    mov [expected_pc], rax
    mov qword [expected_flags], %2
    mov dword [seen], 0
    vmovdqu ymm0, [two]
    vmovdqu ymm1, [one]
    mov r8, 0x1122334455667788
    push qword 0xad7
    popfq
%endmacro

%macro DONE 0
    cmp dword [seen], 1
    jne fail_count
    cmp r8b, 1
    jne fail_result
    vmovdqu [actual], ymm0
    vmovdqu [actual + 32], ymm1
%assign i 0
%rep 4
    mov rax, [actual + i * 8]
    cmp rax, [two + i * 8]
    jne fail_state
    mov rax, [actual + 32 + i * 8]
    cmp rax, [one + i * 8]
    jne fail_state
%assign i i + 1
%endrep
%endmacro

%macro TESTS 1
    ; A fault loading the comparison source must retain pre-comparison flags.
    mov dword [stage], 1
    ARM %%load, 0x8d5
%%load:
    %1 xmm0, [r13]
    seta r8b
    xor ecx, ecx
    DONE

    ; A later fault must expose comparison flags, even if XOR kills them.
    mov dword [stage], 2
    ARM %%after, 0
    %1 xmm0, xmm1
    seta r8b
%%after:
    mov rax, [r13]
    xor ecx, ecx
    DONE

    ; A memory SETA needs the new flags and the SETA PC on a store fault.
    mov dword [stage], 3
    ARM %%store, 0
    %1 xmm0, xmm1
%%store:
    seta byte [r13]
    mov r8b, [r13]
    xor ecx, ecx
    DONE

    ; The source operand is only eight bytes, ending at a protected page.
    ARM %%unused, 0
    %1 xmm0, [r13 - 8]
    seta r8b
    xor ecx, ecx
    cmp dword [seen], 0
    jne fail_count
    cmp r8b, 1
    jne fail_result
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    mov edx, 3
    syscall
%%unused:
%endmacro

section .text
_start:
    ldmxcsr [clean]
    mov eax, 13
    mov edi, 11
    lea rsi, [action]
    xor edx, edx
    mov r10d, 8
    syscall
    test eax, eax
    jne fail_setup
    mov eax, 9
    xor edi, edi
    mov esi, 131072
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
    syscall
    cmp rax, -4095
    jae fail_setup
    mov r12, rax
    lea r13, [rax + 65536]
    mov rax, [one]
    mov [r13], rax
    mov [r13 - 8], rax
    TESTS ucomisd
    TESTS vucomisd
    mov eax, 11
    mov rdi, r12
    mov esi, 131072
    syscall
    xor edi, edi
    jmp exit

handler:
    cmp edi, 11
    jne fail_signal
    cmp dword [seen], 0
    jne fail_count
    cmp [rsi + 16], r13
    jne fail_address
    mov rax, [rdx + 168]
    cmp rax, [expected_pc]
    jne fail_pc
    mov rax, [rdx + 176]
    and eax, 0x8d5
    cmp rax, [expected_flags]
    jne fail_flags
    mov r9, [rdx + 224]
%assign i 0
%rep 2
    mov rax, [r9 + 160 + i * 8]
    cmp rax, [two + i * 8]
    jne fail_state
    mov rax, [r9 + 176 + i * 8]
    cmp rax, [one + i * 8]
    jne fail_state
    mov rax, [r9 + 576 + i * 8]
    cmp rax, [two + 16 + i * 8]
    jne fail_state
    mov rax, [r9 + 592 + i * 8]
    cmp rax, [one + 16 + i * 8]
    jne fail_state
%assign i i + 1
%endrep
    inc dword [seen]
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    mov edx, 3
    syscall
    test eax, eax
    jne fail_setup
    ret
restorer:
    mov eax, 15
    syscall
    ud2
fail_setup: mov edi, 1
    jmp exit
fail_signal: mov edi, 2
    jmp exit
fail_count: mov edi, 3
    jmp exit
fail_address: mov edi, 4
    jmp exit
fail_pc:
    mov edi, [stage]
    imul edi, edi, 10
    add edi, 5
    jmp exit
fail_flags:
    mov edi, [stage]
    imul edi, edi, 10
    add edi, 6
    jmp exit
fail_state: mov edi, 7
    jmp exit
fail_result: mov edi, 8
exit:
    mov eax, 60
    syscall

section .rodata align=32
two: dq 0x4000000000000000, 0x0123456789abcdef
     dq 0x1122334455667788, 0xfedcba9876543210
one: dq 0x3ff0000000000000, 0x9999888877776666
     dq 0xaabbccddeeff0011, 0x123456789abcdef0
action: dq handler, 0x04000004, restorer, 0
clean: dd 0x1f80
section .bss align=32
actual: resb 64
expected_pc: resq 1
expected_flags: resq 1
seen: resd 1
stage: resd 1
section .note.GNU-stack noalloc noexec nowrite progbits
