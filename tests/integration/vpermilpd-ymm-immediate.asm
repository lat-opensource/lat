; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only

bits 64
default rel
global _start

%macro VERIFY 2
	vmovdqu [output], %1
	%assign lane 0
	%rep 4
		mov rax, [output + lane * 8]
		%if %2 && lane >= 2
			test rax, rax
		%else
			cmp rax, [input + ((lane & 2) + ((imm >> lane) & 1)) * 8]
		%endif
		jne fail
		%assign lane lane + 1
	%endrep
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
		vpermilpd ymm0, ymm1, imm
		VERIFY ymm0, 0
		vmovdqu ymm0, [input]
		vpermilpd ymm0, ymm0, imm
		VERIFY ymm0, 0
		vmovdqu ymm0, [junk]
		vpermilpd ymm0, [input], imm
		VERIFY ymm0, 0
		vmovdqu ymm1, [junk]
		vmovdqu xmm1, [input]
		vpermilpd ymm0, ymm1, imm
		VERIFY ymm0, 1
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
