.global _start
.type _start, @function
.section .text
_start:
    sub $256, %rsp
    mov $267, %eax
    mov $-100, %edi
    lea path(%rip), %rsi
    mov %rsp, %rdx
    mov $256, %r10d
    syscall
    test %rax, %rax
    jle failed
    cmp $256, %rax
    jae failed
    cmpb $'/', (%rsp)
    jne failed
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
