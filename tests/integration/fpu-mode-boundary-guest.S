/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Check that softfpu exception isolation leaves other modes unchanged. */
#ifndef TEST_CASE
#define TEST_CASE 1
#endif

.section .text
.global _start
.type _start, @function
_start:
    fninit
#if TEST_CASE == 1
#ifdef __x86_64__
    fldl half(%rip)
#else
    fldl half
#endif
    fptan
    fnstsw %ax
    and $0x3f, %eax
    cmp $0x20, %eax
    jne .Lfail
#elif TEST_CASE == 2
#ifdef __x86_64__
    ldmxcsr clean_mxcsr(%rip)
#else
    ldmxcsr clean_mxcsr
#endif
    fldz
    fldz
    fdivp %st, %st(1)
    fnclex
    fnstsw %ax
    test $0x3f, %eax
    jne .Lfail
#ifdef __x86_64__
    stmxcsr saved_mxcsr(%rip)
    mov saved_mxcsr(%rip), %eax
#else
    stmxcsr saved_mxcsr
    mov saved_mxcsr, %eax
#endif
    test $0x3f, %eax
    jne .Lfail
#else
#error Unsupported TEST_CASE
#endif
    xor %ebx, %ebx
    jmp .Lexit
.Lfail:
    mov $1, %ebx
.Lexit:
#ifdef __x86_64__
    mov %ebx, %edi
    mov $60, %eax
    syscall
#else
    mov $1, %eax
    int $0x80
#endif
.size _start, .-_start
.section .data
.align 8
half: .double 0.5
clean_mxcsr: .long 0x1f80
saved_mxcsr: .long 0
.section .note.GNU-stack,"",@progbits
