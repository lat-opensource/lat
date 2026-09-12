.equ __NR_exit, 1

.section .text
.global _start
.type _start, @function
_start:
    and $-16, %esp
    sub $1024, %esp

    movl $0x5f80, 0(%esp)
    ldmxcsr 0(%esp)
    movl $0, 536(%esp)
    fxsave 512(%esp)

    movl 536(%esp), %eax
    and $0x6000, %eax
    cmp $0x4000, %eax
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
