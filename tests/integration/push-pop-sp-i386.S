.equ __NR_exit, 1
.equ __NR_getpid, 20
.equ __NR_rt_sigreturn, 173
.equ __NR_rt_sigaction, 174
.equ SIGSEGV, 11
.equ SA_SIGINFO, 0x00000004
.equ SA_RESTORER, 0x04000000
.equ UCONTEXT_ESP_OFFSET, 48
.equ UCONTEXT_EIP_OFFSET, 76

.section .data
.align 4
segv_action:
    .long segv_handler
    .long (SA_SIGINFO | SA_RESTORER)
    .long signal_restorer
    .quad 0
saved_esp:
    .long 0
expected_esp:
    .long 0
fault_resume:
    .long 0
fault_seen:
    .long 0

.section .text
.global _start
.type _start, @function
_start:
    mov $__NR_rt_sigaction, %eax
    mov $SIGSEGV, %ebx
    mov $segv_action, %ecx
    xor %edx, %edx
    mov $8, %esi
    int $0x80
    test %eax, %eax
    js .Linstall_failed

    call test_push_pop_32
    mov %eax, %ebx
    mov $__NR_exit, %eax
    int $0x80
.Linstall_failed:
    mov $10, %ebx
    mov $__NR_exit, %eax
    int $0x80

.type test_push_pop_32, @function
test_push_pop_32:
    mov %esp, saved_esp
    mov $0x11, %eax
    mov $0x22, %ebx
    mov $0x33, %ecx
    mov $0x44, %edx
    push %eax
    push %ebx
    push %ecx
    push %edx
    pop %esi
    pop %edi
    pop %ebp
    pop %eax
    cmp $0x44, %esi
    jne .Lpush_pop_failed
    cmp $0x33, %edi
    jne .Lpush_pop_failed
    cmp $0x22, %ebp
    jne .Lpush_pop_failed
    cmp $0x11, %eax
    jne .Lpush_pop_failed
    cmp saved_esp, %esp
    jne .Lpush_pop_failed

    mov %esp, %eax
    push %esp
    pop %ecx
    cmp %eax, %ecx
    jne .Lpush_esp_failed
    cmp saved_esp, %esp
    jne .Lpush_esp_failed

    pushal
    popal
    pushfl
    pop %eax
    push %eax
    popfl
    enter $32, $0
    leave
    cmp saved_esp, %esp
    jne .Lstack_instructions_failed

    push %eax
    push %ebx
    mov $__NR_getpid, %eax
    int $0x80
    test %eax, %eax
    js .Lhelper_failed
    pop %ebx
    pop %eax
    cmp saved_esp, %esp
    jne .Lhelper_failed

    mov %esp, %eax
    sub $8, %eax
    mov %eax, expected_esp
    movl $.Lafter_fault, fault_resume
    movl $0, fault_seen
    push %esi
    push %edi
    xor %eax, %eax
    mov (%eax), %eax
.Lafter_fault:
    cmpl $1, fault_seen
    jne .Lfault_not_seen
    pop %edi
    pop %esi
    cmp saved_esp, %esp
    jne .Lfault_restore_failed

    xor %eax, %eax
    ret
.Lpush_pop_failed:
    mov $20, %eax
    ret
.Lpush_esp_failed:
    mov $21, %eax
    ret
.Lstack_instructions_failed:
    mov $22, %eax
    ret
.Lhelper_failed:
    mov $23, %eax
    ret
.Lfault_not_seen:
    mov $24, %eax
    ret
.Lfault_restore_failed:
    mov $25, %eax
    ret
.size test_push_pop_32, .-test_push_pop_32

.type segv_handler, @function
segv_handler:
    cmpl $SIGSEGV, 4(%esp)
    jne .Lwrong_signal
    mov 12(%esp), %edx
    mov expected_esp, %eax
    cmp %eax, UCONTEXT_ESP_OFFSET(%edx)
    jne .Lwrong_fault_esp
    mov fault_resume, %eax
    mov %eax, UCONTEXT_EIP_OFFSET(%edx)
    movl $1, fault_seen
    ret
.size segv_handler, .-segv_handler

.Lwrong_signal:
    mov $30, %ebx
    jmp .Lexit_now
.Lwrong_fault_esp:
    mov $31, %ebx
.Lexit_now:
    mov $__NR_exit, %eax
    int $0x80

.type signal_restorer, @function
signal_restorer:
    mov $__NR_rt_sigreturn, %eax
    int $0x80
    ud2
.size signal_restorer, .-signal_restorer

.section .note.GNU-stack,"",@progbits
