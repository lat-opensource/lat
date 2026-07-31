.equ __NR_close, 3
.equ __NR_exit_group, 231
.equ __NR_getdents64, 217
.equ __NR_openat, 257
.equ AT_FDCWD, -100
.equ O_DIRECTORY, 0x10000

.section .rodata
proc_path:
    .asciz "/proc"

.section .bss
.align 16
dirents:
    .space 32768

.section .text
.global _start
.type _start, @function
_start:
    mov $__NR_openat, %eax
    mov $AT_FDCWD, %edi
    lea proc_path(%rip), %rsi
    mov $O_DIRECTORY, %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js open_failed
    mov %eax, %r12d

read_dirents:
    mov $__NR_getdents64, %eax
    mov %r12d, %edi
    lea dirents(%rip), %rsi
    mov $32768, %edx
    syscall
    test %rax, %rax
    js getdents_failed
    jz no_numeric_entry

    lea dirents(%rip), %r13
    lea (%r13,%rax), %r14

scan_dirents:
    cmp %r14, %r13
    jae read_dirents
    movzwl 16(%r13), %ebx
    cmp $19, %ebx
    jbe malformed_dirent
    movzbl 19(%r13), %ecx
    cmp $'1', %cl
    jb next_dirent
    cmp $'9', %cl
    jbe found_numeric_entry

next_dirent:
    add %rbx, %r13
    jmp scan_dirents

found_numeric_entry:
    mov %r12d, %edi
    mov $__NR_close, %eax
    syscall
    xor %edi, %edi
    jmp exit

open_failed:
    mov $10, %edi
    jmp exit

getdents_failed:
    mov $11, %edi
    jmp exit

no_numeric_entry:
    mov $12, %edi
    jmp exit

malformed_dirent:
    mov $13, %edi

exit:
    mov $__NR_exit_group, %eax
    syscall
    hlt
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
