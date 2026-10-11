.text
.global _start
.type _start,@function
_start:
    jmp .Loutside
.size _start, .-_start

/* No function symbol owns these targets. */
.Loutside:
    mov $42, %edi
    cmp $42, %edi
    jne .Lfail
    jmp .Lexit
.Lfail:
    mov $1, %edi
.Lexit:
    mov $60, %eax
    syscall
.section .note.GNU-stack,"",@progbits
