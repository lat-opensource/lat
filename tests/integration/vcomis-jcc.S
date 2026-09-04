.intel_syntax noprefix
.global _start

.section .rodata
.p2align 4
.Ltwo_s: .long 0x40000000
.Lone_s: .long 0x3f800000
.Lnan_s: .long 0x7fc00000
.p2align 3
.Ltwo_d: .quad 0x4000000000000000
.Lone_d: .quad 0x3ff0000000000000
.Lnan_d: .quad 0x7ff8000000000000

.text
.macro check_s inst, cc, lhs, rhs, expected, id
    vmovss xmm0, dword ptr [rip + \lhs]
    vmovss xmm1, dword ptr [rip + \rhs]
    \inst xmm0, xmm1
    \cc .Ltaken\@
    mov eax, 0
    jmp .Ldone\@
.Ltaken\@:
    mov eax, 1
.Ldone\@:
    cmp eax, \expected
    je .Lok\@
    mov edi, \id
    jmp .Lfail
.Lok\@:
.endm

.macro check_d inst, cc, lhs, rhs, expected, id
    vmovsd xmm0, qword ptr [rip + \lhs]
    vmovsd xmm1, qword ptr [rip + \rhs]
    \inst xmm0, xmm1
    \cc .Ltaken\@
    mov eax, 0
    jmp .Ldone\@
.Ltaken\@:
    mov eax, 1
.Ldone\@:
    cmp eax, \expected
    je .Lok\@
    mov edi, \id
    jmp .Lfail
.Lok\@:
.endm

.macro check_all_s inst, lhs, rhs, ja, jae, jb, jbe, je, jne, jle, jg, base
    check_s \inst, ja,  \lhs, \rhs, \ja,  \base
    check_s \inst, jae, \lhs, \rhs, \jae, \base + 1
    check_s \inst, jb,  \lhs, \rhs, \jb,  \base + 2
    check_s \inst, jbe, \lhs, \rhs, \jbe, \base + 3
    check_s \inst, je,  \lhs, \rhs, \je,  \base + 4
    check_s \inst, jne, \lhs, \rhs, \jne, \base + 5
    check_s \inst, jl,  \lhs, \rhs, 0,   \base + 6
    check_s \inst, jge, \lhs, \rhs, 1,   \base + 7
    check_s \inst, jle, \lhs, \rhs, \jle, \base + 8
    check_s \inst, jg,  \lhs, \rhs, \jg,  \base + 9
.endm

.macro check_all_d inst, lhs, rhs, ja, jae, jb, jbe, je, jne, jle, jg, base
    check_d \inst, ja,  \lhs, \rhs, \ja,  \base
    check_d \inst, jae, \lhs, \rhs, \jae, \base + 1
    check_d \inst, jb,  \lhs, \rhs, \jb,  \base + 2
    check_d \inst, jbe, \lhs, \rhs, \jbe, \base + 3
    check_d \inst, je,  \lhs, \rhs, \je,  \base + 4
    check_d \inst, jne, \lhs, \rhs, \jne, \base + 5
    check_d \inst, jl,  \lhs, \rhs, 0,   \base + 6
    check_d \inst, jge, \lhs, \rhs, 1,   \base + 7
    check_d \inst, jle, \lhs, \rhs, \jle, \base + 8
    check_d \inst, jg,  \lhs, \rhs, \jg,  \base + 9
.endm

_start:
    /* greater: JA/JAE/JNE/JGE/JG */
    check_all_s vcomiss,  .Ltwo_s, .Lone_s, 1, 1, 0, 0, 0, 1, 0, 1, 1
    check_all_s vucomiss, .Ltwo_s, .Lone_s, 1, 1, 0, 0, 0, 1, 0, 1, 11
    check_all_d vcomisd,  .Ltwo_d, .Lone_d, 1, 1, 0, 0, 0, 1, 0, 1, 21
    check_all_d vucomisd, .Ltwo_d, .Lone_d, 1, 1, 0, 0, 0, 1, 0, 1, 31

    /* less: JB/JBE/JNE/JGE/JG */
    check_all_s vcomiss,  .Lone_s, .Ltwo_s, 0, 0, 1, 1, 0, 1, 0, 1, 41
    check_all_s vucomiss, .Lone_s, .Ltwo_s, 0, 0, 1, 1, 0, 1, 0, 1, 51
    check_all_d vcomisd,  .Lone_d, .Ltwo_d, 0, 0, 1, 1, 0, 1, 0, 1, 61
    check_all_d vucomisd, .Lone_d, .Ltwo_d, 0, 0, 1, 1, 0, 1, 0, 1, 71

    /* equal: JAE/JBE/JE/JGE/JLE */
    check_all_s vcomiss,  .Lone_s, .Lone_s, 0, 1, 0, 1, 1, 0, 1, 0, 81
    check_all_s vucomiss, .Lone_s, .Lone_s, 0, 1, 0, 1, 1, 0, 1, 0, 91
    check_all_d vcomisd,  .Lone_d, .Lone_d, 0, 1, 0, 1, 1, 0, 1, 0, 101
    check_all_d vucomisd, .Lone_d, .Lone_d, 0, 1, 0, 1, 1, 0, 1, 0, 111

    /* unordered: JB/JBE/JE/JGE/JLE */
    check_all_s vcomiss,  .Lnan_s, .Lone_s, 0, 0, 1, 1, 1, 0, 1, 0, 121
    check_all_s vucomiss, .Lnan_s, .Lone_s, 0, 0, 1, 1, 1, 0, 1, 0, 131
    check_all_d vcomisd,  .Lnan_d, .Lone_d, 0, 0, 1, 1, 1, 0, 1, 0, 141
    check_all_d vucomisd, .Lnan_d, .Lone_d, 0, 0, 1, 1, 1, 0, 1, 0, 151

    /* A second branch in the successor TB must see the compare flags. */
    mov ecx, 1000
.Lfallthrough_flags:
    vmovss xmm0, dword ptr [rip + .Ltwo_s]
    vmovss xmm1, dword ptr [rip + .Lone_s]
    vucomiss xmm0, xmm1
    jb .Lfail_201
    jne .Lfallthrough_ok
    jmp .Lfail_201
.Lfallthrough_ok:
    dec ecx
    jnz .Lfallthrough_flags

    mov ecx, 1000
.Ltaken_flags:
    vmovss xmm0, dword ptr [rip + .Lnan_s]
    vmovss xmm1, dword ptr [rip + .Lone_s]
    vucomiss xmm0, xmm1
    jb .Ltaken_path
    jmp .Lfail_202
.Ltaken_path:
    jp .Ltaken_ok
    jmp .Lfail_202
.Ltaken_ok:
    dec ecx
    jnz .Ltaken_flags

    mov eax, 60
    xor edi, edi
    syscall
.Lfail_201:
    mov edi, 201
    jmp .Lfail
.Lfail_202:
    mov edi, 202
    jmp .Lfail
.Lfail:
    mov eax, 60
    syscall

.section .note.GNU-stack,"",@progbits
