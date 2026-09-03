/* SPDX-License-Identifier: GPL-2.0-only */
    .text
    .globl _start
_start:
    cmpq $1, (%rsp)
    jne fault_case

    movl $1, %edi
    movl $0x44, %r9d
    xorl %eax, %eax
    orb $0, %al
    je 1f
    jmp exit_guest
1:  call check_flags

    movl $2, %edi
    movl $0x84, %r9d
    movb $0x80, %al
    orb $1, %al
    js 2f
    jmp exit_guest
2:  call check_flags
    cmpb $0x81, %al
    jne exit_guest

    movl $3, %edi
    movw $0x7f00, %ax
    orw $1, %ax
    jns 3f
    jmp exit_guest
3:  cmpw $0x7f01, %ax
    jne exit_guest

    movl $4, %edi
    movabsq $0x100000000, %rax
    orl $0, %eax
    je 4f
    jmp exit_guest
4:  testq %rax, %rax
    jne exit_guest

    movl $5, %edi
    movq $0x1000, %rax
    orq $1, %rax
    jne 5f
    jmp exit_guest
5:  cmpq $0x1001, %rax
    jne exit_guest

    movl $6, %edi
    movl $0, mem32(%rip)
    orl $0x80000000, mem32(%rip)
    js 6f
    jmp exit_guest
6:  cmpl $0x80000000, mem32(%rip)
    jne exit_guest

    movl $7, %edi
    movq $0x100, %rax
    movq $1, %r10
    orq %r10, %rax
    jne 7f
    jmp exit_guest
7:  cmpq $0x101, %rax
    jne exit_guest

    movl $8, %edi
    movq $0x100, %rax
    orq mem64(%rip), %rax
    jne .Lmem_src_ok
    jmp exit_guest
.Lmem_src_ok:
    cmpq $0x101, %rax
    jne exit_guest

    /* Not-taken sides. */
    movl $9, %edi
    movl $1, %eax
    orl $0, %eax
    je exit_guest
    movl $10, %edi
    xorl %eax, %eax
    orl $0, %eax
    jne exit_guest
    movl $11, %edi
    movl $1, %eax
    orl $0, %eax
    js exit_guest
    movl $12, %edi
    movl $0x80000000, %eax
    orl $0, %eax
    jns exit_guest

    movl $13, %edi
    movl $1, %eax
    orl $2, %eax
    nop
    jne 8f
    jmp exit_guest
8:  cmpl $3, %eax
    jne exit_guest

    movl $14, %edi
    movl $0, %eax
    orl $0, %eax
    jo exit_guest

    movl $15, %edi
    movl $0, lock_value(%rip)
    lock orl $1, lock_value(%rip)
    jne 9f
    jmp exit_guest
9:  cmpl $1, lock_value(%rip)
    jne exit_guest

    movl $100000, %ecx
10:
    movl $1, %eax
    orl $2, %eax
    jne 11f
    jmp exit_guest
11: decl %ecx
    jne 10b

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
    orq $1, (%rax)
    jne 12f
12: movl $16, %edi
    jmp exit_guest

    .data
    .p2align 3
mem32:       .long 0
lock_value:  .long 0
mem64:       .quad 1
