.equ __NR_exit, 60

.section .data
.balign 16
source:
    .byte 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
    .byte 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    .byte 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
    .byte 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f

.balign 16
destination:
    .zero 32

.balign 4096
cross_page_source_base:
    .zero 4093
cross_page_source:
    .byte 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
    .byte 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f

.balign 4096
cross_page_destination_base:
    .zero 4091
cross_page_destination:
    .zero 16

.section .text
.global _start
.type _start, @function
_start:
    lea source(%rip), %rsi
    lea destination(%rip), %rdi

    movdqu 1(%rsi), %xmm0
    movdqu %xmm0, 3(%rdi)

    xor %ecx, %ecx
.Lverify:
    movzbl 1(%rsi,%rcx), %eax
    cmpb %al, 3(%rdi,%rcx)
    jne .Lfail
    inc %ecx
    cmp $16, %ecx
    jne .Lverify

    lea cross_page_source(%rip), %rsi
    lea cross_page_destination(%rip), %rdi
    movdqu (%rsi), %xmm1
    movdqu %xmm1, (%rdi)

    xor %ecx, %ecx
.Lverify_cross_page:
    movzbl (%rsi,%rcx), %eax
    cmpb %al, (%rdi,%rcx)
    jne .Lfail
    inc %ecx
    cmp $16, %ecx
    jne .Lverify_cross_page

    xor %edi, %edi
    jmp .Lexit

.Lfail:
    mov $1, %edi

.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start
