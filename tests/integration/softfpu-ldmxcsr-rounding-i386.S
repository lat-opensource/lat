.equ __NR_exit, 1

.section .text
.global _start
.type _start, @function
_start:
    and $-16, %esp
    sub $32, %esp

    movl $0x3f800000, 0(%esp)
    movl $0x33800000, 4(%esp)
    movl $0x5f80, 8(%esp)
    ldmxcsr 8(%esp)

    movss 0(%esp), %xmm0
    addss 4(%esp), %xmm0
    movss %xmm0, 12(%esp)
    cmpl $0x3f800001, 12(%esp)
    jne .Lfail

    xor %ebx, %ebx
    jmp .Lexit
.Lfail:
    mov $1, %ebx
.Lexit:
    mov $__NR_exit, %eax
    int $0x80
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
