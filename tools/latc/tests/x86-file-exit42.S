.global _start
.type _start, @function
.section .text
_start:
    mov $257, %eax
    mov $-100, %edi
    lea path(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    syscall
    test %eax, %eax
    js .Lfail
    mov %eax, %ebx

    xor %eax, %eax
    mov %ebx, %edi
    lea byte(%rip), %rsi
    mov $1, %edx
    syscall
    cmp $1, %eax
    jne .Lfail_close
    cmpb $0, byte(%rip)
    jne .Lfail_close

    mov $3, %eax
    mov %ebx, %edi
    syscall
    test %eax, %eax
    jne .Lfail
    mov $42, %edi
    jmp .Lexit

.Lfail_close:
    mov $3, %eax
    mov %ebx, %edi
    syscall
.Lfail:
    mov $127, %edi
.Lexit:
    mov $60, %eax
    syscall
    ud2
.size _start, .-_start

.section .rodata
path:
    .asciz "/dev/zero"

.section .bss
byte:
    .skip 1
