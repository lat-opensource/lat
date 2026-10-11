.global _start
.type _start, @function
.section .text
_start:
    sub $16, %rsp
    mov $302, %eax
    xor %edi, %edi
    mov $3, %esi
    xor %edx, %edx
    mov %rsp, %r10
    syscall
    test %rax, %rax
    jne failed
    cmpq $0, (%rsp)
    je failed
    add $16, %rsp
    mov $60, %eax
    mov $42, %edi
    syscall
failed:
    mov $60, %eax
    mov $93, %edi
    syscall
    ud2
.size _start, .-_start
