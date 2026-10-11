.global _start
.type _start, @function
.section .text
_start:
    mov %rsp, %r12
    lea after_call(%rip), %r11
    .byte 0x67, 0xe8
    .long target - (. + 4)
after_call:
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

.type target, @function
target:
    cmp %r11, (%rsp)
    jne failed
    mov $42, %eax
    ret
.size target, .-target

failed:
    mov $60, %eax
    mov $95, %edi
    syscall
    ud2
.size _start, .-_start

.section .rodata
message:
    .ascii "ADDR32 OK\n"
message_end:
