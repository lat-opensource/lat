; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only

bits 64
default rel
global _start

%macro VERIFY 2
	vmovdqu [output], %1
	mov rax, [output]
	cmp rax, [rdi + %2 * 8]
	jne fail
	mov rax, [output + 8]
	cmp rax, [rsi + 8]
	jne fail
	mov rax, [output + 16]
	or rax, [output + 24]
	jne fail
%endmacro

%macro TEST 1
	%assign mode (%1 & 3)
	%if %1 & 4
		mov rdi, r8
	%else
		lea rdi, [r8 + mode * 8]
	%endif
	vmovdqu ymm0, [rsi]
	vmovdqu ymm1, [rsi]
	vmovdqu ymm2, [rsi]
	vroundsd xmm0, xmm1, xmm2, %1
	VERIFY ymm0, 0
	vmovdqu ymm1, [rsi]
	vroundsd xmm1, xmm1, xmm2, %1
	VERIFY ymm1, 0
	vmovdqu ymm1, [rsi]
	vroundsd xmm2, xmm1, xmm2, %1
	VERIFY ymm2, 0
	vmovdqu ymm1, [rsi]
	vroundsd xmm0, xmm1, xmm1, %1
	VERIFY ymm0, 0
	vroundsd xmm1, xmm1, xmm1, %1
	VERIFY ymm1, 0
	vmovdqu ymm1, [rsi]
	vroundsd xmm0, xmm1, [rsi], %1
	VERIFY ymm0, 0
	vroundsd xmm1, xmm1, [rsi], %1
	VERIFY ymm1, 0
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
	lea rsi, [initial]
	mov rax, [r10]
	mov [rsi], rax
	lea r8, [r10 + 8]
	%assign imm 0
	%rep 16
		%if imm & 4
			lea r8, [r10 + 8 + r9 * 8]
		%else
			lea r8, [r10 + 8]
		%endif
		TEST imm
		%assign imm imm + 1
	%endrep
	add r10, 40
	dec r11d
	jnz .case
	inc r9d
	cmp r9d, 4
	jne .rounding
	; An enabled x87 invalid trap must keep the original rounding path.
	fldcw [control]
	lea rsi, [initial]
	mov rax, [cases + 4 * 40]
	mov [rsi], rax
	lea rdi, [cases + 4 * 40 + 8 + 3 * 8]
	vmovdqu ymm1, [rsi]
	vmovdqu ymm2, [rsi]
	vroundsd xmm0, xmm1, xmm2, 3
	VERIFY ymm0, 0
	fninit
	xor edi, edi
	jmp exit
fail:
	mov edi, 1
exit:
	mov eax, 60
	syscall

section .rodata
align 8
cases:
	; Input, nearest-even, floor, ceil, truncation.
	dq 0, 0, 0, 0, 0
	dq 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000
	dq 0x3fe0000000000000, 0, 0, 0x3ff0000000000000, 0
	dq 0xbfe0000000000000, 0x8000000000000000, 0xbff0000000000000, 0x8000000000000000, 0x8000000000000000
	dq 0x3ff8000000000000, 0x4000000000000000, 0x3ff0000000000000, 0x4000000000000000, 0x3ff0000000000000
	dq 0xbff8000000000000, 0xc000000000000000, 0xc000000000000000, 0xbff0000000000000, 0xbff0000000000000
	dq 0x4004000000000000, 0x4000000000000000, 0x4000000000000000, 0x4008000000000000, 0x4000000000000000
	dq 0xc004000000000000, 0xc000000000000000, 0xc008000000000000, 0xc000000000000000, 0xc000000000000000
	dq 0x432fffffffffffff, 0x4330000000000000, 0x432ffffffffffffe, 0x4330000000000000, 0x432ffffffffffffe
	dq 0x4340000000000000, 0x4340000000000000, 0x4340000000000000, 0x4340000000000000, 0x4340000000000000
control: dw 0x37e

section .data
align 32
initial:
	dq 0, 0x123456789abcdef0, -1, -1

section .bss
align 32
output: resb 32
mxcsr: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
