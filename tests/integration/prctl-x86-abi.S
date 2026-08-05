.equ __NR_rt_sigaction, 13
.equ __NR_mmap, 9
.equ __NR_mprotect, 10
.equ __NR_prctl, 157
.equ __NR_exit, 60

.equ EFAULT, 14
.equ EINVAL, 22
.equ EACCES, 13
.equ EPERM, 1
.equ ENXIO, 6
.equ ENODEV, 19
.equ SIGSEGV, 11
.equ SA_SIGINFO, 4
.equ SA_RESTORER, 0x04000000

.equ PROT_READ, 1
.equ PROT_WRITE, 2
.equ PROT_EXEC, 4
.equ MAP_PRIVATE, 2
.equ MAP_ANONYMOUS, 0x20

.equ PR_GET_PDEATHSIG, 2
.equ PR_SET_MM, 35
.equ PR_SET_MM_MAP, 14
.equ PR_SET_MM_MAP_SIZE, 15
.equ PR_GET_TSC, 25
.equ PR_SET_TSC, 26
.equ PR_TSC_ENABLE, 1
.equ PR_TSC_SIGSEGV, 2
.equ PR_SET_MDWE, 65
.equ PR_GET_MDWE, 66
.equ PR_MDWE_REFUSE_EXEC_GAIN, 1
.equ PR_MDWE_NO_INHERIT, 2
.equ PR_SET_VMA, 0x53564d41
.equ PR_SET_VMA_ANON_NAME, 0
.equ PR_GET_AUXV, 0x41555856
.equ PR_GET_SPECULATION_CTRL, 52
.equ PR_SET_SPECULATION_CTRL, 53
.equ PR_SPEC_STORE_BYPASS, 0
.equ PR_SPEC_ENABLE, 2

/* Linux 6.14 x86_64: 2 * (AT_VECTOR_SIZE_BASE + AT_VECTOR_SIZE_ARCH + 1). */
.equ X86_64_AUXV_WORDS, 50
.equ X86_64_AUXV_SIZE, X86_64_AUXV_WORDS * 8
.equ PRCTL_MM_MAP_SIZE, 104

.section .text
.global _start
.type _start, @function
_start:
    /* PR_GET_PDEATHSIG must not silently accept a NULL output pointer. */
    mov $__NR_prctl, %eax
    mov $PR_GET_PDEATHSIG, %edi
    xor %esi, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-EFAULT, %rax
    jne fail_pdeathsig

    /* PR_GET_AUXV returns the fixed x86_64 saved_auxv size. */
    mov $__NR_prctl, %eax
    mov $PR_GET_AUXV, %edi
    lea auxv_buffer(%rip), %rsi
    mov $X86_64_AUXV_SIZE, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $X86_64_AUXV_SIZE, %rax
    jne fail_auxv

    lea auxv_buffer(%rip), %rdi
    mov $X86_64_AUXV_WORDS, %ecx
find_pagesz:
    cmpq $6, (%rdi)
    je check_pagesz
    add $16, %rdi
    sub $2, %ecx
    jnz find_pagesz
    jmp fail_auxv
check_pagesz:
    cmpq $4096, 8(%rdi)
    jne fail_auxv

    mov $__NR_prctl, %eax
    mov $PR_GET_AUXV, %edi
    mov $1, %esi
    mov $1, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-EFAULT, %rax
    jne fail_auxv

    mov $__NR_prctl, %eax
    mov $PR_GET_AUXV, %edi
    lea auxv_buffer(%rip), %rsi
    mov $X86_64_AUXV_SIZE, %edx
    mov $1, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-EINVAL, %rax
    jne fail_auxv

    /* PR_SET_MM map operations are guest ABI data, not host pointers. */
    mov $__NR_prctl, %eax
    mov $PR_SET_MM, %edi
    mov $PR_SET_MM_MAP_SIZE, %esi
    lea mm_map_size(%rip), %rdx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js fail_set_mm
    cmpl $PRCTL_MM_MAP_SIZE, mm_map_size(%rip)
    jne fail_set_mm

    mov $__NR_prctl, %eax
    mov $PR_SET_MM, %edi
    mov $PR_SET_MM_MAP_SIZE, %esi
    lea mm_map_size(%rip), %rdx
    xor %r10d, %r10d
    mov $1, %r8d
    syscall
    cmp $-EINVAL, %rax
    jne fail_set_mm

    mov $__NR_mmap, %eax
    xor %edi, %edi
    mov $16384, %esi
    mov $(PROT_READ | PROT_WRITE), %edx
    mov $(MAP_PRIVATE | MAP_ANONYMOUS), %r10d
    mov $-1, %r8d
    xor %r9d, %r9d
    syscall
    test %rax, %rax
    js fail_set_mm

    lea mm_map(%rip), %rdi
    mov %rax, 0(%rdi)
    lea 1(%rax), %rcx
    mov %rcx, 8(%rdi)
    mov %rax, 16(%rdi)
    mov %rax, 24(%rdi)
    mov %rax, 32(%rdi)
    mov %rax, 40(%rdi)
    mov %rax, 48(%rdi)
    mov %rax, 56(%rdi)
    mov %rax, 64(%rdi)
    mov %rax, 72(%rdi)
    mov %rax, 80(%rdi)
    lea replacement_auxv(%rip), %rcx
    mov %rcx, 88(%rdi)
    movl $16, 96(%rdi)
    movl $-1, 100(%rdi)

    mov $__NR_prctl, %eax
    mov $PR_SET_MM, %edi
    mov $PR_SET_MM_MAP, %esi
    lea mm_map(%rip), %rdx
    mov $PRCTL_MM_MAP_SIZE, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js fail_set_mm

    mov $__NR_prctl, %eax
    mov $PR_GET_AUXV, %edi
    lea auxv_buffer(%rip), %rsi
    mov $16, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $X86_64_AUXV_SIZE, %rax
    jne fail_set_mm
    cmpq $123, auxv_buffer(%rip)
    jne fail_set_mm
    cmpq $456, auxv_buffer+8(%rip)
    jne fail_set_mm

    /* x86 speculation controls describe the virtual CPU, not LoongArch. */
    mov $__NR_prctl, %eax
    mov $PR_GET_SPECULATION_CTRL, %edi
    mov $PR_SPEC_STORE_BYPASS, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    jne fail_speculation

    mov $__NR_prctl, %eax
    mov $PR_GET_SPECULATION_CTRL, %edi
    mov $99, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-ENODEV, %rax
    jne fail_speculation

    mov $__NR_prctl, %eax
    mov $PR_SET_SPECULATION_CTRL, %edi
    mov $PR_SPEC_STORE_BYPASS, %esi
    mov $PR_SPEC_ENABLE, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-ENXIO, %rax
    jne fail_speculation

    /* TSC control is x86 guest state, not a host-LoongArch prctl. */
    mov $__NR_prctl, %eax
    mov $PR_GET_TSC, %edi
    lea tsc_mode(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js fail_tsc
    cmpl $PR_TSC_ENABLE, tsc_mode(%rip)
    jne fail_tsc

    mov $__NR_prctl, %eax
    mov $PR_SET_TSC, %edi
    xor %esi, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-EINVAL, %rax
    jne fail_tsc

    mov $__NR_prctl, %eax
    mov $PR_SET_TSC, %edi
    mov $PR_TSC_SIGSEGV, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js fail_tsc

    mov $__NR_prctl, %eax
    mov $PR_GET_TSC, %edi
    lea tsc_mode(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js fail_tsc
    cmpl $PR_TSC_SIGSEGV, tsc_mode(%rip)
    jne fail_tsc

    /* Pointer and guest-address arguments must be translated for the host. */
    mov $__NR_mmap, %eax
    xor %edi, %edi
    mov $16384, %esi
    mov $(PROT_READ | PROT_WRITE), %edx
    mov $(MAP_PRIVATE | MAP_ANONYMOUS), %r10d
    mov $-1, %r8d
    xor %r9d, %r9d
    syscall
    test %rax, %rax
    js fail_vma
    mov %rax, %r12

    mov $__NR_prctl, %eax
    mov $PR_SET_VMA, %edi
    mov $PR_SET_VMA_ANON_NAME, %esi
    mov %r12, %rdx
    mov $16384, %r10d
    lea anon_name(%rip), %r8
    syscall
    test %rax, %rax
    je vma_done
    /* CONFIG_ANON_VMA_NAME=n has the same EINVAL result on native x86. */
    cmp $-EINVAL, %rax
    jne fail_vma
vma_done:

    /* MDWE is per guest mm and must not constrain LAT's own JIT mappings. */
    mov $__NR_prctl, %eax
    mov $PR_GET_MDWE, %edi
    xor %esi, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    jne fail_mdwe

    mov $__NR_prctl, %eax
    mov $PR_SET_MDWE, %edi
    mov $PR_MDWE_NO_INHERIT, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-EINVAL, %rax
    jne fail_mdwe

    mov $__NR_prctl, %eax
    mov $PR_SET_MDWE, %edi
    mov $PR_MDWE_REFUSE_EXEC_GAIN, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js fail_mdwe

    mov $__NR_prctl, %eax
    mov $PR_GET_MDWE, %edi
    xor %esi, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $PR_MDWE_REFUSE_EXEC_GAIN, %rax
    jne fail_mdwe

    mov $__NR_prctl, %eax
    mov $PR_SET_MDWE, %edi
    xor %esi, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    cmp $-EPERM, %rax
    jne fail_mdwe

    mov $__NR_mmap, %eax
    xor %edi, %edi
    mov $4096, %esi
    mov $(PROT_WRITE | PROT_EXEC), %edx
    mov $(MAP_PRIVATE | MAP_ANONYMOUS), %r10d
    mov $-1, %r8d
    xor %r9d, %r9d
    syscall
    cmp $-EACCES, %rax
    jne fail_mdwe

    mov $__NR_mmap, %eax
    xor %edi, %edi
    mov $4096, %esi
    mov $(PROT_READ | PROT_WRITE), %edx
    mov $(MAP_PRIVATE | MAP_ANONYMOUS), %r10d
    mov $-1, %r8d
    xor %r9d, %r9d
    syscall
    test %rax, %rax
    js fail_mdwe
    mov %rax, %r12

    mov $__NR_mprotect, %eax
    mov %r12, %rdi
    mov $4096, %esi
    mov $(PROT_READ | PROT_EXEC), %edx
    syscall
    cmp $-EACCES, %rax
    jne fail_mdwe

    /* Arrange for an actual RDTSC fault after all non-signal checks pass. */
    mov $__NR_rt_sigaction, %eax
    mov $SIGSEGV, %edi
    lea action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js fail_tsc

    rdtsc
    mov $24, %edi
    jmp exit_now

fail_pdeathsig:
    mov $20, %edi
    jmp exit_now
fail_auxv:
    mov $21, %edi
    jmp exit_now
fail_tsc:
    mov $22, %edi
    jmp exit_now
fail_mdwe:
    mov $23, %edi
    jmp exit_now
fail_vma:
    mov $26, %edi
    jmp exit_now
fail_set_mm:
    mov $27, %edi
    jmp exit_now
fail_speculation:
    mov $28, %edi
    jmp exit_now
.size _start, .-_start

segv_handler:
    cmp $SIGSEGV, %edi
    jne fail_signal
    xor %edi, %edi
    jmp exit_now
fail_signal:
    mov $25, %edi

exit_now:
    mov $__NR_exit, %eax
    syscall
    ud2

sigreturn:
    mov $15, %eax
    syscall
    ud2

.section .rodata
.p2align 3
action:
    .quad segv_handler
    .quad SA_SIGINFO | SA_RESTORER
    .quad sigreturn
    .quad 0
anon_name:
    .asciz "lat-prctl-test"
.p2align 3
replacement_auxv:
    .quad 123, 456

.section .bss
.p2align 3
auxv_buffer:
    .skip X86_64_AUXV_SIZE
tsc_mode:
    .skip 4
mm_map_size:
    .skip 4
.p2align 3
mm_map:
    .skip PRCTL_MM_MAP_SIZE

.section .note.GNU-stack,"",@progbits
