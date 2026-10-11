.global _start
.type _start, @function
.section .text
_start:
    mov (%rsp), %r10
    and $0, %r10
    lea level_one(%rip), %rax
    add %r10, %rax
    lea after_level_one(%rip), %r11
    call *%rax
after_level_one:
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

.type level_one, @function
level_one:
    cmp %r11, (%rsp)
    jne failed
    push %rbx
    mov $0x12345678, %ebx
    mov (%rsp), %r10
    and $0, %r10
    lea level_two(%rip), %rax
    add %r10, %rax
    lea after_level_two(%rip), %r11
    call *%rax
after_level_two:
    cmp $42, %eax
    jne failed
    cmp $0x12345678, %ebx
    jne failed
    pop %rbx
    ret
.size level_one, .-level_one

.type level_two, @function
level_two:
    cmp %r11, (%rsp)
    jne failed
    mov $42, %eax
    ret
.size level_two, .-level_two

failed:
    mov $60, %eax
    mov $99, %edi
    syscall
    ud2
.size _start, .-_start

.section .rodata
message:
    .ascii "INDIRECT OK\n"
message_end:
