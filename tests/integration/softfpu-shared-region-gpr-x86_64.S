.equ __NR_exit, 60

.section .text
.global _start
.type _start, @function
_start:
    and $-64, %rsp
    sub $128, %rsp

    /* A helper followed by another helper using R8 as a memory base. */
    fninit
    movq $0, 0(%rsp)
    movabs $0x3ff0000000000000, %rax
    mov %rax, 16(%rsp)
    fldl 0(%rsp)
    lea 16(%rsp), %r8
    fcos
    faddl (%r8)
    nop
    fstpl 24(%rsp)
    mov 24(%rsp), %rax
    movabs $0x4000000000000000, %rbx
    cmp %rbx, %rax
    jne .Lfail_helper

    /* The following integer operation is inline when fast bit 6 is set. */
    fninit
    movl $1, 40(%rsp)
    fldl 0(%rsp)
    lea 40(%rsp), %r12
    fcos
    fiaddl (%r12)
    nop
    fstpl 48(%rsp)
    mov 48(%rsp), %rax
    cmp %rbx, %rax
    jne .Lfail_inline

    /* FILD also consumes a high-GPR address after a preceding helper. */
    fninit
    movq $1, 64(%rsp)
    fldl 0(%rsp)
    lea 64(%rsp), %r15
    fcos
    fildq (%r15)
    nop
    fistpl 72(%rsp)
    cmpl $1, 72(%rsp)
    jne .Lfail_fild
    fstp %st(0)

    xor %edi, %edi
    jmp .Lexit
.Lfail_helper:
    mov $1, %edi
    jmp .Lexit
.Lfail_inline:
    mov $2, %edi
    jmp .Lexit
.Lfail_fild:
    mov $3, %edi
.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
