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
.else
.if \same
	\kind %eax, %eax
.else
	\kind %ebx, %eax
.endif
.endif
.endm

.macro run_case name, kind, bits, mask, same=0
	mov $0x80008080, %eax
	mov $0x7fff7f81, %ebx
.ifc \kind,bt
	compare bt, \bits
.else
	compare \kind, \bits, \same
.endif
	pushfl
	popl expected_flags
	movl $\mask, flag_mask
	movl $\name\()_fault, expected_pc
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
	mov (%edi), %ecx
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
	mov $174, %eax
	mov $11, %ebx
	mov $action, %ecx
	xor %edx, %edx
	mov $8, %esi
	int $0x80
	test %eax, %eax
	js fail_setup
	mov $192, %eax
	xor %ebx, %ebx
	mov $16384, %ecx
	mov $3, %edx
	mov $0x21, %esi
	mov $-1, %edi
	xor %ebp, %ebp
	int $0x80
	cmp $-4095, %eax
	jae fail_setup
	mov $192, %eax
	xor %ebx, %ebx
	mov $65536, %ecx
	mov $3, %edx
	mov $0x22, %esi
	mov $-1, %edi
	xor %ebp, %ebp
	int $0x80
	cmp $-4095, %eax
	jae fail_setup
	add $16383, %eax
	and $-16384, %eax
	mov %eax, page
	movl $0x12345678, (%eax)

	movl $0xad7, expected_flags
	movl $0x8d5, flag_mask
	movl $plain_fault, expected_pc
	call arm_fault
	call plain
	call check_result
	run_case cmp8, cmp, 8, 0x8d5
	run_case cmp16, cmp, 16, 0x8d5
	run_case cmp32, cmp, 32, 0x8d5
	run_case test8, test, 8, 0x8c5
	run_case test16, test, 16, 0x8c5
	run_case test32, test, 32, 0x8c5
	run_case same8, test, 8, 0x8c5, 1
	run_case same16, test, 16, 0x8c5, 1
	run_case same32, test, 32, 0x8c5, 1
	run_case bt16, bt, 16, 1
	run_case bt32, bt, 32, 1
	xor %ebx, %ebx
	jmp exit

arm_fault:
	movl $0, seen
	mov $125, %eax
	mov page, %ebx
	mov $16384, %ecx
	xor %edx, %edx
	int $0x80
	test %eax, %eax
	js fail_setup
	pushl $0xad7
	popfl
	mov $0x80008080, %eax
	mov $0x7fff7f81, %ebx
	mov $0x55667788, %ecx
	mov page, %edi
	ret

check_result:
	pushfl
	pop %edx
	xor expected_flags, %edx
	and flag_mask, %edx
	jnz fail_flags
	cmpl $1, seen
	jne fail_count
	cmp $0x12345678, %ecx
	jne fail_state
	ret

.p2align 4
plain:
	nop
plain_fault:
	mov (%edi), %ecx
	ret
	fault_function cmp8, cmp, 8
	fault_function cmp16, cmp, 16
	fault_function cmp32, cmp, 32
	fault_function test8, test, 8
	fault_function test16, test, 16
	fault_function test32, test, 32
	fault_function same8, test, 8, 1
	fault_function same16, test, 16, 1
	fault_function same32, test, 32, 1
	fault_function bt16, bt, 16
	fault_function bt32, bt, 32

handler:
	cmpl $11, 4(%esp)
	jne fail_signal
	cmpl $0, seen
	jne fail_count
	mov 8(%esp), %esi
	mov 12(%esi), %eax
	cmp page, %eax
	jne fail_address
	mov 12(%esp), %esi
	mov 76(%esi), %eax
	cmp expected_pc, %eax
	jne fail_pc
	mov 84(%esi), %eax
	xor expected_flags, %eax
	and flag_mask, %eax
	jnz fail_flags
	cmpl $0x80008080, 64(%esi)
	jne fail_state
	cmpl $0x7fff7f81, 52(%esi)
	jne fail_state
	cmpl $0x55667788, 60(%esi)
	jne fail_state
	mov 36(%esi), %eax
	cmp page, %eax
	jne fail_state
	movl $1, seen
	mov $125, %eax
	mov page, %ebx
	mov $16384, %ecx
	mov $3, %edx
	int $0x80
	test %eax, %eax
	js fail_setup
	ret

restorer:
	mov $173, %eax
	int $0x80
	ud2

fail_setup: mov $1, %ebx; jmp exit
fail_signal: mov $2, %ebx; jmp exit
fail_address: mov $3, %ebx; jmp exit
fail_pc: mov $4, %ebx; jmp exit
fail_flags: mov $5, %ebx; jmp exit
fail_state: mov $6, %ebx; jmp exit
fail_count: mov $7, %ebx
exit:
	mov $1, %eax
	int $0x80

.data
.p2align 14
action: .long handler, 0x04000004, restorer, 0, 0
page: .long 0
expected_pc: .long 0
expected_flags: .long 0
flag_mask: .long 0
seen: .long 0
.section .note.GNU-stack,"",@progbits
