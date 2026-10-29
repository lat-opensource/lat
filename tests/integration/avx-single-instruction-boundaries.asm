; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

section .text

%macro CHECK 2
    vmovdqu [actual], ymm0
    lea rsi, [actual]
    lea rdx, [%1]
    mov ecx, 4
%%loop:
    mov rax, [rsi]
    cmp rax, [rdx]
    jne %%fail
    add rsi, 8
    add rdx, 8
    loop %%loop
    jmp %%done
%%fail:
    mov edi, %2
    jmp exit
%%done:
%endmacro

%macro MADD 6
    vmovdqu ymm0, [junk]
    vmovdqu ymm1, [%3]
    vmovdqu ymm2, [%4]
    %if %2 = 1
        vmovdqu ymm0, ymm1
        %1 %6 %+ 0, %6 %+ 0, %6 %+ 2
    %elif %2 = 2
        vmovdqu ymm0, ymm2
        %1 %6 %+ 0, %6 %+ 1, %6 %+ 0
    %else
        %1 %6 %+ 0, %6 %+ 1, %6 %+ 2
    %endif
    %ifidni %6, xmm
        CHECK %5 %+ _xmm, 1
    %else
        CHECK %5, 2
    %endif
%endmacro

%macro CHECK_SCALAR_COPY 1
    vmovdqu ymm0, [junk]
    vmovdqu ymm1, [scalar_input]
    %1 xmm0, xmm1, xmm1
    vmovdqu [scalar_reference], ymm0
    vmovdqu ymm1, [scalar_input]
    %1 xmm1, xmm1, xmm1
    vmovdqu ymm0, ymm1
    CHECK scalar_reference, 14
    vmovdqu ymm1, [scalar_input]
    %1 xmm1, xmm1, [scalar_input]
    vmovdqu ymm0, ymm1
    CHECK scalar_reference, 15
%endmacro

_start:
%assign alias 0
%rep 3
    MADD vpmaddwd, alias, words2, words3, maddwd, xmm
    MADD vpmaddwd, alias, words2, words3, maddwd, ymm
    MADD vpmaddubsw, alias, bytes255, bytes128, maddubsw, xmm
    MADD vpmaddubsw, alias, bytes255, bytes128, maddubsw, ymm
    MADD vpandn, alias, words2, words3, andnot, xmm
    MADD vpandn, alias, words2, words3, andnot, ymm
%assign alias alias + 1
%endrep

    vmovdqu ymm0, [junk]
    vpandn xmm0, xmm0, xmm0
    CHECK zeros, 9
    vmovdqu ymm1, [words2]
    vpandn xmm0, xmm1, [words3]
    CHECK andnot_xmm, 10

    vmovdqu ymm0, [junk]
    vpbroadcastq ymm0, xmm0
    CHECK junk, 11
    vmovdqu ymm1, [broadcast_input]
    vpbroadcastq ymm0, xmm1
    CHECK broadcast_q, 12
    vpbroadcastq ymm0, [broadcast_input]
    CHECK broadcast_q, 13

    CHECK_SCALAR_COPY vrcpss
    CHECK_SCALAR_COPY vrsqrtss

    vmovdqu ymm1, [words2]
    vmovdqu ymm2, [words3]
    vmovdqu ymm0, [junk]
    vpalignr ymm0, ymm1, ymm2, 0
    CHECK words3, 3
    vpalignr ymm0, ymm1, ymm2, 32
    CHECK zeros, 4

    vpcmpeqd ymm1, ymm1, ymm1
    vmovdqu xmm2, [count15]
    vpsrlw ymm0, ymm1, xmm2
    CHECK ones16, 5
    vmovdqu xmm2, [count31]
    vpsrld ymm0, ymm1, xmm2
    CHECK ones32, 6
    vmovdqu xmm2, [count63]
    vpsrlq ymm0, ymm1, xmm2
    CHECK ones64, 7
    vmovdqu xmm2, [count64]
    vpsrlq ymm0, ymm1, xmm2
    CHECK zeros, 8

    xor edi, edi
exit:
    mov eax, 60
    syscall

section .rodata align=32
junk: times 4 dq 0xdeadbeefdeadbeef
words2: times 16 dw 2
words3: times 16 dw 3
bytes255: times 32 db 255
bytes128: times 32 db 128
maddwd: times 8 dd 12
maddwd_xmm: times 4 dd 12
    times 4 dd 0
maddubsw: times 16 dw 0x8000
maddubsw_xmm: times 8 dw 0x8000
    times 8 dw 0
andnot: times 16 dw 1
andnot_xmm: times 8 dw 1
    times 8 dw 0
zeros: times 4 dq 0
ones16: times 16 dw 1
ones32: times 8 dd 1
ones64: times 4 dq 1
count15: dq 15, 0
count31: dq 31, 0
count63: dq 63, 0
count64: dq 64, 0
broadcast_input: dq 0x0123456789abcdef, 0xfedcba9876543210, 0, -1
broadcast_q: times 4 dq 0x0123456789abcdef
scalar_input: dd 0x40800000, 0x11223344, 0x55667788, 0x99aabbcc
    times 4 dd 0xdeadbeef

section .bss align=32
actual: resb 32
scalar_reference: resb 32

section .note.GNU-stack noalloc noexec nowrite progbits
