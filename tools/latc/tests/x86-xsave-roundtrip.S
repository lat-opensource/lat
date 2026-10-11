.intel_syntax noprefix
#ifndef XSAVE_MASK
#define XSAVE_MASK 7
#endif
.global _start
.section .text
_start:
    xor ecx, ecx
    mov ebx, 11
    xgetbv
    and eax, XSAVE_MASK
    cmp eax, XSAVE_MASK
    jne fail
    vmovdqu ymm0, YMMWORD PTR [rip + expected]
    mov eax, XSAVE_MASK
    xor edx, edx
    xsave [rip + saved]
    vpxor ymm0, ymm0, ymm0
    mov eax, XSAVE_MASK
    xor edx, edx
    xrstor [rip + saved]
    mov ebx, 12
    call check
    vmovdqu ymm0, YMMWORD PTR [rip + expected]
    mov eax, XSAVE_MASK
    xor edx, edx
    xsaveopt [rip + saved]
    vpxor ymm0, ymm0, ymm0
    mov eax, XSAVE_MASK
    xor edx, edx
    xrstor [rip + saved]
    mov ebx, 13
    call check
    vzeroupper
    xor edi, edi
    mov eax, 60
    syscall
check:
    vmovdqu YMMWORD PTR [rip + actual], ymm0
    lea rsi, [rip + expected]
    lea rdi, [rip + actual]
    mov ecx, 2
#if XSAVE_MASK == 7
    mov ecx, 4
#endif
    repe cmpsq
    jne fail
    ret
fail:
    mov edi, ebx
    mov eax, 60
    syscall
.section .rodata
.balign 32
expected:
    .quad 0x0102030405060708, 0x1112131415161718
    .quad 0x2122232425262728, 0x3132333435363738
.section .bss
.balign 64
saved: .zero 4096
.balign 32
actual: .zero 32
.section .note.GNU-stack,"",@progbits
