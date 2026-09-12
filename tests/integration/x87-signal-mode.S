/* SPDX-License-Identifier: GPL-2.0-only
 * Static x86-64 Linux guest.
 * Exit 0: fld1 survives getpid after SIGUSR1. Exit 22: value corrupted.
 * stdout is two raw 80-bit values: before and after the getpid syscall.
 * CASE: 0 = negative NaNs, 1 = negative infinities, 2 = second signal,
 *       3 = MMX restore, 4 = x87 handler entry and rounding restore.
 */
#ifndef CASE
#define CASE 0
#endif
.section .data
.balign 8
action:
    .quad handler, 0x04000000, restorer, 0
negative_nan:
#if CASE == 1
    .quad 0x8000000000000000
#else
    .quad 0xc123450000000000
#endif
    .word 0xffff
mmx_value:
    .quad 0x0123456789abcdef
round_down: .word 0x077f
fraction: .quad 0x3ffc000000000000 /* double 1.75 */
.section .bss
.balign 16
observed: .skip 20
seen: .skip 4
handler_cw: .skip 2
handler_sw: .skip 2
integer: .skip 4
.section .text
.global _start
_start:
    mov $13, %eax
    mov $10, %edi
    lea action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js setup_failed
    mov $39, %eax
    syscall
    mov %eax, %r12d
    fninit
#if CASE == 3
    movq mmx_value(%rip), %mm0
    movq %mm0, %mm1
    movq %mm0, %mm2
    movq %mm0, %mm3
    movq %mm0, %mm4
    movq %mm0, %mm5
    movq %mm0, %mm6
    movq %mm0, %mm7
#elif CASE == 4
    fldcw round_down(%rip)
    fld1
#else
    .rept 8
    fldt negative_nan(%rip)
    .endr
#endif
    mov $62, %eax
    mov %r12d, %edi
    mov $10, %esi
    syscall
    cmpl $1, seen(%rip)
    jne setup_failed
#if CASE == 3
    mov mmx_value(%rip), %rdx
    .irp n,0,1,2,3,4,5,6,7
    movq %mm\n, %rax
    cmp %rdx, %rax
    jne corrupted
    .endr
    emms
    xor %edi, %edi
    jmp finish
#elif CASE == 4
    cmpw $0x037f, handler_cw(%rip)
    jne precheck_failed
    cmpw $0, handler_sw(%rip)
    jne precheck_failed
    fstpt observed(%rip)
    movabs $0x8000000000000000, %rax
    cmp %rax, observed(%rip)
    jne corrupted
    cmpw $0x3fff, observed+8(%rip)
    jne corrupted
    fldl fraction(%rip)
    fistpl integer(%rip)
    cmpl $1, integer(%rip)
    jne corrupted
    xor %edi, %edi
    jmp finish
#else
    fstp %st(0)
    fld1
    fstpt observed(%rip)
    fldt observed(%rip)
#if CASE == 2
    mov $62, %eax
    mov %r12d, %edi
    mov $10, %esi
    syscall
    cmpl $2, seen(%rip)
    jne setup_failed
#endif
    mov $39, %eax
    syscall
    fstpt observed+10(%rip)
    mov $1, %eax
    mov $1, %edi
    lea observed(%rip), %rsi
    mov $20, %edx
    syscall
    movabs $0x8000000000000000, %rax
    cmp %rax, observed(%rip)
    jne precheck_failed
    cmpw $0x3fff, observed+8(%rip)
    jne precheck_failed
    cmp %rax, observed+10(%rip)
    jne corrupted
    cmpw $0x3fff, observed+18(%rip)
    jne corrupted
    xor %edi, %edi
    jmp finish
#endif
setup_failed:
    mov $20, %edi
    jmp finish
precheck_failed:
    mov $21, %edi
    jmp finish
corrupted:
    mov $22, %edi
finish:
    mov $60, %eax
    syscall
handler:
#if CASE == 3
    .irp n,0,1,2,3,4,5,6,7
    pxor %mm\n, %mm\n
    .endr
    emms
    fninit
#elif CASE == 4
    fnstcw handler_cw(%rip)
    fnstsw handler_sw(%rip)
    fninit
#endif
    incl seen(%rip)
    ret
restorer:
    mov $15, %eax
    syscall
.section .note.GNU-stack,"",@progbits
