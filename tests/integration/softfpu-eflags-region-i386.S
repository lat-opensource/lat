.equ __NR_exit, 1

.section .rodata
.align 8
one:
    .double 1.0
two:
    .double 2.0

.section .text
.global _start
.type _start, @function
_start:
    fninit
    fldl two
    fldl one
    cmpl %eax, %eax
    fcomi %st(1), %st
    fstp %st(0)
    jb .Lfcomi_ok
    mov $1, %ebx
    jmp .Lexit

.Lfcomi_ok:
    fstp %st(0)
    fldl two
    fldl one
    cmpl %eax, %eax
    fucomi %st(1), %st
    fstp %st(0)
    jb .Lfucomi_ok
    mov $2, %ebx
    jmp .Lexit

.Lfucomi_ok:
    fstp %st(0)
    fldl two
    fldl one
    cmpl %eax, %eax
    fcomip %st(1), %st
    fstp %st(0)
    jb .Lfcomip_ok
    mov $3, %ebx
    jmp .Lexit

.Lfcomip_ok:
    fldl two
    fldl one
    cmpl %eax, %eax
    fucomip %st(1), %st
    fstp %st(0)
    jb .Lfucomip_ok
    mov $4, %ebx
    jmp .Lexit

.Lfucomip_ok:
    xor %ebx, %ebx

.Lexit:
    mov $__NR_exit, %eax
    int $0x80
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
