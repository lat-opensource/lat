.global _start
.type _start, @function
.section .text
_start:
    mov %rsp, %r12
    mov $0x12345678, %r15d

    xor %eax, %eax
    xor %ecx, %ecx
    cpuid
    cmp $0xd, %eax
    jne failed
    cmp %r12, %rsp
    jne failed
    cmp $0x12345678, %r15d
    jne failed
    cmp $0x68747541, %ebx
    jne failed
    cmp $0x69746e65, %edx
    jne failed
    cmp $0x444d4163, %ecx
    jne failed

    movdqu string_a(%rip), %xmm0
    movdqu string_a(%rip), %xmm1
    pcmpistri $0x08, %xmm1, %xmm0
    setc %r8b
    setz %r9b
    sets %r10b
    cmp %r12, %rsp
    jne failed
    cmp $0x12345678, %r15d
    jne failed
    test %ecx, %ecx
    jne failed
    cmp $1, %r8b
    jne failed
    cmp $1, %r9b
    jne failed
    cmp $1, %r10b
    jne failed

    movdqu string_a(%rip), %xmm0
    movdqu string_a(%rip), %xmm1
    pcmpistrm $0x08, %xmm1, %xmm0
    cmp %r12, %rsp
    jne failed
    cmp $0x12345678, %r15d
    jne failed
    movq %xmm0, %rax
    cmp $0xffff, %rax
    jne failed

    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $message_end-message, %edx
    syscall
    mov $60, %eax
    mov $42, %edi
    syscall

failed:
    mov $60, %eax
    mov $98, %edi
    syscall
    ud2
.size _start, .-_start

.section .rodata
.align 16
string_a:
    .byte 'a', 'b', 'c', 0, 0, 0, 0, 0
    .byte 0, 0, 0, 0, 0, 0, 0, 0
message:
    .ascii "HELPERS OK\n"
message_end:
