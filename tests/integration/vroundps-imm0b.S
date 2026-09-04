.intel_syntax noprefix
.section .text
.global _start

_start:
    lea rsi, [rip + .Linput]
    lea rdi, [rip + .Lexpected]
    mov edx, 3
.Lcase:
    vmovdqu ymm0, ymmword ptr [rsi]
    vroundps ymm1, ymm0, 0xb
    vmovdqu ymmword ptr [rip + .Lout], ymm1
    lea r8, [rip + .Lout]
    mov ecx, 8
.Lcheck:
    mov eax, dword ptr [r8]
    cmp eax, dword ptr [rdi]
    jne .Lfail
    add r8, 4
    add rdi, 4
    dec ecx
    jne .Lcheck
    add rsi, 32
    dec edx
    jne .Lcase

    vmovdqu ymm1, ymmword ptr [rip + .Lall_ones]
    vmovdqu xmm0, xmmword ptr [rip + .Linput + 32]
    vroundps xmm1, xmm0, 0xb
    vmovdqu ymmword ptr [rip + .Lout], ymm1
    lea r8, [rip + .Lout]
    lea rdi, [rip + .Lxmm_expected]
    mov ecx, 8
.Lxmm_check:
    mov eax, dword ptr [r8]
    cmp eax, dword ptr [rdi]
    jne .Lfail
    add r8, 4
    add rdi, 4
    dec ecx
    jne .Lxmm_check

    xor edi, edi
    jmp .Lexit
.Lfail:
    mov edi, 1
.Lexit:
    mov eax, 60
    syscall

.section .rodata
.p2align 5
.Linput:
    .long 0x00000000, 0x80000000, 0x00000001, 0x80000001
    .long 0x3f000000, 0xbf000000, 0x3fc00000, 0xbfc00000
    .long 0x4affffff, 0xcaffffff, 0x4b000000, 0xcb000000
    .long 0x7f800000, 0xff800000, 0x7fc12345, 0x7f812345
    .long 0xff812345, 0x3f800000, 0xbf800000, 0x40200000
    .long 0xc0200000, 0x7f7fffff, 0xff7fffff, 0x00800000
.Lexpected:
    .long 0x00000000, 0x80000000, 0x00000000, 0x80000000
    .long 0x00000000, 0x80000000, 0x3f800000, 0xbf800000
    .long 0x4afffffe, 0xcafffffe, 0x4b000000, 0xcb000000
    .long 0x7f800000, 0xff800000, 0x7fc12345, 0x7fc12345
    .long 0xffc12345, 0x3f800000, 0xbf800000, 0x40000000
    .long 0xc0000000, 0x7f7fffff, 0xff7fffff, 0x00000000
.Lall_ones: .long -1, -1, -1, -1, -1, -1, -1, -1
.Lxmm_expected:
    .long 0x4afffffe, 0xcafffffe, 0x4b000000, 0xcb000000
    .long 0, 0, 0, 0

.section .bss
.p2align 5
.Lout: .skip 32

.section .note.GNU-stack,"",@progbits
