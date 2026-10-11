.global _start
.type _start, @function
.section .text
_start:
    mov $257, %eax
    mov $-100, %edi
    lea path(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    syscall
    cmp $-2, %rax
    jne .Lfail
    mov $42, %edi
    jmp .Lexit
.Lfail:
    mov $127, %edi
.Lexit:
    mov $60, %eax
    syscall
    ud2
.size _start, .-_start

.section .rodata
path:
    .asciz "/latc-no-such-file"
