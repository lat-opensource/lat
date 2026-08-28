.intel_syntax noprefix
.global _start

.section .rodata
.p2align 5
.Lsrc1:
    .long 1, 2, 3, 4
    .long 9, 10, 11, 12
.Lsrc2:
    .long 5, 6, 7, 8
.Lqwords:
    .quad 0x1111222233334444, 0x5555666677778888

.section .bss
.p2align 5
.Lout:
    .zero 32

.text
_start:
    mov edi, 1
    vmovaps ymm3, ymmword ptr [rip + .Lsrc1]
    vmovaps xmm1, xmmword ptr [rip + .Lsrc1]
    vmovaps xmm2, xmmword ptr [rip + .Lsrc2]
    vinsertps xmm3, xmm1, xmm2, 0x1c
    vmovups ymmword ptr [rip + .Lout], ymm3
    cmp dword ptr [rip + .Lout], 1
    jne .Lfail
    cmp dword ptr [rip + .Lout + 4], 5
    jne .Lfail
    cmp qword ptr [rip + .Lout + 8], 0
    jne .Lfail
    cmp qword ptr [rip + .Lout + 16], 0
    jne .Lfail
    cmp qword ptr [rip + .Lout + 24], 0
    jne .Lfail

    mov edi, 2
    vmovaps ymm3, ymmword ptr [rip + .Lsrc1]
    vinsertps xmm3, xmm1, xmm1, 0x4c
    vmovups ymmword ptr [rip + .Lout], ymm3
    cmp dword ptr [rip + .Lout], 2
    jne .Lfail
    cmp dword ptr [rip + .Lout + 4], 2
    jne .Lfail
    cmp qword ptr [rip + .Lout + 8], 0
    jne .Lfail
    cmp qword ptr [rip + .Lout + 16], 0
    jne .Lfail
    cmp qword ptr [rip + .Lout + 24], 0
    jne .Lfail

    mov edi, 3
    vmovaps ymm3, ymmword ptr [rip + .Lsrc1]
    movaps xmm3, xmmword ptr [rip + .Lsrc2]
    vinsertps xmm3, xmm1, xmm3, 0x9c
    vmovups ymmword ptr [rip + .Lout], ymm3
    cmp dword ptr [rip + .Lout], 1
    jne .Lfail
    cmp dword ptr [rip + .Lout + 4], 7
    jne .Lfail
    cmp qword ptr [rip + .Lout + 8], 0
    jne .Lfail
    cmp qword ptr [rip + .Lout + 16], 0
    jne .Lfail
    cmp qword ptr [rip + .Lout + 24], 0
    jne .Lfail

    mov edi, 4
    vmovaps ymm3, ymmword ptr [rip + .Lsrc1]
    vmovapd xmm1, xmmword ptr [rip + .Lqwords]
    vshufpd xmm3, xmm1, xmm1, 1
    vmovups ymmword ptr [rip + .Lout], ymm3
    mov rax, qword ptr [rip + .Lqwords + 8]
    cmp qword ptr [rip + .Lout], rax
    jne .Lfail
    mov rax, qword ptr [rip + .Lqwords]
    cmp qword ptr [rip + .Lout + 8], rax
    jne .Lfail
    cmp qword ptr [rip + .Lout + 16], 0
    jne .Lfail
    cmp qword ptr [rip + .Lout + 24], 0
    jne .Lfail

    mov eax, 60
    xor edi, edi
    syscall
.Lfail:
    mov eax, 60
    syscall

.section .note.GNU-stack,"",@progbits
