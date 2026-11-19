; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

; Each subset is consumed before XOR makes all remaining flags dead.
%macro CHECK 5
    vmovdqu ymm0, [%2]
    vmovdqu ymm1, [%3]
    %1 xmm0, xmm1
%if %5 & 1
    setc [result]
%endif
%if %5 & 2
    setp [result + 1]
%endif
%if %5 & 4
    lahf
    mov [result + 2], ah
%endif
%if %5 & 8
    setz [result + 3]
%endif
%if %5 & 16
    sets [result + 4]
%endif
%if %5 & 32
    seto [result + 5]
%endif
    xor eax, eax
%if %5 & 1
    cmp byte [result], (%4 & 1) != 0
    jne fail
%endif
%if %5 & 2
    cmp byte [result + 1], (%4 & 4) != 0
    jne fail
%endif
%if %5 & 4
    cmp byte [result + 2], %4 | 2
    jne fail
%endif
%if %5 & 8
    cmp byte [result + 3], (%4 & 0x40) != 0
    jne fail
%endif
%if %5 & 16
    cmp byte [result + 4], 0
    jne fail
%endif
%if %5 & 32
    cmp byte [result + 5], 0
    jne fail
%endif
%endmacro

%macro SUITE 2
%assign mask 0
%rep 64
    CHECK %1, two_%2, one_%2, 0, mask
    CHECK %1, one_%2, two_%2, 1, mask
    CHECK %1, one_%2, one_%2, 0x40, mask
    CHECK %1, nan_%2, one_%2, 0x45, mask
%assign mask mask + 1
%endrep
%endmacro

section .text
_start:
    ldmxcsr [clean]
    SUITE vcomiss, s
    SUITE vucomiss, s
    SUITE vcomisd, d
    SUITE vucomisd, d
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
one_s: dd 0x3f800000, 1, 2, 3, 4, 5, 6, 7
two_s: dd 0x40000000, 8, 9, 10, 11, 12, 13, 14
nan_s: dd 0x7fc00001, 15, 16, 17, 18, 19, 20, 21
one_d: dq 0x3ff0000000000000, 22, 23, 24
two_d: dq 0x4000000000000000, 25, 26, 27
nan_d: dq 0x7ff8000000000001, 28, 29, 30
clean: dd 0x1f80
section .bss
result: resb 6
section .note.GNU-stack noalloc noexec nowrite progbits
