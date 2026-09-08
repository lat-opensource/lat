.equ __NR_exit, 1

.macro check_down fail
    fldl 8(%esp)
    fcos
    fstps 4(%esp)
    cmpl $0x3f7fffff, 4(%esp)
    jne \fail
.endm

.macro check_sse_up fail
    movss 16(%esp), %xmm0
    addss 20(%esp), %xmm0
    movss %xmm0, 4(%esp)
    cmpl $0x3f800001, 4(%esp)
    jne \fail
.endm

.macro check_nearest fail
    fldl 8(%esp)
    fcos
    fstps 4(%esp)
    cmpl $0x3f800000, 4(%esp)
    jne \fail
.endm

.section .text
.global _start
.type _start, @function
_start:
    and $-16, %esp
    sub $2048, %esp

    movw $0x077f, 0(%esp)
    movl $0, 8(%esp)
    movl $0x3d700000, 12(%esp)
    movl $0x3f800000, 16(%esp)
    movl $0x33800000, 20(%esp)

    /* Reserved PC=01 follows QEMU's default extended-precision behavior. */
    movw $0x017f, 128(%esp)
    fldcw 128(%esp)
    fld1
    fldl 8(%esp)
    faddp
    fstpl 136(%esp)
    cmpl $0x00001000, 136(%esp)
    jne .Lfail_reserved_pc
    cmpl $0x3ff00000, 140(%esp)
    jne .Lfail_reserved_pc

    /* FLDCW must update both softfloat state and the host FCSR. */
    fninit
    fldcw 0(%esp)
    check_down .Lfail_fldcw

    /* FNINIT restores round-to-nearest. */
    fldcw 0(%esp)
    fninit
    check_nearest .Lfail_fninit

    /* FLDENV restores the x87 control word. */
    fldcw 0(%esp)
    fnstenv 32(%esp)
    fninit
    fldenv 32(%esp)
    check_down .Lfail_fldenv

    /* FNSAVE resets the x87 control word; FRSTOR restores it. */
    fldcw 0(%esp)
    fnsave 64(%esp)
    check_nearest .Lfail_fnsave
    frstor 64(%esp)
    check_down .Lfail_frstor

    /* FXRSTOR restores independent x87 and MXCSR rounding controls. */
    fldcw 0(%esp)
    movl $0x5f80, 24(%esp)
    ldmxcsr 24(%esp)
    stmxcsr 28(%esp)
    movl 28(%esp), %eax
    and $0x6000, %eax
    cmp $0x4000, %eax
    jne .Lfail_ldmxcsr
    fxsave 512(%esp)
    movl 536(%esp), %eax
    and $0x6000, %eax
    cmp $0x4000, %eax
    jne .Lfail_fxsave_mxcsr
    fninit
    movl $0x3f80, 24(%esp)
    ldmxcsr 24(%esp)
    fxrstor 512(%esp)
    check_down .Lfail_fxrstor
    stmxcsr 28(%esp)
    movl 28(%esp), %eax
    and $0x6000, %eax
    cmp $0x4000, %eax
    jne .Lfail_fxrstor_saved_mxcsr
    check_sse_up .Lfail_fxrstor_mxcsr
    check_down .Lfail_fxrstor_x87_again

    xor %ebx, %ebx
    jmp .Lexit
.Lfail_fldcw:
    mov $1, %ebx
    jmp .Lexit
.Lfail_fninit:
    mov $2, %ebx
    jmp .Lexit
.Lfail_fldenv:
    mov $3, %ebx
    jmp .Lexit
.Lfail_fnsave:
    mov $4, %ebx
    jmp .Lexit
.Lfail_frstor:
    mov $5, %ebx
    jmp .Lexit
.Lfail_fxrstor:
    mov $6, %ebx
    jmp .Lexit
.Lfail_ldmxcsr:
    mov $7, %ebx
    jmp .Lexit
.Lfail_fxsave_mxcsr:
    mov $8, %ebx
    jmp .Lexit
.Lfail_fxrstor_saved_mxcsr:
    mov $9, %ebx
    jmp .Lexit
.Lfail_fxrstor_mxcsr:
    mov $10, %ebx
    jmp .Lexit
.Lfail_fxrstor_x87_again:
    mov $11, %ebx
    jmp .Lexit
.Lfail_reserved_pc:
    mov $12, %ebx
.Lexit:
    mov $__NR_exit, %eax
    int $0x80
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
