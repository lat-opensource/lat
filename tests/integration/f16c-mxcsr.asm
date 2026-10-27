; SPDX-License-Identifier: GPL-2.0-only
%ifidn __OUTPUT_FORMAT__, elf32
BITS 32
%else
BITS 64
DEFAULT REL
%endif
GLOBAL _start
GLOBAL ftz_cases, daz_cases

; Each failure returns its case number. All exceptions are masked.
%assign case_id 0
%macro BEGIN_CASE 1
    %assign case_id case_id + 1
    mov ebx, case_id
    mov dword [control], %1
    vldmxcsr [control]
    movq mm2, [mmx_bits]
%endmacro

%macro CHECK_STATE 1
    vstmxcsr [status]
    cmp dword [status], %1
    jne bad
    movq [mmx_actual], mm2
    mov eax, [mmx_bits]
    cmp [mmx_actual], eax
    jne bad
    mov eax, [mmx_bits + 4]
    cmp [mmx_actual + 4], eax
    jne bad
%endmacro

%macro PS_CASE 6
    BEGIN_CASE %1
    vmovdqu ymm1, [%2]
    %ifidni %3, reg
        vcvtps2ph xmm2, %4 %+ 1, %5
    %else
        vcvtps2ph [result], %4 %+ 1, %5
    %endif
    CHECK_STATE %6
%endmacro

%macro PH_CASE 3
    BEGIN_CASE %1
    %ifidni %2, reg
        vmovdqu xmm1, [small_halves]
        vcvtph2ps %3 %+ 2, xmm1
    %else
        vcvtph2ps %3 %+ 2, [small_halves]
    %endif
    vmovdqu [result], ymm2
    cmp dword [result], 0x33800000
    jne bad
    cmp dword [result + 4], 0xb3800000
    jne bad
    CHECK_STATE %1
%endmacro

SECTION .text
_start:
%macro WIDTH_CASES 1
    PS_CASE 0x1f80, overflow, reg, %1, 0, 0x1fa8
    PS_CASE 0x1f80, overflow, mem, %1, 0, 0x1fa8
    PS_CASE 0x1f80, tiny, reg, %1, 0, 0x1fb0
    PS_CASE 0x1f80, tiny, mem, %1, 0, 0x1fb0
    PS_CASE 0x1f80, inexact, reg, %1, 0, 0x1fa0
    ; imm8 bit 3 is ignored, not exception suppression.
    PS_CASE 0x1f80, inexact, mem, %1, 8, 0x1fa0
    PS_CASE 0x1f84, exact, reg, %1, 0, 0x1f84
    ; Reloading MXCSR must discard old software exception flags.
    PS_CASE 0x1f80, exact, mem, %1, 0, 0x1f80
%endmacro
    WIDTH_CASES xmm
    WIDTH_CASES ymm
ftz_cases:
%macro FTZ_CASES 1
    ; FTZ must not flush an exactly representable half subnormal.
    PS_CASE 0x9f80, half_subnormal, reg, %1, 0, 0x9f80
    vmovdqu [result], xmm2
    cmp word [result], 1
    jne bad
    PS_CASE 0x9f80, half_subnormal, mem, %1, 0, 0x9f80
    cmp word [result], 1
    jne bad
    PS_CASE 0x9f80, tiny, reg, %1, 0, 0x9fb0
%endmacro
    FTZ_CASES xmm
    FTZ_CASES ymm
daz_cases:
    PH_CASE 0x1f80, reg, xmm
    PH_CASE 0x1fc0, reg, xmm
    PH_CASE 0x1fc0, mem, xmm
    PH_CASE 0x9fc0, reg, ymm
    PH_CASE 0x9fc0, mem, ymm
    ; PH2PS must restore DAZ for the following PS2PH conversion.
    vmovdqu xmm1, [single_subnormal]
    vcvtps2ph xmm2, xmm1, 2
    vmovdqu [result], xmm2
    cmp word [result], 0
    jne bad
    CHECK_STATE 0x9fc0

    ; An immediate rounding override must not change MXCSR rounding.
    BEGIN_CASE 0x3f80
    vmovdqu xmm1, [inexact]
    vcvtps2ph xmm2, xmm1, 2
    vmovdqu [result], xmm2
    cmp word [result], 0x3c01
    jne bad
    CHECK_STATE 0x3fa0
    vcvtps2ph xmm2, xmm1, 4
    vmovdqu [result], xmm2
    cmp word [result], 0x3c00
    jne bad
    CHECK_STATE 0x3fa0

    xor ebx, ebx
bad:
%ifidn __OUTPUT_FORMAT__, elf32
    mov eax, 1
    int 0x80
%else
    mov edi, ebx
    mov eax, 60
    syscall
%endif

SECTION .rodata
ALIGN 32
overflow: times 8 dd 0x7f7fffff
tiny: times 8 dd 0x33000000
inexact: times 8 dd 0x3f800001
exact: times 8 dd 0x3f800000
half_subnormal: times 8 dd 0x33800000
single_subnormal: times 8 dd 1
small_halves: times 4 dw 0x0001, 0x8001
mmx_bits: dq 0xfff1ffffda7eb714
SECTION .bss
ALIGN 32
result: resb 32
control: resd 1
status: resd 1
mmx_actual: resq 1
SECTION .note.GNU-stack noalloc noexec nowrite progbits
