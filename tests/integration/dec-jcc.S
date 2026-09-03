/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Exercise the adjacent GPR DEC + JE/JNE instruction pattern.
 */

    .text
    .globl _start
_start:
    cmpq $1, (%rsp)
    jne fault_case

    /* 8-bit 1 -> 0 with CF set and upper bits preserved. */
    movl $1, %edi
    movl $0x45, %r9d
    movabsq $0x1122334455667701, %rax
    stc
    decb %al
    je 1f
    jmp exit_guest
1:
    call check_flags
    movabsq $0x1122334455667700, %r10
    cmpq %r10, %rax
    jne exit_guest

    /* 8-bit minimum negative value overflows to the maximum positive value. */
    movl $2, %edi
    movl $0x810, %r9d
    movb $0x80, %al
    clc
    decb %al
    jne 2f
    jmp exit_guest
2:
    call check_flags
    cmpb $0x7f, %al
    jne exit_guest

    /* 16-bit 0 -> -1 with CF set and upper bits preserved. */
    movl $3, %edi
    movl $0x95, %r9d
    movabsq $0x1122334455660000, %rax
    stc
    decw %ax
    jne 3f
    jmp exit_guest
3:
    call check_flags
    movabsq $0x112233445566ffff, %r10
    cmpq %r10, %rax
    jne exit_guest

    /* A 32-bit destination must clear the upper half of the GPR. */
    movl $4, %edi
    movl $0x44, %r9d
    movabsq $0xffffffff00000001, %rax
    clc
    decl %eax
    je 4f
    jmp exit_guest
4:
    call check_flags
    testq %rax, %rax
    jne exit_guest

    /* 64-bit signed overflow must preserve CF. */
    movl $5, %edi
    movl $0x815, %r9d
    movabsq $0x8000000000000000, %rax
    stc
    decq %rax
    jne 5f
    jmp exit_guest
5:
    call check_flags
    movabsq $0x7fffffffffffffff, %r10
    cmpq %r10, %rax
    jne exit_guest

    /* Exercise both not-taken sides. */
    movl $6, %edi
    movl $2, %eax
    decl %eax
    je exit_guest
    movl $7, %edi
    movl $1, %eax
    decl %eax
    jne exit_guest

    /* Non-adjacent DEC must execute normally and be rejected. */
    movl $8, %edi
    movl $2, %eax
    decl %eax
    nop
    jne 6f
    jmp exit_guest
6:
    cmpl $1, %eax
    jne exit_guest

    /* Unsupported JO must execute through the ordinary DEC path. */
    movl $9, %edi
    movl $0x80000000, %eax
    decl %eax
    jo 7f
    jmp exit_guest
7:
    cmpl $0x7fffffff, %eax
    jne exit_guest

    /* Memory destinations, including LOCK, remain outside this pattern. */
    movl $10, %edi
    movl $2, mem32(%rip)
    decl mem32(%rip)
    jne 8f
    jmp exit_guest
8:
    cmpl $1, mem32(%rip)
    jne exit_guest

    movl $11, %edi
    movl $1, mem32(%rip)
    lock decl mem32(%rip)
    je 9f
    jmp exit_guest
9:
    cmpl $0, mem32(%rip)
    jne exit_guest

    /* Heat a matching loop and verify CF survives every DEC. */
    movl $12, %edi
    movl $100000, %ecx
    stc
10:
    decl %ecx
    jne 10b
    jnc exit_guest
    testl %ecx, %ecx
    jne exit_guest

    xorl %edi, %edi
exit_guest:
    movl $60, %eax
    syscall

/* Compare CF/PF/AF/ZF/SF/OF without modifying flags before PUSHFQ. */
check_flags:
    pushfq
    popq %r10
    andq $0x8d5, %r10
    cmpq %r9, %r10
    jne exit_guest
    ret

/* With an extra argv, execute a faulting memory DEC. */
fault_case:
    xorl %eax, %eax
    decl (%rax)
    jne 11f
11:
    movl $13, %edi
    jmp exit_guest

    .data
    .p2align 2
mem32:
    .long 0
