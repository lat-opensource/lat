.intel_syntax noprefix
.global _start

.section .text
_start:
    vpcmpeqd ymm0, ymm0, ymm0
    vmovdqu ymm1, ymmword ptr [rip + .Lmask_dwords]
    vmaskmovps ymm0, ymm1, ymmword ptr [rip + .Lmask_source]
    vmovdqu ymmword ptr [rip + .Lactual], ymm0
    lea rsi, [rip + .Lactual]
    lea rdi, [rip + .Lmask_expected]
    call .Lcompare_32
    test eax, eax
    jne .Lfail_maskmov

    vpcmpeqd ymm0, ymm0, ymm0
    vmovdqu ymm1, ymmword ptr [rip + .Lblend_source1]
    vmovdqu ymm2, ymmword ptr [rip + .Lblend_source2]
    vmovdqu ymm3, ymmword ptr [rip + .Lblend_mask]
    vpblendvb ymm0, ymm1, ymm2, ymm3
    vmovdqu ymmword ptr [rip + .Lactual], ymm0
    lea rsi, [rip + .Lactual]
    lea rdi, [rip + .Lblend_expected]
    call .Lcompare_32
    test eax, eax
    jne .Lfail_blendvb

    vpcmpeqd ymm0, ymm0, ymm0
    vmovdqu ymm1, ymmword ptr [rip + .Lword_source1]
    vmovdqu ymm2, ymmword ptr [rip + .Lword_source2]
    vpblendw ymm0, ymm1, ymm2, 0x5a
    vmovdqu ymmword ptr [rip + .Lactual], ymm0
    lea rsi, [rip + .Lactual]
    lea rdi, [rip + .Lword_expected]
    call .Lcompare_32
    test eax, eax
    jne .Lfail_blendw

    vmovdqu ymm0, ymmword ptr [rip + .Lword_source1]
    vmovdqu ymm2, ymmword ptr [rip + .Lword_source2]
    vpblendw ymm0, ymm0, ymm2, 0x55
    vmovdqu ymmword ptr [rip + .Lactual], ymm0
    lea rsi, [rip + .Lactual]
    lea rdi, [rip + .Lblend55_expected]
    call .Lcompare_32
    test eax, eax
    jne .Lfail_blend55

    vmovdqu ymm1, ymmword ptr [rip + .Lword_source1]
    vmovdqu ymm0, ymmword ptr [rip + .Lword_source2]
    vpblendw ymm0, ymm1, ymm0, 0xaa
    vmovdqu ymmword ptr [rip + .Lactual], ymm0
    lea rsi, [rip + .Lactual]
    lea rdi, [rip + .Lblendaa_expected]
    call .Lcompare_32
    test eax, eax
    jne .Lfail_blendaa

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

.Lfail_maskmov:
    mov edi, 1
    jmp .Lexit
.Lfail_blendvb:
    mov edi, 2
    jmp .Lexit
.Lfail_blendw:
    mov edi, 3
    jmp .Lexit
.Lfail_blend55:
    mov edi, 4
    jmp .Lexit
.Lfail_blendaa:
    mov edi, 5
.Lexit:
    vzeroupper
    mov eax, 60
    syscall

.section .rodata
.p2align 5
.Lmask_source:
    .long 10, 11, 12, 13, 14, 15, 16, 17
.Lmask_dwords:
    .long 0, 0x80000000, 0, 0x80000000
    .long 0x80000000, 0, 0x80000000, 0
.Lmask_expected:
    .long 0, 11, 0, 13, 14, 0, 16, 0

.Lblend_source1:
    .byte 0, 1, 2, 3, 4, 5, 6, 7
    .byte 8, 9, 10, 11, 12, 13, 14, 15
    .byte 16, 17, 18, 19, 20, 21, 22, 23
    .byte 24, 25, 26, 27, 28, 29, 30, 31
.Lblend_source2:
    .byte 128, 129, 130, 131, 132, 133, 134, 135
    .byte 136, 137, 138, 139, 140, 141, 142, 143
    .byte 144, 145, 146, 147, 148, 149, 150, 151
    .byte 152, 153, 154, 155, 156, 157, 158, 159
.Lblend_mask:
    .byte 0, 128, 0, 128, 0, 128, 0, 128
    .byte 0, 128, 0, 128, 0, 128, 0, 128
    .byte 0, 128, 0, 128, 0, 128, 0, 128
    .byte 0, 128, 0, 128, 0, 128, 0, 128
.Lblend_expected:
    .byte 0, 129, 2, 131, 4, 133, 6, 135
    .byte 8, 137, 10, 139, 12, 141, 14, 143
    .byte 16, 145, 18, 147, 20, 149, 22, 151
    .byte 24, 153, 26, 155, 28, 157, 30, 159

.Lword_source1:
    .word 0, 1, 2, 3, 4, 5, 6, 7
    .word 8, 9, 10, 11, 12, 13, 14, 15
.Lword_source2:
    .word 100, 101, 102, 103, 104, 105, 106, 107
    .word 108, 109, 110, 111, 112, 113, 114, 115
.Lword_expected:
    .word 0, 101, 2, 103, 104, 5, 106, 7
    .word 8, 109, 10, 111, 112, 13, 114, 15
.Lblend55_expected:
    .word 100, 1, 102, 3, 104, 5, 106, 7
    .word 108, 9, 110, 11, 112, 13, 114, 15
.Lblendaa_expected:
    .word 0, 101, 2, 103, 4, 105, 6, 107
    .word 8, 109, 10, 111, 12, 113, 14, 115

.section .bss
.p2align 5
.Lactual:
    .skip 32

.section .note.GNU-stack,"",@progbits
