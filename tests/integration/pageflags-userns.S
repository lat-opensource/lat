/* SPDX-License-Identifier: GPL-2.0-only */
/* Retiring a page-tree node must preserve a single-threaded guest. */
.globl _start
.text
_start:
    mov $9, %eax
    xor %edi, %edi
    mov $16384, %esi
    mov $3, %edx
    mov $0x22, %r10d
    mov $-1, %r8d
    xor %r9d, %r9d
    syscall
    test %rax, %rax
    js fail
    mov %rax, %rdi
    mov $16384, %esi
    mov $11, %eax
    syscall
    test %rax, %rax
    js fail
    mov $272, %eax
    mov $0x10000000, %edi
    syscall
    test %rax, %rax
    js fail
    xor %edi, %edi
    jmp quit
fail:
    mov $1, %edi
quit:
    mov $60, %eax
    syscall
