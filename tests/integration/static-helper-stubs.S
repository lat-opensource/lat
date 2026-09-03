.equ __NR_write, 1
.equ __NR_rt_sigaction, 13
.equ __NR_rt_sigreturn, 15
.equ __NR_mprotect, 10
.equ __NR_setitimer, 38
.equ __NR_exit, 60
.equ SIGALRM, 14
.equ SA_RESTORER, 0x04000000
.equ RESULT_SIZE, 288
.equ EFLAGS_MASK, 0x8c5

.section .rodata
.align 32
aes_input:
    .byte 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
    .byte 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
aes_key:
    .byte 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08
    .byte 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00
ymm_value:
    .quad 0x0123456789abcdef, 0xfedcba9876543210
    .quad 0x55aa55aa55aa55aa, 0xaa55aa55aa55aa55
pcmp_left:
    .ascii "static-helper-01"
pcmp_right:
    .ascii "static-helper-02"
.align 8
alarm_action:
    .quad alarm_handler
    .quad SA_RESTORER
    .quad signal_restorer
    .quad 0
alarm_timer:
    .quad 0, 1000, 0, 1000
alarm_timer_off:
    .quad 0, 0, 0, 0

.section .bss
.align 32
result:
    .skip RESULT_SIZE
.align 4
alarm_count:
    .long 0

.section .text
.global _start
.type _start, @function
_start:
    mov $__NR_rt_sigaction, %eax
    mov $SIGALRM, %edi
    lea alarm_action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js .Lsignal_failed

    mov $__NR_setitimer, %eax
    xor %edi, %edi
    lea alarm_timer(%rip), %rsi
    xor %edx, %edx
    syscall
    test %rax, %rax
    js .Lsignal_failed

    pxor %xmm0, %xmm0
    pxor %xmm1, %xmm1
    mov $1000000, %r12d
.Lsignal_loop:
    pcmpistri $0, %xmm0, %xmm1
    dec %r12d
    jnz .Lsignal_loop

    mov $__NR_setitimer, %eax
    xor %edi, %edi
    lea alarm_timer_off(%rip), %rsi
    xor %edx, %edx
    syscall
    cmpl $0, alarm_count(%rip)
    je .Lsignal_failed

    lea smc_target(%rip), %rdi
    and $-4096, %rdi
    mov $4096, %esi
    mov $7, %edx
    mov $__NR_mprotect, %eax
    syscall
    test %rax, %rax
    js .Lsmc_failed

    call smc_target
    mov $300, %ecx
.Lsmc_st_loop:
    mov %rcx, smc_data(%rip)
    call smc_target
    cmp $42, %eax
    jne .Lsmc_failed
    dec %ecx
    jnz .Lsmc_st_loop
    cmpq $1, smc_data(%rip)
    jne .Lsmc_failed

    pxor %xmm0, %xmm0
    mov $300, %ecx
.Lsmc_vst_loop:
    movaps %xmm0, smc_data(%rip)
    call smc_target
    cmp $42, %eax
    jne .Lsmc_failed
    dec %ecx
    jnz .Lsmc_vst_loop
    cmpq $0, smc_data(%rip)
    jne .Lsmc_failed
    cmpq $0, smc_data+8(%rip)
    jne .Lsmc_failed

    pxor %xmm0, %xmm0
    pcmpeqb %xmm1, %xmm1
    movdqa %xmm1, %xmm2
    psllq $1, %xmm2
    movdqa %xmm2, %xmm3
    mov $300, %ecx
.Lsmc_vst_x4_loop:
    movaps %xmm0, smc_data(%rip)
    movaps %xmm1, smc_data+16(%rip)
    movaps %xmm2, smc_data+32(%rip)
    movaps %xmm3, smc_data+48(%rip)
    call smc_target
    cmp $42, %eax
    jne .Lsmc_failed
    dec %ecx
    jnz .Lsmc_vst_x4_loop
    cmpq $0, smc_data(%rip)
    jne .Lsmc_failed
    cmpq $0, smc_data+8(%rip)
    jne .Lsmc_failed
    cmpq $-1, smc_data+16(%rip)
    jne .Lsmc_failed
    cmpq $-1, smc_data+24(%rip)
    jne .Lsmc_failed
    cmpq $-2, smc_data+32(%rip)
    jne .Lsmc_failed
    cmpq $-2, smc_data+40(%rip)
    jne .Lsmc_failed
    cmpq $-2, smc_data+48(%rip)
    jne .Lsmc_failed
    cmpq $-2, smc_data+56(%rip)
    jne .Lsmc_failed

    movabs $0x1020304050607080, %rbx
    movabs $0x1122334455667788, %rbp
    movabs $0x2030405060708090, %r8
    movabs $0x30405060708090a0, %r9
    movabs $0x405060708090a0b0, %r10
    movabs $0x5060708090a0b0c0, %r11
    movabs $0x60708090a0b0c0d0, %r12
    movabs $0x708090a0b0c0d0e0, %r13
    movabs $0x8090a0b0c0d0e0f0, %r14
    movabs $0x90a0b0c0d0e0f001, %r15

    fninit
    fld1
    fldpi
    vmovdqu ymm_value(%rip), %ymm15
    movdqa aes_input(%rip), %xmm0
    movdqa aes_key(%rip), %xmm1
    movdqa pcmp_left(%rip), %xmm7

    mov $0x7fffffff, %eax
    add $1, %eax
    stc
    aesenc %xmm1, %xmm0
    pushfq
    pop %rax
    and $EFLAGS_MASK, %eax
    mov %rax, result+64(%rip)

    movdqu %xmm0, result(%rip)
    movdqu %xmm7, result+16(%rip)
    vmovdqu %ymm15, result+32(%rip)

    movdqa aes_input(%rip), %xmm8
    movdqa aes_key(%rip), %xmm9
    pclmulqdq $0, %xmm9, %xmm8

    movdqa pcmp_left(%rip), %xmm2
    movdqa pcmp_right(%rip), %xmm3
    pcmpistri $0x18, %xmm3, %xmm2
    mov %rcx, result+184(%rip)
    pushfq
    pop %rax
    and $EFLAGS_MASK, %eax
    mov %rax, result+192(%rip)

    movdqa pcmp_left(%rip), %xmm2
    movdqa pcmp_right(%rip), %xmm3
    pcmpistrm $0x18, %xmm3, %xmm2
    movdqu %xmm0, result+200(%rip)

    movdqa aes_key(%rip), %xmm4
    aeskeygenassist $0x1b, %xmm4, %xmm5
    movdqu %xmm5, result+216(%rip)
    aesimc %xmm5, %xmm6
    movdqu %xmm6, result+232(%rip)

    mov $16, %eax
    mov $16, %edx
    movdqa pcmp_left(%rip), %xmm2
    movdqa pcmp_right(%rip), %xmm3
    pcmpestri $0x18, %xmm3, %xmm2
    mov %rcx, result+248(%rip)
    pushfq
    pop %rax
    and $EFLAGS_MASK, %eax
    mov %rax, result+256(%rip)

    mov $16, %eax
    mov $16, %edx
    movdqa pcmp_left(%rip), %xmm2
    movdqa pcmp_right(%rip), %xmm3
    pcmpestrm $0x18, %xmm3, %xmm2
    movdqu %xmm0, result+264(%rip)

    mov %rbx, result+72(%rip)
    mov %rbp, result+80(%rip)
    mov %r8, result+88(%rip)
    mov %r9, result+96(%rip)
    mov %r10, result+104(%rip)
    mov %r11, result+112(%rip)
    mov %r12, result+120(%rip)
    mov %r13, result+128(%rip)
    mov %r14, result+136(%rip)
    mov %r15, result+144(%rip)

    fnstsw result+152(%rip)
    fstpt result+160(%rip)
    fstpt result+170(%rip)
    vzeroupper

    mov $__NR_write, %eax
    mov $1, %edi
    lea result(%rip), %rsi
    mov $RESULT_SIZE, %edx
    syscall
    cmp $RESULT_SIZE, %eax
    jne .Lwrite_failed

    xor %edi, %edi
    jmp .Lexit
.Lwrite_failed:
    mov $1, %edi
    jmp .Lexit
.Lsignal_failed:
    mov $2, %edi
    jmp .Lexit
.Lsmc_failed:
    mov $3, %edi
.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.type alarm_handler, @function
alarm_handler:
    incl alarm_count(%rip)
    ret
.size alarm_handler, .-alarm_handler

.type signal_restorer, @function
signal_restorer:
    mov $__NR_rt_sigreturn, %eax
    syscall
.size signal_restorer, .-signal_restorer

.section .smc_text,"ax",@progbits
.balign 65536
.type smc_target, @function
smc_target:
    mov $42, %eax
    ret
.size smc_target, .-smc_target
.balign 16
smc_data:
    .fill 64, 1, 0

.section .note.GNU-stack,"",@progbits
