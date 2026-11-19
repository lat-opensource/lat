; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

%macro CHECK 6
    vmovdqu ymm0, [%3]
    vmovdqu ymm1, [%4]
    %1 %2 %+ 0, %2 %+ 1
%if %6 & 1
    setc [result]
%endif
%if %6 & 2
    setp [result + 1]
%endif
%if %6 & 4
    lahf
    mov [result + 2], ah
%endif
%if %6 & 8
    setz [result + 3]
%endif
%if %6 & 16
    sets [result + 4]
%endif
%if %6 & 32
    seto [result + 5]
%endif
    xor eax, eax
%if %6 & 1
    cmp byte [result], (%5 & 1) != 0
    jne fail
%endif
%if %6 & 2
    cmp byte [result + 1], 0
    jne fail
%endif
%if %6 & 4
    cmp byte [result + 2], %5 | 2
    jne fail
%endif
%if %6 & 8
    cmp byte [result + 3], (%5 & 0x40) != 0
    jne fail
%endif
%if %6 & 16
    cmp byte [result + 4], 0
    jne fail
%endif
%if %6 & 32
    cmp byte [result + 5], 0
    jne fail
%endif
%endmacro

%macro SUITE 2
%assign mask 0
%rep 64
    CHECK %1, %2, zeros, zeros, 0x41, mask
    CHECK %1, %2, zeros, ones, 0x40, mask
    CHECK %1, %2, ones, ones, 1, mask
    CHECK %1, %2, half, ones, 0, mask
%assign mask mask + 1
%endrep
%endmacro

section .text
_start:
    SUITE vptest, xmm
    SUITE vptest, ymm
    SUITE vtestps, xmm
    SUITE vtestps, ymm
    SUITE vtestpd, xmm
    SUITE vtestpd, ymm
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
zeros: times 4 dq 0
ones: times 4 dq -1
half: dq -1, 0, -1, 0
section .bss
result: resb 6
section .note.GNU-stack noalloc noexec nowrite progbits
