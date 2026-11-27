.equ __NR_mmap, 9
.equ __NR_clone, 56
.equ __NR_sched_yield, 24
.equ __NR_exit, 60
.equ __NR_exit_group, 231

.equ PROT_READ_WRITE, 3
.equ MAP_PRIVATE_ANONYMOUS, 0x22
.equ CLONE_THREAD_FLAGS, 0x50f00
.equ STACK_SIZE, 0x100000
.equ ITERATIONS, 200000

.equ X, 0
.equ Y, 4
.equ GENERATION, 8
.equ DONE, 12
.equ RESULT_Y, 16
.equ RESULT_X, 20
.equ NEXT_ID, 24

.section .text
.global _start
.type _start, @function
_start:
    sub $64, %rsp
    movq $0, 0(%rsp)
    movq $0, 8(%rsp)
    movq $0, 16(%rsp)
    movq $0, 24(%rsp)
    mov %rsp, %r12
    mov $2, %r13d

.Lspawn:
    xor %edi, %edi
    mov $STACK_SIZE, %esi
    mov $PROT_READ_WRITE, %edx
    mov $MAP_PRIVATE_ANONYMOUS, %r10d
    mov $-1, %r8
    xor %r9d, %r9d
    mov $__NR_mmap, %eax
    syscall
    test %rax, %rax
    js .Lfail

    lea STACK_SIZE(%rax), %rsi
    mov $CLONE_THREAD_FLAGS, %edi
    xor %edx, %edx
    xor %r10d, %r10d
    xor %r8d, %r8d
    mov $__NR_clone, %eax
    syscall
    test %rax, %rax
    jz .Lchild
    js .Lfail

    dec %r13d
    jne .Lspawn

.Lready:
    cmpl $2, NEXT_ID(%r12)
    je .Lparent_loop
    /* Yield only while waiting, outside the memory sequence under test. */
    mov $__NR_sched_yield, %eax
    syscall
    jmp .Lready

.Lparent_loop:
    movl $0, X(%r12)
    movl $0, Y(%r12)
    movl $0, RESULT_Y(%r12)
    movl $0, RESULT_X(%r12)
    movl $0, DONE(%r12)
    lock incl GENERATION(%r12)

.Lwait_done:
    cmpl $2, DONE(%r12)
    je .Lcheck
    /* Yield only while waiting, outside the memory sequence under test. */
    mov $__NR_sched_yield, %eax
    syscall
    jmp .Lwait_done

.Lcheck:
    cmpl $1, RESULT_Y(%r12)
    jne .Lnext
    cmpl $0, RESULT_X(%r12)
    je .Lfail
.Lnext:
    cmpl $ITERATIONS, GENERATION(%r12)
    jne .Lparent_loop
    xor %edi, %edi
    jmp .Lexit_group

.Lchild:
    mov $1, %eax
    lock xaddl %eax, NEXT_ID(%r12)
    mov %eax, %r13d
    xor %r14d, %r14d

.Lchild_loop:
    inc %r14d
.Lwait_generation:
    cmpl %r14d, GENERATION(%r12)
    je .Lrun
    /* Yield only while waiting, outside the memory sequence under test. */
    mov $__NR_sched_yield, %eax
    syscall
    jmp .Lwait_generation

.Lrun:
    test %r13d, %r13d
    jne .Lreader
.global tso_store_site
tso_store_site:
    movl $1, X(%r12)
    movl $1, Y(%r12)
    jmp .Lcomplete

.Lreader:
    movl Y(%r12), %eax
    movl X(%r12), %edx
    movl %eax, RESULT_Y(%r12)
    movl %edx, RESULT_X(%r12)

.Lcomplete:
    lock incl DONE(%r12)
    cmp $ITERATIONS, %r14d
    jne .Lchild_loop
    xor %edi, %edi
    mov $__NR_exit, %eax
    syscall

.Lfail:
    mov $1, %edi
.Lexit_group:
    mov $__NR_exit_group, %eax
    syscall
.size _start, .-_start
