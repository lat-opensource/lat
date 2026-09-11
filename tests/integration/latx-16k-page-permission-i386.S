.equ __NR_exit, 1
.equ __NR_mprotect, 125
.equ __NR_rt_sigaction, 174
.equ __NR_mmap2, 192

.equ PROT_NONE, 0
.equ PROT_READ, 1
.equ PROT_WRITE, 2
.equ MAP_PRIVATE, 2
.equ MAP_ANONYMOUS, 0x20

.section .text
.global _start
.type _start, @function
_start:
    mov %esp, expected_sp
    mov $__NR_rt_sigaction, %eax
    mov $11, %ebx
    mov $segv_action, %ecx
    xor %edx, %edx
    mov $8, %esi
    int $0x80
    test %eax, %eax
    jnz fail_signal

    mov $__NR_mmap2, %eax
    xor %ebx, %ebx
    mov $16384, %ecx
    mov $(PROT_READ | PROT_WRITE), %edx
    mov $(MAP_PRIVATE | MAP_ANONYMOUS), %esi
    mov $-1, %edi
    xor %ebp, %ebp
    int $0x80
    cmp $-4095, %eax
    jae fail_map
    mov %eax, %edi

    mov $__NR_mprotect, %eax
    lea 4096(%edi), %ebx
    mov $4096, %ecx
    mov $PROT_NONE, %edx
    int $0x80
    test %eax, %eax
    js fail_protect

    cmpl $2, (%esp)
    jne fail_args
    mov 8(%esp), %eax
    cmpb $'r', (%eax)
    je read_protected
    cmpb $'w', (%eax)
    je write_across_boundary
    cmpb $'s', (%eax)
    je write_protected
    cmpb $'c', (%eax)
    je read_across_boundary
    cmpb $'v', (%eax)
    je valid_access
    cmpb $'p', (%eax)
    je read_after_push
    cmpb $'x', (%eax)
    je xchg32_protected
    cmpb $'h', (%eax)
    je xchg16_protected
    cmpb $'a', (%eax)
    je xchg_allowed
    jmp fail_args

xchg32_protected:
    lea 4096(%edi), %eax
    mov %eax, expected_address
    movl $fault_xchg32, expected_pc
    mov $0x12345678, %eax
fault_xchg32:
    xchgl %eax, 4094(%edi)
    jmp fail_access

xchg16_protected:
    lea 4096(%edi), %eax
    mov %eax, expected_address
    movl $fault_xchg16, expected_pc
    mov $0x1234, %eax
fault_xchg16:
    xchgw %ax, 4095(%edi)
    jmp fail_access

xchg_allowed:
    movl $123, 4092(%edi)
    mov $456, %eax
    xchgl %eax, 4092(%edi)
    cmp $123, %eax
    jne fail_access
    cmpl $456, 4092(%edi)
    jne fail_access
    mov $789, %eax
    xchgw %ax, 4092(%edi)
    cmp $456, %ax
    jne fail_access
    cmpw $789, 4092(%edi)
    jne fail_access
    /* Exercise the unaligned LL/SC path and its wider-atomic fallback. */
    movl $123, 4089(%edi)
    mov $456, %eax
    xchgl %eax, 4089(%edi)
    cmp $123, %eax
    jne fail_access
    cmpl $456, 4089(%edi)
    jne fail_access
    movw $123, 4091(%edi)
    mov $456, %eax
    xchgw %ax, 4091(%edi)
    cmp $123, %ax
    jne fail_access
    cmpw $456, 4091(%edi)
    jne fail_access
    movl $123, 4086(%edi)
    mov $456, %eax
    xchgl %eax, 4086(%edi)
    cmp $123, %eax
    jne fail_access
    cmpl $456, 4086(%edi)
    jne fail_access
    movw $123, 4087(%edi)
    mov $456, %eax
    xchgw %ax, 4087(%edi)
    cmp $123, %ax
    jne fail_access
    cmpw $456, 4087(%edi)
    jne fail_access
    xor %ebx, %ebx
    jmp exit

read_protected:
    lea 4096(%edi), %eax
    mov %eax, expected_address
    movl $fault_read, expected_pc
fault_read:
    movl 4096(%edi), %eax
    jmp fail_access

write_across_boundary:
    lea 4096(%edi), %eax
    mov %eax, expected_address
    movl $fault_cross_write, expected_pc
    pxor %xmm0, %xmm0
fault_cross_write:
    movdqu %xmm0, 4092(%edi)
    jmp fail_access

write_protected:
    lea 4100(%edi), %eax
    mov %eax, expected_address
    movl $fault_write, expected_pc
fault_write:
    movl $0, 4100(%edi)
    jmp fail_access

read_across_boundary:
    lea 4096(%edi), %eax
    mov %eax, expected_address
    movl $fault_cross_read, expected_pc
fault_cross_read:
    movl 4094(%edi), %eax
    jmp fail_access

valid_access:
    movl $123, 4092(%edi)
    cmpl $123, 4092(%edi)
    jne fail_access
    xor %ebx, %ebx
    jmp exit

read_after_push:
    lea -8(%esp), %eax
    mov %eax, expected_sp
    lea 4096(%edi), %eax
    mov %eax, expected_address
    movl $fault_after_push, expected_pc
    push %eax
    push %edx
fault_after_push:
    movl 4096(%edi), %eax
    jmp fail_access

/* i386 SA_SIGINFO: handler(signo, siginfo, ucontext). */
segv_handler:
    cmpl $11, 4(%esp)
    jne fail_signal
    mov 8(%esp), %eax
    cmpl $2, 8(%eax) /* SEGV_ACCERR */
    jne fail_signal
    mov 12(%eax), %edx /* si_addr */
    cmp expected_address, %edx
    jne fail_address
    mov 12(%esp), %eax
    mov 76(%eax), %edx /* uc_mcontext.gregs[REG_EIP] */
    cmp expected_pc, %edx
    jne fail_pc
    mov 48(%eax), %edx /* uc_mcontext.gregs[REG_ESP] */
    cmp expected_sp, %edx
    jne fail_sp
    xor %ebx, %ebx
    jmp exit

fail_access:
    mov $12, %ebx
    jmp exit
fail_map:
    mov $10, %ebx
    jmp exit
fail_protect:
    mov $11, %ebx
    jmp exit
fail_args:
    mov $13, %ebx
    jmp exit
fail_signal:
    mov $14, %ebx
    jmp exit
fail_address:
    mov $15, %ebx
    jmp exit
fail_pc:
    mov $16, %ebx
    jmp exit
fail_sp:
    mov $17, %ebx
exit:
    mov $__NR_exit, %eax
    int $0x80

.section .data
.align 4
segv_action:
    .long segv_handler, 4, 0 /* SA_SIGINFO; handler exits directly. */
    .quad 0
expected_address:
    .long 0
expected_pc:
    .long 0
expected_sp:
    .long 0
.section .note.GNU-stack,"",@progbits
