/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * VMASKMOVPS/PD and VPMASKMOVD/Q must only access elements whose mask MSB is
 * set.  Put the second half of a 256-bit access in an unmapped region while
 * selecting only elements in the mapped half: the operation must complete
 * without touching the unmapped half.
 */
.intel_syntax noprefix
.text
.global _start

_start:
    /* mmap(NULL, 131072, PROT_READ|PROT_WRITE,
     *      MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) */
    mov eax, 9
    xor edi, edi
    mov esi, 131072
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
    syscall
    cmp rax, -4096
    jae fail
    mov r12, rax

    /* Keep [base, base+65536) mapped and unmap [base+65536, base+131072).
     * The hole starts at a 64K boundary, so this works for 4K, 16K and 64K
     * host page sizes. */
    lea rdi, [r12 + 65536]
    mov esi, 65536
    mov eax, 11
    syscall
    test rax, rax
    jnz fail

    /* 16 bytes before the hole; a full 256-bit access would cross it. */
    lea r13, [r12 + 65536 - 16]

    /* ---------------- VMASKMOVPS store, partially masked ---------------- */
    mov qword ptr [r13], 0
    mov qword ptr [r13 + 8], 0
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_d_part]
    vmaskmovps [r13], ymm1, ymm0
    vzeroupper
    mov rax, qword ptr [r13]
    cmp rax, qword ptr [rip + src]
    jne fail
    mov rax, qword ptr [r13 + 8]
    cmp rax, qword ptr [rip + src + 8]
    jne fail

    /* ---------------- VMASKMOVPS store, all mask clear ------------------ */
    mov rax, 0xa5a5a5a5a5a5a5a5
    mov qword ptr [r13], rax
    mov rax, 0x5a5a5a5a5a5a5a5a
    mov qword ptr [r13 + 8], rax
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_d_none]
    vmaskmovps [r13], ymm1, ymm0
    vzeroupper
    mov rax, 0xa5a5a5a5a5a5a5a5
    cmp qword ptr [r13], rax
    jne fail
    mov rax, 0x5a5a5a5a5a5a5a5a
    cmp qword ptr [r13 + 8], rax
    jne fail

    /* ---------------- VMASKMOVPS load, partially masked ----------------- */
    mov rax, qword ptr [rip + src]
    mov qword ptr [r13], rax
    mov rax, qword ptr [rip + src + 8]
    mov qword ptr [r13 + 8], rax
    vmovdqu ymm1, [rip + mask_d_part]
    vmaskmovps ymm0, ymm1, [r13]
    vmovdqu [rip + out], ymm0
    vzeroupper
    mov rax, qword ptr [rip + src]
    cmp qword ptr [rip + out], rax
    jne fail
    mov rax, qword ptr [rip + src + 8]
    cmp qword ptr [rip + out + 8], rax
    jne fail
    cmp qword ptr [rip + out + 16], 0
    jne fail
    cmp qword ptr [rip + out + 24], 0
    jne fail

    /* ---------------- VMASKMOVPD store, partially masked ---------------- */
    mov qword ptr [r13], 0
    mov qword ptr [r13 + 8], 0
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_q_part]
    vmaskmovpd [r13], ymm1, ymm0
    vzeroupper
    mov rax, qword ptr [r13]
    cmp rax, qword ptr [rip + src]
    jne fail
    mov rax, qword ptr [r13 + 8]
    cmp rax, qword ptr [rip + src + 8]
    jne fail

    /* ---------------- VPMASKMOVD store, partially masked ---------------- */
    mov qword ptr [r13], 0
    mov qword ptr [r13 + 8], 0
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_d_part]
    vpmaskmovd [r13], ymm1, ymm0
    vzeroupper
    mov rax, qword ptr [r13]
    cmp rax, qword ptr [rip + src]
    jne fail
    mov rax, qword ptr [r13 + 8]
    cmp rax, qword ptr [rip + src + 8]
    jne fail

    /* ---------------- VPMASKMOVQ store, partially masked ---------------- */
    mov qword ptr [r13], 0
    mov qword ptr [r13 + 8], 0
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_q_part]
    vpmaskmovq [r13], ymm1, ymm0
    vzeroupper
    mov rax, qword ptr [r13]
    cmp rax, qword ptr [rip + src]
    jne fail
    mov rax, qword ptr [r13 + 8]
    cmp rax, qword ptr [rip + src + 8]
    jne fail

    /* ---------------- VPMASKMOVD load, partially masked ----------------- */
    vmovdqu [r13], xmm0
    vmovdqu ymm1, [rip + mask_d_part]
    vpmaskmovd ymm0, ymm1, [r13]
    vmovdqu [rip + out], ymm0
    vzeroupper
    mov rax, qword ptr [rip + src]
    cmp qword ptr [rip + out], rax
    jne fail
    mov rax, qword ptr [rip + src + 8]
    cmp qword ptr [rip + out + 8], rax
    jne fail
    cmp qword ptr [rip + out + 16], 0
    jne fail
    cmp qword ptr [rip + out + 24], 0
    jne fail

    xor edi, edi
    jmp exit

fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

.section .rodata
.p2align 5
src:
    .quad 0x1111111122222222, 0x3333333344444444
    .quad 0x5555555566666666, 0x7777777788888888
/* Select the first four dwords (the mapped half), clear the upper half. */
mask_d_part:
    .long 0x80000000, 0x80000000, 0x80000000, 0x80000000
    .long 0, 0, 0, 0
mask_d_none:
    .long 0, 0, 0, 0, 0, 0, 0, 0
/* Select the first two qwords (the mapped half), clear the upper half. */
mask_q_part:
    .quad 0x8000000000000000, 0x8000000000000000, 0, 0

.bss
.p2align 5
out:
    .skip 32

.section .note.GNU-stack,"",@progbits
