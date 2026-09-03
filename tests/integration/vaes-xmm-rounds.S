.intel_syntax noprefix
.global _start

.macro CHECK_XMM reg, expected
    vmovdqu XMMWORD PTR [rip + actual], \reg
    mov rax, QWORD PTR [rip + actual]
    cmp rax, QWORD PTR [rip + \expected]
    jne .Lcheck_fail\@
    mov rax, QWORD PTR [rip + actual + 8]
    cmp rax, QWORD PTR [rip + \expected + 8]
    je .Lcheck_done\@
.Lcheck_fail\@:
    inc DWORD PTR [rip + fail_count]
.Lcheck_done\@:
.endm

.section .data
fail_count: .long 0

.section .bss
.balign 32
actual: .zero 16

.section .rodata
.balign 32
state0: .byte 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
state1: .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
key:    .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
memkey: .byte 32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
ones:   .fill 16,1,0xff

enc0:     .byte 0x7a,0x7b,0x4e,0x56,0x38,0x78,0x25,0x46,0xa8,0xc0,0x47,0x7a,0x3b,0x81,0x3f,0x43
enc1:     .byte 0x16,0x54,0x85,0x36,0x93,0xb7,0x5b,0x5e,0x97,0x57,0x91,0x61,0x4a,0xf8,0x64,0x20
last0:    .byte 0x73,0x7a,0x75,0x65,0xe6,0x14,0xbd,0x6c,0x28,0xce,0x6d,0xde,0xe2,0x61,0x71,0x34
last1:    .byte 0xda,0x48,0xb0,0xd3,0xee,0xc1,0x64,0x6a,0xb5,0xbd,0xd3,0xeb,0x80,0x9f,0x59,0xb0
enc_mem:  .byte 0x4a,0x4b,0x7e,0x66,0x08,0x48,0x15,0x76,0x98,0xf0,0x77,0x4a,0x0b,0xb1,0x0f,0x73
last_mem: .byte 0x43,0x4a,0x45,0x55,0xd6,0x24,0x8d,0x5c,0x18,0xfe,0x5d,0xee,0xd2,0x51,0x41,0x04
chain:    .byte 0x3f,0x53,0x96,0x57,0xb6,0x67,0xe6,0xff,0x39,0x7c,0xdb,0xb1,0x11,0xd2,0x0e,0xb6
enc_mem_twice:  .byte 0x1b,0xd8,0xdd,0xe0,0x8e,0xbf,0x23,0xeb,0x2c,0xd2,0x11,0xaa,0x3b,0x46,0x65,0x0f
last_mem_twice: .byte 0x3a,0x17,0x6e,0xd1,0xd2,0x9e,0xa5,0xdb,0x85,0xf8,0x44,0x61,0x99,0xfb,0x73,0x07

.section .text
_start:
    vmovdqu xmm1, XMMWORD PTR [rip + state0]
    vmovdqu xmm2, XMMWORD PTR [rip + key]

    vaesenc xmm0, xmm1, xmm2
    CHECK_XMM xmm0, enc0
    vaesenclast xmm0, xmm1, xmm2
    CHECK_XMM xmm0, last0
    vaesenc xmm0, xmm1, XMMWORD PTR [rip + memkey]
    CHECK_XMM xmm0, enc_mem
    vaesenclast xmm0, xmm1, XMMWORD PTR [rip + memkey]
    CHECK_XMM xmm0, last_mem

    lea rsi, [rip + memkey]
    xor edx, edx
    vmovdqu xmm0, XMMWORD PTR [rip + state0]
    vaesenc xmm0, xmm0, XMMWORD PTR [rsi + rdx * 2]
    vaesenc xmm0, xmm0, XMMWORD PTR [rsi + rdx * 2]
    CHECK_XMM xmm0, enc_mem_twice

    lea rsi, [rip + memkey]
    vmovdqu xmm0, XMMWORD PTR [rip + state0]
    vaesenclast xmm0, xmm0, XMMWORD PTR [rsi + rdx * 2]
    vaesenclast xmm0, xmm0, XMMWORD PTR [rsi + rdx * 2]
    CHECK_XMM xmm0, last_mem_twice

    vmovdqu xmm1, XMMWORD PTR [rip + state0]
    vaesenc xmm1, xmm1, xmm2
    CHECK_XMM xmm1, enc0

    vmovdqu xmm1, XMMWORD PTR [rip + state0]
    vmovdqu xmm2, XMMWORD PTR [rip + key]
    vaesenc xmm2, xmm1, xmm2
    CHECK_XMM xmm2, enc0

    vmovdqu xmm0, XMMWORD PTR [rip + state0]
    vmovdqu xmm4, XMMWORD PTR [rip + ones]
    vinsertf128 ymm0, ymm0, xmm4, 1
    vaesenc xmm0, xmm1, xmm2
    vextractf128 xmm3, ymm0, 1
    vptest xmm3, xmm3
    je .Lupper_zero
    inc DWORD PTR [rip + fail_count]
.Lupper_zero:

    vmovdqu xmm0, XMMWORD PTR [rip + state0]
    vmovdqu xmm2, XMMWORD PTR [rip + key]
    .rept 9
    vaesenc xmm0, xmm0, xmm2
    .endr
    vaesenclast xmm0, xmm0, xmm2
    CHECK_XMM xmm0, chain

    vmovdqu xmm0, XMMWORD PTR [rip + state0]
    vmovdqu xmm1, XMMWORD PTR [rip + state1]
    vaesenc xmm0, xmm0, xmm2
    vaesenc xmm1, xmm1, xmm2
    CHECK_XMM xmm0, enc0
    CHECK_XMM xmm1, enc1

    vmovdqu xmm0, XMMWORD PTR [rip + state0]
    vmovdqu xmm1, XMMWORD PTR [rip + state1]
    vaesenclast xmm0, xmm0, xmm2
    vaesenclast xmm1, xmm1, xmm2
    CHECK_XMM xmm0, last0
    CHECK_XMM xmm1, last1

    vmovdqu xmm0, XMMWORD PTR [rip + state0]
    mov ecx, 500000
.Laot_loop:
    vaesenc xmm0, xmm0, xmm2
    dec ecx
    jne .Laot_loop

    vzeroupper
    mov edi, DWORD PTR [rip + fail_count]
    mov eax, 60
    syscall

.section .note.GNU-stack,"",@progbits
