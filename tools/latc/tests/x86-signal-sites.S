.text

.macro signal_prepare_state
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    pushfq
    movabs $0x5465768710213243, %rax
    movq %rax, %xmm0
    movabs $0xdcedfe0f98a9bacb, %rax
    movq %rax, %xmm15
    punpcklqdq %xmm15, %xmm0
    movdqa %xmm0, %xmm1
    movdqa %xmm0, %xmm2
    movdqa %xmm0, %xmm3
    movdqa %xmm0, %xmm4
    movdqa %xmm0, %xmm5
    movdqa %xmm0, %xmm6
    movdqa %xmm0, %xmm7
    movdqa %xmm0, %xmm8
    movdqa %xmm0, %xmm9
    movdqa %xmm0, %xmm10
    movdqa %xmm0, %xmm11
    movdqa %xmm0, %xmm12
    movdqa %xmm0, %xmm13
    movdqa %xmm0, %xmm14
    movdqa %xmm0, %xmm15
    movabs $0x0102030405060708, %rax
    movabs $0x1112131415161718, %rbx
    xor %ecx, %ecx
    movabs $0x2122232425262728, %rdx
    movabs $0x3132333435363738, %rsi
    movabs $0x4142434445464748, %rdi
    movabs $0x5152535455565758, %rbp
    movabs $0x6162636465666768, %r8
    movabs $0x7172737475767778, %r9
    mov %rsp, %r10
    movabs $0x9192939495969798, %r11
    movabs $0xa1a2a3a4a5a6a7a8, %r12
    movabs $0xb1b2b3b4b5b6b7b8, %r13
    movabs $0xc1c2c3c4c5c6c7c8, %r14
    movabs $0xd1d2d3d4d5d6d7d8, %r15
    pushq $0xcd7
    popfq
.endm

.macro signal_verify_sigreturn
    pushfq
    pop %r11
    cmp %r10, %rsp
    jne 98f
    movabs $0xe1e2e3e4e5e6e7e8, %rax
    cmp %rax, %r15
    jne 99f
    and $0xcd5, %r11d
    cmp $1, %r11d
    jne 96f
    movabs $0x4b5a69780f1e2d3c, %rax
    movq %rax, %xmm0
    movabs $0xc3d2e1f08796a5b4, %rax
    movq %rax, %xmm1
    punpcklqdq %xmm1, %xmm0
    pcmpeqb %xmm15, %xmm0
    pmovmskb %xmm0, %eax
    cmp $0xffff, %eax
    jne 95f
    xor %eax, %eax
    jmp 97f
95:
    mov $3, %eax
    jmp 97f
96:
    mov $2, %eax
    jmp 97f
99:
    mov $1, %eax
97:
    popfq
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    ret
98:
    ud2
.endm

.global signal_boundary_site
.global signal_boundary_fault
.global signal_boundary_resume
.type signal_boundary_site,@function
signal_boundary_site:
    signal_prepare_state
    jmp signal_boundary_fault
.size signal_boundary_site, .-signal_boundary_site
.p2align 4
signal_boundary_fault:
    div %rcx
signal_boundary_resume:
    signal_verify_sigreturn

.global signal_internal_site
.global signal_internal_fault
.global signal_internal_resume
.type signal_internal_site,@function
signal_internal_site:
    signal_prepare_state
    nop
signal_internal_fault:
    div %rcx
signal_internal_resume:
    signal_verify_sigreturn
.size signal_internal_site, .-signal_internal_site

.global signal_end_site
.global signal_end_fault
.global signal_end_resume
.type signal_end_site,@function
signal_end_site:
    signal_prepare_state
signal_end_fault:
    div %rcx
.size signal_end_site, .-signal_end_site
.p2align 4
signal_end_resume:
    signal_verify_sigreturn

.global signal_helper_site
.global signal_helper_fault
.global signal_helper_resume
.type signal_helper_site,@function
signal_helper_site:
    signal_prepare_state
signal_helper_fault:
    ud2
signal_helper_resume:
    signal_verify_sigreturn
.size signal_helper_site, .-signal_helper_site

.section .note.GNU-stack,"",@progbits
