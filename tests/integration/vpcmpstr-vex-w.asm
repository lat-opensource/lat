; SPDX-License-Identifier: GPL-2.0-only
BITS 64
DEFAULT REL
GLOBAL _start

; Opcode, VEX.W, memory operand. The explicit-length forms must distinguish
; EAX/EDX=1 from RAX/RDX=0x100000001, which saturates to length 16.
%macro CHECK 3
    vmovdqu ymm0, [ones]
    vmovdqu ymm11, [string_a]
    vmovdqu ymm12, [string_b]
    mov rax, 0x100000001
    mov rdx, 0x100000001
    jmp %%run
%%run:
    db 0xc4
%if %3
    db 0x63
%else
    db 0x43
%endif
    db 0x79 | (%2 << 7), %1
%if %3
    db 0x1d
    dd string_b - ($ + 5)
%else
    db 0xdc
%endif
    db 0
    pushfq
    pop r8
    vmovdqu [actual], ymm0
%assign short_lengths (%1 < 0x62 && %2 == 0)
    and r8d, 0x8d5
%if short_lengths
    cmp r8d, 0xc0           ; ZF/SF: both explicit lengths are short.
%else
    cmp r8d, 1             ; CF: matches at byte positions 1..15.
%endif
    jne bad
%if %1 & 1
%if short_lengths
    cmp ecx, 16
%else
    cmp ecx, 1
%endif
    jne bad
    vpxor ymm5, ymm0, [ones] ; Index forms leave the entire YMM0 unchanged.
    vptest ymm5, ymm5
    jnz bad
%else
%if short_lengths
    cmp qword [actual], 0
%else
    cmp qword [actual], 0xfffe
%endif
    jne bad
    cmp qword [actual + 8], 0
    jne bad
    cmp qword [actual + 16], 0
    jne bad
    cmp qword [actual + 24], 0
    jne bad
%endif
    vpxor ymm5, ymm11, [string_a]
    vptest ymm5, ymm5
    jnz bad
    vpxor ymm5, ymm12, [string_b]
    vptest ymm5, ymm5
    jnz bad
%endmacro

SECTION .text
_start:
%assign op 0x60
%rep 4
    CHECK op, 0, 0
    CHECK op, 1, 0
    CHECK op, 0, 1
    CHECK op, 1, 1
%assign op op + 1
%endrep
    xor edi, edi
    mov eax, 60
    syscall
bad:
    mov edi, 1
    mov eax, 60
    syscall

SECTION .rodata
ALIGN 32
ones: times 4 dq -1
string_a: times 16 db 'A'
          times 2 dq -1
string_b: db 'B'
          times 15 db 'A'
          times 2 dq -1
SECTION .bss
ALIGN 32
actual: resb 32
SECTION .note.GNU-stack noalloc noexec nowrite progbits
