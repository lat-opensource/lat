.intel_syntax noprefix
.section .text
.global _start

_start:
    mov eax, 1
    cpuid
    bt ecx, 29
    jnc .Lfail_f16c

    mov eax, 0x80000001
    cpuid
    bt ecx, 5
    jnc .Lfail_lzcnt

    xor eax, eax
    lzcnt eax, eax
    jnc .Lfail_lzcnt_flags
    jz .Lfail_lzcnt_flags
    cmp eax, 32
    jne .Lfail_lzcnt_value

    mov eax, 0x80000000
    lzcnt eax, eax
    jc .Lfail_lzcnt_flags
    jnz .Lfail_lzcnt_flags
    test eax, eax
    jne .Lfail_lzcnt_value

    mov eax, 0x100
    lzcnt ax, ax
    cmp ax, 7
    jne .Lfail_lzcnt_value

    mov rax, 1
    lzcnt rax, rax
    cmp rax, 63
    jne .Lfail_lzcnt_value

    vmovq xmm0, qword ptr [rip + .Lhalf]
    vcvtph2ps xmm1, xmm0
    vmovdqu xmm2, xmmword ptr [rip + .Lfloat]
    vpcmpeqd xmm1, xmm1, xmm2
    vpmovmskb eax, xmm1
    cmp eax, 0xffff
    jne .Lfail_f16c_value

    vcvtps2ph xmm3, xmm2, 0
    vmovq rax, xmm3
    cmp rax, qword ptr [rip + .Lhalf]
    jne .Lfail_f16c_roundtrip

    xor edi, edi
    jmp .Lexit

.Lfail_f16c:
    mov edi, 1
    jmp .Lexit
.Lfail_lzcnt:
    mov edi, 2
    jmp .Lexit
.Lfail_lzcnt_value:
    mov edi, 3
    jmp .Lexit
.Lfail_f16c_value:
    mov edi, 4
    jmp .Lexit
.Lfail_f16c_roundtrip:
    mov edi, 5
    jmp .Lexit
.Lfail_lzcnt_flags:
    mov edi, 6
.Lexit:
    mov eax, 60
    syscall

.section .rodata
.p2align 4
.Lhalf:
    .short 0x3c00, 0xc000, 0x3800, 0x0000
.Lfloat:
    .long 0x3f800000, 0xc0000000, 0x3f000000, 0x00000000

.section .note.GNU-stack,"",@progbits
