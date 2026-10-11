.global _start
.type _start, @function
.section .text
_start:
    cmpq $1, (%rsp)
    jne failed
    mov 8(%rsp), %r13
    test %r13, %r13
    jz failed
    cmpq $0, 16(%rsp)
    jne failed
    lea 24(%rsp), %rsi
scan_envp:
    cmpq $0, (%rsi)
    je envp_done
    add $8, %rsi
    jmp scan_envp
envp_done:
    add $8, %rsi
    xor %r15d, %r15d

scan_auxv:
    mov (%rsi), %rax
    mov 8(%rsi), %rdx
    test %rax, %rax
    jz auxv_done
    cmp $3, %rax
    je check_phdr
    cmp $4, %rax
    je check_phent
    cmp $5, %rax
    je check_phnum
    cmp $6, %rax
    je check_pagesz
    cmp $9, %rax
    je check_entry
    cmp $15, %rax
    je check_platform
    cmp $25, %rax
    je check_random
    cmp $31, %rax
    je check_execfn
next_auxv:
    add $16, %rsi
    jmp scan_auxv

check_phdr:
    test %rdx, %rdx
    jz failed
    or $1, %r15d
    jmp next_auxv
check_phent:
    cmp $56, %rdx
    jne failed
    or $2, %r15d
    jmp next_auxv
check_phnum:
    test %rdx, %rdx
    jz failed
    or $4, %r15d
    jmp next_auxv
check_pagesz:
    test %rdx, %rdx
    jz failed
    lea -1(%rdx), %rax
    test %rdx, %rax
    jne failed
    or $8, %r15d
    jmp next_auxv
check_entry:
    lea _start(%rip), %rax
    cmp %rax, %rdx
    jne failed
    or $16, %r15d
    jmp next_auxv
check_platform:
    test %rdx, %rdx
    jz failed
    cmpl $0x5f363878, (%rdx)
    jne failed
    cmpw $0x3436, 4(%rdx)
    jne failed
    or $32, %r15d
    jmp next_auxv
check_random:
    mov (%rdx), %rax
    or 8(%rdx), %rax
    jz failed
    or $64, %r15d
    jmp next_auxv
check_execfn:
    test %rdx, %rdx
    jz failed
    movzbl (%r13), %eax
    cmp %al, (%rdx)
    jne failed
    or $128, %r15d
    jmp next_auxv

auxv_done:
    cmp $255, %r15d
    jne failed
    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $message_end-message, %edx
    syscall
    mov $60, %eax
    mov $42, %edi
    syscall

failed:
    mov $60, %eax
    mov $96, %edi
    syscall
    ud2
.size _start, .-_start

.section .rodata
message:
    .ascii "AUXV OK\n"
message_end:
