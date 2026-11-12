; SPDX-License-Identifier: GPL-2.0-only
; LDMXCSR must preserve arbitrary MMX payloads, including signaling NaNs.
%ifidn __OUTPUT_FORMAT__, elf64
bits 64
default rel
%else
bits 32
%endif
global _start
%macro CHECK_MMX 1
    ; Loading MXCSR calls a status helper but must preserve MMX registers.
    movq mm0, [sentinel]
    %1 [clean]
    movq [mmx_output], mm0
    mov eax, [mmx_output]
    cmp eax, [sentinel]
    jne fail
    mov eax, [mmx_output + 4]
    cmp eax, [sentinel + 4]
    jne fail
    emms

%endmacro
section .text
_start:
    CHECK_MMX ldmxcsr
    CHECK_MMX vldmxcsr
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
exit:
%if __BITS__ == 64
    mov eax, 60
    syscall
%else
    mov ebx, edi
    mov eax, 1
    int 0x80
%endif
section .rodata align=16
clean: dd 0x1f80
sentinel: dq 0x7ff0000000000001
section .bss align=16
mmx_output: resq 1
section .note.GNU-stack noalloc noexec nowrite progbits
