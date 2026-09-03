.section .text
.global file_mapped_push_pop
.type file_mapped_push_pop, @function
file_mapped_push_pop:
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
    jne .Lfailed
    cmp $0x33, %r9
    jne .Lfailed
    cmp $0x22, %r10
    jne .Lfailed
    cmp $0x11, %rsi
    jne .Lfailed
    cmp %r11, %rsp
    jne .Lfailed
    lea file_mapped_fault(%rip), %rdx
    xor %eax, %eax
    ret
.Lfailed:
    mov $40, %eax
    ret
.size file_mapped_push_pop, .-file_mapped_push_pop

.type file_mapped_fault, @function
file_mapped_fault:
    push %rax
    push %rbx
    xor %eax, %eax
    mov (%rax), %rax
    ud2
.size file_mapped_fault, .-file_mapped_fault

.section .note.GNU-stack,"",@progbits
