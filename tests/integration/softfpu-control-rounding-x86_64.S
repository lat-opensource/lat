.equ __NR_exit, 60

.macro check_down fail
    fldl 8(%rsp)
    fcos
    fstps 4(%rsp)
    cmpl $0x3f7fffff, 4(%rsp)
    jne \fail
.endm

.macro check_sse_up fail
    movss 16(%rsp), %xmm0
    addss 20(%rsp), %xmm0
    movss %xmm0, 4(%rsp)
    cmpl $0x3f800001, 4(%rsp)
    jne \fail
.endm

.section .text
.global _start
.type _start, @function
_start:
    and $-64, %rsp
    sub $2048, %rsp

    movw $0x077f, 0(%rsp)
    movl $0, 8(%rsp)
    movl $0x3d700000, 12(%rsp)
    movl $0x3f800000, 16(%rsp)
    movl $0x33800000, 20(%rsp)

    /* Reserved PC=01 follows QEMU's default extended-precision behavior. */
    movw $0x017f, 128(%rsp)
    fldcw 128(%rsp)
    fld1
    fldl 8(%rsp)
    faddp
    fstpl 136(%rsp)
    cmpl $0x00001000, 136(%rsp)
    jne .Lfail_reserved_pc
    cmpl $0x3ff00000, 140(%rsp)
    jne .Lfail_reserved_pc

    fldcw 0(%rsp)
    movl $0x5f80, 24(%rsp)
    ldmxcsr 24(%rsp)
    fxsave64 512(%rsp)
    movl 536(%rsp), %eax
    and $0x6000, %eax
    cmp $0x4000, %eax
    jne .Lfail_fxsave_mxcsr

    fninit
    movl $0x3f80, 24(%rsp)
    ldmxcsr 24(%rsp)
    fxrstor64 512(%rsp)
    check_down .Lfail_fxrstor
    check_sse_up .Lfail_fxrstor_mxcsr

    xor %edi, %edi
    jmp .Lexit
.Lfail_fxsave_mxcsr:
    mov $1, %edi
    jmp .Lexit
.Lfail_fxrstor:
    mov $2, %edi
    jmp .Lexit
.Lfail_fxrstor_mxcsr:
    mov $3, %edi
    jmp .Lexit
.Lfail_reserved_pc:
    mov $4, %edi
.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
