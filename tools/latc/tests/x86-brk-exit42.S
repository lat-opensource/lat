.global _start
.type _start, @function
.section .text
_start:
    mov $12, %eax
    xor %edi, %edi
    syscall
    test %rax, %rax
    jz .Lfail
    mov %rax, %rbx
    lea 4096(%rax), %rdi
    mov $12, %eax
    syscall
    cmp %rdi, %rax
    jne .Lfail
    movl $35, (%rbx)
    addl $7, (%rbx)
    mov (%rbx), %edi
    jmp .Lexit
.Lfail:
    mov $127, %edi
.Lexit:
    mov $60, %eax
    syscall
    ud2
.size _start, .-_start
