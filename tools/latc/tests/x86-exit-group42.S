.text
.global _start
.type _start,@function
_start:
    mov $42, %edi
    mov $231, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
