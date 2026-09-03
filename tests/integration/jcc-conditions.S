/* SPDX-License-Identifier: GPL-2.0-only */
    .text
    .globl _start
_start:
    cmpq $2, (%rsp)
    je cmp_fault
    cmpq $3, (%rsp)
    je sub_fault
    cmpq $4, (%rsp)
    je and_fault

    /* CMP+JS/JNS: SF is the sign of the width-truncated subtraction. */
    movl $1, %edi
    movl $0x810, %r9d
    movb $0x80, %al
    cmpb $1, %al
    jns .Lcmp8_overflow_ok
    jmp exit_guest
.Lcmp8_overflow_ok:
    call check_arith_flags

    movl $2, %edi
    movb $0, %al
    cmpb $1, %al
    js .Lcmp8_negative_ok
    jmp exit_guest
.Lcmp8_negative_ok:

    movl $3, %edi
    movw $0x8000, %ax
    cmpw $1, %ax
    jns .Lcmp16_overflow_ok
    jmp exit_guest
.Lcmp16_overflow_ok:
    movw $0, %ax
    cmpw $1, %ax
    js .Lcmp16_negative_ok
    jmp exit_guest
.Lcmp16_negative_ok:

    movl $4, %edi
    movl $0x80000000, %eax
    cmpl $1, %eax
    jns .Lcmp32_overflow_ok
    jmp exit_guest
.Lcmp32_overflow_ok:
    xorl %eax, %eax
    cmpl $1, %eax
    js .Lcmp32_negative_ok
    jmp exit_guest
.Lcmp32_negative_ok:

    movl $5, %edi
    movabsq $0x8000000000000000, %rax
    cmpq $1, %rax
    jns .Lcmp64_overflow_ok
    jmp exit_guest
.Lcmp64_overflow_ok:
    xorq %rax, %rax
    cmpq $1, %rax
    js .Lcmp64_negative_ok
    jmp exit_guest
.Lcmp64_negative_ok:

    /* SUB+JS/JNS also has to preserve the written result. */
    movl $6, %edi
    movb $0x80, %al
    subb $1, %al
    jns .Lsub8_overflow_ok
    jmp exit_guest
.Lsub8_overflow_ok:
    cmpb $0x7f, %al
    jne exit_guest

    movl $7, %edi
    movb $0, %al
    subb $1, %al
    js .Lsub8_negative_ok
    jmp exit_guest
.Lsub8_negative_ok:
    cmpb $0xff, %al
    jne exit_guest

    movl $8, %edi
    movw $0x8000, %ax
    subw $1, %ax
    jns .Lsub16_overflow_ok
    jmp exit_guest
.Lsub16_overflow_ok:
    cmpw $0x7fff, %ax
    jne exit_guest

    movl $9, %edi
    movl $0x80000000, %eax
    subl $1, %eax
    jns .Lsub32_overflow_ok
    jmp exit_guest
.Lsub32_overflow_ok:
    cmpl $0x7fffffff, %eax
    jne exit_guest

    movl $10, %edi
    movabsq $0x8000000000000000, %rax
    subq $1, %rax
    jns .Lsub64_overflow_ok
    jmp exit_guest
.Lsub64_overflow_ok:
    movabsq $0x7fffffffffffffff, %r10
    cmpq %r10, %rax
    jne exit_guest

    movl $11, %edi
    movl $0, mem32(%rip)
    subl $1, mem32(%rip)
    js .Lsub_mem_ok
    jmp exit_guest
.Lsub_mem_ok:
    cmpl $-1, mem32(%rip)
    jne exit_guest

    /* AND+JE and the existing JNE path must be complementary. */
    movl $12, %edi
    movl $0x44, %r9d
    movb $0xf0, %al
    andb $0x0f, %al
    je .Land8_zero_ok
    jmp exit_guest
.Land8_zero_ok:
    call check_logic_flags

    movl $13, %edi
    movw $0x100, %ax
    andw $0x100, %ax
    je exit_guest
    cmpw $0x100, %ax
    jne exit_guest

    movl $14, %edi
    movabsq $0x100000001, %rax
    andl $1, %eax
    je exit_guest
    cmpq $1, %rax
    jne exit_guest

    movl $15, %edi
    movq $0x100, %rax
    andq mem64(%rip), %rax
    je .Land64_zero_ok
    jmp exit_guest
.Land64_zero_ok:

    movl $16, %edi
    movl $0xffffffff, mem32(%rip)
    andl $0, mem32(%rip)
    je .Land_mem_ok
    jmp exit_guest
.Land_mem_ok:
    cmpl $0, mem32(%rip)
    jne exit_guest

    /* Existing SHR+JE, including count masking and JE/JNE complement. */
    movl $17, %edi
    movl $1, %eax
    shrl $1, %eax
    je .Lshr_zero_ok
    jmp exit_guest
.Lshr_zero_ok:
    movl $2, %eax
    shrl $1, %eax
    je exit_guest
    jne .Lshr_nonzero_ok
    jmp exit_guest
.Lshr_nonzero_ok:

    movl $18, %edi
    xorl %eax, %eax
    cmpl %eax, %eax
    shrl $32, %eax
    je .Lshr_count_zero_ok
    jmp exit_guest
.Lshr_count_zero_ok:

    /* Rejected cases stay on the ordinary path. */
    movl $19, %edi
    xorl %eax, %eax
    cmpl $1, %eax
    nop
    js .Lcmp_nonadj_ok
    jmp exit_guest
.Lcmp_nonadj_ok:
    subl $1, %eax
    nop
    js .Lsub_nonadj_ok
    jmp exit_guest
.Lsub_nonadj_ok:
    andl $0, %eax
    nop
    je .Land_nonadj_ok
    jmp exit_guest
.Land_nonadj_ok:
    shrl $1, %eax
    nop
    je .Lshr_nonadj_ok
    jmp exit_guest
.Lshr_nonadj_ok:

    movl $20, %edi
    movl $1, lock_value(%rip)
    lock subl $1, lock_value(%rip)
    jns .Llock_sub_ok
    jmp exit_guest
.Llock_sub_ok:
    lock andl $0, lock_value(%rip)
    je .Llock_and_ok
    jmp exit_guest
.Llock_and_ok:
    cmpl $0, lock_value(%rip)
    jne exit_guest

    /* Hot loop for JIT/TU and performance tests. */
    movl $100000, %ecx
.Lhot_loop:
    movl $0x80000000, %eax
    cmpl $1, %eax
    jns .Lhot_cmp_ok
    jmp exit_guest
.Lhot_cmp_ok:
    subl $1, %eax
    jns .Lhot_sub_ok
    jmp exit_guest
.Lhot_sub_ok:
    andl $0, %eax
    je .Lhot_and_ok
    jmp exit_guest
.Lhot_and_ok:
    movl $1, %eax
    shrl $1, %eax
    je .Lhot_shr_ok
    jmp exit_guest
.Lhot_shr_ok:
    decl %ecx
    jne .Lhot_loop

    xorl %edi, %edi
exit_guest:
    movl $60, %eax
    syscall

check_arith_flags:
    pushfq
    popq %r10
    andq $0x8d5, %r10
    cmpq %r9, %r10
    jne exit_guest
    ret

check_logic_flags:
    pushfq
    popq %r10
    andq $0x8c5, %r10
    cmpq %r9, %r10
    jne exit_guest
    ret

cmp_fault:
    xorl %eax, %eax
    cmpq $1, (%rax)
    js .Lcmp_fault_unreachable
.Lcmp_fault_unreachable:
    movl $21, %edi
    jmp exit_guest

sub_fault:
    xorl %eax, %eax
    subq $1, (%rax)
    js .Lsub_fault_unreachable
.Lsub_fault_unreachable:
    movl $22, %edi
    jmp exit_guest

and_fault:
    xorl %eax, %eax
    andq $1, (%rax)
    je .Land_fault_unreachable
.Land_fault_unreachable:
    movl $23, %edi
    jmp exit_guest

    .data
    .p2align 3
mem32:       .long 0
lock_value:  .long 0
mem64:       .quad 0xff
