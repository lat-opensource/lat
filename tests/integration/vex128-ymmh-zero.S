.intel_syntax noprefix
.global _start

.macro CHECK_YMM_HIGH_ZERO reg, bit
    vmovdqu YMMWORD PTR [rip + actual], \reg
    mov rax, QWORD PTR [rip + actual + 16]
    or rax, QWORD PTR [rip + actual + 24]
    jz .Lcheck_done\@
    or DWORD PTR [rip + fail_count], \bit
.Lcheck_done\@:
.endm

.section .data
fail_count: .long 0

.section .bss
.balign 32
actual: .zero 32

.section .text
_start:
    vpcmpeqd ymm0, ymm0, ymm0
    vxorps xmm1, xmm1, xmm1
    vaddps xmm0, xmm1, xmm1
    CHECK_YMM_HIGH_ZERO ymm0, 1

    vpcmpeqd ymm0, ymm0, ymm0
    vaddps xmm0, xmm1, xmm1
    jmp .Ldirect_exit
.Ldirect_exit:
    CHECK_YMM_HIGH_ZERO ymm0, 2

    vpcmpeqd ymm0, ymm0, ymm0
    vaddps xmm0, xmm1, xmm1
    xor ecx, ecx
    test ecx, ecx
    jnz .Ltaken_fail
    CHECK_YMM_HIGH_ZERO ymm0, 4
    jmp .Lconditional_taken
.Ltaken_fail:
    inc DWORD PTR [rip + fail_count]

.Lconditional_taken:
    vpcmpeqd ymm0, ymm0, ymm0
    vaddps xmm0, xmm1, xmm1
    mov ecx, 1
    test ecx, ecx
    jnz .Ltaken_ok
    inc DWORD PTR [rip + fail_count]
.Ltaken_ok:
    CHECK_YMM_HIGH_ZERO ymm0, 8

    vpcmpeqd ymm0, ymm0, ymm0
    mov ecx, 500000
.Laot_loop:
    vaddps xmm0, xmm0, xmm1
    vshufps xmm0, xmm0, xmm0, 0xc9
    dec ecx
    jne .Laot_loop
    CHECK_YMM_HIGH_ZERO ymm0, 16

    vzeroupper
    mov edi, DWORD PTR [rip + fail_count]
    mov eax, 60
    syscall

.section .note.GNU-stack,"",@progbits
