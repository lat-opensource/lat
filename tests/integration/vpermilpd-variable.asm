; SPDX-FileCopyrightText: 2026 LAT Project Authors
; SPDX-License-Identifier: GPL-2.0-only
bits 64
default rel
global _start

%macro CHECK 2
    vmovdqu [actual], %1
    %assign q 0
    %rep 4
        mov rax, [actual + q * 8]
        cmp rax, [%2 + q * 8]
        jne fail
        %assign q q + 1
    %endrep
%endmacro

; Width, alias class, pending src1 upper clear.
; Aliases: distinct, dest=src1, dest=control, equal sources, all equal,
; memory control with distinct dest, memory control with dest=src1.
%macro CASE 3
    %assign q 0
    %rep 4
        %if (%1 = 16 && q >= 2) || (%3 && q >= 2)
            mov qword [expected + q * 8], 0
        %else
            %if %2 = 3 || %2 = 4
                mov rax, [input_a + q * 8]
            %else
                mov rax, [input_b + q * 8]
            %endif
            shr rax, 1
            and eax, 1
            add eax, q & 2
            lea rdx, [input_a]
            mov rax, [rdx + rax * 8]
            mov [expected + q * 8], rax
        %endif
        %assign q q + 1
    %endrep

    vmovdqu ymm14, [input_a]
    vmovdqu ymm15, [input_b]
    %if %2 = 1 || %2 = 4 || %2 = 6
        vmovdqu ymm13, [input_a]
    %elif %2 = 2
        vmovdqu ymm13, [input_b]
    %else
        vmovdqu ymm13, [junk]
    %endif
    %if %3
        %if %2 = 1 || %2 = 4 || %2 = 6
            vmovdqa xmm13, xmm13
        %else
            vmovdqa xmm14, xmm14
        %endif
    %endif

    %if %2 >= 5
        %assign q 0
        %rep 4
            %if %2 = 6
                %if %3 && q >= 2
                    xor eax, eax
                %else
                    mov rax, [input_a + q * 8]
                %endif
            %else
                mov rax, [junk + q * 8]
            %endif
            mov [before13 + q * 8], rax
            %if %3 && %2 = 5 && q >= 2
                xor eax, eax
            %else
                mov rax, [input_a + q * 8]
            %endif
            mov [before14 + q * 8], rax
            %assign q q + 1
        %endrep
        lea rax, [%%target]
        mov [fault_pc], rax
    %endif
    mov eax, 0x7fffffff
    add eax, 1
    pushfq
    pop rax
    mov [expected_flags], rax
    %if %2 >= 5
        ; Isolate the faulting permute from integer address preparation.
        jmp %%target
    %endif
%%target:
    %if %1 = 16
        %if %2 = 0
            vpermilpd xmm13, xmm14, xmm15
        %elif %2 = 1
            vpermilpd xmm13, xmm13, xmm15
        %elif %2 = 2
            vpermilpd xmm13, xmm14, xmm13
        %elif %2 = 3
            vpermilpd xmm13, xmm14, xmm14
        %elif %2 = 4
            vpermilpd xmm13, xmm13, xmm13
        %elif %2 = 5
            vpermilpd xmm13, xmm14, [r10]
        %else
            vpermilpd xmm13, xmm13, [r10]
        %endif
    %else
        %if %2 = 0
            vpermilpd ymm13, ymm14, ymm15
        %elif %2 = 1
            vpermilpd ymm13, ymm13, ymm15
        %elif %2 = 2
            vpermilpd ymm13, ymm14, ymm13
        %elif %2 = 3
            vpermilpd ymm13, ymm14, ymm14
        %elif %2 = 4
            vpermilpd ymm13, ymm13, ymm13
        %elif %2 = 5
            vpermilpd ymm13, ymm14, [r10]
        %else
            vpermilpd ymm13, ymm13, [r10]
        %endif
    %endif
    pushfq
    pop rax
    cmp rax, [expected_flags]
    jne fail
    CHECK ymm13, expected
    %if %3 && %2 != 1 && %2 != 4 && %2 != 6
        CHECK ymm14, input_a_zero
    %else
        CHECK ymm14, input_a
    %endif
    CHECK ymm15, input_b
%endmacro

; Read at an exact page tail, unaligned tail, or one byte across the guard.
%macro MEMORY 3
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    mov edx, 3
    syscall
    test eax, eax
    jne fail
    lea r10, [r13 - %1 + %2]
    %assign q 0
    %rep %1 / 8
        mov rax, [input_b + q * 8]
        mov [r10 + q * 8], rax
        %assign q q + 1
    %endrep
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    xor edx, edx
    syscall
    test eax, eax
    jne fail
    CASE %1, %3, 1
%endmacro

section .text
_start:
    mov r12, 0x91e10da5c79e7b1d
    mov r15d, 4
.rounding:
    mov eax, r15d
    dec eax
    shl eax, 13
    or eax, 0x1fbf
    mov [mxcsr], eax
    ldmxcsr [mxcsr]
    xor r14d, r14d
.controls:
    %assign q 0
    %rep 4
        mov rax, r12
        shl rax, 13
        xor r12, rax
        mov rax, r12
        shr rax, 7
        xor r12, rax
        mov rax, r12
        shl rax, 17
        xor r12, rax
        mov eax, r14d
        add eax, q
        and eax, 7
        lea rdx, [edges]
        mov rdx, [rdx + rax * 8]
        mov rax, r12
        cmp r14d, 8
        cmovb rax, rdx
        mov [input_a + q * 8], rax
        mov rdx, r12
        ror rdx, 23
        and rdx, -8
        mov eax, r14d
        shr eax, q * 3
        and eax, 7
        or rdx, rax
        mov [input_b + q * 8], rdx
        %assign q q + 1
    %endrep
    mov rax, [input_a]
    mov [input_a_zero], rax
    mov rax, [input_a + 8]
    mov [input_a_zero + 8], rax
    %assign alias 0
    %rep 5
        CASE 16, alias, 0
        CASE 32, alias, 0
        CASE 16, alias, 1
        CASE 32, alias, 1
        %assign alias alias + 1
    %endrep
    inc r14d
    cmp r14d, 4096
    jb .controls
    stmxcsr [actual_mxcsr]
    mov eax, [actual_mxcsr]
    cmp eax, [mxcsr]
    jne fail
    dec r15d
    jnz .rounding

    mov eax, 9
    xor edi, edi
    mov esi, 131072
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
    syscall
    cmp rax, -4095
    jae fail
    mov r12, rax
    lea r13, [rax + 65536]
    mov eax, 13
    mov edi, 11
    lea rsi, [action]
    xor edx, edx
    mov r10d, 8
    syscall
    test eax, eax
    jne fail
    MEMORY 16, 0, 5
    MEMORY 16, 0, 6
    MEMORY 32, 0, 5
    MEMORY 32, 0, 6
    MEMORY 16, -1, 5
    MEMORY 32, -1, 6
    MEMORY 16, 1, 5
    MEMORY 16, 1, 6
    MEMORY 32, 1, 5
    MEMORY 32, 1, 6
    cmp dword [faults], 4
    jne fail
    stmxcsr [actual_mxcsr]
    mov eax, [actual_mxcsr]
    cmp eax, [mxcsr]
    jne fail
    mov eax, 11
    mov rdi, r12
    mov esi, 131072
    syscall
    xor edi, edi
    jmp exit

handler:
    cmp edi, 11
    jne fail
    cmp [rsi + 16], r13
    jne fail
    mov rax, [rdx + 168]
    cmp rax, [fault_pc]
    jne fail
    mov rax, [rdx + 176]
    xor rax, [expected_flags]
    and eax, 0x8d5
    jne fail
    mov r8, [rdx + 224]
    mov eax, [r8 + 24]
    cmp eax, [mxcsr]
    jne fail
    %assign reg 13
    %rep 3
        %if reg = 13
            lea r9, [before13]
        %elif reg = 14
            lea r9, [before14]
        %else
            lea r9, [input_b]
        %endif
        %assign q 0
        %rep 4
            mov rax, [r9 + q * 8]
            %if q < 2
                cmp rax, [r8 + 160 + reg * 16 + q * 8]
            %else
                cmp rax, [r8 + 576 + reg * 16 + (q - 2) * 8]
            %endif
            jne fail
            %assign q q + 1
        %endrep
        %assign reg reg + 1
    %endrep
    inc dword [faults]
    mov eax, 10
    mov rdi, r13
    mov esi, 65536
    mov edx, 3
    syscall
    test eax, eax
    jne fail
    ret
restorer:
    mov eax, 15
    syscall
fail:
    mov edi, 1
exit:
    mov eax, 60
    syscall

section .rodata align=32
edges: dq 0, 0xffffffffffffffff, 0x8000000000000000, 0x7ff0000000000000
       dq 0xfff0000000000000, 0x7ff8000000001234, 0x7ff0000000000001, 1
junk: times 4 dq 0xdeadbeefdeadbeef
action: dq handler, 0x04000004, restorer, 0

section .bss align=32
input_a: resq 4
input_b: resq 4
input_a_zero: resq 4
expected: resq 4
actual: resq 4
before13: resq 4
before14: resq 4
expected_flags: resq 1
fault_pc: resq 1
mxcsr: resd 1
actual_mxcsr: resd 1
faults: resd 1

section .note.GNU-stack noalloc noexec nowrite progbits
