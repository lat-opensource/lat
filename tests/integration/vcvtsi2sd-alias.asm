; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only

bits 64
default rel
global _start

%macro CHECK_RESULT 1
	vmovdqu [actual], ymm0
	mov rax, [actual]
	mov rdx, %1
	cmp rax, rdx
	jne fail
	mov rax, [actual + 8]
	cmp rax, [initial + 8]
	jne fail
	mov rax, [actual + 16]
	or rax, [actual + 24]
	jne fail
%endmacro

%macro PREPARE 1
	vmovdqu ymm0, [initial]
	vmovdqu ymm1, [initial]
%if %1
	vmovdqu ymm2, [initial]
%endif
%endmacro

_start:
	PREPARE 1
	mov eax, [src32]
	vcvtsi2sd xmm0, xmm0, eax
	CHECK_RESULT 0xc1e0000000000000

	PREPARE 1
	mov eax, [src32 + 4]
	vcvtsi2sd xmm0, xmm0, eax
	CHECK_RESULT 0x41dfffffffc00000

	PREPARE 1
	mov eax, [src32 + 8]
	vcvtsi2sd xmm0, xmm0, eax
	CHECK_RESULT 0xbff0000000000000

	PREPARE 1
	mov rax, [src64]
	vcvtsi2sd xmm0, xmm0, rax
	CHECK_RESULT 0x4270000000001000

	PREPARE 1
	mov rax, [src64 + 8]
	vcvtsi2sd xmm0, xmm0, rax
	CHECK_RESULT 0xc270000000001000

	PREPARE 1
	mov eax, [src32]
	vcvtsi2sd xmm0, xmm1, eax
	CHECK_RESULT 0xc1e0000000000000

	PREPARE 1
	mov rax, [src64]
	vcvtsi2sd xmm0, xmm1, rax
	CHECK_RESULT 0x4270000000001000

	PREPARE 0
	vcvtsi2sd xmm0, xmm0, dword [src32 + 4]
	CHECK_RESULT 0x41dfffffffc00000

	PREPARE 0
	vcvtsi2sd xmm0, xmm0, qword [src64 + 8]
	CHECK_RESULT 0xc270000000001000

	xor edi, edi
	jmp exit
fail:
	mov edi, 1
exit:
	mov eax, 60
	syscall

section .rodata align=32
initial:
	dq 0x0123456789abcdef, 0x1122334455667788
	dq 0xdeadbeefdeadbeef, 0xfedcba9876543210
src32:
	dd 0x80000000, 0x7fffffff, -1
src64:
	dq 0x0000010000000001, -0x0000010000000001

section .bss align=32
actual: resb 32

section .note.GNU-stack noalloc noexec nowrite progbits
