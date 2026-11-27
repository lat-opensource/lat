/* Carry input and carry output differ in every case. ADX preserves all
 * non-target flags, and 32-bit results must zero the high half. */
.macro CHECK op, width, lhs, input, result, flags
    movabs $\lhs, %rax
    xor %ebx, %ebx
    pushq $\input
    popfq
    .if \width == 32
        \op %ebx, %eax
    .else
        \op %rbx, %rax
    .endif
    pushfq
    pop %rdx
    movabs $\result, %rcx
    cmp %rcx, %rax
    jne fail
    and $0x8d5, %edx
    cmp $\flags, %edx
    jne fail
.endm
.text
.global _start
_start:
    CHECK adcx, 64, 0, 0x8d7, 1, 0x8d4
    CHECK adcx, 64, -1, 0x8d7, 0, 0x8d5
    CHECK adcx, 32, 0xffffffff00000000, 0x8d7, 1, 0x8d4
    CHECK adcx, 32, -1, 0x8d7, 0, 0x8d5
    CHECK adox, 64, 0, 0x8d7, 1, 0xd5
    CHECK adox, 64, -1, 0x8d7, 0, 0x8d5
    CHECK adox, 32, 0xffffffff00000000, 0x8d7, 1, 0xd5
    CHECK adox, 32, -1, 0x8d7, 0, 0x8d5
    xor %edi, %edi
    jmp exit
fail:
    mov $1, %edi
exit:
    mov $60, %eax
    syscall
