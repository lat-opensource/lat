.text
.global _start
.type _start,@function
_start:
    test %rax, %rax
    jne fail
    test %rbx, %rbx
    jne fail
    test %rcx, %rcx
    jne fail
    test %rdx, %rdx
    jne fail
    test %rbp, %rbp
    jne fail
    test %rsi, %rsi
    jne fail
    test %rdi, %rdi
    jne fail
    test %r8, %r8
    jne fail
    test %r9, %r9
    jne fail
    test %r10, %r10
    jne fail
    test %r11, %r11
    jne fail
    test %r12, %r12
    jne fail
    test %r13, %r13
    jne fail
    test %r14, %r14
    jne fail
    test %r15, %r15
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
