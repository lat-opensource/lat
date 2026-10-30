; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only

bits 64
default rel
global _start

%macro VERIFY 1
	vmovdqu [output], %1
	mov eax, [output]
	cmp eax, [rdi]
	jne fail
	mov eax, [output + 4]
	cmp eax, [initial + 4]
	jne fail
	mov rax, [output + 8]
	cmp rax, [initial + 8]
	jne fail
	mov rax, [output + 16]
	or rax, [output + 24]
	jne fail
%endmacro

%macro TEST 1
	%if %1 & 4
		lea rdi, [r10 + 4 + r9 * 4]
	%else
		lea rdi, [r10 + 4 + (%1 & 3) * 4]
	%endif
	vmovdqu ymm0, [initial]
	vmovdqu ymm1, [initial]
	vmovdqu ymm2, [initial]
	vroundss xmm0, xmm1, xmm2, %1
	VERIFY ymm0
	vmovdqu ymm1, [initial]
	vroundss xmm1, xmm1, xmm2, %1
	VERIFY ymm1
	vmovdqu ymm1, [initial]
	vroundss xmm2, xmm1, xmm2, %1
	VERIFY ymm2
	vmovdqu ymm1, [initial]
	vroundss xmm0, xmm1, xmm1, %1
	VERIFY ymm0
	vroundss xmm1, xmm1, xmm1, %1
	VERIFY ymm1
	vmovdqu ymm1, [initial]
	vroundss xmm0, xmm1, [initial], %1
	VERIFY ymm0
	vroundss xmm1, xmm1, [initial], %1
	VERIFY ymm1
%endmacro

section .text
_start:
	xor r9d, r9d
.rounding:
	mov eax, r9d
	shl eax, 13
	or eax, 0x1f80
	mov [mxcsr], eax
	ldmxcsr [mxcsr]
	lea r10, [cases]
	mov r11d, 10
.case:
	mov eax, [r10]
	mov [initial], eax
	%assign imm 0
	%rep 16
		TEST imm
		%assign imm imm + 1
	%endrep
	add r10, 20
	dec r11d
	jnz .case
	inc r9d
	cmp r9d, 4
	jne .rounding
	; Enabled x87 invalid must retain the original fixed-round path.
	fldcw [control]
	lea r10, [cases + 4 * 20]
	mov eax, [r10]
	mov [initial], eax
	TEST 3
	TEST 9
	fninit
	xor edi, edi
	jmp exit
fail:
	mov edi, 1
exit:
	mov eax, 60
	syscall

section .rodata align=4
cases:
	; Input, nearest-even, floor, ceil, truncation.
	dd 0, 0, 0, 0, 0
	dd 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000
	dd 0x3f000000, 0, 0, 0x3f800000, 0
	dd 0xbf000000, 0x80000000, 0xbf800000, 0x80000000, 0x80000000
	dd 0x3fc00000, 0x40000000, 0x3f800000, 0x40000000, 0x3f800000
	dd 0xbfc00000, 0xc0000000, 0xc0000000, 0xbf800000, 0xbf800000
	dd 0x40200000, 0x40000000, 0x40000000, 0x40400000, 0x40000000
	dd 0xc0200000, 0xc0000000, 0xc0400000, 0xc0000000, 0xc0000000
	dd 0x4affffff, 0x4b000000, 0x4afffffe, 0x4b000000, 0x4afffffe
	dd 0x4b800000, 0x4b800000, 0x4b800000, 0x4b800000, 0x4b800000
control: dw 0x37e

section .data align=32
initial:
	dd 0, 0x89abcdef
	dq 0x123456789abcdef0, -1, -1

section .bss align=32
output: resb 32
mxcsr: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
