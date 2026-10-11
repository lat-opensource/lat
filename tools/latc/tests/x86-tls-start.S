.global _start
.type _start, @function
.section .text
_start:
    mov %rsp, %rdi
    and $-16, %rsp
    call guest_main
    mov %eax, %edi
    mov $60, %eax
    syscall
    ud2
.size _start, .-_start
