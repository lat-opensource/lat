.intel_syntax noprefix
.section .text
.global _start

_start:
    lea rdi, [rip + input_zero]
    xor esi, esi
    mov edx, 0x12345678
    call diff_cmov_u16
    cmp eax, 0x8000
    jne .Lfail

    lea rdi, [rip + input_7fff]
    xor esi, esi
    mov edx, 0x12345678
    call diff_cmov_u16
    cmp eax, 0x5678
    jne .Lfail

    lea rdi, [rip + input_one]
    mov esi, 0x7ffc
    mov edx, 0xffff0001
    call diff_cmov_u16
    cmp eax, 1
    jne .Lfail

    mov r11d, 1
    call bsr_not_add
    cmp eax, 14
    jne .Lfail
    mov r11d, 0x8000
    call bsr_not_add
    cmp eax, -1
    jne .Lfail
    mov r11d, 0x80000000
    call bsr_not_add
    cmp eax, -17
    jne .Lfail

    xor edi, edi
    call clamp_u8
    cmp eax, 1
    jne .Lfail
    mov edi, 1
    call clamp_u8
    cmp eax, 1
    jne .Lfail
    mov edi, 254
    call clamp_u8
    cmp eax, 254
    jne .Lfail
    mov edi, 255
    call clamp_u8
    cmp eax, 255
    jne .Lfail
    mov edi, 256
    call clamp_u8
    cmp eax, 255
    jne .Lfail

    /* The matcher rejects this source/scratch alias. */
    mov edi, 123
    call clamp_u8_alias
    cmp eax, 1
    jne .Lfail

    xor edi, edi
    jmp .Lexit
.Lfail:
    mov edi, 1
.Lexit:
    mov eax, 60
    syscall

.type diff_cmov_u16, @function
diff_cmov_u16:
    mov r11d, esi
    movzx r9d, WORD PTR [rdi]
    mov r10d, 0xffff8000
    sub r10d, r9d
    mov ecx, r10d
    sub ecx, r11d
    cmp cx, 5
    cmovb ecx, edx
    movzx r11d, cx
    cmp r11d, 0x7fff
    mov eax, r11d
    ret
.size diff_cmov_u16, .-diff_cmov_u16

.type bsr_not_add, @function
bsr_not_add:
    bsr ecx, r11d
    not ecx
    add ecx, 15
    cmp ecx, ecx
    mov eax, ecx
    ret
.size bsr_not_add, .-bsr_not_add

.type clamp_u8, @function
clamp_u8:
    mov eax, 255
    sub eax, edi
    shr eax, 23
    xor ecx, ecx
    test rdi, rdi
    sete cl
    or eax, edi
    or eax, ecx
    movzx eax, al
    cmp eax, eax
    ret
.size clamp_u8, .-clamp_u8

.type clamp_u8_alias, @function
clamp_u8_alias:
    mov eax, 255
    sub eax, edi
    shr eax, 23
    xor edi, edi
    test rdi, rdi
    sete dil
    or eax, edi
    or eax, edi
    movzx eax, al
    cmp eax, eax
    ret
.size clamp_u8_alias, .-clamp_u8_alias

.section .rodata
.balign 2
input_zero: .word 0
input_one: .word 1
input_7fff: .word 0x7fff

.section .note.GNU-stack,"",@progbits
