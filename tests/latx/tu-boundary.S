/* Build with gcc -m32/-m64 -nostdlib -static -no-pie.
 * f0, p1..p14 and f1 occupy 16 dependency pages in a 64-page address window.
 * Adding f2 would require dependency page 17, so it must remain outside the
 * TU.  Repeated calls exercise chaining after the target is resident.  argv[1]
 * selects an SMC victim:
 * 1 = second member, 2 = external TU.  Success is exit status zero.
 */
        .text
        .globl _start
_start:
#ifdef __x86_64__
        xorl %ebp, %ebp
        cmpq $2, (%rsp)
        jb 1f
        movq 16(%rsp), %rax
        movzbl (%rax), %ebp
1:
#else
        xorl %ebp, %ebp
        cmpl $2, (%esp)
        jb 1f
        movl 8(%esp), %eax
        movzbl (%eax), %ebp
1:
#endif
        cmpl $'4', %ebp
        jne 4f
        movl $1, %eax
        movl $1, %edx
        call f1
        cmpl $3, %eax
        jne fail
4:
        movl $8, %esi
2:
        movl $3, %edx
        call f0
        cmpl $3, %eax
        jne fail
        decl %esi
        jnz 2b
        cmpl $'1', %ebp
        je member
        cmpl $'2', %ebp
        jne pass
        movl $f2_imm, %edi
        movl $2, %ebp
        jmp modify
member:
        movl $f1_imm, %edi
        movl $4, %ebp
modify:
#ifdef __x86_64__
        movl %edi, %r12d
        andl $-4096, %edi
        movl $4096, %esi
        movl $7, %edx
        movl $10, %eax
        syscall
        testl %eax, %eax
        jne fail
        movb %bpl, (%r12)
#else
        movl %edi, %ebx
        andl $-4096, %ebx
        movl $4096, %ecx
        movl $7, %edx
        movl $125, %eax
        int $0x80
        testl %eax, %eax
        jne fail
        movl %ebp, %eax
        movb %al, (%edi)
#endif
        movl $8, %esi
3:
        movl $3, %edx
        call f0
        cmpl $5, %eax
        jne fail
        decl %esi
        jnz 3b
pass:
        xorl %ebx, %ebx
        jmp quit
fail:
        movl $1, %ebx
quit:
#ifdef __x86_64__
        movl %ebx, %edi
        movl $60, %eax
        syscall
#else
        movl $1, %eax
        int $0x80
#endif
        .org 0x1100
f0:
        movl $1, %eax
        jmp p1

        .org 0x2100
p1:     jmp p2
        .org 0x3100
p2:     jmp p3
        .org 0x4100
p3:     jmp p4
        .org 0x5100
p4:     jmp p5
        .org 0x6100
p5:     jmp p6
        .org 0x7100
p6:     jmp p7
        .org 0x8100
p7:     jmp p8
        .org 0x9100
p8:     jmp p9
        .org 0xa100
p9:     jmp p10
        .org 0xb100
p10:    jmp p11
        .org 0xc100
p11:    jmp p12
        .org 0xd100
p12:    jmp p13
        .org 0xe100
p13:    jmp p14
        .org 0xf100
p14:    jmp f1

        .org 0x10100
f2:
        .byte 0x83, 0xc0
f2_imm:
        .byte 0
        ret

        .org 0x40100
f1:
        .byte 0x83, 0xc0
f1_imm:
        .byte 2
        decl %edx
        jz f2
        .globl f1_jump
f1_jump:
        jmp f2
        .section .note.GNU-stack,"",@progbits
