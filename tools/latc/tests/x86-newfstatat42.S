.global _start
.type _start, @function
.section .text
_start:
    sub $256, %rsp
    mov $262, %eax
    mov $-100, %edi
    lea path(%rip), %rsi
    mov %rsp, %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    jne failed
    cmpq $0, 48(%rsp)
    jle failed
    add $256, %rsp
    mov $60, %eax
    mov $42, %edi
    syscall
failed:
    mov $60, %eax
    mov $91, %edi
    syscall
    ud2
.size _start, .-_start

.section .rodata
path:
    .asciz "/proc/self/exe"
