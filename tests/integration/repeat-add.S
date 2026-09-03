.intel_syntax noprefix
.global _start

.text
_start:
    mov rax, 5
    mov rdx, 7
    .rept 100
    add rax, rdx
    .endr
    cmp rax, 705
    jne .Lfail

    mov eax, 60
    xor edi, edi
    syscall

.Lfail:
    mov eax, 60
    mov edi, 1
    syscall

.section .note.GNU-stack,"",@progbits
