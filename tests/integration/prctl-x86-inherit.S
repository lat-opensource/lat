.equ __NR_execve, 59
.equ __NR_rt_sigaction, 13
.equ __NR_prctl, 157
.equ __NR_exit, 60

.equ EINVAL, 22
.equ SIGSEGV, 11
.equ SA_SIGINFO, 4
.equ SA_RESTORER, 0x04000000

.equ PR_GET_TSC, 25
.equ PR_SET_TSC, 26
.equ PR_TSC_ENABLE, 1
.equ PR_TSC_SIGSEGV, 2
.equ PR_SET_MDWE, 65
.equ PR_GET_MDWE, 66
.equ PR_MDWE_REFUSE_EXEC_GAIN, 1
.equ PR_MDWE_NO_INHERIT, 2

.section .text
.global _start
.type _start, @function
_start:
    mov (%rsp), %r12
    lea 16(%rsp,%r12,8), %r13
    mov 16(%rsp), %rax
    cmpb $'n', (%rax)
    je no_inherit_stage
    cmpb $'i', (%rax)
    je inherit_stage
    cmpb $'u', (%rax)
    je untrusted_env_stage

    lea fake_mdwe_env(%rip), %rax
    mov %rax, fake_envp(%rip)
    lea fake_tsc_env(%rip), %rax
    mov %rax, fake_envp+8(%rip)
    lea untrusted_env_arg(%rip), %r14
    lea fake_envp(%rip), %r9
    jmp exec_self

untrusted_env_stage:
    mov $__NR_prctl, %eax
    mov $PR_GET_MDWE, %edi
    xor %esi, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    jne fail_untrusted_mdwe

    mov $__NR_prctl, %eax
    mov $PR_GET_TSC, %edi
    lea tsc_mode(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js fail_untrusted_tsc
    cmpl $PR_TSC_ENABLE, tsc_mode(%rip)
    jne fail_untrusted_tsc

    mov $__NR_prctl, %eax
    mov $PR_SET_MDWE, %edi
    mov $(PR_MDWE_REFUSE_EXEC_GAIN | PR_MDWE_NO_INHERIT), %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js fail_mdwe

    lea no_inherit_arg(%rip), %r14
    mov %r13, %r9
    jmp exec_self

no_inherit_stage:
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
    mov $PR_MDWE_REFUSE_EXEC_GAIN, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js fail_mdwe

    mov $__NR_prctl, %eax
    mov $PR_SET_TSC, %edi
    mov $PR_TSC_SIGSEGV, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js fail_tsc

    lea inherit_arg(%rip), %r14
    mov %r13, %r9

exec_self:
    mov 8(%rsp), %rbx
    cmp $2, %r12
    je first_exec
    mov 24(%rsp), %r15
    jmp fill_exec_argv
first_exec:
    mov 16(%rsp), %r15
fill_exec_argv:
    lea exec_argv(%rip), %rsi
    mov %r15, 0(%rsi)
    mov %rbx, 8(%rsi)
    mov %r14, 16(%rsi)
    mov %r15, 24(%rsi)
    mov $__NR_execve, %eax
    mov %r15, %rdi
    mov %r9, %rdx
    syscall
    mov $32, %edi
    jmp exit_now

inherit_stage:
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

    mov $__NR_rt_sigaction, %eax
    mov $SIGSEGV, %edi
    lea action(%rip), %rsi
    xor %edx, %edx
    mov $8, %r10d
    syscall
    test %rax, %rax
    js fail_tsc

    rdtscp
    mov $33, %edi
    jmp exit_now

fail_mdwe:
    mov $34, %edi
    jmp exit_now
fail_tsc:
    mov $35, %edi
    jmp exit_now
fail_untrusted_mdwe:
    mov $37, %edi
    jmp exit_now
fail_untrusted_tsc:
    mov $38, %edi
    jmp exit_now
.size _start, .-_start

segv_handler:
    cmp $SIGSEGV, %edi
    jne fail_signal
    xor %edi, %edi
    jmp exit_now
fail_signal:
    mov $36, %edi

exit_now:
    mov $__NR_exit, %eax
    syscall
    ud2

sigreturn:
    mov $15, %eax
    syscall
    ud2

.section .rodata
no_inherit_arg:
    .asciz "n"
inherit_arg:
    .asciz "i"
untrusted_env_arg:
    .asciz "u"
fake_mdwe_env:
    .asciz "_LATX_GUEST_MDWE=1"
fake_tsc_env:
    .asciz "_LATX_GUEST_TSC=1"
action:
    .quad segv_handler
    .quad SA_SIGINFO | SA_RESTORER
    .quad sigreturn
    .quad 0

.section .bss
.p2align 3
exec_argv:
    .skip 40
.p2align 3
fake_envp:
    .skip 24
.p2align 2
tsc_mode:
    .skip 4

.section .note.GNU-stack,"",@progbits
