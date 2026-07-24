.equ __NR_exit, 60
.equ __NR_unshare, 272
.equ CLONE_NEWUSER, 0x10000000

.section .text
.global _start
.type _start, @function
_start:
    mov $__NR_unshare, %eax
    mov $CLONE_NEWUSER, %edi
    syscall
    test %rax, %rax
    js unshare_failed
    xor %edi, %edi
    jmp exit

unshare_failed:
    mov $27, %edi

exit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
