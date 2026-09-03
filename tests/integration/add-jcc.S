/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Exercise the adjacent ADD + JE/JNE/JS/JNS instruction pattern.
 */

    .text
    .globl _start
_start:
    cmpq $1, (%rsp)
    jne fault_case

    /* 8-bit zero result with CF/AF/PF/ZF live. */
    movl $1, %edi
    movl $0x55, %r9d
    movb $0xff, %al
    addb $1, %al
    je 1f
    jmp exit_guest
1:
    call check_flags
    testb %al, %al
    jne exit_guest

    /* 8-bit signed overflow and negative result. */
    movl $2, %edi
    movl $0x890, %r9d
    movb $0x7f, %al
    addb $1, %al
    js 2f
    jmp exit_guest
2:
    call check_flags
    cmpb $0x80, %al
    jne exit_guest

    /* 16-bit positive result with carry. */
    movl $3, %edi
    movl $0x11, %r9d
    movw $0xffff, %ax
    addw $2, %ax
    jns 3f
    jmp exit_guest
3:
    call check_flags
    cmpw $1, %ax
    jne exit_guest

    /* A 32-bit destination must clear the upper half of the GPR. */
    movl $4, %edi
    movl $0x55, %r9d
    movq $-1, %rax
    addl $1, %eax
    je 4f
    jmp exit_guest
4:
    call check_flags
    testq %rax, %rax
    jne exit_guest

    /* 64-bit signed overflow and negative result. */
    movl $5, %edi
    movl $0x894, %r9d
    movabsq $0x7fffffffffffffff, %rax
    addq $1, %rax
    js 5f
    jmp exit_guest
5:
    call check_flags
    movabsq $0x8000000000000000, %r10
    cmpq %r10, %rax
    jne exit_guest

    /* Preserve both original inputs when source and destination alias. */
    movl $6, %edi
    movl $0x845, %r9d
    movl $0x80000000, %eax
    addl %eax, %eax
    je 6f
    jmp exit_guest
6:
    call check_flags
    testq %rax, %rax
    jne exit_guest

    /* Memory source. */
    movl $7, %edi
    movl $0x55, %r9d
    movq $-1, %rax
    addq one(%rip), %rax
    je 7f
    jmp exit_guest
7:
    call check_flags
    testq %rax, %rax
    jne exit_guest

    /* Immediate to memory destination. */
    movl $8, %edi
    movl $0x55, %r9d
    movl $-1, mem32(%rip)
    addl $1, mem32(%rip)
    je 8f
    jmp exit_guest
8:
    call check_flags
    cmpl $0, mem32(%rip)
    jne exit_guest

    /* GPR to memory destination. */
    movl $9, %edi
    movl $0x55, %r9d
    movq $-1, mem64(%rip)
    movq $1, %rax
    addq %rax, mem64(%rip)
    je 9f
    jmp exit_guest
9:
    call check_flags
    cmpq $0, mem64(%rip)
    jne exit_guest

    /* Exercise the not-taken side of every supported condition. */
    movl $10, %edi
    xorl %eax, %eax
    addl $1, %eax
    je exit_guest
    movl $11, %edi
    movl $-1, %eax
    addl $1, %eax
    jne exit_guest
    movl $12, %edi
    xorl %eax, %eax
    addl $1, %eax
    js exit_guest
    movl $13, %edi
    movl $-2, %eax
    addl $1, %eax
    jns exit_guest

    /* Non-adjacent ADD must execute normally and be rejected by the scanner. */
    movl $14, %edi
    xorl %eax, %eax
    addl $1, %eax
    nop
    jne 10f
    jmp exit_guest
10:
    cmpl $1, %eax
    jne exit_guest

    /* Unsupported JO must execute through the ordinary ADD path. */
    movl $15, %edi
    movl $0x7fffffff, %eax
    addl $1, %eax
    jo 11f
    jmp exit_guest
11:
    cmpl $0x80000000, %eax
    jne exit_guest

    /* LOCK ADD remains on the ordinary atomic path. */
    movl $16, %edi
    movl $0, lock_value(%rip)
    lock addl $1, lock_value(%rip)
    jne 12f
    jmp exit_guest
12:
    cmpl $1, lock_value(%rip)
    jne exit_guest

    /* Heat a matching loop so the TU/retranslation path is exercised. */
    movl $-100000, %ecx
14:
    addl $1, %ecx
    jne 14b
    cmpl $0, %ecx
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

/* With an extra argv, execute a matching faulting memory ADD. */
fault_case:
    xorl %eax, %eax
    addl $1, (%rax)
    jne 13f
13:
    movl $17, %edi
    jmp exit_guest

    .data
    .p2align 3
one:
    .quad 1
mem64:
    .quad 0
mem32:
    .long 0
lock_value:
    .long 0
