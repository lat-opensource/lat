; SPDX-License-Identifier: GPL-2.0-only
bits 32
global _start

%macro CASE 6
    mov eax, 0x12345678
    mov ebx, eax
    mov ecx, eax
    mov edx, eax
%if %2 = 0
    %1 xmm0, xmm1
%elif %2 = 1
    %1 xmm0, xmm0
%else
    %1 xmm0, [esi + 8]
%endif
    seta %3
    mov ebp, %4
%if %6
    pushfd
    pop edx
%else
    ; Keep this full flag write so the compare flags are not needed.
    cmp ebp, ebp
    pushfd
    pop edx
%endif
    mov edi, [esi + 16]
%if %2 = 1
    mov edi, [esi + 20]
%endif
%if %6
    and edx, 0x8d5
    cmp edx, edi
    jne fail_flags
%endif
    mov eax, edi
    and eax, 0x41
    sete al
    movzx eax, al
%if %5 = 8
    shl eax, 8
    or eax, 0x12340078
%else
    or eax, 0x12345600
%endif
    cmp ebp, eax
    jne fail_result
%endmacro

%macro DESTINATIONS 3
    CASE %1, %2, al, eax, 0, %3
    CASE %1, %2, ah, eax, 8, %3
    CASE %1, %2, bl, ebx, 0, %3
    CASE %1, %2, bh, ebx, 8, %3
    CASE %1, %2, cl, ecx, 0, %3
    CASE %1, %2, ch, ecx, 8, %3
    CASE %1, %2, dl, edx, 0, %3
    CASE %1, %2, dh, edx, 8, %3
%endmacro

section .text
_start:
    ldmxcsr [clean]
    mov esi, pairs
next_pair:
    movsd xmm0, [esi]
    movsd xmm1, [esi + 8]
    call suite
    add esi, 24
    cmp esi, pairs_end
    jne next_pair
    xor ebx, ebx
    jmp exit
suite:
%assign live 0
%rep 2
%assign form 0
%rep 3
    DESTINATIONS ucomisd, form, live
    DESTINATIONS vucomisd, form, live
%assign form form + 1
%endrep
%assign live live + 1
%endrep
    ret
fail_result: mov ebx, 1
    jmp exit
fail_flags: mov ebx, 2
exit:
    mov eax, 1
    int 0x80

%macro PAIR 4
    dq %1, %2
    dd %3, %4
%endmacro
section .rodata align=8
clean: dd 0x1f80
align 8
pairs:
    PAIR 0x4000000000000000, 0x3ff0000000000000, 0, 0x40
    PAIR 0x3ff0000000000000, 0x4000000000000000, 1, 0x40
    PAIR 0x3ff0000000000000, 0x3ff0000000000000, 0x40, 0x40
    PAIR 0, 0x8000000000000000, 0x40, 0x40
    PAIR 0x7ff0000000000000, 0x4000000000000000, 0, 0x40
    PAIR 0xfff0000000000000, 0xbff0000000000000, 1, 0x40
    PAIR 0x7ff8000000000001, 0x3ff0000000000000, 0x45, 0x45
    PAIR 0x3ff0000000000000, 0xfff800000000cafe, 0x45, 0x40
    PAIR 0x7ff0000000000001, 0x3ff0000000000000, 0x45, 0x45
    PAIR 0x3ff0000000000000, 0xfff0000000000001, 0x45, 0x40
    PAIR 1, 0, 0, 0x40
    PAIR 0x8000000000000001, 0, 1, 0x40
pairs_end:
section .note.GNU-stack noalloc noexec nowrite progbits
