.intel_syntax noprefix
.section .text
.global _start

_start:
    vxorps xmm1, xmm1, xmm1
    vmovaps xmm2, xmmword ptr [rip + .Lp1]
    vmovaps xmm3, xmmword ptr [rip + .Lp4]
    vmovaps xmm7, xmmword ptr [rip + .Lones]
    vmovaps xmm8, xmmword ptr [rip + .Lones]

    vmulps xmm9, xmm2, xmmword ptr [rip + .Lones]
    vblendps xmm10, xmm9, xmm1, 8
    vinsertps xmm9, xmm9, xmm9, 0x4c
    vaddps xmm9, xmm10, xmm9

    vmulps xmm10, xmm8, xmmword ptr [rip + .Lp2]
    vblendps xmm11, xmm10, xmm1, 8
    vinsertps xmm10, xmm10, xmm10, 0x4c
    vaddps xmm10, xmm11, xmm10

    vmovaps xmm11, xmmword ptr [rip + .Lones]
    vmulps xmm12, xmm11, xmmword ptr [rip + .Lp3]
    vblendps xmm13, xmm12, xmm1, 8
    vinsertps xmm12, xmm12, xmm12, 0x4c
    vaddps xmm12, xmm13, xmm12

    vmulps xmm7, xmm3, xmm7
    vblendps xmm13, xmm7, xmm1, 8
    vinsertps xmm7, xmm7, xmm7, 0x4c
    vaddps xmm7, xmm13, xmm7

    vmulps xmm8, xmm8, xmmword ptr [rip + .Lp5]
    vblendps xmm13, xmm8, xmm1, 8
    vinsertps xmm8, xmm8, xmm8, 0x4c
    vaddps xmm8, xmm13, xmm8

    vmulps xmm11, xmm11, xmmword ptr [rip + .Lp6]
    vblendps xmm13, xmm11, xmm1, 8
    vinsertps xmm11, xmm11, xmm11, 0x4c
    vaddps xmm11, xmm13, xmm11

    vinsertps xmm13, xmm10, xmm7, 0x1c
    vshufpd xmm10, xmm10, xmm10, 1
    vinsertps xmm7, xmm10, xmm7, 0x9c
    vaddps xmm7, xmm13, xmm7
    vinsertps xmm10, xmm9, xmm8, 0x1c
    vshufpd xmm9, xmm9, xmm9, 1
    vinsertps xmm8, xmm9, xmm8, 0x9c
    vaddps xmm8, xmm10, xmm8
    vaddps xmm7, xmm8, xmm7
    vinsertps xmm8, xmm12, xmm11, 0x1c
    vshufpd xmm9, xmm12, xmm12, 1
    vinsertps xmm9, xmm9, xmm11, 0x9c
    vaddps xmm8, xmm8, xmm9
    vsubps xmm7, xmm7, xmm8

    /* These writes prove that the registers replaced by the pattern are dead. */
    vmovsd xmm12, qword ptr [rip + .Lzero]
    vmovsd xmm10, qword ptr [rip + .Lzero]
    vinsertps xmm8, xmm10, xmm12, 0x4c
    vsqrtss xmm9, xmm1, xmm1

    vmovups xmmword ptr [rip + .Lout], xmm7
    vmovups xmmword ptr [rip + .Lout + 16], xmm11
    vmovups xmmword ptr [rip + .Lout + 32], xmm13

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
.Lones: .float 1.0, 1.0, 1.0, 1.0
.Lp1: .float 1.0, 2.0, 3.0, 4.0
.Lp2: .float 5.0, 6.0, 7.0, 8.0
.Lp3: .float 9.0, 10.0, 11.0, 12.0
.Lp4: .float 13.0, 14.0, 15.0, 16.0
.Lp5: .float 17.0, 18.0, 19.0, 20.0
.Lp6: .float 21.0, 22.0, 23.0, 24.0
.Lzero: .quad 0
.Lexpected:
    .float -6.0, 30.0, 0.0, 0.0
    .float 43.0, 44.0, 23.0, 0.0
    .float 11.0, 27.0, 0.0, 0.0

.section .bss
.p2align 4
.Lout: .skip 48
