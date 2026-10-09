/* Exercise asynchronous SIGILL delivery while executing guest instructions. */
.equ SIGILL, 4
.equ SA_SIGINFO, 4
.equ SA_RESTORER, 0x04000000

.section .data
.balign 8
ill_action:
#ifdef __x86_64__
    .quad ill_handler, (SA_SIGINFO | SA_RESTORER), signal_restorer, 0
#else
    .long ill_handler, (SA_SIGINFO | SA_RESTORER), signal_restorer, 0, 0
#endif
expected_code:
    .long 0
phase:
    .long 0
seen:
    .long 0
ready:
    .ascii "READY\n"

.section .text
.global _start
.type _start, @function
_start:
#ifdef __x86_64__
    mov 16(%rsp), %rax
    cmpb $'t', (%rax)
#else
    mov 8(%esp), %eax
    cmpb $'t', (%eax)
#endif
    jne install
    movl $-6, expected_code /* SI_TKILL */
install:
#ifdef __x86_64__
    mov $13, %eax /* rt_sigaction */
    mov $SIGILL, %edi
    lea ill_action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
#else
    mov $174, %eax /* rt_sigaction */
    mov $SIGILL, %ebx
    mov $ill_action, %ecx
    xor %edx, %edx
    mov $8, %esi
    int $0x80
#endif
    test %eax, %eax
    js install_failed
#ifdef __x86_64__
    mov $1, %eax /* write */
    mov $1, %edi
    lea ready(%rip), %rsi
    mov $6, %edx
    syscall
#else
    mov $4, %eax /* write */
    mov $1, %ebx
    mov $ready, %ecx
    mov $6, %edx
    int $0x80
#endif
    cmp $6, %eax
    jne install_failed

wait_begin:
    cmpl $1, seen
    jne wait_begin
wait_end:
    movl $1, phase
guest_ud2:
    ud2
    cmpl $2, seen
    jne wrong_signal
    xor %ebx, %ebx
    jmp exit
.size _start, .-_start

.type ill_handler, @function
ill_handler:
#ifdef __x86_64__
    cmp $SIGILL, %edi
    jne wrong_signal
    mov 8(%rsi), %eax /* siginfo_t.si_code */
    mov 168(%rdx), %rcx /* ucontext_t.uc_mcontext.gregs[REG_RIP] */
#else
    cmpl $SIGILL, 4(%esp)
    jne wrong_signal
    mov 8(%esp), %eax
    mov 8(%eax), %eax /* siginfo_t.si_code */
    mov 12(%esp), %edx
    mov 76(%edx), %ecx /* ucontext_t.uc_mcontext.gregs[REG_EIP] */
#endif
    cmpl $0, phase
    jne synchronous
    cmp expected_code, %eax
    jne wrong_code
    cmp $wait_begin, %ecx
    jb wrong_pc
    cmp $wait_end, %ecx
    jae wrong_pc
    movl $1, seen
    ret
synchronous:
    test %eax, %eax
    jle wrong_code
    cmp $guest_ud2, %ecx
    jne wrong_pc
#ifdef __x86_64__
    addq $2, 168(%rdx)
#else
    addl $2, 76(%edx)
#endif
    movl $2, seen
    ret
.size ill_handler, .-ill_handler

install_failed:
    mov $10, %ebx
    jmp exit
wrong_signal:
    mov $11, %ebx
    jmp exit
wrong_code:
    mov $12, %ebx
    jmp exit
wrong_pc:
    mov $13, %ebx
exit:
#ifdef __x86_64__
    mov %ebx, %edi
    mov $60, %eax
    syscall
#else
    mov $1, %eax
    int $0x80
#endif

.type signal_restorer, @function
signal_restorer:
#ifdef __x86_64__
    mov $15, %eax
    syscall
#else
    mov $173, %eax
    int $0x80
#endif
.size signal_restorer, .-signal_restorer

.section .note.GNU-stack,"",@progbits
