; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

%macro SUM3 1
    vxorps xmm1, xmm1, xmm1
    vmovaps xmm2, [p1]
    vmovaps xmm3, [p4]
    vmovaps xmm7, [ones]
    vmovaps xmm8, [ones]
    vmulps xmm9, xmm2, [ones]
    vblendps xmm10, xmm9, xmm1, 8
    vinsertps xmm9, xmm9, xmm9, 0x4c
    vaddps xmm9, xmm10, xmm9
    vmulps xmm10, xmm8, [p2]
    vblendps xmm11, xmm10, xmm1, 8
    vinsertps xmm10, xmm10, xmm10, 0x4c
    vaddps xmm10, xmm11, xmm10
    vmovaps xmm11, [ones]
    vmulps xmm12, xmm11, [p3]
    vblendps xmm13, xmm12, xmm1, 8
    vinsertps xmm12, xmm12, xmm12, 0x4c
    vaddps xmm12, xmm13, xmm12
    vmulps xmm7, xmm3, xmm7
    vblendps xmm13, xmm7, xmm1, 8
    vinsertps xmm7, xmm7, xmm7, 0x4c
    vaddps xmm7, xmm13, xmm7
    vmulps xmm8, xmm8, [p5]
    vblendps xmm13, xmm8, xmm1, 8
    vinsertps xmm8, xmm8, xmm8, 0x4c
    vaddps xmm8, xmm13, xmm8
    vmulps xmm11, xmm11, [p6]
    vblendps xmm13, xmm11, xmm1, 8
    vinsertps xmm11, xmm11, xmm11, 0x4c
    vaddps xmm11, xmm13, xmm11
    vinsertps xmm13, xmm10, xmm7, 0x1c
    vshufpd xmm10, xmm10, xmm10, 1
    vinsertps xmm7, xmm10, xmm7, 0x9c
    vaddps xmm7, xmm13, xmm7
    vinsertps xmm10, xmm9, xmm8, 0x1c
    vshufpd xmm9, xmm9, xmm9, 1
    vinsertps xmm8, xmm9, xmm8, 0x9c
    vaddps xmm8, xmm10, xmm8
    vaddps xmm7, xmm8, xmm7
    vinsertps xmm8, xmm12, xmm11, 0x1c
    vshufpd xmm9, xmm12, xmm12, 1
    vinsertps xmm9, xmm9, xmm11, 0x9c
    vaddps xmm8, xmm8, xmm9
    vsubps xmm7, xmm7, xmm8
%if %1
    ; Reading YMM12 before its overwrite must reject the optimization.
    vmovdqu [observed], ymm12
%endif
    vmovsd xmm12, [zero]
    vmovsd xmm10, [zero]
    vinsertps xmm8, xmm10, xmm12, 0x4c
    vsqrtss xmm9, xmm1, xmm1
    vmovups [actual], xmm7
    vmovups [actual + 16], xmm11
    vmovups [actual + 32], xmm13
    lea rsi, [actual]
    lea rdi, [expected]
    mov ecx, 12
%%check:
    mov eax, [rsi]
    cmp eax, [rdi]
    jne fail
    add rsi, 4
    add rdi, 4
    dec ecx
    jne %%check
%endmacro

section .text
_start:
    ldmxcsr [clean]
    SUM3 0
    SUM3 1
    cmp dword [observed], 0x41980000 ; 9 + 10 = 19
    jne fail
    cmp qword [observed + 16], 0
    jne fail
    cmp qword [observed + 24], 0
    jne fail
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
ones: dd 1.0, 1.0, 1.0, 1.0
p1: dd 1.0, 2.0, 3.0, 4.0
p2: dd 5.0, 6.0, 7.0, 8.0
p3: dd 9.0, 10.0, 11.0, 12.0
p4: dd 13.0, 14.0, 15.0, 16.0
p5: dd 17.0, 18.0, 19.0, 20.0
p6: dd 21.0, 22.0, 23.0, 24.0
zero: dq 0
expected: dd -6.0, 30.0, 0.0, 0.0, 43.0, 44.0, 23.0, 0.0
          dd 11.0, 27.0, 0.0, 0.0
align 32
clean: dd 0x1f80
section .bss align=32
actual: resb 64
observed: resb 32
section .note.GNU-stack noalloc noexec nowrite progbits
