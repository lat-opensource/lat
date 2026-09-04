.intel_syntax noprefix
.global _start

.text
_start:
    mov eax, dword ptr [rip + .Lvalue0]
    cmp eax, 0x13579bdf
    jne .Lfail

    mov rax, qword ptr [rip + .Lvalue1]
    mov rdx, 0x0123456789abcdef
    cmp rax, rdx
    jne .Lfail

    vmovdqu xmm0, xmmword ptr [rip + .Lvector]
    vpxor xmm0, xmm0, xmmword ptr [rip + .Lexpected]
    vptest xmm0, xmm0
    jne .Lfail

    mov eax, 60
    xor edi, edi
    syscall

.Lfail:
    mov eax, 60
    mov edi, 1
    syscall

.section .rodata
.p2align 4
.Lvalue0:
    .long 0x13579bdf
.Lvalue1:
    .quad 0x0123456789abcdef
.Lvector:
    .quad 0x0011223344556677, 0x8899aabbccddeeff
.Lexpected:
    .quad 0x0011223344556677, 0x8899aabbccddeeff

.section .note.GNU-stack,"",@progbits
