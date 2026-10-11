.text
.global _start
.type _start,@function
_start:
    sub $160, %rsp
    mov $1, %edi
    mov %rsp, %rsi
    mov $5, %eax
    syscall
    test %rax, %rax
    jne fail
    cmpl $0, 24(%rsp)
    je fail
    cmpq $0, 56(%rsp)
    jle fail
    mov $42, %edi
    jmp exit
fail:
    mov $1, %edi
exit:
    add $160, %rsp
    mov $60, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
