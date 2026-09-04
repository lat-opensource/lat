.intel_syntax noprefix
.section .text
.global _start

_start:
    vmovdqu ymm0, YMMWORD PTR [rip + input]
    vmovdqu ymm2, YMMWORD PTR [rip + nonzero]

    vpsrldq ymm1, ymm0, 8
    vpaddq ymm0, ymm0, ymm1
    vextracti128 xmm1, ymm0, 1
    vpaddq xmm0, xmm0, xmm1
    vmovq rax, xmm0
    vzeroupper

    cmp rax, 10
    jne .Lfail
    vextracti128 xmm3, ymm2, 1
    vptest xmm3, xmm3
    jne .Lfail

    xor edi, edi
    jmp .Lexit
.Lfail:
    mov edi, 1
.Lexit:
    mov eax, 60
    syscall

.section .rodata
.balign 32
input:
    .quad 1, 2, 3, 4
nonzero:
    .quad -1, -1, -1, -1

.section .note.GNU-stack,"",@progbits
