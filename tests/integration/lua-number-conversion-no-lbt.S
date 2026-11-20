.equ __NR_exit, 60
.equ ITERATIONS, 100000

.section .text
.global _start
.type _start, @function
_start:
    mov $ITERATIONS, %ecx

.Lloop:
    movabs $0xc0b2340000000000, %rax
    movq %rax, %xmm0
    cvttsd2si %xmm0, %rax
    cmp $-4660, %rax
    jne .Lfail

    movabs $0x4338000000000000, %rax
    movq %rax, %xmm1
    addsd %xmm0, %xmm1
    movq %xmm1, %rax
    cmp $0xffffedcc, %eax
    jne .Lfail

    dec %ecx
    jne .Lloop
    xor %edi, %edi
    jmp .Lexit

.Lfail:
    mov $1, %edi

.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start
