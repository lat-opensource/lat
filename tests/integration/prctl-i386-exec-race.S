.equ __NR_exit, 1
.equ __NR_execve, 11
.equ __NR_prctl, 172
.equ __NR_execveat, 358

.equ AT_FDCWD, -100
.equ PR_SET_MDWE, 65
.equ PR_GET_MDWE, 66
.equ PR_MDWE_REFUSE_EXEC_GAIN, 1

.section .text
.global _start
.type _start, @function
_start:
    cmpl $2, (%esp)
    jne run_request
    mov 8(%esp), %eax
    cmpl $0x6f72702f, 0(%eax)   /* /pro */
    jne fail
    cmpl $0x65732f63, 4(%eax)   /* c/se */
    jne fail
    cmpl $0x662f666c, 8(%eax)   /* lf/f */
    jne fail
    cmpw $0x2f64, 12(%eax)      /* d/ */
    je check_inherited_mdwe
    jmp fail

run_request:
    cmpl $3, (%esp)
    jne fail
    mov 8(%esp), %eax
    movzbl (%eax), %eax
    mov %al, exec_mode
    mov 12(%esp), %eax
    mov %eax, target_path

    mov $__NR_prctl, %eax
    mov $PR_SET_MDWE, %ebx
    mov $PR_MDWE_REFUSE_EXEC_GAIN, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js fail

    mov target_path, %eax
    mov %eax, target_argv
    cmpb $'u', exec_mode
    je use_execveat

    mov $__NR_execve, %eax
    mov target_path, %ebx
    mov $target_argv, %ecx
    mov $target_envp, %edx
    int $0x80
    jmp fail

use_execveat:
    mov $__NR_execveat, %eax
    mov $AT_FDCWD, %ebx
    mov target_path, %ecx
    mov $target_argv, %edx
    mov $target_envp, %esi
    xor %edi, %edi
    int $0x80

check_inherited_mdwe:
    mov $__NR_prctl, %eax
    mov $PR_GET_MDWE, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    cmp $PR_MDWE_REFUSE_EXEC_GAIN, %eax
    jne fail
    xor %ebx, %ebx
    jmp exit_now

fail:
    mov $135, %ebx
exit_now:
    mov $__NR_exit, %eax
    int $0x80
.size _start, .-_start

.section .data
keep_env:
    .asciz "KEEP=1"
target_envp:
    .long keep_env
    .long 0
target_path:
    .long 0
target_argv:
    .long 0
    .long 0
exec_mode:
    .byte 0

.section .note.GNU-stack,"",@progbits
