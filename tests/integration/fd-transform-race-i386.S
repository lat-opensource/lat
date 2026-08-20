.equ __NR_exit, 1
.equ __NR_fork, 2
.equ __NR_read, 3
.equ __NR_close, 6
.equ __NR_waitpid, 7
.equ __NR_dup2, 63
.equ __NR_getrlimit, 76
.equ __NR_clone, 120
.equ __NR_eventfd2, 328
.equ __NR_sched_yield, 158

.equ RLIMIT_NOFILE, 7
.equ EFD_SEMAPHORE, 1

.equ CLONE_VM, 0x00000100
.equ CLONE_FS, 0x00000200
.equ CLONE_FILES, 0x00000400
.equ CLONE_SIGHAND, 0x00000800
.equ CLONE_THREAD, 0x00010000
.equ CLONE_SYSVSEM, 0x00040000
.equ CLONE_FLAGS, (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND)
.equ CLONE_THREAD_FLAGS, (CLONE_THREAD | CLONE_SYSVSEM)

.equ READER_COUNT, 4
.equ STACK_SHIFT, 16

.section .text
.global _start
.type _start, @function
_start:
    mov $__NR_getrlimit, %eax
    mov $RLIMIT_NOFILE, %ebx
    mov $fd_limit, %ecx
    int $0x80
    test %eax, %eax
    js fail_limit

    mov $__NR_eventfd2, %eax
    mov $0x7fffffff, %ebx
    mov $EFD_SEMAPHORE, %ecx
    int $0x80
    test %eax, %eax
    js fail_eventfd
    mov %eax, eventfd

    /* Cross the next 64-descriptor table boundary. */
    mov %eax, %ecx
    shr $6, %ecx
    inc %ecx
    shl $6, %ecx
    cmpl fd_limit, %ecx
    jae skip_limit
    mov %ecx, grow_fd

spawn_reader:
    mov reader_spawned, %ecx
    cmp $READER_COUNT, %ecx
    je spawn_churner
    inc %ecx
    shl $STACK_SHIFT, %ecx
    add $reader_stacks, %ecx
    mov $__NR_clone, %eax
    mov $(CLONE_FLAGS | CLONE_THREAD_FLAGS), %ebx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js fail_reader_clone
    jz reader
    incl reader_spawned
    jmp spawn_reader

spawn_churner:
    mov $__NR_clone, %eax
    mov $(CLONE_FLAGS | CLONE_THREAD_FLAGS), %ebx
    mov $churner_stack_end, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js fail_churner_clone
    jz churner

wait_readers:
    cmpl $READER_COUNT, reader_ready
    jne wait_readers
wait_churner:
    cmpl $1, churner_ready
    jne wait_churner
    movl $1, start_workers

wait_churner_started:
    cmpl $1, churner_started
    jne wait_churner_started
    mov $__NR_sched_yield, %eax
    int $0x80

    /* Exercise the fd-transform prefork/postfork lock handoff. */
    mov $__NR_fork, %eax
    int $0x80
    test %eax, %eax
    js fail_fork
    jz fork_child
    mov %eax, fork_child_pid

    mov $__NR_waitpid, %eax
    mov fork_child_pid, %ebx
    mov $fork_child_status, %ecx
    xor %edx, %edx
    int $0x80
    cmp fork_child_pid, %eax
    jne fail_fork_wait
    cmpl $0, fork_child_status
    jne fail_fork_child
    jmp wait_churner_done

fork_child:
    mov $__NR_dup2, %eax
    mov eventfd, %ebx
    mov grow_fd, %ecx
    int $0x80
    cmp grow_fd, %eax
    jne fork_child_failed
    mov $__NR_close, %eax
    mov grow_fd, %ebx
    int $0x80
    test %eax, %eax
    js fork_child_failed
    xor %ebx, %ebx
    jmp exit_now

fork_child_failed:
    mov $19, %ebx
    jmp exit_now

wait_churner_done:
    cmpl $1, churner_done
    jne wait_churner_done
wait_readers_done:
    cmpl $READER_COUNT, reader_done
    jne wait_readers_done
    cmpl $0, reader_error
    jne fail_reader_read
    cmpl $0, churner_error
    jne fail_churner_dup

    xor %ebx, %ebx
    jmp exit_now

reader:
    sub $16, %esp
    lock incl reader_ready
wait_reader_start:
    cmpl $1, start_workers
    jne wait_reader_start
reader_loop:
    mov $__NR_read, %eax
    mov eventfd, %ebx
    mov %esp, %ecx
    mov $8, %edx
    int $0x80
    cmp $8, %eax
    jne reader_read_failed
    cmpl $1, churner_done
    jne reader_loop
    lock incl reader_done
    xor %ebx, %ebx
    jmp exit_now

reader_read_failed:
    movl $1, reader_error
    lock incl reader_done
    xor %ebx, %ebx
    jmp exit_now

churner:
    lock incl churner_ready
wait_churner_start:
    cmpl $1, start_workers
    jne wait_churner_start
    movl $1, churner_started

    /* Grow the table while the readers are in read(2). */
    mov $__NR_dup2, %eax
    mov eventfd, %ebx
    mov grow_fd, %ecx
    int $0x80
    cmp grow_fd, %eax
    jne churner_dup_failed
    movl $1, churner_done
    xor %ebx, %ebx
    jmp exit_now

churner_dup_failed:
    movl $1, churner_error
    movl $1, churner_done
    xor %ebx, %ebx
    jmp exit_now

skip_limit:
    mov $77, %ebx
    jmp exit_now
fail_limit:
    mov $10, %ebx
    jmp exit_now
fail_eventfd:
    mov $11, %ebx
    jmp exit_now
fail_reader_clone:
    mov $13, %ebx
    jmp exit_now
fail_churner_clone:
    mov $14, %ebx
    jmp exit_now
fail_churner_dup:
    mov $15, %ebx
    jmp exit_now
fail_reader_read:
    mov $16, %ebx
    jmp exit_now
fail_fork:
    mov $17, %ebx
    jmp exit_now
fail_fork_wait:
    mov $18, %ebx
    jmp exit_now
fail_fork_child:
    mov $19, %ebx

exit_now:
    mov $__NR_exit, %eax
    int $0x80
.size _start, .-_start

.section .bss
.balign 4
fd_limit:
    .space 8
eventfd:
    .space 4
grow_fd:
    .space 4
reader_spawned:
    .space 4
reader_ready:
    .space 4
reader_done:
    .space 4
reader_error:
    .space 4
churner_ready:
    .space 4
churner_started:
    .space 4
churner_done:
    .space 4
churner_error:
    .space 4
start_workers:
    .space 4
fork_child_pid:
    .space 4
fork_child_status:
    .space 4
.balign 16
reader_stacks:
    .space 262144
churner_stack:
    .space 65536
churner_stack_end:

.section .note.GNU-stack,"",@progbits
