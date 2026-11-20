.equ __NR_write, 1
.equ __NR_exit, 60

/* Each Jcc terminates a TB.  MOV and JMP preserve EFLAGS for the next Jcc. */
.macro CHECK condition, offset
    j\condition .Ltaken\@
    movb $0, \offset(%rsp)
    jmp .Ldone\@
.Ltaken\@:
    movb $1, \offset(%rsp)
.Ldone\@:
.endm

.macro CHECK_ALL base
    CHECK o,  \base+0
    CHECK b,  \base+1
    CHECK e,  \base+2
    CHECK s,  \base+3
    CHECK p,  \base+4
    CHECK a,  \base+5
    CHECK g,  \base+6
    CHECK l,  \base+7
.endm

.section .text
.global _start
.type _start, @function
_start:
    sub $256, %rsp

    movabs $0x7fffffffffffffff, %rax
    add $1, %rax
    CHECK_ALL 0

    xor %eax, %eax
    sub $1, %rax
    CHECK_ALL 8

    movabs $0x1122334455667788, %rax
    cmp %rax, %rax
    CHECK_ALL 16

    xor %eax, %eax
    test %rax, %rax
    CHECK_ALL 24

    mov $-1, %rax
    mov $1, %rbx
    stc
    adc %rbx, %rax
    CHECK_ALL 32

    xor %eax, %eax
    mov $-1, %rbx
    stc
    sbb %rbx, %rax
    CHECK_ALL 40

    movabs $0x4000000000000000, %rax
    mov $1, %cl
    shl %cl, %rax
    CHECK_ALL 48

    movabs $0x8000000000000001, %rax
    mov $1, %cl
    ror %cl, %rax
    CHECK_ALL 56

    movabs $0x0123456789abcdef, %rax
    mov $68, %rbx
    bt %rbx, %rax
    CHECK_ALL 64

    movabs $0x3ff0000000000000, %rax
    movabs $0x4000000000000000, %rbx
    movq %rax, %xmm0
    movq %rbx, %xmm1
    ucomisd %xmm1, %xmm0
    CHECK_ALL 72

    movabs $0x7ff8000000000001, %rax
    movabs $0x3ff0000000000000, %rbx
    movq %rax, %xmm0
    movq %rbx, %xmm1
    ucomisd %xmm1, %xmm0
    CHECK_ALL 80

    movabs $0x4000000000000000, %rax
    mov $1, %cl
    rol %cl, %rax
    CHECK_ALL 88

    mov $1, %rax
    mov $1, %cl
    ror %cl, %rax
    CHECK_ALL 96

    movq $0, 120(%rsp)
    mov $3808, %r14d
    cmp $0x10203040, %r14d
    CHECK_ALL 104

    mov $0x1000, %r14d
    movabs $0x1122334455667788, %rax
    xor %ecx, %ecx
    cmp $0x13579bdf, %r14d
    shl %cl, %rax
    CHECK_ALL 112

    mov $3808, %r14d
    movabs $0x1122334455667788, %rax
    xor %ecx, %ecx
    cmp $0x10203040, %r14d
    rol %cl, %eax
    CHECK b, 120

    movq $0, 128(%rsp)
    mov $3808, %r14d
    movabs $0x1122334455667788, %rax
    xor %ecx, %ecx
    cmp $0x10203040, %r14d
    ror %cl, %ax
    CHECK b, 128

    movq $0, 136(%rsp)
    mov $-1, %rax
    mov $2, %rbx
    mul %rbx
    CHECK o, 136
    CHECK b, 137

    movq $0, 144(%rsp)
    mov $3, %rax
    mov $4, %rbx
    mul %rbx
    CHECK o, 144
    CHECK b, 145

    movq $0, 152(%rsp)
    movabs $0x7fffffffffffffff, %rax
    mov $2, %rbx
    imul %rbx, %rax
    CHECK o, 152
    CHECK b, 153

    movq $0, 160(%rsp)
    mov $3, %rax
    mov $4, %rbx
    imul %rbx, %rax
    CHECK o, 160
    CHECK b, 161

    mov $__NR_write, %eax
    mov $1, %edi
    mov %rsp, %rsi
    mov $168, %edx
    syscall

    mov $__NR_exit, %eax
    xor %edi, %edi
    syscall
.size _start, .-_start
