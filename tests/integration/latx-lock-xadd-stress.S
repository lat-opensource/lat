.equ __NR_mmap, 9
.equ __NR_clone, 56
.equ __NR_sched_yield, 24
.equ __NR_exit, 60

.equ PROT_READ_WRITE, 3
.equ MAP_PRIVATE_ANONYMOUS, 0x22
.equ CLONE_THREAD_FLAGS, 0x50f00
.equ STACK_SIZE, 0x100000
.equ THREADS, 8
.equ ITERATIONS, 1000000
.equ EXPECTED, THREADS * ITERATIONS

.section .text
.global _start
.type _start, @function
_start:
    sub $64, %rsp
    movq $0, (%rsp)
    mov %rsp, %r12
    mov $THREADS, %r13d

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

.Lwait:
    cmpl $THREADS, 4(%r12)
    je .Lcheck
    /* Yield only while waiting, outside the memory sequence under test. */
    mov $__NR_sched_yield, %eax
    syscall
    jmp .Lwait

.Lcheck:
    cmpl $EXPECTED, (%r12)
    jne .Lfail
    xor %edi, %edi
    jmp .Lexit

.Lchild:
    mov $ITERATIONS, %ecx
.Lincrement:
    mov $1, %eax
.global lock_xadd_site
lock_xadd_site:
    lock xaddl %eax, (%r12)
    dec %ecx
    jne .Lincrement
    lock incl 4(%r12)
    xor %edi, %edi
    jmp .Lexit

.Lfail:
    mov $1, %edi

.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start
