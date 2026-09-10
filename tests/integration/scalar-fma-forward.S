.intel_syntax noprefix
.section .text
.global _start

_start:
    vmovaps xmm1, xmmword ptr [rip + .Lmul_lhs]
    vmovaps xmm2, xmmword ptr [rip + .Lthree]
    vmovaps xmm3, xmmword ptr [rip + .Lfour]
    vmulss xmm4, xmm1, xmm2
    vfmadd231ss xmm4, xmm2, xmm3
    vmovaps xmmword ptr [rip + .Lout], xmm4

    vmovaps xmm1, xmmword ptr [rip + .Lsub_lhs]
    vmovaps xmm2, xmmword ptr [rip + .Lthree]
    vmovaps xmm3, xmmword ptr [rip + .Ltwo]
    vmovaps xmm4, xmmword ptr [rip + .Lfive]
    vsubss xmm5, xmm1, xmm2
    vfmsub231ss xmm5, xmm3, xmm4
    vmovaps xmmword ptr [rip + .Lout + 16], xmm5

    vmovaps xmm6, xmmword ptr [rip + .Lnan]
    vmovaps xmm2, xmmword ptr [rip + .Lone]
    vmulss xmm6, xmm6, xmm2
    vfmadd231ss xmm6, xmm2, xmm2
    vmovaps xmmword ptr [rip + .Lout + 32], xmm6

    lea rsi, [rip + .Lout]
    lea rdi, [rip + .Lexpected]
    mov ecx, 12
.Lcheck:
    mov eax, dword ptr [rsi]
    cmp eax, dword ptr [rdi]
    jne .Lfail
    add rsi, 4
    add rdi, 4
    dec ecx
    jne .Lcheck

    xor edi, edi
    jmp .Lexit
.Lfail:
    mov edi, 1
.Lexit:
    mov eax, 60
    syscall

.section .rodata
.p2align 4
.Lmul_lhs: .long 0x40000000, 0x11223344, 0x55667788, 0x99aabbcc
.Lsub_lhs: .long 0x41200000, 0xa1b2c3d4, 0xe5f60718, 0x293a4b5c
.Lnan:     .long 0x7fc12345, 0x76543210, 0xfedcba98, 0x89abcdef
.Lone:     .float 1.0, 0.0, 0.0, 0.0
.Ltwo:     .float 2.0, 0.0, 0.0, 0.0
.Lthree:   .float 3.0, 0.0, 0.0, 0.0
.Lfour:    .float 4.0, 0.0, 0.0, 0.0
.Lfive:    .float 5.0, 0.0, 0.0, 0.0
.Lexpected:
    .long 0x41900000, 0x11223344, 0x55667788, 0x99aabbcc
    .long 0x40400000, 0xa1b2c3d4, 0xe5f60718, 0x293a4b5c
    .long 0x7fc12345, 0x76543210, 0xfedcba98, 0x89abcdef

.section .bss
.p2align 4
.Lout: .skip 48
