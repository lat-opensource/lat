.global _start
.type _start, @function
.section .text
_start:
    mov function_pointer(%rip), %rax
    call *%rax
    mov %eax, %edi
    mov $60, %eax
    syscall
    ud2
.size _start, .-_start

.type calculate, @function
calculate:
    mov $42, %eax
    ret
.size calculate, .-calculate

.section .data
.align 8
function_pointer:
    .quad calculate
