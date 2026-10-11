.global _start
.type _start, @function
.section .text
_start:
    mov $60, %eax
    mov $42, %edi
    syscall
    ud2
.size _start, .-_start
