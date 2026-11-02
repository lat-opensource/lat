; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only

bits 64
default rel
global _start

%macro VERIFY 2
	vmovdqu [actual], ymm%1
	mov eax, [actual]
	cmp eax, %2
	jne fail
	mov rax, [actual + 4]
	or rax, [actual + 12]
	or rax, [actual + 20]
	or eax, [actual + 28]
	jne fail
%endmacro

%macro GPR_TEST 1
	vmovdqu ymm%1, [junk]
	vxorps xmm%1, xmm%1, xmm%1
	mov rax, -1
	mov eax, 0x89abcdef
	vmovd xmm%1, eax
	call %%next
%%next:
	add rsp, 8
	VERIFY %1, 0x89abcdef
%endmacro

%macro MEMORY_TEST 1
	vmovdqu ymm%1, [junk]
	vmovss xmm%1, xmm%1, xmm%1
	mov dword [input + 1], 0xfedcba98
	vmovd xmm%1, [input + 1]
	call %%next
%%next:
	add rsp, 8
	VERIFY %1, 0xfedcba98
%endmacro

section .text
_start:
	%assign reg 0
	%rep 16
		GPR_TEST reg
		MEMORY_TEST reg
		%assign reg reg + 1
	%endrep

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
	mov dword [r12 + 4092], 0xfedcba98
	lea rdi, [r12 + 4096]
	mov esi, 4096
	xor edx, edx
	mov eax, 10
	syscall
	test eax, eax
	jne fail
	vmovdqu ymm0, [junk]
	vmovd xmm0, [r12 + 4092]
	VERIFY 0, 0xfedcba98
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
junk: times 4 dq 0xdeadbeefdeadbeef

section .bss align=32
actual: resb 32
padding: resb 1
input: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
