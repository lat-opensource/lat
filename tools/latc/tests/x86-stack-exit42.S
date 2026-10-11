.global _start
.type _start, @function
.section .text
_start:
    cmpq $1, (%rsp)
    jne .Lfail
    mov 8(%rsp), %rax
    cmpb $'l', (%rax)
    jne .Lfail
    cmpq $0, 16(%rsp)
    jne .Lfail
    mov $42, %edi
    jmp .Lexit
.Lfail:
    mov $127, %edi
.Lexit:
    mov $60, %eax
    syscall
    ud2
.size _start, .-_start
