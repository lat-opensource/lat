.equ __NR_exit, 1
.equ __NR_write, 4

.section .text
.global _start

_start:
    mov %esp, %ebp
    mov (%ebp), %ecx
    lea 8(%ebp,%ecx,4), %esi

next_env:
    mov (%esi), %edx
    test %edx, %edx
    jz not_found

    mov $prefix, %edi
    mov %edx, %ebx
compare:
    movb (%edi), %al
    test %al, %al
    jz found
    cmpb %al, (%ebx)
    jne skip_env
    inc %edi
    inc %ebx
    jmp compare

skip_env:
    add $4, %esi
    jmp next_env

found:
    mov %ebx, %ecx
    xor %edx, %edx
measure:
    cmpb $0, (%ecx,%edx)
    je print
    inc %edx
    jmp measure

print:
    mov $__NR_write, %eax
    mov $1, %ebx
    int $0x80

    mov $__NR_write, %eax
    mov $1, %ebx
    mov $newline, %ecx
    mov $1, %edx
    int $0x80

    xor %ebx, %ebx
    mov $__NR_exit, %eax
    int $0x80

not_found:
    mov $1, %ebx
    mov $__NR_exit, %eax
    int $0x80

.section .rodata
prefix:
    .asciz "LD_LIBRARY_PATH="
newline:
    .byte 10
