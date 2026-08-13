.equ __NR_exit, 60

.section .text
.global _start
.type _start, @function
_start:
    mov $91, %edi
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
