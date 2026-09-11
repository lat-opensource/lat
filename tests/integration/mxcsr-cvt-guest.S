/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Freestanding x86_64 guest for CVT/CVTT exception-state tests.
 *
 * Build this source once per TEST_CASE.  It writes a fixed-size binary report
 * to stdout so that the host runner, rather than translated guest branches,
 * decides whether the observed values are correct.
 */

#ifndef TEST_CASE
#error TEST_CASE must select one CVT/CVTT test
#endif

.equ __NR_write, 1
.equ __NR_exit, 60
.equ MXCSR_STATUS_MASK, 0x3f
.equ REPORT_SIZE, 20

.section .text
.global _start
.type _start, @function
_start:
    /* Do not use a guest fenv function as the test fixture. */
    fnclex
    ldmxcsr clean_mxcsr(%rip)

#if TEST_CASE == 1
    /* Exact CVTT: result 7, no exception status. */
    cvttss2si exact_seven(%rip), %eax
    movl %eax, report_results(%rip)
#elif TEST_CASE == 2
    /* Positive infinity: x86 indefinite integer and Invalid. */
    cvttss2si positive_infinity(%rip), %eax
    movl %eax, report_results(%rip)
#elif TEST_CASE == 3
    /* Rounded CVT of qNaN: x86 indefinite integer and Invalid. */
    cvtss2si quiet_nan(%rip), %eax
    movl %eax, report_results(%rip)
#elif TEST_CASE == 4
    /* Invalid in lane 0; the remaining lanes are exact. */
    movups packed_lane0_invalid(%rip), %xmm0
    cvttps2dq %xmm0, %xmm1
    movdqu %xmm1, report_results(%rip)
#elif TEST_CASE == 5
    /* A later exact conversion must not erase the earlier Invalid flag. */
    cvttss2si positive_infinity(%rip), %eax
    movl %eax, report_results(%rip)
    cvttss2si exact_seven(%rip), %eax
    movl %eax, report_results+4(%rip)
#elif TEST_CASE == 6
    /* Truncating 1.5: result 1 and Precision. */
    cvttss2si inexact_one_point_five(%rip), %eax
    movl %eax, report_results(%rip)
#else
#error unsupported TEST_CASE
#endif

    /* Observe immediately after the producer sequence in the same TB. */
    stmxcsr report_status(%rip)
    andl $MXCSR_STATUS_MASK, report_status(%rip)

    /* write(1, &report_status, REPORT_SIZE) */
    movl $__NR_write, %eax
    movl $1, %edi
    leaq report_status(%rip), %rsi
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
exact_seven:
    .long 0x40e00000
positive_infinity:
    .long 0x7f800000
quiet_nan:
    .long 0x7fc00000
inexact_one_point_five:
    .long 0x3fc00000

.align 16
packed_lane0_invalid:
    .long 0x7fc00000
    .long 0x3f800000
    .long 0x40000000
    .long 0x40400000

.section .bss
.align 16
/* Five little-endian uint32_t values: status followed by four results. */
report_status:
    .zero 4
report_results:
    .zero 16

.section .note.GNU-stack,"",@progbits
