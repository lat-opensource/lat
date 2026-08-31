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

    # HBR only models XMM values.  Keep a full YMM value live across the TB
    # boundary created by DIV, then mix XMM and YMM operations in the next TB.
    # The four qwords must survive unchanged on every TU-internal entry.
    vmovdqu ymm10, YMMWORD PTR [rip + live_ymm]
    vpxor ymm11, ymm11, ymm11
    mov r15d, 200000
    mov r14d, 7
.Lmixed_width_loop:
    mov eax, 123
    xor edx, edx
    div r14d
    vmovd xmm0, r9d
    vpxor xmm13, xmm13, xmm13
    vpcmpeqd ymm14, ymm11, ymm11
    vextracti128 xmm15, ymm14, 1
    vmovq rax, xmm10
    vpextrq rcx, xmm10, 1
    vextracti128 xmm0, ymm10, 1
    vmovq rdx, xmm0
    vpextrq rsi, xmm0, 1
    cmp rax, 1
    jne .Lmixed_width_fail
    cmp rcx, 2
    jne .Lmixed_width_fail
    cmp rdx, 3
    jne .Lmixed_width_fail
    cmp rsi, 4
    jne .Lmixed_width_fail
    dec r15d
    jne .Lmixed_width_loop
    jmp .Lmixed_width_done
.Lmixed_width_fail:
    or DWORD PTR [rip + fail_count], 32
.Lmixed_width_done:

    vzeroupper
    mov edi, DWORD PTR [rip + fail_count]
    mov eax, 60
    syscall

.section .rodata
.balign 32
live_ymm:
    .quad 1, 2, 3, 4

.section .note.GNU-stack,"",@progbits
