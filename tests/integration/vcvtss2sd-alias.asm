; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only

bits 64
default rel
global _start

%macro VERIFY 1
	vmovdqu [output], %1
	mov rax, [output]
	cmp rax, [r10 + 8]
	jne fail
	mov rax, [output + 8]
	cmp rax, [initial + 8]
	jne fail
	mov rax, [output + 16]
	or rax, [output + 24]
	jne fail
%endmacro

%macro TEST 0
	vmovdqu ymm0, [initial]
	vmovdqu ymm1, [initial]
	vmovdqu ymm2, [initial]
	vcvtss2sd xmm0, xmm1, xmm2
	VERIFY ymm0
	vmovdqu ymm1, [initial]
	vcvtss2sd xmm1, xmm1, xmm2
	VERIFY ymm1
	vmovdqu ymm2, [initial]
	vcvtss2sd xmm2, xmm1, xmm2
	VERIFY ymm2
	vmovdqu ymm1, [initial]
	vcvtss2sd xmm0, xmm1, xmm1
	VERIFY ymm0
	vcvtss2sd xmm1, xmm1, xmm1
	VERIFY ymm1
	vmovdqu ymm1, [initial]
	vcvtss2sd xmm0, xmm1, [initial]
	VERIFY ymm0
	vcvtss2sd xmm1, xmm1, [initial]
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
	mov r11d, 12
.case:
	mov eax, [r10]
	mov [initial], eax
	TEST
	add r10, 16
	dec r11d
	jnz .case
	inc r9d
	cmp r9d, 4
	jne .rounding
	xor edi, edi
	jmp exit
fail:
	mov edi, 1
exit:
	mov eax, 60
	syscall

section .rodata align=8
cases:
	dd 0, 0
	dq 0
	dd 0x80000000, 0
	dq 0x8000000000000000
	dd 0x3f000000, 0
	dq 0x3fe0000000000000
	dd 0xbf000000, 0
	dq 0xbfe0000000000000
	dd 0x3fc00000, 0
	dq 0x3ff8000000000000
	dd 0xbfc00000, 0
	dq 0xbff8000000000000
	dd 0x00800000, 0
	dq 0x3810000000000000
	dd 0x80800000, 0
	dq 0xb810000000000000
	dd 0x7f7fffff, 0
	dq 0x47efffffe0000000
	dd 0xff7fffff, 0
	dq 0xc7efffffe0000000
	dd 0x7f800000, 0
	dq 0x7ff0000000000000
	dd 0xff800000, 0
	dq 0xfff0000000000000

section .data align=32
initial:
	dd 0, 0x89abcdef
	dq 0x123456789abcdef0, -1, -1

section .bss align=32
output: resb 32
mxcsr: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
