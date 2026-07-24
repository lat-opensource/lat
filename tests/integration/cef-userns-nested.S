.equ __NR_clone, 56
.equ __NR_close, 3
.equ __NR_getgid, 104
.equ __NR_getuid, 102
.equ __NR_openat, 257
.equ __NR_newfstatat, 262
.equ __NR_futex, 202
.equ __NR_exit_group, 231
.equ __NR_pipe, 22
.equ __NR_read, 0
.equ __NR_write, 1
.equ __NR_wait4, 61
.equ __NR_exit, 60
.equ __NR_unshare, 272
.equ AT_FDCWD, -100
.equ O_WRONLY, 1
.equ CLONE_NEWUSER, 0x10000000
.equ CLONE_NEWPID, 0x20000000
.equ CLONE_NEWNET, 0x40000000
.equ CLONE_VM, 0x00000100
.equ CLONE_FS, 0x00000200
.equ CLONE_FILES, 0x00000400
.equ CLONE_SIGHAND, 0x00000800
.equ CLONE_THREAD, 0x00010000
.equ CLONE_SYSVSEM, 0x00040000
.equ CLONE_THREAD_FLAGS, (CLONE_VM | CLONE_FS | CLONE_FILES)
.equ CLONE_THREAD_FLAGS_2, (CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM)
.equ FUTEX_WAIT, 0
.equ FUTEX_WAKE, 1
.equ SIGCHLD, 17

.section .rodata
proc_prefix:
    .ascii "/proc/"
self_name:
    .ascii "self"
uid_map_suffix:
    .asciz "/uid_map"
gid_map_suffix:
    .asciz "/gid_map"
setgroups_suffix:
    .asciz "/setgroups"
deny_setgroups:
    .ascii "deny"
task_path:
    .asciz "/proc/self/task/"

.section .bss
.align 4
status:
    .long 0
pipefds:
    .long 0, 0
sync_byte:
    .byte 0
pathbuf:
    .space 64
mapbuf:
    .space 64
digits:
    .space 20
.align 8
statbuf:
    .space 144
.align 16
thread_stack:
    .space 65536
thread_started:
    .long 0
thread_release:
    .long 0

.section .text
.global _start
.type _start, @function
_start:
    mov $__NR_getuid, %eax
    syscall
    mov %eax, %r12d
    mov $__NR_getgid, %eax
    syscall
    mov %eax, %ebx

    mov $__NR_pipe, %eax
    lea pipefds(%rip), %rdi
    syscall
    test %rax, %rax
    js pipe_failed

    xor %r13d, %r13d
    mov $__NR_clone, %eax
    mov $(CLONE_NEWUSER | SIGCHLD), %edi
    xor %esi, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js clone_failed
    jz child

    mov %rax, %r13

    mov pipefds(%rip), %edi
    mov $__NR_close, %eax
    syscall

    mov pipefds+4(%rip), %edi
    lea sync_byte(%rip), %rsi
    mov $1, %edx
    mov $__NR_write, %eax
    syscall
    cmp $1, %rax
    jne sync_failed

    mov pipefds+4(%rip), %edi
    mov $__NR_close, %eax
    syscall

    mov %r13, %rdi
    lea status(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $__NR_wait4, %eax
    syscall
    test %rax, %rax
    js wait_failed

    mov status(%rip), %edi
    test $0x7f, %edi
    jnz child_signaled
    shr $8, %edi
    and $0xff, %edi
    test %edi, %edi
    jnz exit

    movl $1, thread_started(%rip)
    mov $__NR_clone, %eax
    mov $(CLONE_THREAD_FLAGS | CLONE_THREAD_FLAGS_2), %edi
    lea thread_stack+65536(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js guest_thread_failed
    jz guest_thread

    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea task_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js parent_task_stat_failed
    cmpq $4, statbuf+16(%rip)
    jne parent_task_stat_failed

    mov $__NR_clone, %eax
    mov $(CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET | SIGCHLD), %edi
    xor %esi, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    syscall
    test %rax, %rax
    js combined_clone_failed
    jz combined_child

    mov %rax, %rdi
    lea status(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $__NR_wait4, %eax
    syscall
    test %rax, %rax
    js wait_failed
    mov status(%rip), %edi
    test $0x7f, %edi
    jnz child_signaled
    shr $8, %edi
    and $0xff, %edi
    jmp exit

combined_child:
    mov $39, %eax
    syscall
    cmp $1, %rax
    jne combined_child_pid_failed
    xor %edi, %edi
    jmp exit

guest_thread:
    mov $__NR_futex, %eax
    lea thread_release(%rip), %rdi
    mov $FUTEX_WAIT, %esi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    xor %r9d, %r9d
    syscall
    xor %edi, %edi
    mov $__NR_exit, %eax
    syscall

child:
    mov pipefds+4(%rip), %edi
    mov $__NR_close, %eax
    syscall

    mov pipefds(%rip), %edi
    lea sync_byte(%rip), %rsi
    mov $1, %edx
    mov $__NR_read, %eax
    syscall
    cmp $1, %rax
    jne child_sync_failed

    mov pipefds(%rip), %edi
    mov $__NR_close, %eax
    syscall

    call write_uid_map
    test %eax, %eax
    jnz child_uid_map_failed
    call write_setgroups
    test %eax, %eax
    jnz child_uid_map_failed
    call write_gid_map
    test %eax, %eax
    jnz child_uid_map_failed

    mov $__NR_newfstatat, %eax
    mov $AT_FDCWD, %edi
    lea task_path(%rip), %rsi
    lea statbuf(%rip), %rdx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js child_task_stat_failed
    cmpq $3, statbuf+16(%rip)
    jne child_task_stat_failed

    mov $__NR_unshare, %eax
    mov $CLONE_NEWUSER, %edi
    syscall
    test %rax, %rax
    js child_unshare_failed
    xor %edi, %edi
    jmp exit

clone_failed:
    mov $10, %edi
    jmp exit

pipe_failed:
    mov $13, %edi
    jmp exit

uid_map_failed:
    mov $14, %edi
    jmp exit

sync_failed:
    mov $15, %edi
    jmp exit

wait_failed:
    mov $11, %edi
    jmp exit

child_signaled:
    mov $12, %edi
    jmp exit

child_unshare_failed:
    mov $20, %edi
    jmp exit

child_sync_failed:
    mov $21, %edi
    jmp exit

child_uid_map_failed:
    mov $22, %edi
    jmp exit

child_task_stat_failed:
    mov $23, %edi
    jmp exit

combined_clone_failed:
    mov $24, %edi
    jmp exit

combined_child_pid_failed:
    mov $25, %edi
    jmp exit

guest_thread_failed:
    movl $0, thread_started(%rip)
    mov $26, %edi
    jmp exit

parent_task_stat_failed:
    mov $28, %edi

exit:
    cmpl $0, thread_started(%rip)
    je .Lsingle_thread_exit
    mov %edi, %r12d
    movl $1, thread_release(%rip)
    mov $__NR_futex, %eax
    lea thread_release(%rip), %rdi
    mov $FUTEX_WAKE, %esi
    mov $1, %edx
    syscall
    mov %r12d, %edi
    mov $__NR_exit_group, %eax
    syscall

.Lsingle_thread_exit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.type write_uid_map, @function
write_uid_map:
    lea uid_map_suffix(%rip), %rsi
    mov %r12, %rax
    jmp write_id_map
.size write_uid_map, .-write_uid_map

.type write_gid_map, @function
write_gid_map:
    lea gid_map_suffix(%rip), %rsi
    mov %rbx, %rax
    jmp write_id_map
.size write_gid_map, .-write_gid_map

.type write_id_map, @function
write_id_map:
    mov %rsi, %r8
    lea mapbuf(%rip), %rdi
    movb $'0', (%rdi)
    movb $' ', 1(%rdi)
    add $2, %rdi

    lea digits+20(%rip), %rsi
    mov %rsi, %r10
    mov $10, %r9
.Luid_digit:
    xor %edx, %edx
    div %r9
    dec %rsi
    add $'0', %dl
    mov %dl, (%rsi)
    test %rax, %rax
    jne .Luid_digit

    mov %r10, %rcx
    sub %rsi, %rcx
    rep movsb
    movb $' ', (%rdi)
    movb $'1', 1(%rdi)
    movb $'\n', 2(%rdi)
    add $3, %rdi
    lea mapbuf(%rip), %rdx
    sub %rdx, %rdi
    mov %rdi, %rdx
    mov %r8, %rsi
    lea mapbuf(%rip), %r11
    jmp write_proc_file
.size write_id_map, .-write_id_map

.type write_setgroups, @function
write_setgroups:
    lea setgroups_suffix(%rip), %rsi
    lea deny_setgroups(%rip), %r11
    mov $4, %edx
    jmp write_proc_file
.size write_setgroups, .-write_setgroups

.type write_proc_file, @function
write_proc_file:
    mov %rdx, %r14
    mov %r11, %r15
    mov %rsi, %r10

    lea pathbuf(%rip), %rdi
    lea proc_prefix(%rip), %rsi
    mov $6, %ecx
    rep movsb

    test %r13, %r13
    jnz .Lformat_pid
    lea self_name(%rip), %rsi
    mov $4, %ecx
    rep movsb
    jmp .Lappend_suffix

.Lformat_pid:
    mov %r13, %rax
    lea digits+20(%rip), %rsi
    mov %rsi, %r8
    mov $10, %r9
.Lpid_digit:
    xor %edx, %edx
    div %r9
    dec %rsi
    add $'0', %dl
    mov %dl, (%rsi)
    test %rax, %rax
    jne .Lpid_digit

    mov %r8, %rcx
    sub %rsi, %rcx
    rep movsb
.Lappend_suffix:
    mov %r10, %rsi
.Lcopy_suffix:
    lodsb
    stosb
    test %al, %al
    jne .Lcopy_suffix

    mov $__NR_openat, %eax
    mov $AT_FDCWD, %edi
    lea pathbuf(%rip), %rsi
    mov $O_WRONLY, %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js .Luid_map_error
    mov %rax, %r8

    mov $__NR_write, %eax
    mov %r8, %rdi
    mov %r15, %rsi
    mov %r14, %rdx
    syscall
    cmp %r14, %rax
    jne .Luid_map_close_error

    mov $__NR_close, %eax
    mov %r8, %rdi
    syscall
    xor %eax, %eax
    ret

.Luid_map_close_error:
    mov $__NR_close, %eax
    mov %r8, %rdi
    syscall
.Luid_map_error:
    mov $-1, %eax
    ret
.size write_proc_file, .-write_proc_file

.section .note.GNU-stack,"",@progbits
