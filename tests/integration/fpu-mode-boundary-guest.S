/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Check that softfpu exception isolation leaves other modes unchanged. */
.section .text
.global _start
.type _start, @function
_start:
    fninit
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
.section .note.GNU-stack,"",@progbits
