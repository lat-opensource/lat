; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

%macro CASE 1
    mov dword [seen], 0
    mov dword [expected_set], 0x40
    lea rax, [%%trap]
    mov [expected_pc], rax
    lea rax, [%%resume]
    mov [resume_pc], rax
    movsd xmm0, [one]
    movsd xmm1, [snan]
    mov eax, 1
    test eax, eax
    pushfq
    pop rdi
    %1 xmm0, xmm0
    seta al
    setc r8b
    ; This flag write can be removed because the next comparison defines ZF.
    xor ecx, ecx
%%trap:
    %1 xmm1, xmm0
%%resume:
    setz r9b
    xor ecx, ecx
    cmp r9b, 1
    jne fail_flags
    cmp r8b, 0
    jne fail_result
    cmp dword [seen], 1
    jne fail_count
%endmacro

%macro ONLY_OVERFLOW_LIVE 1
    mov dword [seen], 0
    mov dword [expected_set], 0x8d5
    lea rax, [%%trap]
    mov [expected_pc], rax
    lea rax, [%%resume]
    mov [resume_pc], rax
    movsd xmm0, [one]
    movsd xmm1, [snan]
    push qword 0x8d7
    popfq
    jmp %%trap
%%trap:
    %1 xmm1, xmm0
%%resume:
    seta al
    seto r8b
    cmp ecx, ecx
    pushfq
    pop rcx
    test al, al
    jne fail_result
    cmp r8b, 1
    jne fail_flags
    cmp dword [seen], 1
    jne fail_count
%endmacro

%macro SNAN_MASK 3
    mov dword [seen], 0
    mov dword [expected_set], 0x8d5
    lea rax, [%%trap]
    mov [expected_pc], rax
    lea rax, [%%resume]
    mov [resume_pc], rax
    ldmxcsr [mxcsr]
    movsd xmm0, [snan]
    movsd xmm1, [one]
    push qword 0x8d7
    popfq
    jmp %%trap
%%trap:
%if %2 = 0
    %1 xmm0, xmm1
%elif %2 = 1
    %1 xmm0, xmm0
%else
    %1 xmm0, [one]
%endif
%%resume:
    seta al
%if %3 & 1
    setc r8b
%endif
%if %3 & 2
    setp r9b
%endif
%if %3 & 4
    lahf
%endif
%if %3 & 8
    setz r10b
%endif
%if %3 & 16
    sets r11b
%endif
%if %3 & 32
    seto r12b
%endif
    cmp ecx, ecx
    pushfq
    pop rcx
    test al, al
    jne fail_result
    cmp dword [seen], 1
    jne fail_count
%endmacro

section .text
_start:
    mov eax, 13
    mov edi, 8
    lea rsi, [action]
    xor edx, edx
    mov r10d, 8
    syscall
    test eax, eax
    jne fail_setup
    ; Both x87 and SSE invalid-operation traps are enabled, not soft float.
    fldcw [control]
    ldmxcsr [mxcsr]
    CASE ucomisd
    CASE vucomisd
    ONLY_OVERFLOW_LIVE ucomisd
    ONLY_OVERFLOW_LIVE vucomisd
%assign mask 0
%rep 64
%assign form 0
%rep 3
    SNAN_MASK ucomisd, form, mask
    SNAN_MASK vucomisd, form, mask
%assign form form + 1
%endrep
%assign mask mask + 1
%endrep
    xor edi, edi
    jmp exit
handler:
    cmp edi, 8
    jne fail_signal
    mov rax, [rdx + 168]
    cmp rax, [expected_pc]
    jne fail_pc
    mov eax, [rdx + 176]
    and eax, [expected_set]
    cmp eax, [expected_set]
    jne fail_flags
    cmp dword [seen], 0
    jne fail_count
    inc dword [seen]
    ; Skip the deliberate fault, as the Lative random-test handler does.
    mov rax, [resume_pc]
    mov [rdx + 168], rax
    ret
restorer:
    mov eax, 15
    syscall
    ud2
fail_setup: mov edi, 1
    jmp exit
fail_signal: mov edi, 2
    jmp exit
fail_pc: mov edi, 3
    jmp exit
fail_flags: mov edi, 4
    jmp exit
fail_result: mov edi, 5
    jmp exit
fail_count: mov edi, 6
exit:
    mov eax, 60
    syscall
section .rodata
one: dq 0x3ff0000000000000
snan: dq 0x7ff0000000000001
control: dw 0x037e
mxcsr: dd 0x1f00
action: dq handler, 0x04000004, restorer, 0
section .bss
expected_pc: resq 1
resume_pc: resq 1
seen: resd 1
expected_set: resd 1
section .note.GNU-stack noalloc noexec nowrite progbits
