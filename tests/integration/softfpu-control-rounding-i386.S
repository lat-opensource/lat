.equ __NR_exit, 1

.macro check_down fail
    fldl 8(%esp)
    fcos
    fstps 4(%esp)
    cmpl $0x3f7fffff, 4(%esp)
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
    sub $1024, %esp

    movw $0x077f, 0(%esp)
    movl $0, 8(%esp)
    movl $0x3d700000, 12(%esp)

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

    /* FXRSTOR also restores the x87 control word. */
    fldcw 0(%esp)
    fxsave 256(%esp)
    fninit
    fxrstor 256(%esp)
    check_down .Lfail_fxrstor

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
.Lexit:
    mov $__NR_exit, %eax
    int $0x80
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
