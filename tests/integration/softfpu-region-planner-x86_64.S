.equ __NR_exit, 60

.section .text
.global _start
.type _start, @function
_start:
    and $-64, %rsp
    sub $256, %rsp

    movabs $0x0123456789abcdef, %rax
    mov %rax, 192(%rsp)
    movabs $0xfedcba9876543210, %rax
    mov %rax, 200(%rsp)

    /* A conditional FILD helper may bridge two required helpers. */
    movdqu 192(%rsp), %xmm0
    movq $1, 0(%rsp)
    fninit
    fldz
    fcos
    lea 0(%rsp), %r15
    fildq (%r15)
    fsin
    nop
    movdqu %xmm0, 208(%rsp)
    mov 208(%rsp), %rax
    cmp 192(%rsp), %rax
    jne .Lfail_fild_bridge
    mov 216(%rsp), %rax
    cmp 200(%rsp), %rax
    jne .Lfail_fild_bridge

    /* The fast FIADD path may stay between two required helpers. */
    movdqu 192(%rsp), %xmm0
    movl $1, 16(%rsp)
    fninit
    fldz
    fcos
    lea 16(%rsp), %r12
    fiaddl (%r12)
    fsin
    nop
    movdqu %xmm0, 208(%rsp)
    mov 208(%rsp), %rax
    cmp 192(%rsp), %rax
    jne .Lfail_fast_bridge
    mov 216(%rsp), %rax
    cmp 200(%rsp), %rax
    jne .Lfail_fast_bridge

    /* Force the fast FISTP path to fall back inside a shared region. */
    movdqu 192(%rsp), %xmm0
    movabs $0x7e37e43c8800759c, %rax
    mov %rax, 32(%rsp)
    movl $0x12345678, 40(%rsp)
    fninit
    fldz
    fcos
    fldl 32(%rsp)
    lea 40(%rsp), %r13
    fistpl (%r13)
    fsin
    nop
    cmpl $0x80000000, 40(%rsp)
    jne .Lfail_fistp_bridge
    movdqu %xmm0, 208(%rsp)
    mov 208(%rsp), %rax
    cmp 192(%rsp), %rax
    jne .Lfail_fistp_bridge
    mov 216(%rsp), %rax
    cmp 200(%rsp), %rax
    jne .Lfail_fistp_bridge

    /* An isolated conditional FILD helper must create a local region. */
    movdqu 192(%rsp), %xmm0
    fninit
    lea 0(%rsp), %r14
    fildq (%r14)
    nop
    movdqu %xmm0, 208(%rsp)
    mov 208(%rsp), %rax
    cmp 192(%rsp), %rax
    jne .Lfail_fild_local
    mov 216(%rsp), %rax
    cmp 200(%rsp), %rax
    jne .Lfail_fild_local

    /* An isolated fast FISTP fallback must also create a local region. */
    movdqu 192(%rsp), %xmm0
    fninit
    fldl 32(%rsp)
    movl $0x12345678, 44(%rsp)
    lea 44(%rsp), %r11
    fistpl (%r11)
    nop
    cmpl $0x80000000, 44(%rsp)
    jne .Lfail_fistp_local
    movdqu %xmm0, 208(%rsp)
    mov 208(%rsp), %rax
    cmp 192(%rsp), %rax
    jne .Lfail_fistp_local
    mov 216(%rsp), %rax
    cmp 200(%rsp), %rax
    jne .Lfail_fistp_local

    xor %edi, %edi
    jmp .Lexit
.Lfail_fild_bridge:
    mov $1, %edi
    jmp .Lexit
.Lfail_fast_bridge:
    mov $2, %edi
    jmp .Lexit
.Lfail_fistp_bridge:
    mov $3, %edi
    jmp .Lexit
.Lfail_fild_local:
    mov $4, %edi
    jmp .Lexit
.Lfail_fistp_local:
    mov $5, %edi
.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
