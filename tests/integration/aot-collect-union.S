.global _start, path_a, path_b
.text
_start:
    mov 16(%rsp),%rax
    cmpb $98,(%rax)
    lea path_a(%rip),%rax
    lea path_b(%rip),%rcx
    cmove %rcx,%rax
    call *%rax
    mov $60,%eax
    xor %edi,%edi
    syscall
path_a:
    mov $11,%eax
    add $7,%eax
    ret
path_b:
    mov $23,%eax
    sub $3,%eax
    ret
.section .note.GNU-stack,"",@progbits
