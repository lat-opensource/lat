.global _start
.type _start, @function
.section .text
_start:
    call calculate
    mov %eax, %edi
    mov $60, %eax
    syscall
    ud2
.size _start, .-_start

.type calculate, @function
calculate:
    mov $35, %eax
    add $7, %eax
    ret
.size calculate, .-calculate
