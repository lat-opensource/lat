.equ __NR_write, 1
.equ __NR_exit, 60
#ifndef ITERATIONS
#define ITERATIONS 4096
#endif

.macro NEXT_RANDOM out
    imul %r15, %r13
    add $1, %r13
    mov %r13, \out
.endm

/*
 * Fold the architected flags, all important SETcc consumers, and RAX into
 * one family checksum.  SETcc does not change EFLAGS, so all consumers see
 * the state captured by PUSHFQ.
 */
.macro RECORD
    pushfq
    pop %rdx
    seto 0(%rsp)
    setc 1(%rsp)
    setz 2(%rsp)
    sets 3(%rsp)
    setp 4(%rsp)
    seta 5(%rsp)
    setg 6(%rsp)
    setl 7(%rsp)
    and $0x8d5, %edx
#ifdef TRACE_RECORD
    mov %rdx, 8(%rsp)
    mov %rax, 16(%rsp)
    mov $__NR_write, %eax
    mov $1, %edi
    lea 0(%rsp), %rsi
    mov $24, %edx
    syscall
    movabs $1099511628211, %r11
#else
    xor %rdx, %r12
    imul %r11, %r12
    mov 0(%rsp), %rdx
    xor %rdx, %r12
    imul %r11, %r12
    xor %rax, %r12
    imul %r11, %r12
#endif
.endm

.macro BEGIN_FAMILY
    movabs $0xcbf29ce484222325, %r12
    mov $ITERATIONS, %r14d
.endm

.macro END_FAMILY offset
#ifndef TRACE_RECORD
    mov %r12, \offset(%rsp)
    mov $__NR_write, %eax
    mov $1, %edi
    lea \offset(%rsp), %rsi
    mov $8, %edx
    syscall
    movabs $1099511628211, %r11
#endif
.endm

.section .text
.global _start
.type _start, @function
_start:
    sub $128, %rsp
    movabs $0x9e3779b97f4a7c15, %r13
    movabs $6364136223846793005, %r15
    movabs $1099511628211, %r11

    /* 64-bit arithmetic, including carry-in and INC/DEC CF preservation. */
    BEGIN_FAMILY
.Lalu64:
    NEXT_RANDOM %rax
    NEXT_RANDOM %rbx
    mov %rax, %r10
    add %rbx, %rax
    RECORD
    mov %r10, %rax
    sub %rbx, %rax
    RECORD
    mov %r10, %rax
    cmp %rbx, %rax
    RECORD
    mov %r10, %rax
    test %rbx, %rax
    RECORD
    mov %r10, %rax
    clc
    adc %rbx, %rax
    RECORD
    mov %r10, %rax
    stc
    adc %rbx, %rax
    RECORD
    mov %r10, %rax
    clc
    sbb %rbx, %rax
    RECORD
    mov %r10, %rax
    stc
    sbb %rbx, %rax
    RECORD
    mov %r10, %rax
    stc
    inc %rax
    RECORD
    mov %r10, %rax
    clc
    dec %rax
    RECORD
    mov %r10, %rax
    and %rbx, %rax
    RECORD
    mov %r10, %rax
    or %rbx, %rax
    RECORD
    mov %r10, %rax
    xor %rbx, %rax
    RECORD
    mov %r10, %rax
    neg %rax
    RECORD
    dec %r14d
    jne .Lalu64
    END_FAMILY 32

    /* 32-bit arithmetic and zero-extension behavior. */
    BEGIN_FAMILY
.Lalu32:
    NEXT_RANDOM %rax
    NEXT_RANDOM %rbx
    mov %eax, %r10d
    add %ebx, %eax
    RECORD
    mov %r10d, %eax
    sub %ebx, %eax
    RECORD
    mov %r10d, %eax
    cmp %ebx, %eax
    RECORD
    mov %r10d, %eax
    clc
    adc %ebx, %eax
    RECORD
    mov %r10d, %eax
    stc
    sbb %ebx, %eax
    RECORD
    mov %r10d, %eax
    and %ebx, %eax
    RECORD
    mov %r10d, %eax
    or %ebx, %eax
    RECORD
    mov %r10d, %eax
    xor %ebx, %eax
    RECORD
    mov %r10d, %eax
    neg %eax
    RECORD
    mov %r10d, %eax
    stc
    inc %eax
    RECORD
    mov %r10d, %eax
    clc
    dec %eax
    RECORD
    dec %r14d
    jne .Lalu32
    END_FAMILY 40

    /* 16-bit arithmetic; upper bits deliberately remain live. */
    BEGIN_FAMILY
.Lalu16:
    NEXT_RANDOM %rax
    NEXT_RANDOM %rbx
    mov %rax, %r10
    add %bx, %ax
    RECORD
    mov %r10, %rax
    sub %bx, %ax
    RECORD
    mov %r10, %rax
    cmp %bx, %ax
    RECORD
    mov %r10, %rax
    stc
    adc %bx, %ax
    RECORD
    mov %r10, %rax
    stc
    sbb %bx, %ax
    RECORD
    mov %r10, %rax
    and %bx, %ax
    RECORD
    mov %r10, %rax
    or %bx, %ax
    RECORD
    mov %r10, %rax
    xor %bx, %ax
    RECORD
    mov %r10, %rax
    neg %ax
    RECORD
    mov %r10, %rax
    stc
    inc %ax
    RECORD
    mov %r10, %rax
    clc
    dec %ax
    RECORD
    dec %r14d
    jne .Lalu16
    END_FAMILY 48

    /* 8-bit arithmetic exercises partial-register handling. */
    BEGIN_FAMILY
.Lalu8:
    NEXT_RANDOM %rax
    NEXT_RANDOM %rbx
    mov %rax, %r10
    add %bl, %al
    RECORD
    mov %r10, %rax
    sub %bl, %al
    RECORD
    mov %r10, %rax
    cmp %bl, %al
    RECORD
    mov %r10, %rax
    stc
    adc %bl, %al
    RECORD
    mov %r10, %rax
    stc
    sbb %bl, %al
    RECORD
    mov %r10, %rax
    and %bl, %al
    RECORD
    mov %r10, %rax
    or %bl, %al
    RECORD
    mov %r10, %rax
    xor %bl, %al
    RECORD
    mov %r10, %rax
    neg %al
    RECORD
    mov %r10, %rax
    stc
    inc %al
    RECORD
    mov %r10, %rax
    clc
    dec %al
    RECORD
    dec %r14d
    jne .Lalu8
    END_FAMILY 56

    /* Signed multiply: result plus architecturally defined CF/OF. */
    BEGIN_FAMILY
.Limul:
    NEXT_RANDOM %rax
    NEXT_RANDOM %rbx
    mov %rax, %r10
    imul %rbx, %rax
    RECORD
    mov %r10d, %eax
    imul %ebx, %eax
    RECORD
    mov %r10, %rax
    imul %bx, %ax
    RECORD
    dec %r14d
    jne .Limul
    END_FAMILY 60

    /* CMOVcc consumers, using the same compare before every condition. */
    BEGIN_FAMILY
.Lcmov:
    NEXT_RANDOM %r10
    NEXT_RANDOM %rbx
    movabs $0x1122334455667788, %r8
    movabs $0x8877665544332211, %r9
#define CMOV_RECORD64(cc) \
    mov %r10, %rax; cmp %rbx, %rax; mov %r8, %rax; cmov##cc %r9, %rax; RECORD
    CMOV_RECORD64(o)
    CMOV_RECORD64(b)
    CMOV_RECORD64(e)
    CMOV_RECORD64(s)
    CMOV_RECORD64(p)
    CMOV_RECORD64(a)
    CMOV_RECORD64(g)
    CMOV_RECORD64(l)
#undef CMOV_RECORD64
#define CMOV_RECORD32(cc) \
    mov %r10d, %eax; cmp %ebx, %eax; mov %r8d, %eax; cmov##cc %r9d, %eax; RECORD
    CMOV_RECORD32(o)
    CMOV_RECORD32(b)
    CMOV_RECORD32(e)
    CMOV_RECORD32(s)
    CMOV_RECORD32(p)
    CMOV_RECORD32(a)
    CMOV_RECORD32(g)
    CMOV_RECORD32(l)
#undef CMOV_RECORD32
    dec %r14d
    jne .Lcmov
    END_FAMILY 62

    /* Variable shifts include masked zero, one, width, and large counts. */
    BEGIN_FAMILY
.Lshift:
    NEXT_RANDOM %rax
    mov %r14b, %cl
    cmp $0x13579bdf, %r14d
    shl %cl, %rax
    RECORD
    NEXT_RANDOM %rax
    mov %r14b, %cl
    cmp $0x2468ace0, %r14d
    shr %cl, %rax
    RECORD
    NEXT_RANDOM %rax
    mov %r14b, %cl
    cmp $0x10203040, %r14d
    sar %cl, %rax
    RECORD
    NEXT_RANDOM %rax
    mov %r14b, %cl
    cmp $0x55667788, %r14d
    shl %cl, %eax
    RECORD
    NEXT_RANDOM %rax
    mov %r14b, %cl
    cmp $0x11223344, %r14d
    shr %cl, %ax
    RECORD
    NEXT_RANDOM %rax
    mov %r14b, %cl
    cmp $0x44332211, %r14d
    sar %cl, %al
    RECORD
    dec %r14d
    jne .Lshift
    END_FAMILY 64

    /* Rotates define CF for nonzero counts and OF for count one. */
    BEGIN_FAMILY
.Lrotate:
    NEXT_RANDOM %rax
    mov %r14b, %cl
    cmp $0x13579bdf, %r14d
    rol %cl, %rax
    RECORD
    NEXT_RANDOM %rax
    mov %r14b, %cl
    cmp $0x2468ace0, %r14d
    ror %cl, %rax
    RECORD
    NEXT_RANDOM %rax
    mov %r14b, %cl
    cmp $0x10203040, %r14d
    rol %cl, %eax
    RECORD
    NEXT_RANDOM %rax
    mov %r14b, %cl
    cmp $0x55667788, %r14d
    ror %cl, %ax
    RECORD
    dec %r14d
    jne .Lrotate
    END_FAMILY 72

    /* Register BT with signed and out-of-range bit indexes. */
    BEGIN_FAMILY
.Lbt:
    NEXT_RANDOM %rax
    NEXT_RANDOM %rbx
    cmp $0x55aa55aa, %r14d
    bt %rbx, %rax
    RECORD
    NEXT_RANDOM %rax
    mov %r14d, %ebx
    neg %ebx
    cmp $0xaa55aa55, %r14d
    bt %rbx, %rax
    RECORD
    dec %r14d
    jne .Lbt
    END_FAMILY 80

    /* Ordered/unordered scalar floating comparisons, including random NaNs. */
    BEGIN_FAMILY
.Lfp:
    NEXT_RANDOM %rax
    NEXT_RANDOM %rbx
    movq %rax, %xmm0
    movq %rbx, %xmm1
    ucomisd %xmm1, %xmm0
    RECORD
    movd %eax, %xmm0
    movd %ebx, %xmm1
    ucomiss %xmm1, %xmm0
    RECORD
    dec %r14d
    jne .Lfp
    END_FAMILY 88

    mov $__NR_exit, %eax
    xor %edi, %edi
    syscall
.size _start, .-_start
