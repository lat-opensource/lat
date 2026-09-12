.equ __NR_exit, 1

.macro check_down fail
    fldl 8(%esp)
    fcos
    fstps 4(%esp)
    cmpl $0x3f7fffff, 4(%esp)
    jne \fail
.endm

.macro check_sse_nearest fail
    movss 16(%esp), %xmm0
    addss 20(%esp), %xmm0
    movss %xmm0, 4(%esp)
    cmpl $0x3f800000, 4(%esp)
    jne \fail
.endm

.macro check_sse_up fail
    movss 16(%esp), %xmm0
    addss 20(%esp), %xmm0
    movss %xmm0, 4(%esp)
    cmpl $0x3f800001, 4(%esp)
    jne \fail
.endm

.section .text
.global _start
.type _start, @function
_start:
    and $-64, %esp
    sub $2048, %esp

    movw $0x077f, 0(%esp)
    movl $0, 8(%esp)
    movl $0x3d700000, 12(%esp)
    movl $0x3f800000, 16(%esp)
    movl $0x33800000, 20(%esp)

    xor %ecx, %ecx
    xgetbv
    and $3, %eax
    cmp $3, %eax
    jne .Lfail_xcr0

    fldcw 0(%esp)
    movl $0x5f80, 24(%esp)
    ldmxcsr 24(%esp)
    xor %ecx, %ecx
    xgetbv
    xsave 512(%esp)

    movl 536(%esp), %ecx
    and $0x6000, %ecx
    cmp $0x4000, %ecx
    jne .Lfail_xsave_mxcsr

    fninit
    movl $0x1f80, 24(%esp)
    ldmxcsr 24(%esp)
    mov $1, %eax
    xor %edx, %edx
    xrstor 512(%esp)
    /* Check SSE before an x87 exit can repair a stale host FCSR. */
    check_sse_nearest .Lfail_xrstor_mask
    check_down .Lfail_xrstor_fp

    mov $3, %eax
    xor %edx, %edx
    xrstor 512(%esp)
    check_sse_up .Lfail_xrstor_both_mxcsr
    check_down .Lfail_xrstor_both_x87

    xor %ebx, %ebx
    jmp .Lexit
.Lfail_xcr0:
    mov $1, %ebx
    jmp .Lexit
.Lfail_xsave_mxcsr:
    mov $2, %ebx
    jmp .Lexit
.Lfail_xrstor_fp:
    mov $3, %ebx
    jmp .Lexit
.Lfail_xrstor_mask:
    mov $4, %ebx
    jmp .Lexit
.Lfail_xrstor_both_x87:
    mov $5, %ebx
    jmp .Lexit
.Lfail_xrstor_both_mxcsr:
    mov $6, %ebx
.Lexit:
    mov $__NR_exit, %eax
    int $0x80
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
