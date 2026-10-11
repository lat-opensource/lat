.global _start
.type _start, @function
.section .text
_start:
    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $3, %edx
    syscall
    mov $60, %eax
    mov $42, %edi
    syscall
    ud2
.size _start, .-_start

.section .rodata
message:
    .ascii "OK\n"
