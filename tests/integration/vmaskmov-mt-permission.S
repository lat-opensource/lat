/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * LATX_MT guest-page permission checks for VMASKMOV/VPMASKMOV.
 *
 * A 16K host page is split into four 4K guest pages.  The second guest page is
 * mprotect(PROT_NONE); because the surrounding host page is still writable,
 * the host access alone succeeds.  With LATX_MT enabled the translator must
 * still raise a guest permission fault for selected elements in that page, and
 * must not fault for unselected elements or an all-clear mask.
 *
 * Usage: vmaskmov-mt-permission <case>
 *   a: VMASKMOVPS store, upper half (denied) selected    -> SIGSEGV
 *   b: VMASKMOVPS load,  upper half (denied) selected    -> SIGSEGV
 *   c: VMASKMOVPD store, upper half (denied) selected    -> SIGSEGV
 *   d: VMASKMOVPD load,  upper half (denied) selected    -> SIGSEGV
 *   e: VPMASKMOVD store, upper half (denied) selected    -> SIGSEGV
 *   f: VPMASKMOVQ store, upper half (denied) selected    -> SIGSEGV
 *   g: VMASKMOVPS store, all selected, vector in denied  -> SIGSEGV
 *   h: VMASKMOVPS load,  all selected, vector in denied  -> SIGSEGV
 *   i: VMASKMOVPS store, only lower (permitted) selected -> no fault
 *   j: VMASKMOVPS load,  all mask clear, vector in denied -> no fault
 *   k: VMASKMOVPS store, lane 0 selected, vector in denied -> SIGSEGV
 *   l: VMASKMOVPS store, lane 4 selected, vector in denied -> SIGSEGV
 *   m: VMASKMOVPS store, all selected, starts permitted, ends denied -> SIGSEGV
 *   n: VMASKMOVPS load,  all selected, starts permitted, ends denied -> SIGSEGV
 *   o: VPMASKMOVD store, all selected, starts permitted, ends denied -> SIGSEGV
 *   p: VMASKMOVPD store, one 8-byte element straddles the boundary -> SIGSEGV
 *   q: VMASKMOVPD load,  one 8-byte element straddles the boundary -> SIGSEGV
 *   r: VMASKMOVDQU store, permitted dwords selected, denied page not
 *      selected -> no fault
 *   s: VMASKMOVDQU store, one selected byte lies in the denied page -> SIGSEGV
 *   t: MASKMOVDQU (non-VEX) store, permitted dwords selected, denied page
 *      not selected -> no fault
 *   u: MASKMOVDQU (non-VEX) store, one selected byte lies in the denied
 *      page -> SIGSEGV
 *   v: MASKMOVQ (MMX) store, permitted dword selected, denied page not
 *      selected -> no fault
 *   w: MASKMOVQ (MMX) store, one selected byte lies in the denied page
 *      -> SIGSEGV
 */
.intel_syntax noprefix
.equ __NR_exit, 60
.equ __NR_mmap, 9
.equ __NR_mprotect, 10
.equ __NR_rt_sigaction, 13
.equ SIGSEGV, 11
.equ SEGV_ACCERR, 2
.equ SA_SIGINFO, 4
.equ SA_RESTORER, 0x04000000
.equ PROT_READ, 1
.equ PROT_WRITE, 2
.equ PROT_NONE, 0
.equ MAP_PRIVATE, 2
.equ MAP_ANONYMOUS, 0x20

.section .data
.align 8
sig_action:
    .quad segv_handler
    .quad (SA_SIGINFO | SA_RESTORER)
    .quad signal_restorer
    .quad 0
expected_addr:
    .quad 0

.section .bss
.align 32
out:
    .skip 32

.section .text
.global _start
.type _start, @function
_start:
    /* Install the SIGSEGV handler. */
    mov eax, __NR_rt_sigaction
    mov edi, SIGSEGV
    lea rsi, [rip + sig_action]
    xor edx, edx
    mov r10d, 8
    syscall
    test rax, rax
    jnz fail_setup

    /* mmap 64K and align the test window to a 16K host page. */
    mov eax, __NR_mmap
    xor edi, edi
    mov esi, 65536
    mov edx, (PROT_READ | PROT_WRITE)
    mov r10d, (MAP_PRIVATE | MAP_ANONYMOUS)
    mov r8, -1
    xor r9d, r9d
    syscall
    cmp rax, -4096
    jae fail_map
    lea r12, [rax + 16383]
    and r12, -16384

    /* Deny the second 4K guest page; the host 16K page stays writable. */
    lea rdi, [r12 + 4096]
    mov esi, 4096
    mov edx, PROT_NONE
    mov eax, __NR_mprotect
    syscall
    test rax, rax
    jnz fail_protect
    lea r13, [r12 + 4096]

    mov rax, [rsp]
    cmp rax, 2
    jl fail_args
    mov rax, [rsp + 16]
    movzx eax, byte ptr [rax]

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
    cmp al, 'm'
    je case_m
    cmp al, 'n'
    je case_n
    cmp al, 'o'
    je case_o
    cmp al, 'p'
    je case_p
    cmp al, 'q'
    je case_q
    cmp al, 'r'
    je case_r
    cmp al, 's'
    je case_s
    cmp al, 't'
    je case_t
    cmp al, 'u'
    je case_u
    cmp al, 'v'
    je case_v
    cmp al, 'w'
    je case_w
    jmp fail_args

/* Vector base starts 16 bytes before the denied page; select only the upper
 * half, whose bytes lie in the denied page. */
case_a:
    mov qword ptr [rip + expected_addr], r13
    lea r14, [r13 - 16]
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_upper_d]
    vmaskmovps [r14], ymm1, ymm0
    jmp unexpectedly_allowed

case_b:
    mov qword ptr [rip + expected_addr], r13
    lea r14, [r13 - 16]
    vmovdqu ymm1, [rip + mask_upper_d]
    vmaskmovps ymm0, ymm1, [r14]
    jmp unexpectedly_allowed

case_c:
    mov qword ptr [rip + expected_addr], r13
    lea r14, [r13 - 16]
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_upper_q]
    vmaskmovpd [r14], ymm1, ymm0
    jmp unexpectedly_allowed

case_d:
    mov qword ptr [rip + expected_addr], r13
    lea r14, [r13 - 16]
    vmovdqu ymm1, [rip + mask_upper_q]
    vmaskmovpd ymm0, ymm1, [r14]
    jmp unexpectedly_allowed

case_e:
    mov qword ptr [rip + expected_addr], r13
    lea r14, [r13 - 16]
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_upper_d]
    vpmaskmovd [r14], ymm1, ymm0
    jmp unexpectedly_allowed

case_f:
    mov qword ptr [rip + expected_addr], r13
    lea r14, [r13 - 16]
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_upper_q]
    vpmaskmovq [r14], ymm1, ymm0
    jmp unexpectedly_allowed

/* A full vector placed entirely in the denied page must fault too. */
case_g:
    mov qword ptr [rip + expected_addr], r13
    mov r14, r13
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_full]
    vmaskmovps [r14], ymm1, ymm0
    jmp unexpectedly_allowed

case_h:
    mov qword ptr [rip + expected_addr], r13
    mov r14, r13
    vmovdqu ymm1, [rip + mask_full]
    vmaskmovps ymm0, ymm1, [r14]
    jmp unexpectedly_allowed

/* Denied bytes are not selected: they must stay unchecked and unaccessed. */
case_i:
    lea r14, [r13 - 16]
    mov qword ptr [r14], 0
    mov qword ptr [r14 + 8], 0
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_lower_d]
    vmaskmovps [r14], ymm1, ymm0
    mov rax, qword ptr [rip + src]
    cmp qword ptr [r14], rax
    jne fail_data
    mov rax, qword ptr [rip + src + 8]
    cmp qword ptr [r14 + 8], rax
    jne fail_data
    jmp pass

/* All mask bits clear: no access and the whole destination is zeroed. */
case_j:
    mov r14, r13
    vmovdqu ymm1, [rip + mask_none]
    vmaskmovps ymm0, ymm1, [r14]
    vmovdqu [rip + out], ymm0
    cmp qword ptr [rip + out], 0
    jne fail_data
    cmp qword ptr [rip + out + 8], 0
    jne fail_data
    cmp qword ptr [rip + out + 16], 0
    jne fail_data
    cmp qword ptr [rip + out + 24], 0
    jne fail_data
    jmp pass

case_k:
    mov qword ptr [rip + expected_addr], r13
    mov r14, r13
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_lane0]
    vmaskmovps [r14], ymm1, ymm0
    jmp unexpectedly_allowed

case_l:
    lea r14, [r13]
    mov rax, r13
    add rax, 16
    mov qword ptr [rip + expected_addr], rax
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_lane4]
    vmaskmovps [r14], ymm1, ymm0
    jmp unexpectedly_allowed

/* The 32-byte access starts in the permitted page and ends in the denied
 * page; the end byte must be checked as well. */
case_m:
    lea r14, [r13 - 16]
    mov qword ptr [rip + expected_addr], r13
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_full]
    vmaskmovps [r14], ymm1, ymm0
    jmp unexpectedly_allowed

case_n:
    lea r14, [r13 - 16]
    mov qword ptr [rip + expected_addr], r13
    vmovdqu ymm1, [rip + mask_full]
    vmaskmovps ymm0, ymm1, [r14]
    jmp unexpectedly_allowed

case_o:
    lea r14, [r13 - 16]
    mov qword ptr [rip + expected_addr], r13
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_full]
    vpmaskmovd [r14], ymm1, ymm0
    jmp unexpectedly_allowed

case_p:
    lea r14, [r13 - 4]
    mov qword ptr [rip + expected_addr], r13
    vmovdqu ymm0, [rip + src]
    vmovdqu ymm1, [rip + mask_lane0_q]
    vmaskmovpd [r14], ymm1, ymm0
    jmp unexpectedly_allowed

case_q:
    lea r14, [r13 - 4]
    mov qword ptr [rip + expected_addr], r13
    vmovdqu ymm1, [rip + mask_lane0_q]
    vmaskmovpd ymm0, ymm1, [r14]
    jmp unexpectedly_allowed

/* VMASKMOVDQU selects the two dwords in the permitted page; the denied page
 * is not selected and must not be written or faulted on. */
case_r:
    lea r14, [r13 - 8]
    mov qword ptr [r14], 0
    vmovdqu xmm0, [rip + src]
    vmovdqu xmm1, [rip + mask_low_q]
    mov rdi, r14
    vmaskmovdqu xmm0, xmm1
    mov rax, qword ptr [r14]
    cmp rax, qword ptr [rip + src]
    jne fail_data
    jmp pass

/* The same store with one selected byte in the denied page must fault. */
case_s:
    lea r14, [r13 - 8]
    mov qword ptr [rip + expected_addr], r13
    vmovdqu xmm0, [rip + src]
    vmovdqu xmm1, [rip + mask_low_q_byte8]
    mov rdi, r14
    vmaskmovdqu xmm0, xmm1
    jmp unexpectedly_allowed

/* The SSE2 form has the same masked-byte semantics. */
case_t:
    lea r14, [r13 - 8]
    mov qword ptr [r14], 0
    vmovdqu xmm0, [rip + src]
    vmovdqu xmm1, [rip + mask_low_q]
    mov rdi, r14
    maskmovdqu xmm0, xmm1
    mov rax, qword ptr [r14]
    cmp rax, qword ptr [rip + src]
    jne fail_data
    jmp pass

case_u:
    lea r14, [r13 - 8]
    mov qword ptr [rip + expected_addr], r13
    vmovdqu xmm0, [rip + src]
    vmovdqu xmm1, [rip + mask_low_q_byte8]
    mov rdi, r14
    maskmovdqu xmm0, xmm1
    jmp unexpectedly_allowed

/* MMX MASKMOVQ spans eight bytes starting four bytes before the denied page. */
case_v:
    lea r14, [r13 - 4]
    mov dword ptr [r14], 0
    movq mm0, qword ptr [rip + src]
    movq mm1, qword ptr [rip + mask_low_mmx]
    mov rdi, r14
    maskmovq mm0, mm1
    emms
    mov eax, dword ptr [r14]
    cmp eax, dword ptr [rip + src]
    jne fail_data
    jmp pass

case_w:
    lea r14, [r13 - 4]
    mov qword ptr [rip + expected_addr], r13
    movq mm0, qword ptr [rip + src]
    movq mm1, qword ptr [rip + mask_low5_mmx]
    mov rdi, r14
    maskmovq mm0, mm1
    emms
    jmp unexpectedly_allowed

unexpectedly_allowed:
    mov edi, 20
    jmp exit

pass:
    xor edi, edi
    jmp exit

fail_setup:
    mov edi, 10
    jmp exit
fail_map:
    mov edi, 11
    jmp exit
fail_protect:
    mov edi, 12
    jmp exit
fail_args:
    mov edi, 13
    jmp exit
fail_data:
    mov edi, 14
    jmp exit

exit:
    mov eax, __NR_exit
    syscall

/* The fault must report SEGV_ACCERR at the exact guest address the
 * translator checked. */
segv_handler:
    cmp edi, SIGSEGV
    jne h_bad_signo
    cmp dword ptr [rsi + 8], SEGV_ACCERR
    jne h_bad_code
    mov rcx, qword ptr [rsi + 16]
    cmp rcx, qword ptr [rip + expected_addr]
    jne h_bad_addr
    xor edi, edi
    jmp exit
h_bad_signo:
    mov edi, 15
    jmp exit
h_bad_code:
    mov edi, 16
    jmp exit
h_bad_addr:
    mov edi, 17
    jmp exit

.type signal_restorer, @function
signal_restorer:
    mov eax, 15
    syscall

.section .rodata
.align 32
src:
    .quad 0x1111111122222222, 0x3333333344444444
    .quad 0x5555555566666666, 0x7777777788888888
/* Select the four dwords in the upper half (denied page). */
mask_upper_d:
    .long 0, 0, 0, 0
    .long 0x80000000, 0x80000000, 0x80000000, 0x80000000
/* Select the four dwords in the lower half (permitted page). */
mask_lower_d:
    .long 0x80000000, 0x80000000, 0x80000000, 0x80000000
    .long 0, 0, 0, 0
/* Select the two qwords in the upper half (denied page). */
mask_upper_q:
    .quad 0, 0
    .quad 0x8000000000000000, 0x8000000000000000
mask_full:
    .quad -1, -1, -1, -1
mask_none:
    .quad 0, 0, 0, 0
mask_lane0:
    .long 0x80000000, 0, 0, 0
    .long 0, 0, 0, 0
mask_lane4:
    .long 0, 0, 0, 0
    .long 0x80000000, 0, 0, 0
mask_lane0_q:
    .quad 0x8000000000000000, 0, 0, 0
/* Select the two dwords in the permitted page. */
mask_low_q:
    .quad 0x8080808080808080, 0
/* Also select the first byte of the denied page. */
mask_low_q_byte8:
    .quad 0x8080808080808080, 0x0000000000000080
/* MMX: select the first dword. */
mask_low_mmx:
    .quad 0x0000000080808080
/* MMX: also select the first byte of the denied page. */
mask_low5_mmx:
    .quad 0x0000008080808080
.section .note.GNU-stack,"",@progbits
