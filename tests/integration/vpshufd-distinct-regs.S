.intel_syntax noprefix
.section .text
.global _start

_start:
    vmovdqu ymm0, ymmword ptr [rip + .Ljunk]
    vmovdqu xmm1, xmmword ptr [rip + .Lsrc]
    vpshufd xmm0, xmm1, 0x1b
    vmovdqu ymmword ptr [rip + .Lout], ymm0

    mov edi, 1
    lea rsi, [rip + .Lout]
    lea rdx, [rip + .Lexpected_xmm]
    mov ecx, 8
    call .Lcheck

    vmovdqu ymm0, ymmword ptr [rip + .Ljunk]
    vmovdqu ymm2, ymmword ptr [rip + .Lsrc]
    vpshufd ymm0, ymm2, 0x1b
    vmovdqu ymmword ptr [rip + .Lout], ymm0

    mov edi, 2
    lea rsi, [rip + .Lout]
    lea rdx, [rip + .Lexpected_ymm]
    mov ecx, 8
    call .Lcheck

    xor edi, edi
.Lexit:
    mov eax, 60
    syscall

.Lcheck:
    mov eax, dword ptr [rsi]
    cmp eax, dword ptr [rdx]
    jne .Lexit
    add rsi, 4
    add rdx, 4
    dec ecx
    jne .Lcheck
    ret

.section .rodata
.p2align 5
.Lsrc:
    .long 1, 2, 3, 4, 5, 6, 7, 8
.Ljunk:
    .long 101, 102, 103, 104, 105, 106, 107, 108
.Lexpected_xmm:
    .long 4, 3, 2, 1, 0, 0, 0, 0
.Lexpected_ymm:
    .long 4, 3, 2, 1, 8, 7, 6, 5

.section .bss
.p2align 5
.Lout:
    .skip 32

.section .note.GNU-stack,"",@progbits
