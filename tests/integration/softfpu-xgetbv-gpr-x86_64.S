.equ __NR_exit, 60

.section .text
.global _start
.type _start, @function
_start:
    movabs $0x1122334455667708, %r8
    movabs $0x1122334455667709, %r9
    movabs $0x1122334455667710, %r10
    movabs $0x1122334455667711, %r11
    movabs $0x1122334455667712, %r12
    movabs $0x1122334455667713, %r13
    movabs $0x1122334455667714, %r14
    movabs $0x1122334455667715, %r15

    xor %ecx, %ecx
    xgetbv

    movabs $0x1122334455667708, %rbx
    cmp %rbx, %r8
    jne .Lfail_r8
    movabs $0x1122334455667709, %rbx
    cmp %rbx, %r9
    jne .Lfail_r9
    movabs $0x1122334455667710, %rbx
    cmp %rbx, %r10
    jne .Lfail_r10
    movabs $0x1122334455667711, %rbx
    cmp %rbx, %r11
    jne .Lfail_r11
    movabs $0x1122334455667712, %rbx
    cmp %rbx, %r12
    jne .Lfail_r12
    movabs $0x1122334455667713, %rbx
    cmp %rbx, %r13
    jne .Lfail_r13
    movabs $0x1122334455667714, %rbx
    cmp %rbx, %r14
    jne .Lfail_r14
    movabs $0x1122334455667715, %rbx
    cmp %rbx, %r15
    jne .Lfail_r15

    xor %edi, %edi
    jmp .Lexit

.Lfail_r8:
    mov $8, %edi
    jmp .Lexit
.Lfail_r9:
    mov $9, %edi
    jmp .Lexit
.Lfail_r10:
    mov $10, %edi
    jmp .Lexit
.Lfail_r11:
    mov $11, %edi
    jmp .Lexit
.Lfail_r12:
    mov $12, %edi
    jmp .Lexit
.Lfail_r13:
    mov $13, %edi
    jmp .Lexit
.Lfail_r14:
    mov $14, %edi
    jmp .Lexit
.Lfail_r15:
    mov $15, %edi

.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
