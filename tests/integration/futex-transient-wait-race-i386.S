/*
 * Reproduce the race between i386 private futex waits and LATX locked
 * instruction lowering.  Concurrent FUTEX_WAIT_PRIVATE and
 * FUTEX_WAIT_BITSET_PRIVATE calls on the same page must not observe EFAULT,
 * while unaligned and overlapping LOCK INC / LOCK CMPXCHG8B operations retain
 * their exact counts.
 */

.equ __NR_clone, 120
.equ __NR_exit, 1
.equ __NR_exit_group, 252
.equ __NR_futex, 240
.equ __NR_mmap2, 192
.equ __NR_clock_gettime, 265
.equ __NR_rt_sigaction, 174

.equ CLONE_VM, 0x00000100
.equ CLONE_FS, 0x00000200
.equ CLONE_FILES, 0x00000400
.equ CLONE_SIGHAND, 0x00000800
.equ CLONE_THREAD, 0x00010000
.equ CLONE_SYSVSEM, 0x00040000
.equ CLONE_FLAGS, (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM)

.equ PROT_NONE, 0
.equ PROT_READ, 1
.equ PROT_WRITE, 2
.equ MAP_PRIVATE, 2
.equ MAP_ANONYMOUS, 32

.equ CLOCK_MONOTONIC, 1
.equ FUTEX_WAIT_PRIVATE, 0x80
.equ FUTEX_WAIT_BITSET_PRIVATE, 0x89
.equ FUTEX_WAKE_PRIVATE, 0x81
.equ FUTEX_BITSET_MATCH_ANY, -1
.equ EFAULT, 14
.equ SIGSEGV, 11
.equ KERNEL_SIGSET_SIZE, 8

.equ PAGE_SIZE, 4096
.equ WAITER_COUNT, 12
.equ INC_WORKER_COUNT, 4
.equ CMPXCHG8B_WORKER_COUNT, 4
.equ ATOMIC_WORKER_COUNT, (INC_WORKER_COUNT + CMPXCHG8B_WORKER_COUNT)
.equ ATOMIC_ITERATIONS, 5000
.equ STACK_SIZE, 32768

.section .text
.global _start
.type _start, @function
_start:
    /* mmap2(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) */
    mov $__NR_mmap2, %eax
    xor %ebx, %ebx
    mov $PAGE_SIZE, %ecx
    mov $(PROT_READ | PROT_WRITE), %edx
    mov $(MAP_PRIVATE | MAP_ANONYMOUS), %esi
    mov $-1, %edi
    xor %ebp, %ebp
    int $0x80
    test %eax, %eax
    js exit_failure
    mov %eax, shared_page
    movl $0, 5(%eax)
    movl $0, 9(%eax)
    movl $0, 16(%eax)
    movl $0, 60(%eax)
    movl $0, 64(%eax)

    /* Establish the futex page before any locked operation can migrate it. */
    mov $__NR_futex, %eax
    mov shared_page, %ebx
    lea 16(%ebx), %ebx
    mov $FUTEX_WAKE_PRIVATE, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    xor %ebp, %ebp
    int $0x80

    movl $waiter_stacks_end, next_waiter_stack
    movl $WAITER_COUNT, waiters_to_spawn
    movl $atomic_stacks_end, next_atomic_stack
    movl $ATOMIC_WORKER_COUNT, atomics_to_spawn

spawn_atomic:
    cmpl $0, atomics_to_spawn
    je spawn_waiter
    decl atomics_to_spawn
    mov atomics_to_spawn, %edx
    mov $__NR_clone, %eax
    mov $CLONE_FLAGS, %ebx
    mov next_atomic_stack, %ecx
    subl $STACK_SIZE, next_atomic_stack
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js exit_failure
    jz atomic_worker
    jmp spawn_atomic

spawn_waiter:
    cmpl $0, waiters_to_spawn
    je wait_for_workers
    decl waiters_to_spawn
    mov next_waiter_stack, %ecx
    subl $STACK_SIZE, next_waiter_stack
    mov waiters_to_spawn, %edx
    mov $__NR_clone, %eax
    mov $CLONE_FLAGS, %ebx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js exit_failure
    jz waiter
    jmp spawn_waiter

wait_for_workers:
    cmpl $(WAITER_COUNT + ATOMIC_WORKER_COUNT), ready_workers
    jne wait_for_workers
    movl $1, start_waiters

wait_for_waiters_to_enter:
    cmpl $WAITER_COUNT, waiters_entered
    jne wait_for_waiters_to_enter
    movl $1, start_atomics

wait_for_atomic_completion:
    cmpl $ATOMIC_WORKER_COUNT, atomic_workers_done
    jne wait_for_atomic_completion
    mov shared_page, %eax
    cmpl $(ATOMIC_WORKER_COUNT * ATOMIC_ITERATIONS), 5(%eax)
    jne exit_primary_count_failure
    cmpl $0, 9(%eax)
    jne exit_primary_count_failure
    cmpl $(CMPXCHG8B_WORKER_COUNT * ATOMIC_ITERATIONS), 60(%eax)
    jne exit_crossing_low_count_failure
    cmpl $(INC_WORKER_COUNT * ATOMIC_ITERATIONS), 64(%eax)
    jne exit_crossing_high_count_failure
    jmp start_fault_cleanup_test

atomic_worker:
    lock incl ready_workers
wait_for_atomic_start:
    cmpl $1, start_atomics
    jne wait_for_atomic_start
    test $1, %edx
    jnz atomic_cmpxchg8b_worker

atomic_inc_worker:
    mov shared_page, %esi
    push $ATOMIC_ITERATIONS
atomic_inc_loop:
    /* A 32-bit operand at +5 crosses a host 8-byte atomic boundary. */
    lock incl 5(%esi)
    /* This overlaps the high half of the CMPXCHG8B operand at +60. */
    lock incl 64(%esi)
    mov $__NR_futex, %eax
    lea 16(%esi), %ebx
    mov $FUTEX_WAKE_PRIVATE, %ecx
    mov $0x7fffffff, %edx
    xor %esi, %esi
    xor %edi, %edi
    xor %ebp, %ebp
    int $0x80
    mov shared_page, %esi
    decl (%esp)
    jne atomic_inc_loop
    add $4, %esp
    jmp atomic_done

atomic_cmpxchg8b_worker:
    mov shared_page, %esi
    mov $ATOMIC_ITERATIONS, %edi
atomic_cmpxchg8b_loop:
    mov 5(%esi), %eax
    mov 9(%esi), %edx
cmpxchg8b_retry:
    lea 1(%eax), %ebx
    mov %edx, %ecx
    /* A 64-bit operand at +5 crosses a host 8-byte atomic boundary. */
    lock cmpxchg8b 5(%esi)
    jnz cmpxchg8b_retry

    /*
     * +60 crosses a 64-byte lock stripe.  The increment at +64 overlaps its
     * high 32 bits, so they must serialize even though their base addresses
     * fall in adjacent stripes.
     */
    mov 60(%esi), %eax
    mov 64(%esi), %edx
cmpxchg8b_crossing_retry:
    lea 1(%eax), %ebx
    mov %edx, %ecx
    lock cmpxchg8b 60(%esi)
    jnz cmpxchg8b_crossing_retry

    push %edi
    mov $__NR_futex, %eax
    lea 16(%esi), %ebx
    mov $FUTEX_WAKE_PRIVATE, %ecx
    mov $0x7fffffff, %edx
    xor %esi, %esi
    xor %edi, %edi
    xor %ebp, %ebp
    int $0x80
    pop %edi
    mov shared_page, %esi
    decl %edi
    jnz atomic_cmpxchg8b_loop

atomic_done:
    lock incl atomic_workers_done
    mov $__NR_exit, %eax
    xor %ebx, %ebx
    int $0x80
    ud2

/*
 * A fault in the load/store part of a spinlock-lowered LOCK instruction
 * leaves the current TB through cpu_loop_exit().  The SIGSEGV handler waits
 * for another guest thread to execute a valid LOCK INC.  If the faulting
 * thread kept the internal lock, that worker cannot make progress and this
 * test times out.
 */
start_fault_cleanup_test:
    mov $__NR_mmap2, %eax
    xor %ebx, %ebx
    mov $PAGE_SIZE, %ecx
    mov $PROT_NONE, %edx
    mov $(MAP_PRIVATE | MAP_ANONYMOUS), %esi
    mov $-1, %edi
    xor %ebp, %ebp
    int $0x80
    test %eax, %eax
    js exit_failure
    mov %eax, fault_page

    mov $__NR_rt_sigaction, %eax
    mov $SIGSEGV, %ebx
    mov $fault_sigaction, %ecx
    xor %edx, %edx
    mov $KERNEL_SIGSET_SIZE, %esi
    int $0x80
    test %eax, %eax
    js exit_failure

    mov $__NR_clone, %eax
    mov $CLONE_FLAGS, %ebx
    mov $fault_worker_stack_end, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    js exit_failure
    jz fault_worker

    mov fault_page, %esi
    lock incl (%esi)
    jmp exit_failure

fault_worker:
fault_worker_wait:
    cmpl $1, fault_seen
    jne fault_worker_wait
    lock incl fault_worker_done
    mov $__NR_exit, %eax
    xor %ebx, %ebx
    int $0x80
    ud2

fault_signal_handler:
    movl $1, fault_seen
fault_handler_wait:
    cmpl $1, fault_worker_done
    jne fault_handler_wait
    xor %ebx, %ebx
    jmp exit_group

waiter:
    sub $8, %esp
    lock incl ready_workers
wait_for_waiter_start:
    cmpl $1, start_waiters
    jne wait_for_waiter_start
    test $1, %edx
    jnz waiter_plain_enter
waiter_bitset_enter:
    mov $__NR_clock_gettime, %eax
    mov $CLOCK_MONOTONIC, %ebx
    mov %esp, %ecx
    int $0x80
    test %eax, %eax
    js exit_failure
    incl (%esp)
    lock incl waiters_entered
    jmp waiter_bitset_futex

waiter_bitset_loop:
    mov $__NR_clock_gettime, %eax
    mov $CLOCK_MONOTONIC, %ebx
    mov %esp, %ecx
    int $0x80
    test %eax, %eax
    js exit_failure
    incl (%esp)

waiter_bitset_futex:
    mov $__NR_futex, %eax
    mov shared_page, %ebx
    lea 16(%ebx), %ebx
    mov $FUTEX_WAIT_BITSET_PRIVATE, %ecx
    xor %edx, %edx
    mov %esp, %esi
    xor %edi, %edi
    mov $FUTEX_BITSET_MATCH_ANY, %ebp
    int $0x80
    cmpl $-EFAULT, %eax
    je exit_efault
    jmp waiter_bitset_loop

waiter_plain_enter:
    lock incl waiters_entered
waiter_plain_loop:
    mov $__NR_futex, %eax
    mov shared_page, %ebx
    lea 16(%ebx), %ebx
    mov $FUTEX_WAIT_PRIVATE, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    xor %ebp, %ebp
    int $0x80
    cmpl $-EFAULT, %eax
    je exit_efault
    jmp waiter_plain_loop

exit_efault:
    mov $EFAULT, %ebx
    jmp exit_group

exit_primary_count_failure:
    mov $2, %ebx
    jmp exit_group

exit_crossing_low_count_failure:
    mov $3, %ebx
    jmp exit_group

exit_crossing_high_count_failure:
    mov $4, %ebx
    jmp exit_group

exit_failure:
    mov $1, %ebx
exit_group:
    mov $__NR_exit_group, %eax
    int $0x80
    ud2
.size _start, .-_start

.section .data
.balign 4
fault_sigaction:
    .long fault_signal_handler
    .long 0
    .long 0
    .long 0
    .long 0

.section .bss
.balign 4
shared_page:
    .space 4
ready_workers:
    .space 4
start_waiters:
    .space 4
start_atomics:
    .space 4
waiters_entered:
    .space 4
waiters_to_spawn:
    .space 4
next_waiter_stack:
    .space 4
atomic_workers_done:
    .space 4
fault_page:
    .space 4
fault_seen:
    .space 4
fault_worker_done:
    .space 4
.balign 16
next_atomic_stack:
    .space 4
atomics_to_spawn:
    .space 4
atomic_stacks:
    .space (ATOMIC_WORKER_COUNT * STACK_SIZE)
atomic_stacks_end:
.balign 16
waiter_stacks:
    .space (WAITER_COUNT * STACK_SIZE)
waiter_stacks_end:
.balign 16
fault_worker_stack:
    .space STACK_SIZE
fault_worker_stack_end:

.section .note.GNU-stack,"",@progbits
