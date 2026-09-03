.intel_syntax noprefix
.global _start

.macro CHECK_YMM reg, expected
    vmovdqu YMMWORD PTR [rip + actual], \reg
    lea rsi, [rip + actual]
    lea rdi, [rip + \expected]
    mov ecx, 4
    cld
    repe cmpsq
    je .Lcheck_done\@
    inc DWORD PTR [rip + fail_count]
.Lcheck_done\@:
.endm

.section .data
fail_count: .long 0

.section .bss
.balign 32
actual: .zero 32

.section .rodata
.balign 32
state: .byte 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
       .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
key:   .byte 32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
       .byte 48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
expected_enc:
       .byte 0x4a,0x4b,0x7e,0x66,0x08,0x48,0x15,0x76,0x98,0xf0,0x77,0x4a,0x0b,0xb1,0x0f,0x73
       .byte 0x36,0x74,0xa5,0x16,0xb3,0x97,0x7b,0x7e,0xb7,0x77,0xb1,0x41,0x6a,0xd8,0x44,0x00
expected_last:
       .byte 0x43,0x4a,0x45,0x55,0xd6,0x24,0x8d,0x5c,0x18,0xfe,0x5d,0xee,0xd2,0x51,0x41,0x04
       .byte 0xfa,0x68,0x90,0xf3,0xce,0xe1,0x44,0x4a,0x95,0x9d,0xf3,0xcb,0xa0,0xbf,0x79,0x90
expected_enc_twice:
       .byte 0x1b,0xd8,0xdd,0xe0,0x8e,0xbf,0x23,0xeb,0x2c,0xd2,0x11,0xaa,0x3b,0x46,0x65,0x0f
       .byte 0x12,0x1f,0x91,0xba,0xb6,0xc3,0x51,0xe0,0x27,0xab,0xf0,0x41,0x37,0xe0,0x72,0x97
expected_last_twice:
       .byte 0x3a,0x17,0x6e,0xd1,0xd2,0x9e,0xa5,0xdb,0x85,0xf8,0x44,0x61,0x99,0xfb,0x73,0x07
       .byte 0x1d,0xc9,0x3f,0x53,0xbf,0x6b,0x80,0x3a,0x12,0x31,0x5a,0xed,0xdc,0x78,0x25,0x20

.section .text
_start:
    mov r10d, 500000
.Lx8_loop:
    vmovdqu ymm0, YMMWORD PTR [rip + state]
    vmovdqu ymm1, YMMWORD PTR [rip + state]
    vmovdqu ymm2, YMMWORD PTR [rip + state]
    vmovdqu ymm3, YMMWORD PTR [rip + state]
    vmovdqu ymm4, YMMWORD PTR [rip + state]
    vmovdqu ymm5, YMMWORD PTR [rip + state]
    vmovdqu ymm6, YMMWORD PTR [rip + state]
    vmovdqu ymm7, YMMWORD PTR [rip + state]
    vmovdqu ymm8, YMMWORD PTR [rip + key]
    vmovdqu ymm15, YMMWORD PTR [rip + key]
    vaesenc ymm0, ymm0, ymm8
    vaesenc ymm1, ymm1, ymm8
    vaesenc ymm2, ymm2, ymm8
    vaesenc ymm3, ymm3, ymm8
    vaesenc ymm4, ymm4, ymm8
    vaesenc ymm5, ymm5, ymm8
    vaesenc ymm6, ymm6, ymm8
    vaesenc ymm7, ymm7, ymm8
    dec r10d
    jne .Lx8_loop
    CHECK_YMM ymm0, expected_enc
    CHECK_YMM ymm1, expected_enc
    CHECK_YMM ymm2, expected_enc
    CHECK_YMM ymm3, expected_enc
    CHECK_YMM ymm4, expected_enc
    CHECK_YMM ymm5, expected_enc
    CHECK_YMM ymm6, expected_enc
    CHECK_YMM ymm7, expected_enc
    CHECK_YMM ymm15, key

    vmovdqu ymm1, YMMWORD PTR [rip + state]
    vmovdqu ymm2, YMMWORD PTR [rip + key]
    vaesenc ymm0, ymm1, ymm2
    CHECK_YMM ymm0, expected_enc

    vmovdqu ymm1, YMMWORD PTR [rip + state]
    vaesenc ymm1, ymm1, ymm2
    CHECK_YMM ymm1, expected_enc

    vmovdqu ymm1, YMMWORD PTR [rip + state]
    vaesenc ymm0, ymm1, YMMWORD PTR [rip + key]
    CHECK_YMM ymm0, expected_enc

    vmovdqu ymm1, YMMWORD PTR [rip + state]
    vaesenclast ymm0, ymm1, ymm2
    CHECK_YMM ymm0, expected_last

    vmovdqu ymm1, YMMWORD PTR [rip + state]
    vaesenclast ymm1, ymm1, ymm2
    CHECK_YMM ymm1, expected_last

    vmovdqu ymm1, YMMWORD PTR [rip + state]
    vaesenclast ymm0, ymm1, YMMWORD PTR [rip + key]
    CHECK_YMM ymm0, expected_last

    lea rsi, [rip + key]
    xor edx, edx
    vmovdqu ymm0, YMMWORD PTR [rip + state]
    vaesenc ymm0, ymm0, YMMWORD PTR [rsi + rdx * 2]
    vaesenc ymm0, ymm0, YMMWORD PTR [rsi + rdx * 2]
    CHECK_YMM ymm0, expected_enc_twice

    lea rsi, [rip + key]
    vmovdqu ymm0, YMMWORD PTR [rip + state]
    vaesenclast ymm0, ymm0, YMMWORD PTR [rsi + rdx * 2]
    vaesenclast ymm0, ymm0, YMMWORD PTR [rsi + rdx * 2]
    CHECK_YMM ymm0, expected_last_twice

    vmovdqu ymm1, YMMWORD PTR [rip + state]
    vmovdqu ymm2, YMMWORD PTR [rip + key]
    vaesenc ymm2, ymm1, ymm2
    CHECK_YMM ymm2, expected_enc

    vzeroupper
    mov edi, DWORD PTR [rip + fail_count]
    mov eax, 60
    syscall

.section .note.GNU-stack,"",@progbits
