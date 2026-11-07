/* Dedicated pages keep permission/mapping tests away from their own driver. */
        .text
        .p2align 12
        .globl tu_f0
tu_f0:
        movl $1, %eax
        jmp tu_f1
        .p2align 12
        .globl tu_f1, tu_f1_imm
tu_f1:
        .byte 0x83, 0xc0
tu_f1_imm:
        .byte 2
        jmp tu_f2
        .p2align 12
        .globl tu_f2
tu_f2:
        ret
        .p2align 12
        .section .note.GNU-stack,"",@progbits
