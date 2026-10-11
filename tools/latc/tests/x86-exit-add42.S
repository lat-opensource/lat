.global _start
.type _start, @function
.section .text
_start:
    mov $35, %edi
    add $7, %edi
    mov $60, %eax
    syscall
    ud2
.size _start, .-_start
