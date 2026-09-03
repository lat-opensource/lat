/* SPDX-License-Identifier: GPL-2.0-only */
    .text
    .globl _start
_start:
    cmpq $1, (%rsp)
    jne fault_case

    /* Same-register clear, 8-bit zero result. */
    movl $1, %edi
    movl $0x44, %r9d
    movb $0x5a, %al
    xorb %al, %al
    je 1f
    jmp exit_guest
1:  call check_flags

    /* Nonzero 8-bit result catches XOR(result, result) flag rebuilds. */
    movl $2, %edi
    movl $0x84, %r9d
    movb $0x81, %al
    xorb $0, %al
    js 2f
    jmp exit_guest
2:  call check_flags
    cmpb $0x81, %al
    jne exit_guest

    movl $3, %edi
    xorl %r9d, %r9d
    movw $0x7f00, %ax
    xorw $1, %ax
    jns 3f
    jmp exit_guest
3:  call check_flags
    cmpw $0x7f01, %ax
    jne exit_guest

    /* A 32-bit destination must clear the upper half of the GPR. */
    movl $4, %edi
    movl $0x44, %r9d
    movq $-1, %rax
    xorl $-1, %eax
    je 4f
    jmp exit_guest
4:  call check_flags
    testq %rax, %rax
    jne exit_guest

    movl $5, %edi
    movl $0x84, %r9d
    movq $-1, %rax
    xorq $0, %rax
    js 5f
    jmp exit_guest
5:  call check_flags
    cmpq $-1, %rax
    jne exit_guest

    /* Same-register clear, 64-bit zero result. */
    movl $6, %edi
    movl $0x44, %r9d
    movq $0x1234, %rax
    xorq %rax, %rax
    je 6f
    jmp exit_guest
6:  call check_flags

    /* Different registers with a nonzero result and flags fallback. */
    movl $7, %edi
    xorl %r9d, %r9d
    movq $0x100, %rax
    movq $1, %r10
    xorq %r10, %rax
    jne 7f
    jmp exit_guest
7:  call check_flags
    cmpq $0x101, %rax
    jne exit_guest

    movl $8, %edi
    xorl %r9d, %r9d
    movq $0x100, %rax
    xorq mem64(%rip), %rax
    jne 8f
    jmp exit_guest
8:  call check_flags
    cmpq $0x101, %rax
    jne exit_guest

    movl $9, %edi
    movl $0x84, %r9d
    movl $0x80000000, mem32(%rip)
    xorl $0, mem32(%rip)
    js 9f
    jmp exit_guest
9:  call check_flags
    cmpl $0x80000000, mem32(%rip)
    jne exit_guest

    movl $10, %edi
    movl $0x44, %r9d
    movq $1, mem64(%rip)
    movq $1, %rax
    xorq %rax, mem64(%rip)
    je 10f
    jmp exit_guest
10: call check_flags
    cmpq $0, mem64(%rip)
    jne exit_guest

    /* Not-taken sides. */
    movl $11, %edi
    movl $1, %eax
    xorl $0, %eax
    je exit_guest
    movl $12, %edi
    xorl %eax, %eax
    xorl %eax, %eax
    jne exit_guest
    movl $13, %edi
    movl $1, %eax
    xorl $0, %eax
    js exit_guest
    movl $14, %edi
    movl $0x80000000, %eax
    xorl $0, %eax
    jns exit_guest

    /* Non-adjacent and unsupported-cc rejection paths. */
    movl $15, %edi
    movl $1, %eax
    xorl $2, %eax
    nop
    jne 11f
    jmp exit_guest
11: cmpl $3, %eax
    jne exit_guest

    movl $16, %edi
    xorl %eax, %eax
    xorl $0, %eax
    jo exit_guest

    movl $17, %edi
    movl $0, lock_value(%rip)
    lock xorl $1, lock_value(%rip)
    jne 12f
    jmp exit_guest
12: cmpl $1, lock_value(%rip)
    jne exit_guest

    /* Keep the existing XOR+DIV pattern independent. */
    movl $18, %edi
    movl $3, %r10d
    movl $10, %eax
    xorl %edx, %edx
    divl %r10d
    cmpl $3, %eax
    jne exit_guest
    cmpl $1, %edx
    jne exit_guest

    movl $100000, %ecx
13:
    movl $0x12345678, %eax
    xorl $0x12345678, %eax
    je 14f
    jmp exit_guest
14: decl %ecx
    jne 13b

    /* The taken TU successor consumes CF while fallthrough does not. */
    xorl %r8d, %r8d
    movl $100000, %ecx
.Ltu_target_loop:
    stc
    movl $1, %eax
    xorl $1, %eax
    je .Ltu_target
    jmp exit_guest
.Ltu_target:
    adcl $0, %r8d
    decl %ecx
    jne .Ltu_target_loop
    testl %r8d, %r8d
    jne exit_guest

    xorl %edi, %edi
exit_guest:
    movl $60, %eax
    syscall

check_flags:
    pushfq
    popq %r10
    andq $0x8c5, %r10
    cmpq %r9, %r10
    jne exit_guest
    ret

fault_case:
    xorl %eax, %eax
    xorq $1, (%rax)
    jne 15f
15: movl $19, %edi
    jmp exit_guest

    .data
    .p2align 3
mem32:       .long 0
lock_value:  .long 0
mem64:       .quad 1
