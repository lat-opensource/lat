; SPDX-License-Identifier: GPL-2.0-only
; The runner supplies TEST, READ_BYTES, and optional conversion expectations.
%ifndef CONVERSION
%define CONVERSION 0
%endif
%ifndef DEST_BYTES
%define DEST_BYTES 16
%endif

%if __BITS__ == 64
default rel
%define A rax
%define S rsi
%define T rdi
%define BP rbp
%define PTR qword
%define NR_exit 60
%define NR_mprotect 10
%define NR_sigaction 13
%macro sys3 4
    mov eax, %1
    mov rdi, %2
    mov rsi, %3
    mov rdx, %4
    syscall
%endmacro
%else
%define A eax
%define S esi
%define T edi
%define BP ebp
%define PTR dword
%define NR_exit 1
%define NR_mprotect 125
%define NR_sigaction 174
%macro sys3 4
    mov eax, %1
    mov ebx, %2
    mov ecx, %3
    mov edx, %4
    int 0x80
%endmacro
%endif

section .text
global _start
_start:
%if __BITS__ == 64
    mov r10d, 8
%else
    mov esi, 8
%endif
    sys3 NR_sigaction, 11, action, 0
    test A, A
    jnz fail
%if __BITS__ == 64
    mov eax, 9
    xor edi, edi
    mov esi, 131072
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
    syscall
%else
    mov eax, 192
    xor ebx, ebx
    mov ecx, 131072
    mov edx, 3
    mov esi, 0x22
    mov edi, -1
    xor ebp, ebp
    int 0x80
%endif
    cmp A, -4095
    jae fail
    ; Align the guard for both 4 KiB and 16 KiB host pages.
    add A, 65535
    and A, -65536
    add A, 32768
    mov [boundary], A
    sub A, READ_BYTES
    mov BP, A
    mov T, A
    mov S, input
    mov ecx, READ_BYTES
    rep movsb
    sys3 NR_mprotect, PTR [boundary], 16384, 0
    test A, A
    jnz fail

    call execute
%if CONVERSION
    vmovdqu [actual], ymm0
    mov S, actual
    mov T, expected
    mov ecx, DEST_BYTES
    repe cmpsb
    jne fail
%if DEST_BYTES == 16
    mov ecx, 4
.upper:
    cmp dword [S], 0
    jne fail
    add S, 4
    loop .upper
%endif
%endif
    ; One byte beyond the declared operand must fault at the same instruction.
    inc BP
    mov [fault_start], BP
    mov byte [expect_fault], 1
    call execute
    jmp fail

execute:
    vpcmpeqd ymm0, ymm0, ymm0
    vpxor ymm1, ymm1, ymm1
    mov eax, 1
    mov ecx, 1
    mov edx, 1
tested_load:
    TEST
    ret

segv_handler:
    cmp byte [expect_fault], 1
    jne fail
%if __BITS__ == 64
    cmp edi, 11
    jne fail
    cmp dword [rsi + 8], 2
    jne fail
    mov A, [rsi + 16]
%else
    cmp dword [esp + 4], 11
    jne fail
    mov ecx, [esp + 8]
    cmp dword [ecx + 8], 2
    jne fail
    mov A, [ecx + 12]
%endif
    ; Unaligned host accesses may report the load start or the protected byte.
    cmp A, [fault_start]
    jb fail
    cmp A, [boundary]
    ja fail
%if __BITS__ == 64
    mov A, [rdx + 168]
%else
    mov ecx, [esp + 12]
    mov A, [ecx + 76]
%endif
    cmp A, tested_load
    jne fail
    sys3 NR_exit, 0, 0, 0
fail:
    sys3 NR_exit, 1, 0, 0
restorer:
%if __BITS__ == 64
    mov eax, 15
    syscall
%else
    mov eax, 173
    int 0x80
%endif

section .data
align 16
action:
%if __BITS__ == 64
    dq segv_handler, 0x04000004, restorer, 0
%else
    dd segv_handler, 0x04000004, restorer
    dq 0
%endif
input:
%if CONVERSION == 2
    times 2 dd 1, -2, 3, -4
%else
    times 2 dd 0x3f800000, 0xc0000000, 0x40400000, 0xc0800000
%endif
expected:
    dq 0x3ff0000000000000, 0xc000000000000000
    dq 0x4008000000000000, 0xc010000000000000

section .bss
align 32
actual: resb 32
boundary: resq 1
fault_start: resq 1
expect_fault: resb 1
section .note.GNU-stack noalloc noexec nowrite progbits
