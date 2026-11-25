/* Discover a PC thunk between decoding and translating a terminal CALL.
 * The first TU claims late_continuation through a profiled Jcc edge.
 * The runner can suppress its predecessor admission to keep that TU separate.
 * The late_caller TU decodes late_call before its other Jcc successor,
 * late_thunk. Inlining late_call therefore needs a cross-TU continuation.
 * The runner clears the thunk table before AOT generation.
 * Build as an i386 static ELF with -m32 -nostdlib -static -no-pie.
 */
        .text
        .globl _start, late_caller, late_call, late_continuation, late_thunk
_start:
        movl $late_continuation, %ecx
        call late_continuation
        testl %ecx, %ecx
        jz late_continuation
        movl $100, %ebp
1:      call late_caller
        decl %ebp
        jnz 1b
        xorl %ebx, %ebx
        jmp quit
fail:
        movl $1, %ebx
quit:
        movl $1, %eax
        int $0x80
        .org 0x4000
late_caller:
        testl %ebp, %ebp
        jz late_thunk
late_call:
        call late_thunk
late_continuation:
        cmpl $late_continuation, %ecx
        jne fail
        ret
        .org 0x8000
late_thunk:
        movl (%esp), %ecx
        ret
        .section .note.GNU-stack,"",@progbits
