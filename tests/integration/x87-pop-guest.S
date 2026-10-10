/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Freestanding x86_64 guest for x87 arithmetic-and-pop semantics.
 *
 * Each case leaves the arithmetic result in ST(0) and a sentinel in ST(1),
 * then stores both values.  This checks both the destination register and
 * that the arithmetic instruction pops the x87 stack exactly once.
 */

#ifndef TEST_CASE
#error TEST_CASE must select one x87 pop test
#endif

.equ __NR_write, 1
.equ __NR_exit, 60
.equ REPORT_SIZE, 16

.section .text
.global _start
.type _start, @function
_start:
    fninit

    /* Build ST(0)=right, ST(1)=left, ST(2)=sentinel. */
    fldl sentinel(%rip)
    fldl left(%rip)
    fldl right(%rip)

#if TEST_CASE == 1
    /* ST(1) = 2.0 + 4.0, then pop: ST(0)=6.0, ST(1)=9.0. */
    faddp %st, %st(1)
#elif TEST_CASE == 2
    /* ST(1) = 2.0 * 4.0, then pop: ST(0)=8.0, ST(1)=9.0. */
    fmulp %st, %st(1)
#elif TEST_CASE == 3
    /* ST(1) = 2.0 - 4.0, then pop: ST(0)=-2.0, ST(1)=9.0. */
    /* AT&T fsubrp encodes the architectural FSUBP st(1), st(0). */
    fsubrp %st, %st(1)
#elif TEST_CASE == 4
    /* ST(1) = 2.0 / 4.0, then pop: ST(0)=0.5, ST(1)=9.0. */
    /* AT&T fdivrp encodes the architectural FDIVP st(1), st(0). */
    fdivrp %st, %st(1)
#elif TEST_CASE == 5
    /* ST(1) = 4.0 - 2.0, then pop: ST(0)=2.0, ST(1)=9.0. */
    /* AT&T fsubp encodes the architectural FSUBRP st(1), st(0). */
    fsubp %st, %st(1)
#elif TEST_CASE == 6
    /* ST(1) = 4.0 / 2.0, then pop: ST(0)=2.0, ST(1)=9.0. */
    /* AT&T fdivp encodes the architectural FDIVRP st(1), st(0). */
    fdivp %st, %st(1)
#else
#error unsupported TEST_CASE
#endif

    fstpl report_first(%rip)
    fstpl report_second(%rip)

    /* write(1, &report_first, REPORT_SIZE) */
    movl $__NR_write, %eax
    movl $1, %edi
    leaq report_first(%rip), %rsi
    movl $REPORT_SIZE, %edx
    syscall

    xorl %edi, %edi
    movl $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .rodata
.align 8
sentinel:
    .quad 0x4022000000000000       /* 9.0 */
left:
    .quad 0x4000000000000000       /* 2.0 */
right:
    .quad 0x4010000000000000       /* 4.0 */

.section .bss
.align 8
report_first:
    .zero 8
report_second:
    .zero 8

.section .note.GNU-stack,"",@progbits
