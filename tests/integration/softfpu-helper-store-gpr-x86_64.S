.equ __NR_exit, 60

.section .text
.global _start
.type _start, @function
_start:
    and $-64, %rsp
    sub $128, %rsp
    fninit

    /* FIST helper result stored through a high guest GPR base. */
    movabs $0x3ff8000000000000, %rax
    mov %rax, 0(%rsp)
    movl $0x12345678, 16(%rsp)
    fldl 0(%rsp)
    lea 16(%rsp), %r8
    fistl (%r8)
    cmpl $2, 16(%rsp)
    jne .Lfail_fist
    fstp %st(0)

    /* Large FIST uses the conditional helper fallback in fast mode. */
    movabs $0x7e37e43c8800759c, %rax
    mov %rax, 0(%rsp)
    movl $0x12345678, 20(%rsp)
    fldl 0(%rsp)
    lea 20(%rsp), %r8
    fistl (%r8)
    cmpl $0x80000000, 20(%rsp)
    jne .Lfail_fist_fallback
    fstp %st(0)

    /* Large FISTP uses its local helper fallback in fast mode. */
    movl $0x12345678, 24(%rsp)
    fldl 0(%rsp)
    lea 24(%rsp), %r8
    fistpl (%r8)
    cmpl $0x80000000, 24(%rsp)
    jne .Lfail_fistp_fallback

    /* FST helper result stored through a high guest GPR base. */
    movabs $0x3ff8000000000000, %rax
    mov %rax, 0(%rsp)
    movq $0, 32(%rsp)
    fldl 0(%rsp)
    lea 32(%rsp), %r8
    fstl (%r8)
    mov 32(%rsp), %rax
    movabs $0x3ff8000000000000, %rbx
    cmp %rbx, %rax
    jne .Lfail_fst
    fstp %st(0)

    xor %edi, %edi
    jmp .Lexit

.Lfail_fist:
    mov $1, %edi
    jmp .Lexit
.Lfail_fist_fallback:
    mov $2, %edi
    jmp .Lexit
.Lfail_fistp_fallback:
    mov $3, %edi
    jmp .Lexit
.Lfail_fst:
    mov $4, %edi

.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
