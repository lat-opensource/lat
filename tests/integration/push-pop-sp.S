.equ __NR_rt_sigaction, 13
.equ __NR_rt_sigreturn, 15
.equ __NR_mmap, 9
.equ __NR_mprotect, 10
.equ __NR_munmap, 11
.equ __NR_close, 3
.equ __NR_getpid, 39
.equ __NR_exit, 60
.equ __NR_openat, 257
.equ __NR_sigaltstack, 131
.equ SIGSEGV, 11
.equ SA_SIGINFO, 0x00000004
.equ SA_ONSTACK, 0x08000000
.equ SA_RESTORER, 0x04000000
.equ AT_FDCWD, -100
.equ PROT_NONE, 0
.equ PROT_READ_WRITE, 3
.equ PROT_READ_EXEC, 5
.equ MAP_PRIVATE, 2
.equ MAP_PRIVATE_ANONYMOUS, 0x22
.equ UCONTEXT_RSP_OFFSET, 160
.equ UCONTEXT_RIP_OFFSET, 168

.section .data
.align 8
segv_action:
    .quad segv_handler
    .quad (SA_SIGINFO | SA_ONSTACK | SA_RESTORER)
    .quad signal_restorer
    .quad 0
altstack:
    .quad 0
    .quad 0
    .quad 65536
expected_rsp:
    .quad 0
fault_resume:
    .quad 0
resume_rsp:
    .quad 0
fault_seen:
    .long 0
.align 8
file_code_path:
    .quad 0

.section .text
.global _start
.type _start, @function
_start:
    cmpq $2, (%rsp)
    jb .Largs_ready
    mov 16(%rsp), %rax
    mov %rax, file_code_path(%rip)
.Largs_ready:
    mov $__NR_mmap, %eax
    xor %edi, %edi
    mov $65536, %esi
    mov $PROT_READ_WRITE, %edx
    mov $MAP_PRIVATE_ANONYMOUS, %r10d
    mov $-1, %r8
    xor %r9d, %r9d
    syscall
    test %rax, %rax
    js altstack_failed
    mov %rax, altstack(%rip)
    mov $__NR_sigaltstack, %eax
    lea altstack(%rip), %rdi
    xor %esi, %esi
    syscall
    test %rax, %rax
    js altstack_failed

    mov $__NR_rt_sigaction, %eax
    mov $SIGSEGV, %edi
    lea segv_action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js install_failed

    call test_push_pop
    test %eax, %eax
    jnz exit
    call test_rsp_uses
    test %eax, %eax
    jnz exit
    call test_stack_instructions
    test %eax, %eax
    jnz exit
    call test_helper_boundary
    test %eax, %eax
    jnz exit
    call test_fault_restore
    test %eax, %eax
    jnz exit
    call test_push_guard_fault
    test %eax, %eax
    jnz exit
    call test_file_mapping
    test %eax, %eax
    jnz exit

    xor %edi, %edi
    jmp do_exit

install_failed:
    mov $10, %edi
    jmp do_exit
altstack_failed:
    mov $11, %edi
    jmp do_exit

.type test_push_pop, @function
test_push_pop:
    mov %rsp, %r11
    mov $0x11, %rax
    mov $0x22, %rbx
    mov $0x33, %rcx
    mov $0x44, %rdx
    push %rax
    push %rbx
    push %rcx
    push %rdx
    pop %r8
    pop %r9
    pop %r10
    pop %rsi
    cmp $0x44, %r8
    jne .Lpush_pop_failed
    cmp $0x33, %r9
    jne .Lpush_pop_failed
    cmp $0x22, %r10
    jne .Lpush_pop_failed
    cmp $0x11, %rsi
    jne .Lpush_pop_failed
    cmp %r11, %rsp
    jne .Lpush_pop_failed

    mov $0x1122, %eax
    mov $0x3344, %ebx
    pushw %ax
    pushw %bx
    popw %cx
    popw %dx
    cmp $0x3344, %cx
    jne .Lpush_pop_failed
    cmp $0x1122, %dx
    jne .Lpush_pop_failed
    cmp %r11, %rsp
    jne .Lpush_pop_failed
    xor %eax, %eax
    ret
.Lpush_pop_failed:
    mov $20, %eax
    ret
.size test_push_pop, .-test_push_pop

.type test_rsp_uses, @function
test_rsp_uses:
    mov %rsp, %r11
    movabs $0x1122334455667788, %rax
    push %rax
    cmp %rax, (%rsp)
    jne .Lrsp_uses_failed
    mov %sp, %sp
    mov %spl, %spl
    pop %rcx
    cmp %rax, %rcx
    jne .Lrsp_uses_failed
    cmp %r11, %rsp
    jne .Lrsp_uses_failed
    xor %eax, %eax
    ret
.Lrsp_uses_failed:
    mov $21, %eax
    ret
.size test_rsp_uses, .-test_rsp_uses

.type test_stack_instructions, @function
test_stack_instructions:
    mov %rsp, %r11
    pushfq
    pop %rax
    push %rax
    popfq
    enter $32, $0
    leave
    cmp %r11, %rsp
    jne .Lstack_instructions_failed
    xor %eax, %eax
    ret
.Lstack_instructions_failed:
    mov $22, %eax
    ret
.size test_stack_instructions, .-test_stack_instructions

.type test_helper_boundary, @function
test_helper_boundary:
    mov %rsp, %r12
    push %rax
    push %rbx
    mov $__NR_getpid, %eax
    syscall
    test %rax, %rax
    js .Lhelper_boundary_failed
    pop %rbx
    pop %rax
    cmp %r12, %rsp
    jne .Lhelper_boundary_failed
    xor %eax, %eax
    ret
.Lhelper_boundary_failed:
    mov $23, %eax
    ret
.size test_helper_boundary, .-test_helper_boundary

.type test_fault_restore, @function
test_fault_restore:
    mov %rsp, %r11
    lea -16(%r11), %rax
    mov %rax, expected_rsp(%rip)
    mov %rax, resume_rsp(%rip)
    lea .Lafter_fault(%rip), %rax
    mov %rax, fault_resume(%rip)
    movl $0, fault_seen(%rip)
    push %r12
    push %r13
    xor %eax, %eax
    mov (%rax), %rax
.Lafter_fault:
    cmpl $1, fault_seen(%rip)
    jne .Lfault_not_seen
    pop %r13
    pop %r12
    cmp %r11, %rsp
    jne .Lfault_restore_failed
    xor %eax, %eax
    ret
.Lfault_not_seen:
    mov $24, %eax
    ret
.Lfault_restore_failed:
    mov $25, %eax
    ret
.size test_fault_restore, .-test_fault_restore

.type test_push_guard_fault, @function
test_push_guard_fault:
    mov %rsp, %r15
    mov $__NR_mmap, %eax
    xor %edi, %edi
    mov $8192, %esi
    mov $PROT_READ_WRITE, %edx
    mov $MAP_PRIVATE_ANONYMOUS, %r10d
    mov $-1, %r8
    xor %r9d, %r9d
    syscall
    test %rax, %rax
    js .Lguard_setup_failed
    mov %rax, %r14

    mov $__NR_mprotect, %eax
    mov %r14, %rdi
    mov $4096, %esi
    mov $PROT_NONE, %edx
    syscall
    test %rax, %rax
    js .Lguard_unmap_failed

    lea 4096(%r14), %rax
    mov %rax, expected_rsp(%rip)
    mov %r15, resume_rsp(%rip)
    lea .Lafter_guard_fault(%rip), %rax
    mov %rax, fault_resume(%rip)
    movl $0, fault_seen(%rip)
    lea 4104(%r14), %rsp
    push %r12
    push %r13
.Lafter_guard_fault:
    cmpl $1, fault_seen(%rip)
    jne .Lguard_fault_not_seen
    mov $__NR_munmap, %eax
    mov %r14, %rdi
    mov $8192, %esi
    syscall
    xor %eax, %eax
    ret

.Lguard_unmap_failed:
    mov $__NR_munmap, %eax
    mov %r14, %rdi
    mov $8192, %esi
    syscall
.Lguard_setup_failed:
    mov $29, %eax
    ret
.Lguard_fault_not_seen:
    mov $28, %eax
    ret
.size test_push_guard_fault, .-test_push_guard_fault

.type test_file_mapping, @function
test_file_mapping:
    mov file_code_path(%rip), %rsi
    test %rsi, %rsi
    jz .Lfile_mapping_failed
    mov $__NR_openat, %eax
    mov $AT_FDCWD, %edi
    xor %edx, %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js .Lfile_mapping_failed
    mov %rax, %r14

    mov $__NR_mmap, %eax
    xor %edi, %edi
    mov $4096, %esi
    mov $PROT_READ_EXEC, %edx
    mov $MAP_PRIVATE, %r10d
    mov %r14, %r8
    xor %r9d, %r9d
    syscall
    test %rax, %rax
    js .Lclose_mapping_fd

    call *%rax
    mov %eax, %r13d
    mov %rdx, %r12
    mov $__NR_close, %eax
    mov %r14, %rdi
    syscall
    test %r13d, %r13d
    jnz .Lfile_mapping_result
    test %r12, %r12
    jz .Lfile_mapping_failed

    lea -24(%rsp), %rax
    mov %rax, expected_rsp(%rip)
    mov %rsp, %rax
    mov %rax, resume_rsp(%rip)
    lea .Lafter_file_fault(%rip), %rax
    mov %rax, fault_resume(%rip)
    movl $0, fault_seen(%rip)
    call *%r12
.Lafter_file_fault:
    cmpl $1, fault_seen(%rip)
    jne .Lfile_fault_not_seen
    xor %eax, %eax
    ret

.Lfile_mapping_result:
    mov %r13d, %eax
    ret
.Lfile_fault_not_seen:
    mov $27, %eax
    ret

.Lclose_mapping_fd:
    mov $__NR_close, %eax
    mov %r14, %rdi
    syscall
.Lfile_mapping_failed:
    mov $26, %eax
    ret
.size test_file_mapping, .-test_file_mapping

.type segv_handler, @function
segv_handler:
    cmp $SIGSEGV, %edi
    jne wrong_signal
    mov expected_rsp(%rip), %rax
    cmp %rax, UCONTEXT_RSP_OFFSET(%rdx)
    jne wrong_fault_rsp
    mov resume_rsp(%rip), %rax
    mov %rax, UCONTEXT_RSP_OFFSET(%rdx)
    mov fault_resume(%rip), %rax
    mov %rax, UCONTEXT_RIP_OFFSET(%rdx)
    movl $1, fault_seen(%rip)
    ret
.size segv_handler, .-segv_handler

wrong_signal:
    mov $30, %edi
    jmp do_exit
wrong_fault_rsp:
    mov $31, %edi
    jmp do_exit

exit:
    mov %eax, %edi
do_exit:
    mov $__NR_exit, %eax
    syscall

.type signal_restorer, @function
signal_restorer:
    mov $__NR_rt_sigreturn, %eax
    syscall
.size signal_restorer, .-signal_restorer

.section .note.GNU-stack,"",@progbits
