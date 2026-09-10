.intel_syntax noprefix
.equ __NR_rt_sigaction, 13
.equ __NR_rt_sigreturn, 15
.equ __NR_alarm, 37
.equ __NR_exit, 60
.equ SIGALRM, 14
.equ SA_RESTORER, 0x04000000

.section .data
.balign 8
alarm_action:
    .quad alarm_handler
    .quad SA_RESTORER
    .quad signal_restorer
    .quad 0

.balign 32
initial:
    .quad -1, -1, -1, -1

.section .bss
.balign 4
done:
    .long 0
failed:
    .long 0

.section .text
.global _start
.type _start, @function
_start:
    mov eax, __NR_rt_sigaction
    mov edi, SIGALRM
    lea rsi, [rip + alarm_action]
    xor edx, edx
    mov r10d, 8
    syscall
    test rax, rax
    js .Linstall_failed

    mov eax, __NR_alarm
    mov edi, 1
    syscall
    vmovdqu ymm0, YMMWORD PTR [rip + initial]
.Lwait:
    vpxor xmm1, xmm1, xmm1
    vmovdqa xmm0, xmm1
    cmp DWORD PTR [rip + done], 0
    je .Lwait

    mov edi, DWORD PTR [rip + failed]
    jmp .Lexit

.Linstall_failed:
    mov edi, 2
.Lexit:
    mov eax, __NR_exit
    syscall
.size _start, .-_start

.type alarm_handler, @function
alarm_handler:
    vextracti128 xmm2, ymm0, 1
    vptest xmm2, xmm2
    setne BYTE PTR [rip + failed]
    mov DWORD PTR [rip + done], 1
    ret
.size alarm_handler, .-alarm_handler

.type signal_restorer, @function
signal_restorer:
    mov eax, __NR_rt_sigreturn
    syscall
.size signal_restorer, .-signal_restorer

.section .note.GNU-stack,"",@progbits
