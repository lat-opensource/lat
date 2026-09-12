/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Exercise adjacent immediate SAR/SHR + Jcc patterns and x86 count masking.
 */

    .text
    .globl _start
_start:
    cmpq $1, (%rsp)
    jne fault_case

    /* SAR: all supported conditions and operand widths. */
    movl $1, %edi
    movb $0x80, %al
    sarb $1, %al
    js 1f
    jmp exit_guest
1:
    cmpb $0xc0, %al
    jne exit_guest

    movl $2, %edi
    movw $1, %ax
    sarw $1, %ax
    je 2f
    jmp exit_guest
2:
    cmpw $0, %ax
    jne exit_guest

    movl $3, %edi
    movq $-1, %rax
    sarl $31, %eax
    jns exit_guest
    movl $0xffffffff, %r10d
    cmpq %r10, %rax
    jne exit_guest

    movl $4, %edi
    movabsq $0x8000000000000000, %rax
    sarq $63, %rax
    je exit_guest
    cmpq $-1, %rax
    jne exit_guest

    /* SHR: JE is new; JNE remains on its legacy independent option. */
    movl $5, %edi
    movb $1, %al
    shrb $1, %al
    je 3f
    jmp exit_guest
3:
    testb %al, %al
    jne exit_guest

    movl $6, %edi
    movw $0x8000, %ax
    shrw $15, %ax
    jne 4f
    jmp exit_guest
4:
    cmpw $1, %ax
    jne exit_guest

    movl $7, %edi
    movq $-1, %rax
    shrl $31, %eax
    jne 5f
    jmp exit_guest
5:
    cmpq $1, %rax
    jne exit_guest

    movl $8, %edi
    movq $-1, %rax
    shrq $63, %rax
    jne 6f
    jmp exit_guest
6:
    cmpq $1, %rax
    jne exit_guest

    /* Counts at and beyond 8/16-bit widths are not modulo the width. */
    movl $9, %edi
    movb $0x80, %al
    sarb $8, %al
    js 7f
    jmp exit_guest
7:
    cmpb $0xff, %al
    jne exit_guest

    movl $10, %edi
    movb $0xff, %al
    shrb $9, %al
    je 8f
    jmp exit_guest
8:
    testb %al, %al
    jne exit_guest

    movl $11, %edi
    movw $0x8000, %ax
    sarw $16, %ax
    js 9f
    jmp exit_guest
9:
    cmpw $0xffff, %ax
    jne exit_guest

    movl $12, %edi
    movw $0xffff, %ax
    shrw $17, %ax
    je 10f
    jmp exit_guest
10:
    testw %ax, %ax
    jne exit_guest

    /* 31/63 remain effective for narrow operands; 32/64 mask to zero. */
    movl $13, %edi
    movw $0x8000, %ax
    sarw $31, %ax
    js 11f
    jmp exit_guest
11:
    cmpw $0xffff, %ax
    jne exit_guest

    movl $14, %edi
    movb $0xff, %al
    shrb $63, %al
    je 12f
    jmp exit_guest
12:
    testb %al, %al
    jne exit_guest

    /* Effective count zero must preserve both the destination and flags. */
    movl $15, %edi
    movl $0x12345678, %eax
    cmpl %eax, %eax
    sarl $32, %eax
    jne exit_guest
    pushfq
    popq %r10
    testq $0x40, %r10
    je exit_guest
    cmpl $0x12345678, %eax
    jne exit_guest

    movl $16, %edi
    movabsq $0x8000000000000000, %rax
    cmpq %rax, %rax
    shrq $64, %rax
    jne exit_guest
    pushfq
    popq %r10
    testq $0x40, %r10
    je exit_guest
    movabsq $0x8000000000000000, %r10
    cmpq %r10, %rax
    jne exit_guest

    movl $26, %edi
    movl $0x87654321, %eax
    cmpl %eax, %eax
    shrl $32, %eax
    je 23f
    jmp exit_guest
23:
    pushfq
    popq %r10
    testq $0x40, %r10
    je exit_guest
    cmpl $0x87654321, %eax
    jne exit_guest

    /* width+1 becomes one for 32/64-bit operands. */
    movl $17, %edi
    movl $0x80000000, %eax
    sarl $33, %eax
    js 13f
    jmp exit_guest
13:
    movl $0xc0000000, %r10d
    cmpq %r10, %rax
    jne exit_guest

    movl $18, %edi
    movq $2, %rax
    shrq $65, %rax
    jne 14f
    jmp exit_guest
14:
    cmpq $1, %rax
    jne exit_guest

    /* Preserve defined flags when a successor reads them. */
    movl $19, %edi
    movl $0x85, %r9d
    movb $0x81, %al
    sarb $1, %al
    js 15f
    jmp exit_guest
15:
    call check_flags

    movl $20, %edi
    movl $0x45, %r9d
    movb $1, %al
    shrb $1, %al
    je 16f
    jmp exit_guest
16:
    call check_flags

    /* Memory destination, plus scanner rejection paths. */
    movl $21, %edi
    movq $-2, mem64(%rip)
    sarq $1, mem64(%rip)
    js 17f
    jmp exit_guest
17:
    cmpq $-1, mem64(%rip)
    jne exit_guest

    movl $22, %edi
    movl $4, %eax
    movb $1, %cl
    shrl %cl, %eax
    jne 18f
    jmp exit_guest
18:
    cmpl $2, %eax
    jne exit_guest

    movl $23, %edi
    movl $0x80000000, %eax
    sarl $1, %eax
    jo exit_guest

    movl $24, %edi
    movl $2, %eax
    shrl $1, %eax
    nop
    jne 19f
    jmp exit_guest
19:
    cmpl $1, %eax
    jne exit_guest

    /* Heat matching backward branches for TU/retranslation coverage. */
    movl $0x40000000, %ecx
20:
    sarl $1, %ecx
    jne 20b
    cmpl $0, %ecx
    jne exit_guest

    movl $0x80000000, %ecx
21:
    shrl $1, %ecx
    jne 21b

    xorl %edi, %edi
exit_guest:
    movl $60, %eax
    syscall

/* Compare the defined CF/PF/ZF/SF/OF flags without perturbing them first. */
check_flags:
    pushfq
    popq %r10
    andq $0x8c5, %r10
    cmpq %r9, %r10
    jne exit_guest
    ret

fault_case:
    xorl %eax, %eax
    sarq $1, (%rax)
    jne 24f
24:
    movl $25, %edi
    jmp exit_guest

    .data
    .p2align 3
mem64:
    .quad 0
