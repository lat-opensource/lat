; SPDX-License-Identifier: GPL-2.0-only
BITS 64
GLOBAL _start

SECTION .text
_start:
    ; Use aligned regions on both 4 KiB and 16 KiB hosts.
    mov eax, 9
    xor edi, edi
    mov esi, 131072
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
    syscall
    test rax, rax
    js fail
    lea rbx, [rax + 65536]
    mov rdi, rbx
    mov esi, 65536
    xor edx, edx
    mov eax, 10
    syscall
    test rax, rax
    jnz fail

    mov rax, 0x3c003c003c003c00
    mov [rbx - 8], rax
    mov [rbx - 16], rax
    ; XMM reads exactly eight bytes; YMM reads exactly sixteen.
    vcvtph2ps xmm0, [rbx - 8]
    vmovups [rel actual], ymm0
    lea rsi, [rel actual]
    mov ecx, 4
    call check_ones
    cmp qword [rsi], 0
    jne fail
    cmp qword [rsi + 8], 0
    jne fail

    vcvtph2ps ymm0, [rbx - 16]
    vmovups [rel actual], ymm0
    lea rsi, [rel actual]
    mov ecx, 8
    call check_ones
    xor edi, edi
    jmp exit
check_ones:
    cmp dword [rsi], 0x3f800000
    jne fail
    add rsi, 4
    loop check_ones
    ret
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

SECTION .bss
ALIGN 32
actual: resb 32
SECTION .note.GNU-stack noalloc noexec nowrite progbits
