.equ __NR_exit, 60

.section .rodata
alternate_fcw:
    .word 0x027f

.section .bss
.align 64
fx_state:
    .skip 512
restored_fcw:
    .skip 2

.section .text
.global _start
.type _start, @function
_start:
    fninit
    fxsave64 fx_state(%rip)
    cmpw $0x037f, fx_state(%rip)
    jne .Lfxsave_failed

    fldcw alternate_fcw(%rip)
    fxrstor64 fx_state(%rip)
    fnstcw restored_fcw(%rip)
    cmpw $0x037f, restored_fcw(%rip)
    jne .Lfxrstor_failed

    xor %edi, %edi
    jmp .Lexit
.Lfxsave_failed:
    mov $42, %edi
    jmp .Lexit
.Lfxrstor_failed:
    mov $43, %edi
.Lexit:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
