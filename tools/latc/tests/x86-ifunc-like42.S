.global _start
.type _start, @function
.section .text
_start:
    mov %rsp, %r12
    call resolver
    cmp %r12, %rsp
    jne failed
    call *%rax
    cmp %r12, %rsp
    jne failed
    cmp $42, %eax
    jne failed

    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $message_end-message, %edx
    syscall
    mov $60, %eax
    mov $42, %edi
    syscall

.type resolver, @function
resolver:
    lea implementation(%rip), %rax
    ret
.size resolver, .-resolver

.type implementation, @function
implementation:
    mov $42, %eax
    ret
.size implementation, .-implementation

failed:
    mov $60, %eax
    mov $97, %edi
    syscall
    ud2
.size _start, .-_start

.section .rodata
message:
    .ascii "IFUNC OK\n"
message_end:
