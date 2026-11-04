; SPDX-License-Identifier: GPL-2.0-only
%ifndef TEST_CASE
%define TEST_CASE 0
%endif
%ifidn __OUTPUT_FORMAT__, elf64
BITS 64
default rel
%define DST rdi
%else
BITS 32
%define DST edi
%endif

%macro check_value 2
    mov eax, [%1]
    cmp eax, [values + 10 * %2]
    jne fail
    mov eax, [%1 + 4]
    cmp eax, [values + 10 * %2 + 4]
    jne fail
    mov ax, [%1 + 8]
    cmp ax, [values + 10 * %2 + 8]
    jne fail
%endmacro

%macro clear_image 1
    lea DST, [storage]
    mov ecx, 144
    mov al, %1
    cld
    rep stosb
%endmacro

; TOP, environment width, waiting save, number of occupied stack slots.
%macro save_case 4
%assign case_id case_id + 1
    mov ebx, case_id
    clear_image 0xa5
    fninit
%assign value_index %4 - 1
%rep %4
    fld tword [values + 10 * value_index]
%assign value_index value_index - 1
%endrep
%if %4 = 8
%rep %1
    fincstp
%endrep
%endif
%if %2 = 16
%assign data_offset 14
%assign image_size 94
%assign sw_offset 2
%assign tag_offset 4
%if %3
    o16 fsave [image]
%else
    o16 fnsave [image]
%endif
%else
%assign data_offset 28
%assign image_size 108
%assign sw_offset 4
%assign tag_offset 8
%if %3
    fsave [image]
%else
    fnsave [image]
%endif
%endif
    cmp byte [image - 1], 0xa5
    jne fail
    cmp byte [image + image_size], 0xa5
    jne fail
    cmp word [image], 0x037f
    jne fail
    mov ax, [image + sw_offset]
    and ax, 0x383f
    cmp ax, %1 << 11
    jne fail
%if %4 = 8
    cmp word [image + tag_offset], 0
%else
    cmp word [image + tag_offset], 0x03ff
%endif
    jne fail
%assign i 0
%rep %4
%if %4 = 8
%assign expected (i + %1) % 8
%else
%assign expected i
%endif
    check_value image + data_offset + 10 * i, expected
%assign i i + 1
%endrep
    ; A save must initialize x87 after storing the old state.
    fnstenv [environment]
    cmp word [environment], 0x037f
    jne fail
    cmp word [environment + 4], 0
    jne fail
    cmp word [environment + 8], 0xffff
    jne fail
%if %2 = 16
    o16 frstor [image]
%else
    frstor [image]
%endif
%assign i 0
%rep %4
%if %4 = 8
%assign expected (i + %1) % 8
%else
%assign expected i
%endif
    fstp tword [result]
    check_value result, expected
%assign i i + 1
%endrep
%endmacro

; Restore an independently constructed architectural image, so matching save
; and restore bugs cannot hide one another.
%macro restore_case 2
%assign case_id case_id + 1
    mov ebx, case_id
    clear_image 0
    fninit
%if %2 = 16
%assign data_offset 14
%assign sw_offset 2
%else
%assign data_offset 28
%assign sw_offset 4
%endif
    mov word [image], 0x037f
    mov word [image + sw_offset], %1 << 11
%assign i 0
%rep 8
    mov eax, [values + 10 * i]
    mov [image + data_offset + 10 * i], eax
    mov eax, [values + 10 * i + 4]
    mov [image + data_offset + 10 * i + 4], eax
    mov ax, [values + 10 * i + 8]
    mov [image + data_offset + 10 * i + 8], ax
%assign i i + 1
%endrep
%if %2 = 16
    o16 frstor [image]
%else
    frstor [image]
%endif
    fnstsw ax
    and ax, 0x383f
    cmp ax, %1 << 11
    jne fail
%assign i 0
%rep 8
    fstp tword [result]
    check_value result, i
%assign i i + 1
%endrep
    fnstenv [environment]
    cmp word [environment], 0x037f
    jne fail
    cmp word [environment + 8], 0xffff
    jne fail
%endmacro

section .text
global _start
_start:
%assign case_id 0
%assign top 0
%rep 8
%if TEST_CASE = 0
    save_case top, 32, 0, 8
    save_case top, 16, 0, 8
    save_case top, 32, 1, 8
    save_case top, 16, 1, 8
%elif TEST_CASE = 1
    restore_case top, 32
    restore_case top, 16
%else
%error Unsupported TEST_CASE
%endif
%assign top top + 1
%endrep
%if TEST_CASE = 0
    ; Three occupied slots reproduce the original nonzero-TOP report.
    save_case 5, 32, 0, 3
    save_case 5, 16, 0, 3
    save_case 5, 32, 1, 3
    save_case 5, 16, 1, 3
%endif
    xor ebx, ebx
fail:
%ifidn __OUTPUT_FORMAT__, elf64
    mov edi, ebx
    mov eax, 60
    syscall
%else
    mov eax, 1
    int 0x80
%endif

section .data align=16
; Distinct normal extended values, including bits below binary64 precision.
values:
%assign i 0
%rep 8
    dq 0x8000000000000001 + (i << 56) + i
    dw 0x3fff + i
%assign i i + 1
%endrep
section .bss align=16
storage: resb 16
image: resb 128
environment: resb 28
result: resb 10
section .note.GNU-stack noalloc noexec nowrite progbits
