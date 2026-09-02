.intel_syntax noprefix
.global _start

.section .rodata
.balign 2
.Lword:
    .word 0x807f
.Lexpected_extended:
    .rept 8
    .word 0x007f, 0xff80
    .endr
.Lexpected_broadcast:
    .rept 8
    .word 0x807f
    .endr
    .zero 16

.section .bss
.balign 32
.Lactual:
    .zero 32

.section .text
_start:
    /* The destination may alias the 128-bit source. */
    vpcmpeqd ymm0, ymm0, ymm0
    vpbroadcastw xmm0, word ptr [rip + .Lword]
    vpmovsxbw ymm0, xmm0
    vmovdqu ymmword ptr [rip + .Lactual], ymm0
    lea rsi, [rip + .Lactual]
    lea rdi, [rip + .Lexpected_extended]
    call .Lcompare_32
    test eax, eax
    jne .Lfail_alias

    /* A distinct destination must not consume the source's upper-zero state. */
    vpcmpeqd ymm1, ymm1, ymm1
    vpbroadcastw xmm1, word ptr [rip + .Lword]
    vpcmpeqd ymm2, ymm2, ymm2
    vxorps xmm2, xmm2, xmm2
    vpmovsxbw ymm2, xmm1
    vmovdqu ymmword ptr [rip + .Lactual], ymm2
    lea rsi, [rip + .Lactual]
    lea rdi, [rip + .Lexpected_extended]
    call .Lcompare_32
    test eax, eax
    jne .Lfail_distinct

    /* Reading YMM1 must observe the VEX.128 zeroed high half. */
    vmovdqu ymmword ptr [rip + .Lactual], ymm1
    lea rsi, [rip + .Lactual]
    lea rdi, [rip + .Lexpected_broadcast]
    call .Lcompare_32
    test eax, eax
    jne .Lfail_source_high

    xor edi, edi
    jmp .Lexit

.Lcompare_32:
    xor eax, eax
    mov ecx, 4
.Lcompare_loop:
    mov r8, qword ptr [rsi]
    cmp r8, qword ptr [rdi]
    jne .Lcompare_fail
    add rsi, 8
    add rdi, 8
    dec ecx
    jne .Lcompare_loop
    ret
.Lcompare_fail:
    mov eax, 1
    ret

.Lfail_alias:
    mov edi, 1
    jmp .Lexit
.Lfail_distinct:
    mov edi, 2
    jmp .Lexit
.Lfail_source_high:
    mov edi, 3
.Lexit:
    mov eax, 60
    syscall

.section .note.GNU-stack,"",@progbits
