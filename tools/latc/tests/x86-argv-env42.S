.text
.global _start
.type _start,@function
_start:
    cmpq $3, (%rsp)
    jne fail
    mov 16(%rsp), %rsi
    cmpl $0x68706c61, (%rsi)
    jne fail
    cmpb $'a', 4(%rsi)
    jne fail
    cmpb $0, 5(%rsi)
    jne fail
    mov 24(%rsp), %rsi
    cmpl $0x61746562, (%rsi)
    jne fail
    cmpb $0, 4(%rsi)
    jne fail
    cmpq $0, 32(%rsp)
    jne fail

    lea 40(%rsp), %r12
scan_env:
    mov (%r12), %rsi
    test %rsi, %rsi
    jz fail
    movabs $0x5345545f4354414c, %rax
    cmp %rax, (%rsi)
    jne next_env
    movabs $0x6f773d564e455f54, %rax
    cmp %rax, 8(%rsi)
    jne next_env
    cmpl $0x00736b72, 16(%rsi)
    jne next_env
    mov $42, %edi
    jmp exit
next_env:
    add $8, %r12
    jmp scan_env
fail:
    mov $1, %edi
exit:
    mov $60, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
