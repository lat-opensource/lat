.global _start
.type _start, @function
.section .text
_start:
    mov $9, %eax
    xor %edi, %edi
    mov $4096, %esi
    mov $3, %edx
    mov $0x22, %r10d
    mov $-1, %r8
    xor %r9d, %r9d
    syscall
    cmp $-4095, %rax
    jae .Lfail
    movl $35, (%rax)
    addl $7, (%rax)
    mov (%rax), %edi
    jmp .Lexit
.Lfail:
    mov $127, %edi
.Lexit:
    mov $60, %eax
    syscall
    ud2
.size _start, .-_start
