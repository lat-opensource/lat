.global _start
.type _start, @function
.section .text
_start:
    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $message_end-message, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
    ud2
.size _start, .-_start

.section .rodata
message:
    .ascii "Hello, LATC!\n"
message_end:
