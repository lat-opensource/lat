; SPDX-License-Identifier: GPL-2.0-only
%ifidn __OUTPUT_FORMAT__, elf32
BITS 32
%define SRC_PTR esi
%define DST_PTR edi
%else
BITS 64
DEFAULT REL
%define SRC_PTR rsi
%define DST_PTR rdi
%endif
GLOBAL _start

%macro CHECK_BYTES 2
    lea SRC_PTR, [%1]
    lea DST_PTR, [%2]
    mov ecx, 32
    repe cmpsb
    jne bad
%endmacro

; Both source widths must clear destination bits 255:128. Alias cases
; must still convert the original input; distinct sources must survive.
%macro CHECK_REGISTER 3
    vmovdqu ymm%1, [ones]
    vmovdqu ymm%2, [floats]
    vcvtps2ph xmm%1, %3%2, 0
    vmovdqu [actual], ymm%1
    %ifidni %3, xmm
        CHECK_BYTES actual, expected_xmm
    %else
        CHECK_BYTES actual, expected_ymm
    %endif
    %if %1 != %2
        vmovdqu [actual], ymm%2
        CHECK_BYTES actual, floats
    %endif
%endmacro

; Memory destinations must write exactly 8/16 bytes without changing
; the source YMM or clearing bytes following the result.
%macro CHECK_MEMORY 1
    vmovdqu ymm0, [ones]
    vmovdqu [actual], ymm0
    vmovdqu ymm1, [floats]
    vcvtps2ph [actual], %1 %+ 1, 0
    %ifidni %1, xmm
        CHECK_BYTES actual, expected_m64
    %else
        CHECK_BYTES actual, expected_m128
    %endif
    vmovdqu [actual], ymm1
    CHECK_BYTES actual, floats
%endmacro

SECTION .text
_start:
    cld
    CHECK_REGISTER 2, 1, xmm
    CHECK_REGISTER 2, 1, ymm
    CHECK_REGISTER 1, 1, xmm
    CHECK_REGISTER 1, 1, ymm
    CHECK_REGISTER 0, 7, xmm
    CHECK_REGISTER 7, 0, ymm
    CHECK_REGISTER 7, 7, xmm
    CHECK_REGISTER 7, 7, ymm
%ifidn __OUTPUT_FORMAT__, elf64
    CHECK_REGISTER 15, 8, xmm
    CHECK_REGISTER 8, 15, ymm
    CHECK_REGISTER 15, 15, xmm
    CHECK_REGISTER 15, 15, ymm
%endif
    CHECK_MEMORY xmm
    CHECK_MEMORY ymm
%ifidn __OUTPUT_FORMAT__, elf32
    xor ebx, ebx
    mov eax, 1
    int 0x80
%else
    xor edi, edi
    mov eax, 60
    syscall
%endif
bad:
%ifidn __OUTPUT_FORMAT__, elf32
    mov ebx, 1
    mov eax, 1
    int 0x80
%else
    mov edi, 1
    mov eax, 60
    syscall
%endif

SECTION .rodata
ALIGN 32
ones: times 32 db 0xff
floats: dd 0x3f800000, 0xc0000000, 0x3f000000, 0x477fe000
        dd 0x40000000, 0x40400000, 0x40800000, 0x40a00000
expected_xmm: dw 0x3c00, 0xc000, 0x3800, 0x7bff
              times 24 db 0
expected_ymm: dw 0x3c00, 0xc000, 0x3800, 0x7bff
              dw 0x4000, 0x4200, 0x4400, 0x4500
              times 16 db 0
expected_m64: dw 0x3c00, 0xc000, 0x3800, 0x7bff
              times 24 db 0xff
expected_m128: dw 0x3c00, 0xc000, 0x3800, 0x7bff
               dw 0x4000, 0x4200, 0x4400, 0x4500
               times 16 db 0xff
SECTION .bss
ALIGN 32
actual: resb 32
SECTION .note.GNU-stack noalloc noexec nowrite progbits
