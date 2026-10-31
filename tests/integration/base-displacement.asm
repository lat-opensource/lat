; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

%macro CHECK_OFFSET 1
    lea rbx, [data]
    mov r14, rbx
    mov rax, %1
    sub rbx, rax
    mov rax, [rbx + %1]
    cmp rax, [data]
    jne fail
    mov rax, 0x8877665544332211
    mov [rbx + %1], rax
    cmp [data], rax
    jne fail
    lea rax, [rbx + %1]
    cmp rax, r14
    jne fail
    mov rax, [rbx + %1 + 8]
    cmp rax, [data + 8]
    jne fail
    vmovdqu ymm0, [vector]
    vmovdqu [rbx + %1], ymm0
    vmovdqu ymm1, [rbx + %1]
    vpxor ymm1, ymm1, ymm0
    vptest ymm1, ymm1
    jnz fail
    mov rax, [data]
    mov rcx, 5
    add qword [rbx + %1], rcx
    add rax, rcx
    cmp [data], rax
    jne fail
    xchg rax, [rbx + %1]
    cmp [data], rax
    jne fail
    mov ecx, 1
    bt qword [rbx + %1], rcx
    setc dl
    bt qword [data], rcx
    setc cl
    cmp dl, cl
    jne fail
%endmacro

section .text
_start:
    CHECK_OFFSET -2147483648
    CHECK_OFFSET -65537
    CHECK_OFFSET -29904
    CHECK_OFFSET -29124
    CHECK_OFFSET -4097
    CHECK_OFFSET -2049
    CHECK_OFFSET -2048
    CHECK_OFFSET 0
    CHECK_OFFSET 2047
    CHECK_OFFSET 2048
    CHECK_OFFSET 4095
    CHECK_OFFSET 4096
    CHECK_OFFSET 2147481599
    CHECK_OFFSET 2147481600
    CHECK_OFFSET 2147483632

    ; An aliased load must consume the base before publishing its result.
    lea rax, [data - 29904]
    mov rax, [rax + 29904]
    cmp rax, [data]
    jne fail
    lea rax, [data + 29904]
    mov eax, [rax - 29904]
    cmp eax, [data]
    jne fail
    lea rbp, [data - 29904]
    mov rbp, [rbp + 29904]
    cmp rbp, [data]
    jne fail
    mov r15, rsp
    lea rsp, [data + 29904]
    mov rsp, [rsp - 29904]
    mov rax, rsp
    mov rsp, r15
    cmp rax, [data]
    jne fail
    lea rbx, [data - 29904]
    mov [rbx + 29904], rbx
    cmp [data], rbx
    jne fail

    ; Register bit offsets may move the access away from the nominal base.
    mov qword [data], 1
    mov qword [data + 8], 0
    mov ecx, 64
    bts qword [rbx + 29904], rcx
    jc fail
    cmp qword [data + 8], 1
    jne fail
    mov rcx, -64
    btr qword [rbx + 29912], rcx
    jnc fail
    cmp qword [data], 0
    jne fail

    ; Atomic and no-offset callers retain complete addresses.
    mov qword [data], 7
    mov eax, 7
    mov edx, 11
    lock cmpxchg [rbx + 29904], rdx
    jne fail
    cmp qword [data], 11
    jne fail
    mov edx, 5
    lock xadd [rbx + 29904], rdx
    cmp edx, 11
    jne fail
    cmp qword [data], 16
    jne fail

    ; Legacy SSE shares the helper. x87 needs room for a second access at +8.
    movdqu xmm0, [vector]
    movdqu [rbx + 29904], xmm0
    movdqu xmm1, [rbx + 29904]
    pxor xmm1, xmm0
    ptest xmm1, xmm1
    jnz fail
    lea rbx, [data - 6143]
    fld tword [one]
    fstp tword [rbx + 6143]
    fld tword [rbx + 6143]
    fstp tword [scratch]
    mov rax, [scratch]
    cmp rax, [one]
    jne fail
    mov ax, [scratch + 8]
    cmp ax, [one + 8]
    jne fail

    ; Address-size, index and segment fallbacks must still include every term.
    lea rbx, [data]
    sub rbx, 29904
    mov rax, [ebx + 29904]
    cmp rax, [data]
    jne fail
    mov ecx, 7
    sub rbx, rcx
    mov rax, [rbx + rcx + 29904]
    cmp rax, [data]
    jne fail
    mov eax, 158
    mov edi, 0x1002
    lea rsi, [data]
    syscall
    test eax, eax
    jnz fail
    mov rbx, 29904
    mov rax, [fs:rbx - 29904]
    cmp rax, [data]
    jne fail
    mov eax, 158
    mov edi, 0x1002
    xor esi, esi
    syscall

    ; Fault on each crossing access, reopen the protected region, then retry.
    mov eax, 13
    mov edi, 11
    lea rsi, [action]
    xor edx, edx
    mov r10d, 8
    syscall
    test eax, eax
    jnz fail
    mov eax, 9
    xor edi, edi
    mov esi, 196608
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
    syscall
    cmp rax, -4095
    jae fail
    mov r12, rax
    lea r13, [r12 + 65536]
    lea rbx, [r13 - 16 + 29904]
    vmovdqu ymm0, [vector]
    vmovdqu [rbx - 29904], ymm0
    call protect
    lea rax, [fault_load]
    mov [expected_pc], rax
fault_load:
    vmovdqu ymm1, [rbx - 29904]
    vpxor ymm2, ymm1, ymm0
    vptest ymm2, ymm2
    jnz fail
    call protect
    lea rax, [fault_store]
    mov [expected_pc], rax
fault_store:
    vmovdqu [rbx - 29904], ymm0
    cmp dword [faults], 2
    jne fail
    vmovdqu ymm2, [rbx - 29904]
    vpxor ymm2, ymm2, ymm0
    vptest ymm2, ymm2
    jnz fail
    xor edi, edi
    jmp exit
protect:
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    xor edx, edx
    syscall
    test eax, eax
    jnz fail
    ret
handler:
    cmp edi, 11
    jne fail
    cmp [rsi + 16], r13
    jne fail
    mov rax, [expected_pc]
    cmp [rdx + 168], rax
    jne fail
    inc dword [faults]
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    mov edx, 3
    syscall
    test eax, eax
    jnz fail
    ret
restorer:
    mov eax, 15
    syscall
    ud2
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
vector: dq 0x8877665544332211, 0x1122334455667788
        dq 0x0123456789abcdef, 0xfedcba9876543210
one: dq 0x8000000000000000
     dw 0x3fff
action: dq handler, 0x04000004, restorer, 0
section .data align=32
data: times 4 dq 0x8877665544332211
section .bss align=8
faults: resd 1
alignb 8
expected_pc: resq 1
scratch: resb 16
section .note.GNU-stack noalloc noexec nowrite progbits
