.intel_syntax noprefix
.section .text
.global _start

_start:
    lea rsi, [rip + .Linput]
    lea rdi, [rip + .Lfactor]
    lea rbx, [rip + .Lacc]
    lea r11, [rip + .Ltable]
    lea r12, [rip + .Lsub]
    lea r13, [rip + .Loutput]
    xor eax, eax
    xor r14d, r14d
    xor r15d, r15d
    mov ecx, 1
    vmovups xmm1, xmmword ptr [rip + .Lgarbage]
    vmovups xmm2, xmmword ptr [rip + .Lgarbage]

.Lloop:
    movzx r8d, byte ptr [rsi]
    imul r8d, r8d, 0x36
    movzx r9d, byte ptr [rsi + 1]
    imul r9d, r9d, 0xb7
    add r9d, r8d
    movzx r8d, byte ptr [rsi + 2]
    lea r10d, [r8 + r8 * 8]
    lea r10d, [r8 + r10 * 2]
    add r10d, r9d
    shr r10d, 8
    vmovss xmm1, dword ptr [rdi + r10 * 4]
    movsxd rax, eax
    vaddss xmm2, xmm1, dword ptr [rbx + rax * 4]
    vmovss dword ptr [rbx + rax * 4], xmm2

    movzx r10d, byte ptr [rsi]
    lea r10, [r10 + r10 * 2]
    vmovss xmm2, dword ptr [r11 + r10 * 4]
    vsubss xmm2, xmm2, dword ptr [r12]
    vfmadd213ss xmm2, xmm1, dword ptr [r13]
    vmovss dword ptr [r13], xmm2

    movzx r10d, byte ptr [rsi + 1]
    lea r10, [r10 + r10 * 2]
    vmovss xmm2, dword ptr [r11 + r10 * 4 + 4]
    vsubss xmm2, xmm2, dword ptr [r12]
    vfmadd213ss xmm2, xmm1, dword ptr [r13 + 4]
    vmovss dword ptr [r13 + 4], xmm2

    movzx r10d, byte ptr [rsi + 2]
    lea r10, [r10 + r10 * 2]
    vmovss xmm2, dword ptr [r11 + r10 * 4 + 8]
    vsubss xmm2, xmm2, dword ptr [r12]
    vfmadd213ss xmm2, xmm1, dword ptr [r13 + 8]
    vmovss dword ptr [r13 + 8], xmm2

    inc r14d
    add r15d, 2
    dec ecx
    jne .Lloop

    cmp r8d, 3
    jne .Lfail
    cmp r9d, 420
    jne .Lfail
    cmp r10d, 9
    jne .Lfail
    cmp eax, 0
    jne .Lfail
    cmp r14d, 1
    jne .Lfail
    cmp r15d, 2
    jne .Lfail

    lea rsi, [rip + .Lacc]
    lea rdi, [rip + .Lexpected_acc]
    mov ecx, 1
    call .Lcheck_words
    test eax, eax
    jne .Lfail
    lea rsi, [rip + .Loutput]
    lea rdi, [rip + .Lexpected_output]
    mov ecx, 3
    call .Lcheck_words
    test eax, eax
    jne .Lfail

    vmovdqu xmmword ptr [rip + .Lxmm_out], xmm1
    vmovdqu xmmword ptr [rip + .Lxmm_out + 16], xmm2
    lea rsi, [rip + .Lxmm_out]
    lea rdi, [rip + .Lexpected_xmm]
    mov ecx, 8
    call .Lcheck_words
    test eax, eax
    jne .Lfail

    xor edi, edi
    jmp .Lexit

.Lcheck_words:
    xor eax, eax
.Lcheck_next:
    mov edx, dword ptr [rsi]
    cmp edx, dword ptr [rdi]
    jne .Lcheck_bad
    add rsi, 4
    add rdi, 4
    dec ecx
    jne .Lcheck_next
    ret
.Lcheck_bad:
    mov eax, 1
    ret

.Lfail:
    mov edi, 1
.Lexit:
    mov eax, 60
    syscall

.section .rodata
.p2align 5
.Linput: .byte 1, 2, 3
.p2align 4
.Lfactor: .float 0.0, 2.0
.Ltable:
    .float 0.0, 0.0, 0.0, 10.0
    .float 0.0, 0.0, 0.0, 20.0
    .float 0.0, 0.0, 0.0, 30.0
.Lsub: .float 1.0
.Lgarbage: .long 0x3f000000, 0x11223344, 0x55667788, 0x99aabbcc
.Lexpected_acc: .float 6.0
.Lexpected_output: .float 23.0, 44.0, 65.0
.Lexpected_xmm:
    .long 0x40000000, 0, 0, 0
    .long 0x42820000, 0, 0, 0

.section .data
.p2align 4
.Lacc: .float 4.0
.Loutput: .float 5.0, 6.0, 7.0

.section .bss
.p2align 4
.Lxmm_out: .skip 32

.section .note.GNU-stack,"",@progbits
