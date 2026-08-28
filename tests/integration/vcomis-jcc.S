.intel_syntax noprefix
.global _start

.section .rodata
.p2align 4
.Ltwo: .long 0x40000000
.Lone: .long 0x3f800000
.Lnan: .long 0x7fc00000

.text
.macro check_ja lhs, rhs, expected
    vmovss xmm0, dword ptr [rip + \lhs]
    vmovss xmm1, dword ptr [rip + \rhs]
    vucomiss xmm0, xmm1
    vsqrtss xmm2, xmm0, xmm0
    vdivss xmm2, xmm0, xmm2
    ja 1f
    mov eax, 0
    jmp 2f
1:  mov eax, 1
2:  cmp eax, \expected
    jne .Lfail
.endm

.macro check_jne lhs, rhs, expected
    vmovss xmm0, dword ptr [rip + \lhs]
    vmovss xmm1, dword ptr [rip + \rhs]
    vucomiss xmm0, xmm1
    jne 1f
    mov eax, 0
    jmp 2f
1:  mov eax, 1
2:  cmp eax, \expected
    jne .Lfail
.endm

_start:
    check_ja .Ltwo, .Lone, 1
    check_ja .Lone, .Ltwo, 0
    check_ja .Lone, .Lone, 0
    check_ja .Lnan, .Lone, 0
    check_jne .Ltwo, .Lone, 1
    check_jne .Lone, .Lone, 0
    check_jne .Lnan, .Lone, 0
    mov eax, 60
    xor edi, edi
    syscall
.Lfail:
    mov eax, 60
    mov edi, 1
    syscall

.section .note.GNU-stack,"",@progbits
