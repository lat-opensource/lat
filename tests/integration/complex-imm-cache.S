/* SPDX-License-Identifier: GPL-2.0-only */

    .equ __NR_rt_sigaction, 13
    .equ __NR_rt_sigreturn, 15
    .equ __NR_mmap, 9
    .equ __NR_munmap, 11
    .equ __NR_getpid, 39
    .equ __NR_fork, 57
    .equ __NR_exit, 60
    .equ __NR_wait4, 61
    .equ __NR_kill, 62
    .equ __NR_arch_prctl, 158
    .equ ARCH_SET_GS, 0x1001
    .equ PROT_RWX, 7
    .equ MAP_PRIVATE_ANONYMOUS, 0x22
    .equ SIGUSR1, 10
    .equ SA_RESTORER, 0x04000000

    .section .text
    .globl _start
    .type _start, @function
_start:
    call .Ltest_base_disp
    test %eax, %eax
    jne .Lexit
    call .Ltest_index_disp
    test %eax, %eax
    jne .Lexit
    call .Ltest_base_index_disp
    test %eax, %eax
    jne .Lexit
    call .Ltest_register_writes
    test %eax, %eax
    jne .Lexit
    call .Ltest_enter_leave
    test %eax, %eax
    jne .Lexit
    call .Ltest_string_writes
    test %eax, %eax
    jne .Lexit
    call .Ltest_xlat_write
    test %eax, %eax
    jne .Lexit
    call .Ltest_precache_mixed_access
    test %eax, %eax
    jne .Lexit
    call .Ltest_cache_pressure
    test %eax, %eax
    jne .Lexit
    call .Ltest_helper
    test %eax, %eax
    jne .Lexit
    call .Ltest_excluded_addressing
    test %eax, %eax
    jne .Lexit
    call .Ltest_signal
    test %eax, %eax
    jne .Lexit
    call .Ltest_smc
    test %eax, %eax
    jne .Lexit
    call .Ltest_fork

.Lexit:
    mov %eax, %edi
    mov $__NR_exit, %eax
    syscall
    .size _start, .-_start

.Ltest_base_disp:
    lea base_data(%rip), %r8
    mov 4096(%r8), %eax
    mov 4104(%r8), %ecx
    cmp $0x11, %eax
    jne .Lfail
    cmp $0x22, %ecx
    jne .Lfail

    lea base_anchor(%rip), %r8
    mov -4096(%r8), %eax
    mov -4088(%r8), %ecx
    cmp $0x11, %eax
    jne .Lfail
    cmp $0x22, %ecx
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_index_disp:
    xor %r9d, %r9d
    mov index_data(,%r9,1), %eax
    mov index_data + 8(,%r9,1), %ecx
    cmp $0x31, %eax
    jne .Lfail
    cmp $0x32, %ecx
    jne .Lfail

    mov index_data(,%r9,2), %eax
    mov index_data + 8(,%r9,2), %ecx
    cmp $0x31, %eax
    jne .Lfail
    cmp $0x32, %ecx
    jne .Lfail

    mov index_data(,%r9,4), %eax
    mov index_data + 8(,%r9,4), %ecx
    cmp $0x31, %eax
    jne .Lfail
    cmp $0x32, %ecx
    jne .Lfail

    mov index_data(,%r9,8), %eax
    mov index_data + 8(,%r9,8), %ecx
    cmp $0x31, %eax
    jne .Lfail
    cmp $0x32, %ecx
    jne .Lfail

    lea index_data + 4096(%rip), %r9
    mov -4096(,%r9,1), %eax
    mov -4088(,%r9,1), %ecx
    cmp $0x31, %eax
    jne .Lfail
    cmp $0x32, %ecx
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_base_index_disp:
    lea combined_data(%rip), %r8
    xor %r9d, %r9d
    mov 0(%r8,%r9,1), %eax
    mov 8(%r8,%r9,1), %ecx
    cmp $0x41, %eax
    jne .Lfail
    cmp $0x42, %ecx
    jne .Lfail

    mov 0(%r8,%r9,2), %eax
    mov 8(%r8,%r9,2), %ecx
    cmp $0x41, %eax
    jne .Lfail
    cmp $0x42, %ecx
    jne .Lfail

    mov 0(%r8,%r9,4), %eax
    mov 8(%r8,%r9,4), %ecx
    cmp $0x41, %eax
    jne .Lfail
    cmp $0x42, %ecx
    jne .Lfail

    mov 0(%r8,%r9,8), %eax
    mov 8(%r8,%r9,8), %ecx
    cmp $0x41, %eax
    jne .Lfail
    cmp $0x42, %ecx
    jne .Lfail

    lea combined_data + 4096(%rip), %r8
    xor %r9d, %r9d
    mov -4096(%r8,%r9,1), %eax
    mov -4088(%r8,%r9,1), %ecx
    cmp $0x41, %eax
    jne .Lfail
    cmp $0x42, %ecx
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_register_writes:
    /* XCHG writes both explicit operands. */
    lea write_data(%rip), %r8
    lea write_data + 8(%rip), %r10
    xor %r9d, %r9d
    mov (%r8,%r9,1), %eax
    xchg %r8, %r10
    mov (%r8,%r9,1), %ecx
    cmp $0x51, %eax
    jne .Lfail
    cmp $0x52, %ecx
    jne .Lfail

    lea write_data(%rip), %r8
    xor %r9d, %r9d
    mov $8, %r10
    mov (%r8,%r9,1), %eax
    xchg %r9, %r10
    mov (%r8,%r9,1), %ecx
    cmp $0x51, %eax
    jne .Lfail
    cmp $0x52, %ecx
    jne .Lfail

    /* XADD also writes its source operand. */
    lea write_data(%rip), %r8
    xor %r9d, %r9d
    mov $8, %r10
    mov (%r8,%r9,1), %eax
    xadd %r9, %r10
    mov (%r8,%r9,1), %ecx
    cmp $0x51, %eax
    jne .Lfail
    cmp $0x52, %ecx
    jne .Lfail

    /* MULX writes both explicit destinations. */
    lea write_data(%rip), %r8
    xor %r9d, %r9d
    mov (%r8,%r9,1), %eax
    lea write_data + 8(%rip), %rdx
    mov $1, %r10
    mulx %r10, %r8, %r11
    mov (%r8,%r9,1), %ecx
    cmp $0x51, %eax
    jne .Lfail
    cmp $0x52, %ecx
    jne .Lfail

    /* A 32-bit destination write zero-extends and invalidates the base. */
    lea write_data(%rip), %r8
    mov (%r8,%r9,1), %eax
    mov $write_data + 8, %r8d
    mov (%r8,%r9,1), %ecx
    cmp $0x51, %eax
    jne .Lfail
    cmp $0x52, %ecx
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_enter_leave:
    sub $128, %rsp
    mov %rbp, %r11
    lea -64(%rsp), %rbp
    mov %ebp, %r10d
    movl $0xa1, 0(%rsp)
    movl $0xa2, -24(%rsp)
    movl $0xa3, 0(%rbp)
    xor %r9d, %r9d

    /* Cache addresses based on the pre-ENTER RSP and RBP. */
    mov 0(%rsp,%r9,1), %eax
    mov 0(%rbp,%r9,1), %ebx
    enter $16, $0

    /* ENTER changes both RSP and RBP. */
    mov 0(%rsp,%r9,1), %ecx
    mov 0(%rbp,%r9,1), %edx
    leave

    /* LEAVE changes both registers again. */
    mov 0(%rsp,%r9,1), %esi
    mov 0(%rbp,%r9,1), %edi
    mov %r11, %rbp
    add $128, %rsp

    cmp $0xa1, %eax
    jne .Lfail
    cmp $0xa3, %ebx
    jne .Lfail
    cmp $0xa2, %ecx
    jne .Lfail
    cmp %r10d, %edx
    jne .Lfail
    cmp $0xa1, %esi
    jne .Lfail
    cmp $0xa3, %edi
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_string_writes:
    cld
    xor %r9d, %r9d

    /* LODS advances RSI. */
    lea string_lods_data(%rip), %rsi
    movzbl (%rsi,%r9,1), %r10d
    lodsb
    movzbl (%rsi,%r9,1), %ecx
    cmp $0xb1, %r10d
    jne .Lfail
    cmp $0xb2, %ecx
    jne .Lfail

    /* STOS and SCAS advance RDI. */
    lea string_stos_data(%rip), %rdi
    movzbl (%rdi,%r9,1), %r10d
    mov $0xbf, %al
    stosb
    movzbl (%rdi,%r9,1), %ecx
    cmp $0xc1, %r10d
    jne .Lfail
    cmp $0xc2, %ecx
    jne .Lfail

    lea string_scas_data(%rip), %rdi
    movzbl (%rdi,%r9,1), %r10d
    mov $0xd1, %al
    scasb
    movzbl (%rdi,%r9,1), %ecx
    cmp $0xd1, %r10d
    jne .Lfail
    cmp $0xd2, %ecx
    jne .Lfail

    /* MOVS and CMPS advance both RSI and RDI. */
    lea string_movs_src(%rip), %rsi
    lea string_movs_dst(%rip), %rdi
    movzbl (%rsi,%r9,1), %r10d
    movzbl (%rdi,%r9,1), %r11d
    movsb
    movzbl (%rsi,%r9,1), %ecx
    movzbl (%rdi,%r9,1), %edx
    cmp $0xe1, %r10d
    jne .Lfail
    cmp $0xf1, %r11d
    jne .Lfail
    cmp $0xe2, %ecx
    jne .Lfail
    cmp $0xf2, %edx
    jne .Lfail

    lea string_cmps_src(%rip), %rsi
    lea string_cmps_dst(%rip), %rdi
    movzbl (%rsi,%r9,1), %r10d
    movzbl (%rdi,%r9,1), %r11d
    cmpsb
    movzbl (%rsi,%r9,1), %ecx
    movzbl (%rdi,%r9,1), %edx
    cmp $0x21, %r10d
    jne .Lfail
    cmp $0x31, %r11d
    jne .Lfail
    cmp $0x22, %ecx
    jne .Lfail
    cmp $0x32, %edx
    jne .Lfail

    /* REP string operations also consume RCX. */
    lea string_rep_addr(%rip), %r8
    mov $1, %ecx
    movzbl (%r8,%rcx,1), %r10d
    lea string_rep_src(%rip), %rsi
    lea string_rep_dst(%rip), %rdi
    rep movsb
    movzbl (%r8,%rcx,1), %edx
    cmp $0x42, %r10d
    jne .Lfail
    cmp $0x41, %edx
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_xlat_write:
    lea xlat_target(%rip), %rax
    lea xlat_table(%rip), %rbx
    xor %r9d, %r9d
    mov (%rax,%r9,1), %r10d
    xlatb
    mov (%rax,%r9,1), %ecx
    cmp $0x71, %r10d
    jne .Lfail
    cmp $0x72, %ecx
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_precache_mixed_access:
    lea precache_mixed_data(%rip), %r8
    xor %eax, %eax
    xor %edx, %edx
    mov $0x11223344, %ebx
    mov $0x55667788, %ecx
    cmpxchg8b 4097(%r8)
    mov 4096(%r8), %eax
    mov 4104(%r8), %ecx
    cmp $0x44332211, %eax
    jne .Lfail
    cmp $0x88776655, %ecx
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_cache_pressure:
    lea pressure_data(%rip), %r8
    lea pressure_data(%rip), %r9
    lea pressure_data(%rip), %r10
    lea pressure_data(%rip), %r11
    lea pressure_data(%rip), %r12
    xor %r15d, %r15d

    mov 0(%r8,%r15,1), %eax
    mov 8(%r8,%r15,1), %ecx
    mov 0(%r9,%r15,1), %edx
    mov 8(%r9,%r15,1), %esi
    mov 0(%r10,%r15,1), %edi
    mov 8(%r10,%r15,1), %ebp
    mov 0(%r11,%r15,1), %ebx
    mov 8(%r11,%r15,1), %r14d
    mov 0(%r12,%r15,1), %r13d
    mov 8(%r12,%r15,1), %r12d

    cmp $0x91, %eax
    jne .Lfail
    cmp $0x92, %ecx
    jne .Lfail
    cmp $0x91, %edx
    jne .Lfail
    cmp $0x92, %esi
    jne .Lfail
    cmp $0x91, %edi
    jne .Lfail
    cmp $0x92, %ebp
    jne .Lfail
    cmp $0x91, %ebx
    jne .Lfail
    cmp $0x92, %r14d
    jne .Lfail
    cmp $0x91, %r13d
    jne .Lfail
    cmp $0x92, %r12d
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_helper:
    lea helper_data(%rip), %r8
    xor %r9d, %r9d
    mov (%r8,%r9,1), %eax
    pxor %xmm0, %xmm0
    pcmpistri $0, %xmm0, %xmm0
    fld1
    fsin
    fstp %st(0)
    mov 8(%r8,%r9,1), %ecx
    cmp $0x61, %eax
    jne .Lfail
    cmp $0x62, %ecx
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_excluded_addressing:
    xor %esi, %esi
    mov $ARCH_SET_GS, %edi
    mov $__NR_arch_prctl, %eax
    syscall
    test %eax, %eax
    js .Lfail

    lea excluded_data(%rip), %r8
    xor %r9d, %r9d
    mov %gs:(%r8,%r9,1), %eax
    mov %gs:8(%r8,%r9,1), %ecx
    cmp $0x71, %eax
    jne .Lfail
    cmp $0x72, %ecx
    jne .Lfail

    mov $excluded_data, %r8d
    xor %r9d, %r9d
    addr32 mov (%r8d,%r9d,1), %eax
    addr32 mov 8(%r8d,%r9d,1), %ecx
    cmp $0x71, %eax
    jne .Lfail
    cmp $0x72, %ecx
    jne .Lfail
    xor %eax, %eax
    ret

.Ltest_signal:
    lea .Lsignal_handler(%rip), %rax
    mov %rax, signal_action(%rip)
    lea .Lsignal_restorer(%rip), %rax
    mov %rax, signal_action + 16(%rip)
    mov $__NR_rt_sigaction, %eax
    mov $SIGUSR1, %edi
    lea signal_action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %eax, %eax
    js .Lfail

    mov $__NR_getpid, %eax
    syscall
    mov %eax, %edi
    mov $SIGUSR1, %esi
    mov $__NR_kill, %eax
    syscall
    test %eax, %eax
    js .Lfail
    cmpb $1, signal_seen(%rip)
    jne .Lfail
    xor %eax, %eax
    ret

.Lsignal_handler:
    lea signal_data(%rip), %r8
    xor %r9d, %r9d
    mov (%r8,%r9,1), %eax
    mov 8(%r8,%r9,1), %ecx
    cmp $0x81, %eax
    jne .Lsignal_handler_fail
    cmp $0x82, %ecx
    jne .Lsignal_handler_fail
    movb $1, signal_seen(%rip)
    ret

.Lsignal_handler_fail:
    movb $42, signal_seen(%rip)
    ret

.Lsignal_restorer:
    mov $__NR_rt_sigreturn, %eax
    syscall

.Ltest_smc:
    mov $__NR_mmap, %eax
    xor %edi, %edi
    mov $4096, %esi
    mov $PROT_RWX, %edx
    mov $MAP_PRIVATE_ANONYMOUS, %r10d
    mov $-1, %r8d
    xor %r9d, %r9d
    syscall
    test %rax, %rax
    js .Lfail
    mov %rax, %r13

    /* mov (%rdi,%rsi),eax; mov 4(%rdi,%rsi),ecx; add ecx,eax; ret */
    movabs $0x0104374c8b37048b, %rax
    mov %rax, 0(%r13)
    movw $0xc3c8, 8(%r13)
    lea smc_data(%rip), %rdi
    xor %esi, %esi
    call *%r13
    cmp $3, %eax
    jne .Lsmc_unmap_fail

    movb $8, 6(%r13)
    lea smc_data(%rip), %rdi
    xor %esi, %esi
    call *%r13
    mov %eax, %r12d

    mov $__NR_munmap, %eax
    mov %r13, %rdi
    mov $4096, %esi
    syscall
    test %eax, %eax
    js .Lfail
    cmp $5, %r12d
    jne .Lfail
    xor %eax, %eax
    ret

.Lsmc_unmap_fail:
    mov $__NR_munmap, %eax
    mov %r13, %rdi
    mov $4096, %esi
    syscall
    jmp .Lfail

.Ltest_fork:
    mov $__NR_fork, %eax
    syscall
    test %eax, %eax
    js .Lfail
    jz .Lfork_child

    mov %eax, %edi
    lea child_status(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $__NR_wait4, %eax
    syscall
    test %eax, %eax
    js .Lfail
    cmpl $0, child_status(%rip)
    jne .Lfail
    xor %eax, %eax
    ret

.Lfork_child:
    call .Ltest_base_index_disp
    mov %eax, %edi
    mov $__NR_exit, %eax
    syscall

.Lfail:
    mov $42, %eax
    ret

    .section .data
    .balign 16
base_data:
    .space 4096
    .quad 0x11, 0x22
    .space 4080
base_anchor:

    .balign 16
index_data:
    .quad 0x31, 0x32
    .space 4080

    .balign 16
combined_data:
    .quad 0x41, 0x42
    .space 4080

    .balign 16
write_data:
    .quad 0x51, 0x52

    .balign 16
helper_data:
    .quad 0x61, 0x62

    .balign 16
excluded_data:
    .quad 0x71, 0x72

    .balign 16
signal_data:
    .quad 0x81, 0x82

    .balign 16
smc_data:
    .long 1, 2, 4

    .balign 16
pressure_data:
    .quad 0x91, 0x92

string_lods_data:
    .byte 0xb1, 0xb2
string_stos_data:
    .byte 0xc1, 0xc2
string_scas_data:
    .byte 0xd1, 0xd2
string_movs_src:
    .byte 0xe1, 0xe2
string_movs_dst:
    .byte 0xf1, 0xf2
string_cmps_src:
    .byte 0x21, 0x22
string_cmps_dst:
    .byte 0x31, 0x32
string_rep_addr:
    .byte 0x41, 0x42
string_rep_src:
    .byte 0x51
string_rep_dst:
    .byte 0x61

    .balign 256
xlat_target:
    .long 0x71
    .long 0
    .long 0x72
xlat_table:
    .byte 8

    .balign 16
precache_mixed_data:
    .space 4096
    .byte 0x11, 0x22, 0x33, 0x44
    .long 0
    .byte 0x55, 0x66, 0x77, 0x88

    .balign 8
signal_action:
    .quad 0
    .quad SA_RESTORER
    .quad 0
    .quad 0
child_status:
    .long 0
signal_seen:
    .byte 0
