; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only

bits 64
default rel
global _start

%macro VERIFY 1
	vmovdqu [output], %1
	mov rax, [output]
	cmp rax, [input + (imm & 1) * 8]
	jne fail
	mov rax, [output + 8]
	cmp rax, [input + ((imm >> 1) & 1) * 8]
	jne fail
	mov rax, [output + 16]
	or rax, [output + 24]
	jne fail
%endmacro

section .text
_start:
	mov ecx, 4
.rounding:
	mov eax, ecx
	dec eax
	shl eax, 13
	or eax, 0x1fbf
	mov [mxcsr], eax
	ldmxcsr [mxcsr]
	%assign imm 0
	%rep 256
		vmovdqu ymm0, [junk]
		vmovdqu ymm1, [input]
		vpermilpd xmm0, xmm1, imm
		VERIFY ymm0
		vmovdqu ymm0, [input]
		vpermilpd xmm0, xmm0, imm
		VERIFY ymm0
		vmovdqu ymm0, [junk]
		vpermilpd xmm0, [input], imm
		VERIFY ymm0
		; A pending source upper-zero must not affect the low-lane shuffle.
		vmovdqu xmm1, [input]
		vpermilpd xmm0, xmm1, imm
		VERIFY ymm0
		%assign imm imm + 1
	%endrep
	stmxcsr [actual_mxcsr]
	mov eax, [actual_mxcsr]
	cmp eax, [mxcsr]
	jne fail
	dec ecx
	jnz .rounding
	xor edi, edi
	jmp exit
fail:
	mov edi, 1
exit:
	mov eax, 60
	syscall

section .rodata align=32
input: dq 0x7ff0000000000001, 0x8000000000000000
	dq 0x0123456789abcdef, 0xfedcba9876543210
junk: times 4 dq 0xdeadbeefdeadbeef

section .bss align=32
output: resb 32
mxcsr: resd 1
actual_mxcsr: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
