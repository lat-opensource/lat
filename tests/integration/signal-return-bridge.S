/* Self-checking x86_64 signal-return test; no libc is involved.
 * Exit 10: setup failed; 11: wrong signal; 12: nested return/order failed;
 * 13: final counts failed. SIGILL nests inside SIGTRAP, with distinct
 * restorers. Both restorers must run their guest-side marker instructions.
 */
.section .data
.balign 8
trap_action:
    .quad outer_handler, 0x04000004, outer_restorer, 0
ill_action:
    .quad inner_handler, 0x04000004, inner_restorer, 0

.section .bss
.balign 4
outer_seen: .long 0
inner_seen: .long 0
leaf_seen: .long 0
outer_restored: .long 0
inner_restored: .long 0

.section .text
.global _start
_start:
    mov $13, %eax                 /* rt_sigaction(SIGTRAP, ...) */
    mov $5, %edi
    lea trap_action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js setup_failed
    mov $13, %eax
    mov $4, %edi                  /* SIGILL */
    lea ill_action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js setup_failed

    int3
    int3                         /* Reuse both return targets. */
    cmpl $2, outer_seen(%rip)
    jne counts_failed
    cmpl $2, inner_seen(%rip)
    jne counts_failed
    cmpl $4, leaf_seen(%rip)
    jne counts_failed
    cmpl $2, outer_restored(%rip)
    jne counts_failed
    cmpl $2, inner_restored(%rip)
    jne counts_failed
    xor %edi, %edi
    jmp finish

outer_handler:
    cmp $5, %edi
    jne wrong_signal
    incl outer_seen(%rip)
    sub $8, %rsp                 /* Align for an ordinary nested CALL. */
    call leaf
    add $8, %rsp
    ud2                          /* Nested SIGILL; handler advances RIP. */
    mov outer_seen(%rip), %eax
    cmp inner_restored(%rip), %eax
    jne nested_failed
    mov outer_restored(%rip), %ecx
    inc %ecx
    cmp %ecx, %eax
    jne nested_failed
    ret

inner_handler:
    cmp $4, %edi
    jne wrong_signal
    /* x86_64 ucontext uc_mcontext.gregs[REG_RIP] is at offset 168. */
    addq $2, 168(%rdx)
    incl inner_seen(%rip)
    sub $8, %rsp
    call leaf
    add $8, %rsp
    ret

leaf:
    incl leaf_seen(%rip)
    ret

outer_restorer:
    incl outer_restored(%rip)
    mov $15, %eax                /* rt_sigreturn */
    syscall
    jmp nested_failed
inner_restorer:
    incl inner_restored(%rip)
    mov $15, %eax
    syscall
    jmp nested_failed

setup_failed:
    mov $10, %edi
    jmp finish
wrong_signal:
    mov $11, %edi
    jmp finish
nested_failed:
    mov $12, %edi
    jmp finish
counts_failed:
    mov $13, %edi
finish:
    mov $60, %eax
    syscall
.section .note.GNU-stack,"",@progbits
