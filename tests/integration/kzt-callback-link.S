.global _start
.text
_start:
    jmp target
.align 16
target:
    inc %r12d
    cmp $5,%r12d
    jb _start
    mov $60,%eax
    xor %edi,%edi
    syscall
