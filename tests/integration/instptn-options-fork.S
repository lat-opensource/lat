/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Exercise instruction-pattern matching, rejection statistics and fork
 * accounting without relying on a guest libc.
 */

    .text
    .globl _start
_start:
    movl $2, %ecx
1:
    subl $1, %ecx
    jne 1b

    /* Unsupported Jcc for the existing SUB pattern. */
    subl $0, %r8d
    jo 2f
2:
    /* Existing SUB pattern is adjacent-only. */
    subl $0, %r8d
    nop
    jne 3f
3:
    /* Existing SHR pattern accepts only an immediate count. */
    movl $1, %r9d
    movl $1, %ecx
    shrl %cl, %r9d
    jne 4f
4:
    /* A masked zero count must not be fused. */
    shrl $0, %r9d
    jne 5f
5:
    movl $57, %eax                 /* fork */
    syscall
    testl %eax, %eax
    je 6f

    movl %eax, %edi
    xorl %esi, %esi
    xorl %edx, %edx
    xorl %r10d, %r10d
    movl $61, %eax                 /* wait4(child, NULL, 0, NULL) */
    syscall
6:
    xorl %edi, %edi
    movl $60, %eax                 /* exit(0) */
    syscall
