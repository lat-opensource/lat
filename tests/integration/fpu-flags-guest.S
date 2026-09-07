/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Freestanding x86_64 guest for x87/SSE exception-state isolation tests.
 *
 * Each test starts from clean x87 and MXCSR state and reports only the
 * architected exception flags.  The runner compares the x87 status word and
 * MXCSR independently, which makes cross-domain contamination visible.
 */

#ifndef TEST_CASE
#error TEST_CASE must select one FPU flag test
#endif

.equ __NR_write, 1
.equ __NR_exit, 60
.equ X87_STATUS_MASK, 0x003f
.equ MXCSR_STATUS_MASK, 0x0000003f
.equ REPORT_SIZE, 8

.section .text
.global _start
.type _start, @function
_start:
    fninit
    fnclex
    ldmxcsr clean_mxcsr(%rip)

#if TEST_CASE == 1
    /* x87 0/0: Invalid belongs to x87, not MXCSR. */
    fldz
    fldz
    fdivp %st, %st(1)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 2
    /* SSE 0/0: Invalid belongs to MXCSR, not x87 status. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 3
    /* x87 Invalid must not leak into a later SSE read. */
    fldz
    fldz
    fdivp %st, %st(1)
    movss exact_one(%rip), %xmm0
    addss %xmm0, %xmm0
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 4
    /* SSE Invalid must not leak into a later x87 read. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    fld1
    fld1
    faddp %st, %st(1)
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 5
    /* An exact x87 operation must not clear an earlier x87 Invalid. */
    fldz
    fldz
    fdivp %st, %st(1)
    fld1
    fld1
    faddp %st, %st(1)
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 6
    /* An exact SSE operation must not clear an earlier SSE Invalid. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    movss exact_one(%rip), %xmm0
    addss %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 7
    /* x87 1/3: Precision belongs to x87, not MXCSR. */
    fld1
    fdivl inexact_three(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 8
    /* SSE Invalid and x87 Precision must remain in their own domains. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    fld1
    fdivl inexact_three(%rip)
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 9
    /* x87 Precision and later SSE Invalid must remain in their own domains. */
    fld1
    fdivl inexact_three(%rip)
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 10
    /* Scalar double 0/0: DIVSD Invalid belongs to MXCSR. */
    xorpd %xmm0, %xmm0
    divsd %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 11
    /* Packed single 0/0: DIVPS Invalid belongs to MXCSR. */
    xorps %xmm0, %xmm0
    divps %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 12
    /* Packed double 0/0: DIVPD Invalid belongs to MXCSR. */
    xorpd %xmm0, %xmm0
    divpd %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 13
    /* Scalar single +Inf + -Inf: ADDSS raises Invalid. */
    movss positive_infinity_s(%rip), %xmm0
    movss negative_infinity_s(%rip), %xmm1
    addss %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 14
    /* Scalar double +Inf + -Inf: ADDSD raises Invalid. */
    movsd positive_infinity_d(%rip), %xmm0
    movsd negative_infinity_d(%rip), %xmm1
    addsd %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 15
    /* Packed single +Inf + -Inf: ADDPS raises Invalid. */
    movaps packed_positive_infinity_s(%rip), %xmm0
    movaps packed_negative_infinity_s(%rip), %xmm1
    addps %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 16
    /* Packed double +Inf + -Inf: ADDPD raises Invalid. */
    movapd packed_positive_infinity_d(%rip), %xmm0
    movapd packed_negative_infinity_d(%rip), %xmm1
    addpd %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 17
    /* Scalar single Inf - Inf: SUBSS raises Invalid. */
    movss positive_infinity_s(%rip), %xmm0
    movss positive_infinity_s(%rip), %xmm1
    subss %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 18
    /* Scalar double Inf - Inf: SUBSD raises Invalid. */
    movsd positive_infinity_d(%rip), %xmm0
    movsd positive_infinity_d(%rip), %xmm1
    subsd %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 19
    /* Packed single Inf - Inf: SUBPS raises Invalid. */
    movaps packed_positive_infinity_s(%rip), %xmm0
    movaps packed_positive_infinity_s(%rip), %xmm1
    subps %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 20
    /* Packed double Inf - Inf: SUBPD raises Invalid. */
    movapd packed_positive_infinity_d(%rip), %xmm0
    movapd packed_positive_infinity_d(%rip), %xmm1
    subpd %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 21
    /* Scalar single 0 * Inf: MULSS raises Invalid. */
    xorps %xmm0, %xmm0
    movss positive_infinity_s(%rip), %xmm1
    mulss %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 22
    /* Scalar double 0 * Inf: MULSD raises Invalid. */
    xorpd %xmm0, %xmm0
    movsd positive_infinity_d(%rip), %xmm1
    mulsd %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 23
    /* Packed single 0 * Inf: MULPS raises Invalid. */
    xorps %xmm0, %xmm0
    movaps packed_positive_infinity_s(%rip), %xmm1
    mulps %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 24
    /* Packed double 0 * Inf: MULPD raises Invalid. */
    xorpd %xmm0, %xmm0
    movapd packed_positive_infinity_d(%rip), %xmm1
    mulpd %xmm1, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 25
    /* Scalar single sqrt(-1): SQRTSS raises Invalid. */
    movss negative_one_s(%rip), %xmm0
    sqrtss %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 26
    /* Scalar double sqrt(-1): SQRTSD raises Invalid. */
    movsd negative_one_d(%rip), %xmm0
    sqrtsd %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 27
    /* Packed single sqrt(-1): SQRTPS raises Invalid. */
    movaps packed_negative_one_s(%rip), %xmm0
    sqrtps %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 28
    /* Packed double sqrt(-1): SQRTPD raises Invalid. */
    movapd packed_negative_one_d(%rip), %xmm0
    sqrtpd %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 29
    /* x87 +Inf + -Inf: FADD raises Invalid in x87 status only. */
    fldl positive_infinity_d(%rip)
    faddl negative_infinity_d(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 30
    /* x87 Inf - Inf: FSUB raises Invalid in x87 status only. */
    fldl positive_infinity_d(%rip)
    fsubl positive_infinity_d(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 31
    /* x87 0 * Inf: FMUL raises Invalid in x87 status only. */
    fldz
    fmull positive_infinity_d(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 32
    /* x87 sqrt(-1): FSQRT raises Invalid in x87 status only. */
    fldl negative_one_d(%rip)
    fsqrt
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 33
    /* x87 +Inf + -Inf: FADDP raises Invalid before popping the stack. */
    fldl positive_infinity_d(%rip)
    fldl negative_infinity_d(%rip)
    faddp %st, %st(1)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 34
    /* x87 Inf - Inf: FSUBP raises Invalid before popping the stack. */
    fldl positive_infinity_d(%rip)
    fldl positive_infinity_d(%rip)
    /* AT&T fsubrp encodes the architectural FSUBP st(1), st(0). */
    fsubrp %st, %st(1)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 35
    /* x87 0 * Inf: FMULP raises Invalid before popping the stack. */
    fldz
    fldl positive_infinity_d(%rip)
    fmulp %st, %st(1)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 36
    /* x87 3/10 or 10/3: FDIVP raises Precision before popping. */
    fldl inexact_three(%rip)
    fldl inexact_ten(%rip)
    /* AT&T fdivrp encodes the architectural FDIVP st(1), st(0). */
    fdivrp %st, %st(1)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 37
    /* x87 +Inf reverse-subtracted from +Inf raises Invalid. */
    fldl positive_infinity_d(%rip)
    fsubrl positive_infinity_d(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 38
    /* Architectural FSUBRP raises Invalid before popping the stack. */
    fldl positive_infinity_d(%rip)
    fldl positive_infinity_d(%rip)
    /* AT&T fsubp encodes the architectural FSUBRP st(1), st(0). */
    fsubp %st, %st(1)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 39
    /* SSE Invalid and later x87 reverse-divide Precision stay isolated. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    fldl inexact_ten(%rip)
    fdivrl inexact_three(%rip)       /* 3.0 / 10.0 */
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 40
    /* SSE Invalid and later x87 FDIVRP Precision stay isolated. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    fldl inexact_ten(%rip)
    fldl inexact_three(%rip)
    /* AT&T fdivp encodes the architectural FDIVRP st(1), st(0). */
    fdivp %st, %st(1)               /* 3.0 / 10.0 */
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 41
    /* x87 integer add: 2^64 + 1 is inexact at extended precision. */
    fldl two_pow_64(%rip)
    fiaddl integer_one(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 42
    /* x87 integer subtract: 2^65 - 1 is inexact at extended precision. */
    fldl two_pow_65(%rip)
    fisubl integer_one(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 43
    /* x87 reverse integer subtract: 1 - 2^65 is inexact. */
    fldl two_pow_65(%rip)
    fisubrl integer_one(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 44
    /* x87 integer multiply: +Inf * 0 raises Invalid. */
    fldl positive_infinity_d(%rip)
    fimull integer_zero(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 45
    /* x87 integer divide: 1.0 / 3 is inexact. */
    fld1
    fidivl integer_three(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 46
    /* x87 reverse integer divide: 3 / 10.0 is inexact. */
    fldl inexact_ten(%rip)
    fidivrl integer_three(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 47
    /* x87 1/0: FNSTSW must observe Divide-by-Zero from FCSR Flags. */
    fld1
    fdivl positive_zero_d(%rip)
    stmxcsr report_mxcsr(%rip)
    fnstsw %ax
#elif TEST_CASE == 48
    /* FXSAVE must commit x87 Precision into its saved FSW. */
    fld1
    fdivl inexact_three(%rip)
    fxsave fxsave_area(%rip)
    movw fxsave_area+2(%rip), %ax
    movl fxsave_area+24(%rip), %ecx
    movl %ecx, report_mxcsr(%rip)
#elif TEST_CASE == 49
    /* FXSAVE must commit SSE Invalid into its saved MXCSR. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    fxsave fxsave_area(%rip)
    movw fxsave_area+2(%rip), %ax
    movl fxsave_area+24(%rip), %ecx
    movl %ecx, report_mxcsr(%rip)
#elif TEST_CASE == 50
    /* FXSAVE must commit x87 Divide-by-Zero into its saved FSW. */
    fld1
    fdivl positive_zero_d(%rip)
    fxsave fxsave_area(%rip)
    movw fxsave_area+2(%rip), %ax
    movl fxsave_area+24(%rip), %ecx
    movl %ecx, report_mxcsr(%rip)
#elif TEST_CASE == 51
    /* FXSAVE must commit x87 Invalid from 0/0 into its saved FSW. */
    fldz
    fdivl positive_zero_d(%rip)
    fxsave fxsave_area(%rip)
    movw fxsave_area+2(%rip), %ax
    movl fxsave_area+24(%rip), %ecx
    movl %ecx, report_mxcsr(%rip)
#elif TEST_CASE == 52
    /* XSAVE must commit x87 Precision into its saved FSW. */
    fld1
    fdivl inexact_three(%rip)
    movl $3, %eax
    xorl %edx, %edx
    xsave xsave_area(%rip)
    movw xsave_area+2(%rip), %ax
    movl xsave_area+24(%rip), %ecx
    movl %ecx, report_mxcsr(%rip)
#elif TEST_CASE == 53
    /* XSAVE must commit SSE Invalid into its saved MXCSR. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    movl $3, %eax
    xorl %edx, %edx
    xsave xsave_area(%rip)
    movw xsave_area+2(%rip), %ax
    movl xsave_area+24(%rip), %ecx
    movl %ecx, report_mxcsr(%rip)
#elif TEST_CASE == 54
    /* XSAVEOPT must commit x87 Precision into its saved FSW. */
    fld1
    fdivl inexact_three(%rip)
    movl $3, %eax
    xorl %edx, %edx
    xsaveopt xsave_area(%rip)
    movw xsave_area+2(%rip), %ax
    movl xsave_area+24(%rip), %ecx
    movl %ecx, report_mxcsr(%rip)
#elif TEST_CASE == 55
    /* XSAVEOPT must commit SSE Invalid into its saved MXCSR. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    movl $3, %eax
    xorl %edx, %edx
    xsaveopt xsave_area(%rip)
    movw xsave_area+2(%rip), %ax
    movl xsave_area+24(%rip), %ecx
    movl %ecx, report_mxcsr(%rip)
#elif TEST_CASE == 56
    /* FXRSTOR of clean state must discard pending x87 Precision. */
    fxsave fxsave_area(%rip)
    fld1
    fdivl inexact_three(%rip)
    fxrstor fxsave_area(%rip)
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 57
    /* FXRSTOR of clean state must discard pending SSE Invalid. */
    fxsave fxsave_area(%rip)
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    fxrstor fxsave_area(%rip)
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 58
    /* FXRSTOR must expose flags that are present in the restored image. */
    fxsave fxsave_area(%rip)
    orw $0x20, fxsave_area+2(%rip)
    orl $0x01, fxsave_area+24(%rip)
    fxrstor fxsave_area(%rip)
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 59
    /* FNCLEX clears x87 status only; pending SSE Invalid must survive. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    fnclex
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 60
    /* FNCLEX must clear an x87 Invalid without affecting MXCSR. */
    fldz
    fldz
    fdivp %st, %st(1)
    fnclex
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 61
    /* FNINIT resets x87 only; pending SSE Invalid must survive. */
    xorps %xmm0, %xmm0
    divss %xmm0, %xmm0
    fninit
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 62
    /* AVX scalar divide by zero must set MXCSR Divide-by-Zero. */
    movss exact_one(%rip), %xmm0
    xorps %xmm1, %xmm1
    vdivss %xmm1, %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 63
    /* AVX scalar sqrt(-1) must set MXCSR Invalid. */
    vmovss negative_one_s(%rip), %xmm0
    vsqrtss %xmm0, %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#elif TEST_CASE == 64
    /* AVX packed add of 1 and half ULP must set MXCSR Precision. */
    vmovups packed_one_s(%rip), %xmm0
    vaddps packed_half_ulp_s(%rip), %xmm0, %xmm0
    fnstsw %ax
    stmxcsr report_mxcsr(%rip)
#else
#error unsupported TEST_CASE
#endif

    andw $X87_STATUS_MASK, %ax
    movw %ax, report_x87(%rip)
    andl $MXCSR_STATUS_MASK, report_mxcsr(%rip)

    /* write(1, &report_x87, REPORT_SIZE) */
    movl $__NR_write, %eax
    movl $1, %edi
    leaq report_x87(%rip), %rsi
    movl $REPORT_SIZE, %edx
    syscall

    xorl %edi, %edi
    movl $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .rodata
.align 16
clean_mxcsr:
    .long 0x00001f80
exact_one:
    .long 0x3f800000
inexact_three:
    .quad 0x4008000000000000
inexact_ten:
    .quad 0x4024000000000000
positive_infinity_s:
    .long 0x7f800000
negative_infinity_s:
    .long 0xff800000
positive_infinity_d:
    .quad 0x7ff0000000000000
negative_infinity_d:
    .quad 0xfff0000000000000
negative_one_s:
    .long 0xbf800000
negative_one_d:
    .quad 0xbff0000000000000
positive_zero_d:
    .quad 0x0000000000000000
two_pow_64:
    .quad 0x43f0000000000000
two_pow_65:
    .quad 0x4400000000000000
integer_zero:
    .long 0
integer_one:
    .long 1
integer_three:
    .long 3

.p2align 4
packed_positive_infinity_s:
    .long 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000
packed_negative_infinity_s:
    .long 0xff800000, 0xff800000, 0xff800000, 0xff800000
packed_positive_infinity_d:
    .quad 0x7ff0000000000000, 0x7ff0000000000000
packed_negative_infinity_d:
    .quad 0xfff0000000000000, 0xfff0000000000000
packed_negative_one_s:
    .long 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000
packed_negative_one_d:
    .quad 0xbff0000000000000, 0xbff0000000000000
packed_one_s:
    .long 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
packed_half_ulp_s:
    .long 0x33800000, 0x00000000, 0x00000000, 0x00000000

.section .bss
.align 4
report_x87:
    .zero 2
.zero 2
report_mxcsr:
    .zero 4
.p2align 4
fxsave_area:
    .zero 512
.p2align 6
xsave_area:
    .zero 4096

.section .note.GNU-stack,"",@progbits
