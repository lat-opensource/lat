; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only

bits 64
default rel
global _start

%macro CHECK_RESULT 1
	vmovdqu [actual], ymm0
	lea rdx, [%1]
	mov ecx, 4
%%check:
	mov rax, [actual + rcx * 8 - 8]
	cmp rax, [rdx + rcx * 8 - 8]
	jne fail
	dec ecx
	jnz %%check
%endmacro

%macro PREPARE 1
	vmovdqu ymm1, [src1]
	vmovdqu ymm2, [src2]
%if %1 == 0
	vmovdqu ymm0, [src2]
%endif
%endmacro

_start:
	PREPARE 0
	vblendpd xmm2, xmm1, xmm2, 0
	vmovdqu ymm0, ymm2
	CHECK_RESULT expected0

	PREPARE 0
	vblendpd xmm2, xmm1, xmm2, 1
	vmovdqu ymm0, ymm2
	CHECK_RESULT expected1

	PREPARE 0
	vblendpd xmm2, xmm1, xmm2, 2
	vmovdqu ymm0, ymm2
	CHECK_RESULT expected2

	PREPARE 0
	vblendpd xmm2, xmm1, xmm2, 3
	vmovdqu ymm0, ymm2
	CHECK_RESULT expected3

	PREPARE 0
	vblendpd xmm1, xmm1, xmm2, 0
	vmovdqu ymm0, ymm1
	CHECK_RESULT expected0

	PREPARE 0
	vblendpd xmm1, xmm1, xmm2, 1
	vmovdqu ymm0, ymm1
	CHECK_RESULT expected1

	PREPARE 0
	vblendpd xmm1, xmm1, xmm2, 2
	vmovdqu ymm0, ymm1
	CHECK_RESULT expected2

	PREPARE 0
	vblendpd xmm1, xmm1, xmm2, 3
	vmovdqu ymm0, ymm1
	CHECK_RESULT expected3

	PREPARE 0
	vblendpd xmm1, xmm1, xmm1, 1
	vmovdqu ymm0, ymm1
	CHECK_RESULT expected0

	PREPARE 1
	vblendpd xmm0, xmm1, xmm2, 1
	CHECK_RESULT expected1

	PREPARE 1
	vblendpd xmm0, xmm1, [src2], 2
	CHECK_RESULT expected2

	xor edi, edi
	jmp exit
fail:
	mov edi, 1
exit:
	mov eax, 60
	syscall

section .rodata align=32
src1:
	dq 0x3ff0000000000000, 0x4000000000000000
	dq 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb
src2:
	dq 0x4008000000000000, 0x4010000000000000
	dq 0xcccccccccccccccc, 0xdddddddddddddddd
expected0:
	dq 0x3ff0000000000000, 0x4000000000000000, 0, 0
expected1:
	dq 0x4008000000000000, 0x4000000000000000, 0, 0
expected2:
	dq 0x3ff0000000000000, 0x4010000000000000, 0, 0
expected3:
	dq 0x4008000000000000, 0x4010000000000000, 0, 0

section .bss align=32
actual: resb 32

section .note.GNU-stack noalloc noexec nowrite progbits
