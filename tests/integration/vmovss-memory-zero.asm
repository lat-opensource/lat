; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only

bits 64
default rel
global _start

%macro VERIFY 1
	vmovdqu [output], ymm%1
	mov eax, [output]
	cmp eax, [input]
	jne fail
	cmp dword [output + 4], 0
	jne fail
	mov rax, [output + 8]
	or rax, [output + 16]
	or rax, [output + 24]
	jne fail
	stmxcsr [status]
	mov eax, [status]
	cmp eax, [control]
	jne fail
%endmacro

%macro TEST 1
	vmovdqu ymm%1, [initial]
	vmovss xmm%1, [input]
	VERIFY %1
	; Overwrite an older deferred upper-half clear before a TB boundary.
	vmovdqu ymm%1, [initial]
	vmovss xmm%1, xmm%1, xmm%1
	vmovss xmm%1, [input]
	call %%next
%%next:
	add rsp, 8
	VERIFY %1
%endmacro

section .text
_start:
	xor r9d, r9d
.rounding:
	mov eax, r9d
	shl eax, 13
	or eax, 0x1fbf
	mov [control], eax
	ldmxcsr [control]
	lea r10, [values]
	mov r11d, 16
.value:
	mov eax, [r10]
	mov [input], eax
	%assign reg 0
	%rep 16
		TEST reg
		%assign reg reg + 1
	%endrep
	add r10, 4
	dec r11d
	jnz .value
	inc r9d
	cmp r9d, 4
	jne .rounding
	; A four-byte load at a page end must not read the protected next page.
	mov eax, 9
	xor edi, edi
	mov esi, 8192
	mov edx, 3
	mov r10d, 0x22
	mov r8, -1
	xor r9d, r9d
	syscall
	cmp rax, -4095
	jae fail
	mov r12, rax
	mov dword [r12 + 4092], 0x7f800001
	lea rdi, [r12 + 4096]
	mov esi, 4096
	xor edx, edx
	mov eax, 10
	syscall
	test eax, eax
	jne fail
	vmovdqu ymm0, [initial]
	vmovss xmm0, [r12 + 4092]
	mov dword [input], 0x7f800001
	VERIFY 0
	mov eax, 11
	mov rdi, r12
	mov esi, 8192
	syscall
	test eax, eax
	jne fail
	xor edi, edi
	jmp exit
fail:
	mov edi, 1
exit:
	mov eax, 60
	syscall

section .rodata align=32
initial: dq -1, 0x123456789abcdef0, -1, 0xfedcba9876543210
values:
	dd 0, 0x80000000, 1, 0x80000001
	dd 0x007fffff, 0x807fffff, 0x00800000, 0x80800000
	dd 0x3f800000, 0xbf800000, 0x7f7fffff, 0xff7fffff
	dd 0x7f800000, 0xff800000, 0x7fc00001, 0x7f800001

section .bss align=32
output: resb 32
control: resd 1
status: resd 1
padding: resb 1
input: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
