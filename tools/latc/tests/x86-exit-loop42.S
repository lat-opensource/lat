.global _start
.type _start, @function
.section .text
_start:
    xor %edi, %edi
    mov $6, %ecx
.Lloop:
    add $7, %edi
    dec %ecx
    jne .Lloop
    mov $60, %eax
    syscall
    ud2
.size _start, .-_start
