.text
.global _start
.type _start,@function
_start:
    xor %eax, %eax
    .rept 300
    inc %eax
    .endr
    cmp $300, %eax
    jne fail
    mov $42, %edi
    jmp exit
fail:
    mov $1, %edi
exit:
    mov $60, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
