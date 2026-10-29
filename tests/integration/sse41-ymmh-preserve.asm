; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

; Each bit in the exit status identifies an instruction family. Check the
; complete result, including scalar XMM lanes and the legacy YMM upper half.
%macro CHECK 3
    vmovdqu [actual], ymm0
    mov rax, [actual]
    cmp rax, [%1]
    jne %%fail
    mov rax, [actual + 8]
    cmp rax, [%1 + 8]
    jne %%fail
%if %2
    xor eax, eax
%else
    mov rax, 0x0123456789abcdef
%endif
    cmp rax, [actual + 16]
    jne %%fail
%if !%2
    mov rax, 0xfedcba9876543210
%endif
    cmp rax, [actual + 24]
    je %%done
%%fail:
    or r15d, %3
%%done:
%endmacro

; op, input, expected, immediate, source form, pending zero, failure bit
%macro CASE 7
    vmovdqu ymm0, [%2]
    vmovdqu ymm1, [%2]
%if %6
    ; The physical upper half may still contain the sentinel at this point.
    ; A following legacy SSE instruction must preserve the architectural zero.
    vmovaps xmm0, xmm0
%endif
%if %5 == 0
    %1 xmm0, xmm1, %4
%elif %5 == 1
    %1 xmm0, xmm0, %4
%else
    %1 xmm0, [%2], %4
%endif
    CHECK %3, %6, %7
%endmacro

%macro ROUND_SWEEP 4
%assign rc 0
%rep 4
    ldmxcsr [mxcsr + rc * 4]
%assign imm 0
%rep 16
%if rc == 0 || (imm & 4)
%assign mode imm & 3
%if imm & 4
%assign mode rc
%endif
%assign form 0
%rep 3
    CASE %1, %2, %3 %+ mode, imm, form, 0, %4
    CASE %1, %2, %3 %+ mode, imm, form, 1, %4
%assign form form + 1
%endrep
%endif
%assign imm imm + 1
%endrep
%assign rc rc + 1
%endrep
%endmacro

%macro DOT_SWEEP 4
%assign form 0
%rep 3
    CASE %1, %2, zero, 0, form, 0, %4
    CASE %1, %2, zero, 0, form, 1, %4
    CASE %1, %2, %3, 0xff, form, 0, %4
    CASE %1, %2, %3, 0xff, form, 1, %4
%assign form form + 1
%endrep
%endmacro

section .text
_start:
    xor r15d, r15d
    ROUND_SWEEP roundps, singles, ps, 1
    ROUND_SWEEP roundpd, doubles, pd, 2
    ROUND_SWEEP roundss, singles, ss, 4
    ROUND_SWEEP roundsd, doubles, sd, 8
    ldmxcsr [mxcsr]
    DOT_SWEEP dpps, singles, dotps, 16
    DOT_SWEEP dppd, doubles, dotpd, 32

    ; Audit another legacy SSE4.1 path that uses a 256-bit host operation.
%assign form 0
%rep 3
%assign pending 0
%rep 2
    vmovdqu ymm0, [words]
    vmovdqu ymm1, [words]
%if pending
    vmovaps xmm0, xmm0
%endif
%if form == 0
    phminposuw xmm0, xmm1
%elif form == 1
    phminposuw xmm0, xmm0
%else
    phminposuw xmm0, [words]
%endif
    CHECK minpos, pending, 64
%assign pending pending + 1
%endrep
%assign form form + 1
%endrep
    ; PTEST writes flags only, including when its source aliases the destination.
%assign form 0
%rep 3
%assign pending 0
%rep 2
    vmovdqu ymm0, [words]
    vmovdqu ymm1, [words]
%if pending
    vmovaps xmm0, xmm0
%endif
%if form == 0
    ptest xmm0, xmm1
%elif form == 1
    ptest xmm0, xmm0
%else
    ptest xmm0, [words]
%endif
    CHECK words, pending, 128
%assign pending pending + 1
%endrep
%assign form form + 1
%endrep
    mov edi, r15d
    mov eax, 60
    syscall

section .rodata
align 32
singles:
    dd 1.25, -1.25, 2.75, -2.75
    dq 0x0123456789abcdef, 0xfedcba9876543210
doubles:
    dq 1.25, -1.25
    dq 0x0123456789abcdef, 0xfedcba9876543210
words:
    dw 8, 7, 1, 6, 5, 4, 3, 2
    dq 0x0123456789abcdef, 0xfedcba9876543210
mxcsr:
    dd 0x1f80, 0x3f80, 0x5f80, 0x7f80
zero: dq 0, 0
ps0: dd 1.0, -1.0, 3.0, -3.0
ps1: dd 1.0, -2.0, 2.0, -3.0
ps2: dd 2.0, -1.0, 3.0, -2.0
ps3: dd 1.0, -1.0, 2.0, -2.0
pd0: dq 1.0, -1.0
pd1: dq 1.0, -2.0
pd2: dq 2.0, -1.0
pd3: dq 1.0, -1.0
ss0: dd 1.0, -1.25, 2.75, -2.75
ss1: dd 1.0, -1.25, 2.75, -2.75
ss2: dd 2.0, -1.25, 2.75, -2.75
ss3: dd 1.0, -1.25, 2.75, -2.75
sd0: dq 1.0, -1.25
sd1: dq 1.0, -1.25
sd2: dq 2.0, -1.25
sd3: dq 1.0, -1.25
dotps: dd 18.25, 18.25, 18.25, 18.25
dotpd: dq 3.125, 3.125
minpos: dd 0x00020001, 0, 0, 0

section .bss
align 32
actual: resb 32
