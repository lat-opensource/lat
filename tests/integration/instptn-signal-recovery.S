/* SPDX-License-Identifier: GPL-2.0-only */

.macro compare kind, bits, same=0
.if \bits == 8
.if \same
	\kind %al, %al
.else
	\kind %bl, %al
.endif
.elseif \bits == 16
.if \same
	\kind %ax, %ax
.else
	\kind %bx, %ax
.endif
.elseif \bits == 32
.if \same
	\kind %eax, %eax
.else
	\kind %ebx, %eax
.endif
.else
.if \same
	\kind %rax, %rax
.else
	\kind %rbx, %rax
.endif
.endif
.endm

.macro run_case name, kind, bits, mask, same=0
	movabs $0x8000000080008080, %rax
	movabs $0x7fffffff7fff7f81, %rbx
.ifc \kind,bt
	compare bt, \bits
.else
	compare \kind, \bits, \same
.endif
	pushfq
	popq expected_flags(%rip)
	movq $\mask, flag_mask(%rip)
	lea \name\()_fault(%rip), %rax
	mov %rax, expected_pc(%rip)
	call arm_fault
	call \name
	call check_result
.endm

.macro fault_function name, kind, bits, same=0
.p2align 4
\name:
.ifc \kind,bt
	compare bt, \bits
.else
	compare \kind, \bits, \same
.endif
\name\()_fault:
	mov (%rdi), %rcx
.ifc \kind,bt
	jc 1f
.else
	jz 1f
.endif
	ret
1:
	ret
.endm

.text
.globl _start
_start:
	mov $13, %eax
	mov $11, %edi
	lea action(%rip), %rsi
	xor %edx, %edx
	mov $8, %r10d
	syscall
	test %rax, %rax
	js fail_setup
	/* Shared mappings select the real parallel recovery path even without
	 * another thread. It must not depend on a fresh push/pop retranslation. */
	mov $9, %eax
	xor %edi, %edi
	mov $16384, %esi
	mov $3, %edx
	mov $0x21, %r10d
	mov $-1, %r8
	xor %r9d, %r9d
	syscall
	test %rax, %rax
	js fail_setup
	mov $9, %eax
	xor %edi, %edi
	mov $65536, %esi
	mov $3, %edx
	mov $0x22, %r10d
	mov $-1, %r8
	xor %r9d, %r9d
	syscall
	test %rax, %rax
	js fail_setup
	add $16383, %rax
	and $-16384, %rax
	mov %rax, page(%rip)
	movq $0x12345678, (%rax)
	movdqu poison(%rip), %xmm0

	/* No delayed comparison: hot AOT must not consult a temporary IR1 list. */
	movq $0xad7, expected_flags(%rip)
	movq $0x8d5, flag_mask(%rip)
	lea plain_fault(%rip), %rax
	mov %rax, expected_pc(%rip)
	call arm_fault
	call plain
	call check_result

	run_case cmp8, cmp, 8, 0x8d5
	run_case cmp16, cmp, 16, 0x8d5
	run_case cmp32, cmp, 32, 0x8d5
	run_case cmp64, cmp, 64, 0x8d5
	run_case test8, test, 8, 0x8c5
	run_case test16, test, 16, 0x8c5
	run_case test32, test, 32, 0x8c5
	run_case test64, test, 64, 0x8c5
	run_case same8, test, 8, 0x8c5, 1
	run_case same16, test, 16, 0x8c5, 1
	run_case same32, test, 32, 0x8c5, 1
	run_case same64, test, 64, 0x8c5, 1
	run_case bt16, bt, 16, 1
	run_case bt32, bt, 32, 1
	run_case bt64, bt, 64, 1

	xor %edi, %edi
	jmp exit

/* Protect a whole host page; no shared-page interpretation is involved. */
arm_fault:
	movl $0, seen(%rip)
	mov $10, %eax
	mov page(%rip), %rdi
	mov $16384, %esi
	xor %edx, %edx
	syscall
	test %rax, %rax
	js fail_setup
	pushq $0xad7
	popfq
	movabs $0x8000000080008080, %rax
	movabs $0x7fffffff7fff7f81, %rbx
	movabs $0x1122334455667788, %rcx
	ret

check_result:
	pushfq
	pop %r8
	xor expected_flags(%rip), %r8
	and flag_mask(%rip), %r8
	jnz fail_flags
	cmpl $1, seen(%rip)
	jne fail_count
	cmp $0x12345678, %rcx
	jne fail_state
	movdqu %xmm0, actual(%rip)
	mov actual(%rip), %r8
	cmp poison(%rip), %r8
	jne fail_state
	mov actual+8(%rip), %r8
	cmp poison+8(%rip), %r8
	jne fail_state
	ret

.p2align 4
plain:
	nop
plain_fault:
	mov (%rdi), %rcx
	ret

	fault_function cmp8, cmp, 8
	fault_function cmp16, cmp, 16
	fault_function cmp32, cmp, 32
	fault_function cmp64, cmp, 64
	fault_function test8, test, 8
	fault_function test16, test, 16
	fault_function test32, test, 32
	fault_function test64, test, 64
	fault_function same8, test, 8, 1
	fault_function same16, test, 16, 1
	fault_function same32, test, 32, 1
	fault_function same64, test, 64, 1
	fault_function bt16, bt, 16
	fault_function bt32, bt, 32
	fault_function bt64, bt, 64

handler:
	cmp $11, %edi
	jne fail_signal
	cmpl $0, seen(%rip)
	jne fail_count
	mov 16(%rsi), %r8
	cmp page(%rip), %r8
	jne fail_address
	mov 168(%rdx), %r8
	cmp expected_pc(%rip), %r8
	jne fail_pc
	mov 176(%rdx), %r8
	xor expected_flags(%rip), %r8
	and flag_mask(%rip), %r8
	jnz fail_flags
	movabs $0x8000000080008080, %r8
	cmp 144(%rdx), %r8
	jne fail_state
	movabs $0x7fffffff7fff7f81, %r8
	cmp 128(%rdx), %r8
	jne fail_state
	movabs $0x1122334455667788, %r8
	cmp 152(%rdx), %r8
	jne fail_state
	mov 104(%rdx), %r8
	cmp page(%rip), %r8
	jne fail_state
	mov 224(%rdx), %r9
	mov 160(%r9), %r8
	cmp poison(%rip), %r8
	jne fail_state
	mov 168(%r9), %r8
	cmp poison+8(%rip), %r8
	jne fail_state
	movl $1, seen(%rip)
	mov $10, %eax
	mov page(%rip), %rdi
	mov $16384, %esi
	mov $3, %edx
	syscall
	test %rax, %rax
	js fail_setup
	ret

restorer:
	mov $15, %eax
	syscall
	ud2

fail_setup: mov $1, %edi; jmp exit
fail_signal: mov $2, %edi; jmp exit
fail_address: mov $3, %edi; jmp exit
fail_pc: mov $4, %edi; jmp exit
fail_flags: mov $5, %edi; jmp exit
fail_state: mov $6, %edi; jmp exit
fail_count: mov $7, %edi
exit:
	mov $60, %eax
	syscall

.data
/* Keep mutable test state out of the code's 16 KiB host page. */
.p2align 14
action: .quad handler, 0x04000004, restorer, 0
poison: .quad 0x0123456789abcdef, 0xfedcba9876543210
page: .quad 0
expected_pc: .quad 0
expected_flags: .quad 0
flag_mask: .quad 0
seen: .long 0
.bss
.p2align 4
actual: .skip 16
.section .note.GNU-stack,"",@progbits
