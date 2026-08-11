.equ __NR_exit, 1
.equ __NR_getpid, 20
.equ __NR_mprotect, 125
.equ __NR_prctl, 172
.equ __NR_rt_sigreturn, 173
.equ __NR_rt_sigaction, 174
.equ __NR_mmap2, 192
.equ __NR_tunnel_virtual, 600

.equ EACCES, 13
.equ EFAULT, 14
.equ EINVAL, 22
.equ EOPNOTSUPP, 95
.equ SIGSYS, 31
.equ SA_SIGINFO, 4
.equ SA_RESTORER, 0x04000000
.equ SYS_USER_DISPATCH, 2
.equ AUDIT_ARCH_I386, 0x40000003

.equ PROT_READ, 1
.equ PROT_WRITE, 2
.equ PROT_EXEC, 4
.equ MAP_PRIVATE, 2
.equ MAP_ANONYMOUS, 0x20

.equ PR_GET_PDEATHSIG, 2
.equ PR_GET_TSC, 25
.equ PR_SET_TSC, 26
.equ PR_TSC_ENABLE, 1
.equ PR_TSC_SIGSEGV, 2
.equ PR_SET_MM, 35
.equ PR_SET_MM_MAP, 14
.equ PR_SET_MM_MAP_SIZE, 15
.equ PR_SET_SYSCALL_USER_DISPATCH, 59
.equ PR_SYS_DISPATCH_OFF, 0
.equ PR_SYS_DISPATCH_ON, 1
.equ SYSCALL_DISPATCH_FILTER_ALLOW, 0
.equ SYSCALL_DISPATCH_FILTER_BLOCK, 1
.equ PR_SET_MDWE, 65
.equ PR_GET_MDWE, 66
.equ PR_MDWE_REFUSE_EXEC_GAIN, 1
.equ PR_SET_MEMORY_MERGE, 67
.equ PR_GET_MEMORY_MERGE, 68
.equ PR_GET_AUXV, 0x41555856
.equ PR_RSEQ_SLICE_EXTENSION, 79
.equ PR_RSEQ_SLICE_EXTENSION_GET, 1
.equ PR_RSEQ_SLICE_EXTENSION_SET, 2
.equ PR_RSEQ_SLICE_EXT_ENABLE, 1
.equ PR_GET_CFI, 80
.equ PR_CFI_BRANCH_LANDING_PADS, 0

/* A compat task on a native x86_64 kernel sees the full saved_auxv array. */
.equ X86_AUXV_SIZE, 54 * 8
.equ PRCTL_MM_MAP_SIZE, 104

.section .text
.global _start
.type _start, @function
_start:
    /* Pointer-bearing prctl operations must use guest addresses. */
    mov $__NR_prctl, %eax
    mov $PR_GET_PDEATHSIG, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    cmp $-EFAULT, %eax
    jne fail_pointer

    mov $__NR_prctl, %eax
    mov $PR_GET_AUXV, %ebx
    mov $auxv_buffer, %ecx
    mov $X86_AUXV_SIZE, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    cmp $X86_AUXV_SIZE, %eax
    jne fail_auxv

    mov $auxv_buffer, %ecx
    mov $54, %edx
find_pagesz:
    cmpl $6, (%ecx)
    je check_pagesz
    add $8, %ecx
    dec %edx
    jnz find_pagesz
    jmp fail_auxv
check_pagesz:
    cmpl $4096, 4(%ecx)
    jne fail_auxv

    /* The UAPI PR_SET_MM map stays a 104-byte structure for compat tasks. */
    mov $__NR_prctl, %eax
    mov $PR_SET_MM, %ebx
    mov $PR_SET_MM_MAP_SIZE, %ecx
    mov $mm_map_size, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js fail_set_mm
    cmpl $PRCTL_MM_MAP_SIZE, mm_map_size
    jne fail_set_mm

    /* A 64-bit UAPI address that cannot fit i386 must not be truncated. */
    mov $__NR_prctl, %eax
    mov $PR_SET_MM, %ebx
    mov $PR_SET_MM_MAP, %ecx
    mov $overflow_mm_map, %edx
    mov $PRCTL_MM_MAP_SIZE, %esi
    xor %edi, %edi
    int $0x80
    cmp $-EINVAL, %eax
    jne fail_set_mm

    /* SUD is x86 guest state and OFF accepts only zero trailing args. */
    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %ebx
    mov $PR_SYS_DISPATCH_OFF, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js fail_sud

    mov $__NR_rt_sigaction, %eax
    mov $SIGSYS, %ebx
    mov $sud_action, %ecx
    xor %edx, %edx
    mov $8, %esi
    int $0x80
    test %eax, %eax
    js fail_sud

    movb $SYSCALL_DISPATCH_FILTER_ALLOW, sud_selector
    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %ebx
    mov $PR_SYS_DISPATCH_ON, %ecx
    xor %edx, %edx
    xor %esi, %esi
    mov $sud_selector, %edi
    int $0x80
    test %eax, %eax
    js fail_sud

    movl $__NR_getpid, sud_expected_syscall
    movl $sud_getpid_after, sud_expected_address
    movb $0, sud_seen
    movb $0, sud_failed
    movb $SYSCALL_DISPATCH_FILTER_BLOCK, sud_selector
    mov $__NR_getpid, %eax
    int $0x80
sud_getpid_after:
    cmp $__NR_getpid, %eax
    jne fail_sud
    cmpb $1, sud_seen
    jne fail_sud
    cmpb $0, sud_failed
    jne fail_sud

    /* Syscall 600 is special only for an authenticated loader callsite. */
    movl $__NR_tunnel_virtual, sud_expected_syscall
    movl $sud_tunnel_after, sud_expected_address
    movb $0, sud_seen
    movb $0, sud_failed
    movb $SYSCALL_DISPATCH_FILTER_BLOCK, sud_selector
    mov $__NR_tunnel_virtual, %eax
    xor %ebx, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    xor %ebp, %ebp
    int $0x80
sud_tunnel_after:
    cmp $__NR_tunnel_virtual, %eax
    jne fail_sud
    cmpb $1, sud_seen
    jne fail_sud
    cmpb $0, sud_failed
    jne fail_sud

    mov $__NR_prctl, %eax
    mov $PR_SET_SYSCALL_USER_DISPATCH, %ebx
    mov $PR_SYS_DISPATCH_OFF, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js fail_sud

    /* TSC mode belongs to the virtual x86 CPU. */
    mov $__NR_prctl, %eax
    mov $PR_GET_TSC, %ebx
    mov $tsc_mode, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js fail_tsc
    cmpl $PR_TSC_ENABLE, tsc_mode
    jne fail_tsc

    mov $__NR_prctl, %eax
    mov $PR_SET_TSC, %ebx
    mov $PR_TSC_SIGSEGV, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js fail_tsc

    mov $__NR_prctl, %eax
    mov $PR_GET_TSC, %ebx
    mov $tsc_mode, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js fail_tsc
    cmpl $PR_TSC_SIGSEGV, tsc_mode
    jne fail_tsc

    mov $__NR_prctl, %eax
    mov $PR_SET_TSC, %ebx
    mov $PR_TSC_ENABLE, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80

    /* MDWE must cover both mmap2() and later mprotect() on i386. */
    mov $__NR_mmap2, %eax
    xor %ebx, %ebx
    mov $4096, %ecx
    mov $(PROT_READ | PROT_WRITE), %edx
    mov $(MAP_PRIVATE | MAP_ANONYMOUS), %esi
    mov $-1, %edi
    xor %ebp, %ebp
    int $0x80
    test %eax, %eax
    js fail_mdwe
    mov %eax, %ebp

    mov $__NR_prctl, %eax
    mov $PR_GET_MDWE, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    jne fail_mdwe

    mov $__NR_prctl, %eax
    mov $PR_SET_MDWE, %ebx
    mov $PR_MDWE_REFUSE_EXEC_GAIN, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js fail_mdwe

    mov $__NR_mprotect, %eax
    mov %ebp, %ebx
    mov $4096, %ecx
    mov $(PROT_READ | PROT_EXEC), %edx
    int $0x80
    cmp $-EACCES, %eax
    jne fail_mdwe

    mov $__NR_mmap2, %eax
    xor %ebx, %ebx
    mov $4096, %ecx
    mov $(PROT_WRITE | PROT_EXEC), %edx
    mov $(MAP_PRIVATE | MAP_ANONYMOUS), %esi
    mov $-1, %edi
    xor %ebp, %ebp
    int $0x80
    cmp $-EACCES, %eax
    jne fail_mdwe

    /* Unsupported current x86 facilities retain Linux argument semantics. */
    mov $__NR_prctl, %eax
    mov $PR_RSEQ_SLICE_EXTENSION, %ebx
    mov $PR_RSEQ_SLICE_EXTENSION_GET, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    cmp $-EOPNOTSUPP, %eax
    jne fail_current

    mov $__NR_prctl, %eax
    mov $PR_RSEQ_SLICE_EXTENSION, %ebx
    mov $PR_RSEQ_SLICE_EXTENSION_SET, %ecx
    mov $PR_RSEQ_SLICE_EXT_ENABLE, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    cmp $-EOPNOTSUPP, %eax
    jne fail_current

    mov $__NR_prctl, %eax
    mov $PR_GET_CFI, %ebx
    mov $PR_CFI_BRANCH_LANDING_PADS, %ecx
    mov $cfi_state, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    cmp $-EINVAL, %eax
    jne fail_current

    /* Guest KSM controls must not alter the LoongArch translator mm. */
    mov $__NR_prctl, %eax
    mov $PR_GET_MEMORY_MERGE, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    cmp $-EINVAL, %eax
    jne fail_current

    mov $__NR_prctl, %eax
    mov $PR_SET_MEMORY_MERGE, %ebx
    mov $1, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    cmp $-EINVAL, %eax
    jne fail_current

    xor %ebx, %ebx
    jmp exit_now

fail_pointer:
    mov $20, %ebx
    jmp exit_now
fail_auxv:
    mov $21, %ebx
    jmp exit_now
fail_set_mm:
    mov $22, %ebx
    jmp exit_now
fail_sud:
    mov $23, %ebx
    jmp exit_now
fail_tsc:
    mov $24, %ebx
    jmp exit_now
fail_mdwe:
    mov $25, %ebx
    jmp exit_now
fail_current:
    mov $26, %ebx

exit_now:
    mov $__NR_exit, %eax
    int $0x80
.size _start, .-_start

.type sud_handler, @function
sud_handler:
    movb $SYSCALL_DISPATCH_FILTER_ALLOW, sud_selector
    mov 8(%esp), %edx
    cmpl $SIGSYS, 4(%esp)
    jne sud_bad_siginfo
    cmpl $0, 4(%edx)
    jne sud_bad_siginfo
    cmpl $SYS_USER_DISPATCH, 8(%edx)
    jne sud_bad_siginfo
    mov sud_expected_address, %eax
    cmpl %eax, 12(%edx)
    jne sud_bad_siginfo
    mov sud_expected_syscall, %eax
    cmpl %eax, 16(%edx)
    jne sud_bad_siginfo
    cmpl $AUDIT_ARCH_I386, 20(%edx)
    jne sud_bad_siginfo
    movb $1, sud_seen
    ret
sud_bad_siginfo:
    movb $1, sud_failed
    ret
.size sud_handler, .-sud_handler

.type sud_sigreturn, @function
sud_sigreturn:
    mov $__NR_rt_sigreturn, %eax
    int $0x80
    ud2
.size sud_sigreturn, .-sud_sigreturn

.section .data
.align 8
sud_action:
    .long sud_handler
    .long SA_SIGINFO | SA_RESTORER
    .long sud_sigreturn
    .quad 0
sud_expected_address:
    .long 0
sud_expected_syscall:
    .long 0
sud_selector:
    .byte 0
sud_seen:
    .byte 0
sud_failed:
    .byte 0
.align 8
overflow_mm_map:
    .quad 0x0000000100100000
    .quad 0x0000000000100001
    .quad 0x0000000000200000
    .quad 0x0000000000200000
    .quad 0x0000000000300000
    .quad 0x0000000000300000
    .quad 0x0000000000400000
    .quad 0x0000000000400000
    .quad 0x0000000000400000
    .quad 0x0000000000400000
    .quad 0x0000000000400000
    .quad 0
    .long 0
    .long -1

.section .bss
.align 8
auxv_buffer:
    .skip X86_AUXV_SIZE
mm_map_size:
    .skip 4
tsc_mode:
    .skip 4
cfi_state:
    .skip 4
