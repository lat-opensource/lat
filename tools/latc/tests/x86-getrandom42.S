.global _start
.type _start, @function
.section .text
_start:
    sub $16, %rsp
    mov $318, %eax
    mov %rsp, %rdi
    mov $16, %esi
    xor %edx, %edx
    syscall
    cmp $16, %rax
    jne failed
    mov (%rsp), %rax
    or 8(%rsp), %rax
    jz failed
    add $16, %rsp
    mov $60, %eax
    mov $42, %edi
    syscall
failed:
    mov $60, %eax
    mov $92, %edi
    syscall
    ud2
.size _start, .-_start
