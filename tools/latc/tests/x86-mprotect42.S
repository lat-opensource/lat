.text
.global _start
.type _start,@function
_start:
    xor %edi, %edi
    mov $16384, %esi
    mov $3, %edx
    mov $0x22, %r10d
    mov $-1, %r8
    xor %r9d, %r9d
    mov $9, %eax
    syscall
    test %rax, %rax
    js fail

    mov %rax, %r12
    movq $35, (%r12)
    mov %r12, %rdi
    mov $16384, %esi
    mov $1, %edx
    mov $10, %eax
    syscall
    test %rax, %rax
    jne fail

    mov (%r12), %r13
    add $7, %r13
    mov %r12, %rdi
    mov $16384, %esi
    mov $3, %edx
    mov $10, %eax
    syscall
    test %rax, %rax
    jne fail

    mov %r13d, %edi
    jmp exit
fail:
    mov $1, %edi
exit:
    mov $60, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
