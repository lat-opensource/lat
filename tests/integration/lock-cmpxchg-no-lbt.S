.equ __NR_mmap, 9
.equ __NR_exit, 60

.equ PROT_READ, 0x1
.equ PROT_WRITE, 0x2
.equ MAP_SHARED, 0x1
.equ MAP_ANONYMOUS, 0x20

.section .text
.global _start
.type _start, @function
_start:
    xor %edi, %edi
    mov $4096, %esi
    mov $(PROT_READ | PROT_WRITE), %edx
    mov $(MAP_SHARED | MAP_ANONYMOUS), %r10d
    mov $-1, %r8
    xor %r9d, %r9d
    mov $__NR_mmap, %eax
    syscall
    test %rax, %rax
    js .Lfail

    mov %rax, %rsi
    movl $1, (%rsi)

    mov $1, %eax
    mov $2, %ecx
    lock cmpxchgl %ecx, (%rsi)
    jne .Lfail
    cmpl $2, (%rsi)
    jne .Lfail

    mov $1, %eax
    mov $3, %ecx
    lock cmpxchgl %ecx, (%rsi)
    je .Lfail
    cmp $2, %eax
    jne .Lfail
    cmpl $2, (%rsi)
    jne .Lfail

    movq $1, (%rsi)
    movq $2, 8(%rsi)
    mov $1, %eax
    mov $2, %edx
    mov $3, %ebx
    mov $4, %ecx
    lock cmpxchg16b (%rsi)
    jne .Lfail
    cmpq $3, (%rsi)
    jne .Lfail
    cmpq $4, 8(%rsi)
    jne .Lfail

    mov $1, %eax
    mov $2, %edx
    mov $5, %ebx
    mov $6, %ecx
    lock cmpxchg16b (%rsi)
    je .Lfail
    cmp $3, %rax
    jne .Lfail
    cmp $4, %rdx
    jne .Lfail

    xor %edi, %edi
    jmp .Lexit

.Lfail:
    mov $1, %edi

.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start
