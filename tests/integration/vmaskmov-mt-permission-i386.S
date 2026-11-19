/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * i386 LATX_MT guest-page permission checks for masked vector accesses.
 *
 * A 16K host page is split into four 4K guest pages.  The second guest page is
 * mprotect(PROT_NONE); because the surrounding host page is still writable,
 * the host access alone succeeds.  With LATX_MT enabled the translator must
 * still raise a guest permission fault for selected elements in that page, and
 * must not fault for unselected elements.
 *
 * Usage: vmaskmov-mt-permission-i386 <case>
 *   a: MASKMOVDQU store, permitted dwords selected, denied page not selected
 *   b: MASKMOVDQU store, one selected byte in the denied page
 *   c: MASKMOVQ store, permitted dword selected, denied page not selected
 *   d: MASKMOVQ store, one selected byte in the denied page
 *   e: VMASKMOVDQU store, permitted dwords selected, denied page not selected
 *   f: VMASKMOVDQU store, one selected byte in the denied page
 *   g: VMASKMOVPS store, permitted element selected, denied page not selected
 *   h: VMASKMOVPS store, one selected element in the denied page
 *   i: VPMASKMOVD store, permitted element selected, denied page not selected
 *   j: VPMASKMOVD store, one selected element in the denied page
 *   k: VMASKMOVPS load, permitted element selected, denied page not selected
 *   l: VMASKMOVPS load, one selected element in the denied page
 */
.intel_syntax noprefix
.equ __NR_exit, 1
.equ __NR_mprotect, 125
.equ __NR_rt_sigaction, 174
.equ __NR_mmap2, 192
.equ SIGSEGV, 11
.equ SEGV_ACCERR, 2
.equ SA_SIGINFO, 4
.equ PROT_READ, 1
.equ PROT_WRITE, 2
.equ PROT_NONE, 0
.equ MAP_PRIVATE, 2
.equ MAP_ANONYMOUS, 0x20

.section .data
.align 4
sig_action:
    .long segv_handler, SA_SIGINFO, 0
    .quad 0
expected_addr:
    .long 0

.section .bss
.align 16
out:
    .skip 16

.section .text
.global _start
.type _start, @function
_start:
    mov eax, __NR_rt_sigaction
    mov ebx, SIGSEGV
    mov ecx, offset sig_action
    xor edx, edx
    mov esi, 8
    int 0x80
    test eax, eax
    jnz fail_setup

    mov eax, __NR_mmap2
    xor ebx, ebx
    mov ecx, 16384
    mov edx, (PROT_READ | PROT_WRITE)
    mov esi, (MAP_PRIVATE | MAP_ANONYMOUS)
    mov edi, -1
    xor ebp, ebp
    int 0x80
    cmp eax, -4095
    jae fail_map
    mov ebp, eax
    and ebp, -16384

    mov eax, __NR_mprotect
    lea ebx, [ebp + 4096]
    mov ecx, 4096
    mov edx, PROT_NONE
    int 0x80
    test eax, eax
    jnz fail_protect

    mov eax, [esp]
    cmp eax, 2
    jl fail_args
    mov eax, [esp + 8]
    movzx eax, byte ptr [eax]

    cmp al, 'a'
    je case_a
    cmp al, 'b'
    je case_b
    cmp al, 'c'
    je case_c
    cmp al, 'd'
    je case_d
    cmp al, 'e'
    je case_e
    cmp al, 'f'
    je case_f
    cmp al, 'g'
    je case_g
    cmp al, 'h'
    je case_h
    cmp al, 'i'
    je case_i
    cmp al, 'j'
    je case_j
    cmp al, 'k'
    je case_k
    cmp al, 'l'
    je case_l
    jmp fail_args

/* MASKMOVDQU selects two dwords in the permitted page. */
case_a:
    mov ebx, ebp
    mov dword ptr [ebx + 4088], 0
    mov dword ptr [ebx + 4092], 0
    mov edi, ebx
    add edi, 4088
    movdqu xmm0, [src]
    movdqu xmm1, [mask_bytes8]
    maskmovdqu xmm0, xmm1
    mov eax, dword ptr [ebx + 4088]
    cmp eax, dword ptr [src]
    jne fail_data
    mov eax, dword ptr [ebx + 4092]
    cmp eax, dword ptr [src + 4]
    jne fail_data
    jmp pass

case_b:
    lea eax, [ebp + 4096]
    mov dword ptr [expected_addr], eax
    mov edi, ebp
    add edi, 4088
    movdqu xmm0, [src]
    movdqu xmm1, [mask_bytes8_byte8]
    maskmovdqu xmm0, xmm1
    jmp unexpectedly_allowed

/* MASKMOVQ selects one dword in the permitted page. */
case_c:
    mov ebx, ebp
    mov dword ptr [ebx + 4092], 0
    lea edi, [ebx + 4092]
    movq mm0, qword ptr [src]
    movq mm1, qword ptr [mask_bytes4]
    maskmovq mm0, mm1
    emms
    mov eax, dword ptr [ebx + 4092]
    cmp eax, dword ptr [src]
    jne fail_data
    jmp pass

case_d:
    lea eax, [ebp + 4096]
    mov dword ptr [expected_addr], eax
    lea edi, [ebp + 4092]
    movq mm0, qword ptr [src]
    movq mm1, qword ptr [mask_bytes5]
    maskmovq mm0, mm1
    emms
    jmp unexpectedly_allowed

/* VMASKMOVDQU has the same masked-byte semantics as MASKMOVDQU. */
case_e:
    mov ebx, ebp
    mov dword ptr [ebx + 4088], 0
    mov dword ptr [ebx + 4092], 0
    mov edi, ebx
    add edi, 4088
    movdqu xmm0, [src]
    movdqu xmm1, [mask_bytes8]
    vmaskmovdqu xmm0, xmm1
    mov eax, dword ptr [ebx + 4088]
    cmp eax, dword ptr [src]
    jne fail_data
    mov eax, dword ptr [ebx + 4092]
    cmp eax, dword ptr [src + 4]
    jne fail_data
    jmp pass

case_f:
    lea eax, [ebp + 4096]
    mov dword ptr [expected_addr], eax
    mov edi, ebp
    add edi, 4088
    movdqu xmm0, [src]
    movdqu xmm1, [mask_bytes8_byte8]
    vmaskmovdqu xmm0, xmm1
    jmp unexpectedly_allowed

/* VMASKMOVPS/VPMASKMOVD 128-bit explicit-address forms. */
case_g:
    lea ebx, [ebp + 4092]
    mov dword ptr [ebx], 0
    movdqu xmm0, [src]
    movdqu xmm1, [mask_d_lane0]
    vmaskmovps [ebx], xmm1, xmm0
    mov eax, dword ptr [ebx]
    cmp eax, dword ptr [src]
    jne fail_data
    jmp pass

case_h:
    lea eax, [ebp + 4096]
    mov dword ptr [expected_addr], eax
    lea ebx, [ebp + 4092]
    movdqu xmm0, [src]
    movdqu xmm1, [mask_d_lane1]
    vmaskmovps [ebx], xmm1, xmm0
    jmp unexpectedly_allowed

case_i:
    lea ebx, [ebp + 4092]
    mov dword ptr [ebx], 0
    movdqu xmm0, [src]
    movdqu xmm1, [mask_d_lane0]
    vpmaskmovd [ebx], xmm1, xmm0
    mov eax, dword ptr [ebx]
    cmp eax, dword ptr [src]
    jne fail_data
    jmp pass

case_j:
    lea eax, [ebp + 4096]
    mov dword ptr [expected_addr], eax
    lea ebx, [ebp + 4092]
    movdqu xmm0, [src]
    movdqu xmm1, [mask_d_lane1]
    vpmaskmovd [ebx], xmm1, xmm0
    jmp unexpectedly_allowed

case_k:
    lea ebx, [ebp + 4092]
    mov dword ptr [ebx], 0x12345678
    movdqu xmm1, [mask_d_lane0]
    pxor xmm0, xmm0
    vmaskmovps xmm0, xmm1, [ebx]
    movdqu [out], xmm0
    mov eax, dword ptr [out]
    cmp eax, 0x12345678
    jne fail_data
    mov eax, dword ptr [out + 4]
    test eax, eax
    jnz fail_data
    jmp pass

case_l:
    lea eax, [ebp + 4096]
    mov dword ptr [expected_addr], eax
    lea ebx, [ebp + 4092]
    movdqu xmm1, [mask_d_lane1]
    vmaskmovps xmm0, xmm1, [ebx]
    jmp unexpectedly_allowed

unexpectedly_allowed:
    mov ebx, 20
    jmp exit
pass:
    xor ebx, ebx
    jmp exit
fail_setup:
    mov ebx, 10
    jmp exit
fail_map:
    mov ebx, 11
    jmp exit
fail_protect:
    mov ebx, 12
    jmp exit
fail_args:
    mov ebx, 13
    jmp exit
fail_data:
    mov ebx, 14
    jmp exit

exit:
    mov eax, __NR_exit
    int 0x80

/* i386 SA_SIGINFO handler(signo, siginfo, ucontext). */
segv_handler:
    cmp dword ptr [esp + 4], SIGSEGV
    jne h_bad_signo
    mov eax, [esp + 8]
    cmp dword ptr [eax + 8], SEGV_ACCERR
    jne h_bad_code
    mov edx, [eax + 12]
    cmp edx, dword ptr [expected_addr]
    jne h_bad_addr
    xor ebx, ebx
    jmp exit
h_bad_signo:
    mov ebx, 15
    jmp exit
h_bad_code:
    mov ebx, 16
    jmp exit
h_bad_addr:
    mov ebx, 17
    jmp exit

.section .rodata
.align 16
src:
    .quad 0x1111111122222222, 0x3333333344444444
/* MASKMOVDQU selects two dwords, then also the first denied byte. */
mask_bytes8:
    .quad 0x8080808080808080, 0
mask_bytes8_byte8:
    .quad 0x8080808080808080, 0x0000000000000080
/* MASKMOVQ selects one dword, then also the first denied byte. */
mask_bytes4:
    .quad 0x0000000080808080
mask_bytes5:
    .quad 0x0000008080808080
/* VMASKMOVPS/VPMASKMOVD lane masks at offset 4092. */
mask_d_lane0:
    .long 0x80000000, 0, 0, 0
mask_d_lane1:
    .long 0, 0x80000000, 0, 0

.section .note.GNU-stack,"",@progbits
