; SPDX-License-Identifier: GPL-2.0-only
; Build with KIND=0 (mask move), 1 (byte mask), 2 (gather), 3 (convert),
; 4 (scalar store), 5 (XSAVE family), 6 (vector access), or 7 (MMX byte mask).
; No libc or guest runtime is needed.
%ifndef KIND
%define KIND 0
%endif
%ifndef OP
%define OP vmaskmovps
%endif
%ifndef WIDE
%define WIDE 0
%endif
%ifndef ELEMENT
%define ELEMENT 4
%endif
%ifndef INDEX64
%define INDEX64 0
%endif
%ifndef SCALE
%define SCALE 1
%endif

%if __BITS__ == 64
default rel
%define A rax
%define B rbx
%define C rcx
%define D rdx
%define S rsi
%define T rdi
%define BP rbp
%define SP rsp
%define PTR qword
%define P 8
%define NR_exit 60
%define NR_mprotect 10
%define NR_sigaction 13
%macro sys3 4
    mov eax, %1
    mov rdi, %2
    mov rsi, %3
    mov rdx, %4
    syscall
%endmacro
%else
%define A eax
%define B ebx
%define C ecx
%define D edx
%define S esi
%define T edi
%define BP ebp
%define SP esp
%define PTR dword
%define P 4
%define NR_exit 1
%define NR_mprotect 125
%define NR_sigaction 174
%macro sys3 4
    mov eax, %1
    mov ebx, %2
    mov ecx, %3
    mov edx, %4
    int 0x80
%endmacro
%endif

%define VECTOR_BYTES (16 + 16 * WIDE)
%if KIND == 7
%undef VECTOR_BYTES
%define VECTOR_BYTES 8
%endif
%define LANES (VECTOR_BYTES / ELEMENT)
%if WIDE
%define V0 ymm0
%define V1 ymm1
%define V2 ymm2
%else
%define V0 xmm0
%define V1 xmm1
%define V2 xmm2
%endif

%macro byte_mask_store 0
%if KIND == 7
    movdq2q mm0, xmm0
    movdq2q mm1, xmm1
    mov PTR [expected_pc], %%store
%%store:
    OP mm0, mm1
%else
    OP xmm0, xmm1
%endif
%endmacro

section .text
global _start
_start:
    mov byte [scenario], 'v'
    mov PTR [page_offset], 16384
    cmp PTR [SP], 1
    je .args_done
    cmp PTR [SP], 3
    jne fail
    mov A, [SP + 2 * P]
    mov al, [A]
    mov [scenario], al
    mov A, [SP + 3 * P]
    cmp byte [A], '4'
    jne .args_done
    mov PTR [page_offset], 4096
.args_done:
%if __BITS__ == 64
    mov r10d, 8
%else
    mov esi, 8
%endif
    sys3 NR_sigaction, 11, action, 0
    test A, A
    jnz fail
%if __BITS__ == 64
    mov eax, 9
    xor edi, edi
    mov esi, 65536
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
    syscall
%else
    mov eax, 192
    xor ebx, ebx
    mov ecx, 65536
    mov edx, 3
    mov esi, 0x22
    mov edi, -1
    xor ebp, ebp
    int 0x80
%endif
    cmp A, -4095
    jae fail
    add A, 16383
    and A, -16384
    mov [mapping], A
    mov T, A
    mov ecx, 32768
    mov al, 0x55
    rep stosb
    mov A, [mapping]
    add A, [page_offset]
    mov [boundary], A
    mov [expected_address], A
    call prepare_test
    cmp byte [scenario], 'v'
    je run_test
    mov edx, 0
    cmp byte [scenario], 'w'
    je .readonly
    cmp byte [scenario], 'W'
    jne .protect
.readonly:
    mov edx, 1
.protect:
    sys3 NR_mprotect, PTR [boundary], PTR [page_offset], D
    test A, A
    jnz fail
    jmp run_test

segv_handler:
    cmp byte [expected_fault], 1
    jne fail
%if __BITS__ == 64
    cmp edi, 11
    jne fail
    cmp dword [rsi + 8], 2
    jne fail
    mov A, [rsi + 16]
    cmp A, [expected_address]
    jne fail
    mov A, [rdx + 168]
%else
    cmp dword [esp + 4], 11
    jne fail
    mov ecx, [esp + 8]
    cmp dword [ecx + 8], 2
    jne fail
    mov A, [ecx + 12]
    cmp A, [expected_address]
    jne fail
    mov ecx, [esp + 12]
    mov A, [ecx + 76]
%endif
    cmp A, [expected_pc]
    jne fail
    cmp byte [scenario], 'c'
    jne .ordinary_fault
    sys3 NR_exit, 42, 0, 0
.ordinary_fault:
%if KIND == 2
    cmp byte [scenario], 'p'
    je .resume
    cmp byte [scenario], 'h'
    jne pass
.resume:
    sys3 NR_mprotect, PTR [boundary], PTR [page_offset], 3
    test A, A
    jnz fail
    mov A, [boundary]
    mov dword [A - ELEMENT], 0x12121212
    mov dword [A], 0x34343434
    ret
%endif
    jmp pass

restorer:
%if __BITS__ == 64
    mov eax, 15
    syscall
%else
    mov eax, 173
    int 0x80
%endif
pass:
    sys3 NR_exit, 0, 0, 0
fail:
    sys3 NR_exit, 1, 0, 0

prepare_test:
%if KIND == 3
    mov T, [boundary]
    sub T, 16
    mov S, conversion_input
    mov ecx, 16
    rep movsb
%elif KIND == 5
    mov T, [boundary]
    sub T, 576
    mov [state_address], T
    xor eax, eax
    mov ecx, 1024 / 4
    rep stosd
    mov T, [state_address]
    vmovdqu ymm0, [values]
    mov eax, 7
    xor edx, edx
    xsave [T]
    cmp byte [scenario], 'i'
    jne .state_ready
    mov dword [T + 512], 0
.state_ready:
%endif
.prepare_done:
    ret

run_test:
%if KIND == 0 || KIND == 1 || KIND == 7
    cmp byte [scenario], 'c'
    jne .mask_cases
    mov BP, [boundary]
    vpxor V1, V1, V1
    vmovdqu V0, [values]
    mov byte [expected_fault], 1
    mov PTR [expected_pc], .mask_probe
%if KIND == 0
.mask_probe:
    OP V0, V1, [BP]
%else
    mov T, BP
%if KIND == 7
    movdq2q mm0, xmm0
    movdq2q mm1, xmm1
.mask_probe:
    OP mm0, mm1
%else
.mask_probe:
    OP xmm0, xmm1
%endif
%endif
    jmp pass
.mask_cases:
    cmp byte [scenario], 'v'
    je mask_values
    mov BP, [boundary]
    vmovdqu V1, [partial_mask]
    vmovdqu V0, [values]
    cmp byte [scenario], 'z'
    jne .not_zero
    vpxor V1, V1, V1
    jmp .suppressed
.not_zero:
    cmp byte [scenario], 't'
    jne .fault
    sub BP, ELEMENT
.suppressed:
%if KIND == 0
    OP V0, V1, [BP]
    vmovdqu [result], ymm0
    cmp byte [scenario], 'z'
    jne .tail_value
    mov S, result
    mov ecx, 32 / 4
.zero_check:
    cmp dword [S], 0
    jne fail
    add S, 4
    loop .zero_check
    jmp .suppressed_store
.tail_value:
    cmp dword [result], 0x55555555
    jne fail
    mov S, result + ELEMENT
    mov ecx, (32 - ELEMENT) / 4
.tail_check:
    cmp dword [S], 0
    jne fail
    add S, 4
    loop .tail_check
.suppressed_store:
    vmovdqu V0, [values]
    OP [BP], V1, V0
%else
    mov T, BP
    byte_mask_store
%endif
    jmp pass
.fault:
    mov byte [expected_fault], 1
    cmp byte [scenario], 'R'
    je .full
    cmp byte [scenario], 'W'
    jne .choose
.full:
    vmovdqu V1, [full_mask]
    sub BP, ELEMENT
.choose:
%if KIND == 0
    cmp byte [scenario], 'r'
    je .read
    cmp byte [scenario], 'R'
    je .read
    mov PTR [expected_pc], .store
.store:
    OP [BP], V1, V0
    jmp fail
.read:
    mov PTR [expected_pc], .load
.load:
    OP V0, V1, [BP]
%else
    mov T, BP
    mov PTR [expected_pc], .byte_store
.byte_store:
    byte_mask_store
%endif
    jmp fail

mask_values:
    mov dword [pattern], 0
.pattern:
    mov T, mask_buffer
    mov edx, [pattern]
    mov ecx, LANES
.build:
%if ELEMENT == 1
    mov al, 0x7f
    test edx, 1
    jz .mask_element
    mov al, 0x80
.mask_element:
    stosb
%else
%if ELEMENT == 8
    mov eax, 0x80000000
    stosd
%endif
    mov eax, 0x7fffffff
    test edx, 1
    jz .mask_element
    mov eax, 0x80000000
.mask_element:
    stosd
%endif
    shr edx, 1
    loop .build
    vmovdqu V1, [mask_buffer]
%if KIND == 0
    OP V0, V1, [values]
    vmovdqu [result], ymm0
    call check_mask_load
    ; Destination/mask aliasing must not destroy mask bits before the load.
    OP V1, V1, [values]
    vmovdqu [result], ymm1
    call check_mask_load
    vmovdqu V1, [mask_buffer]
%endif
    mov T, [mapping]
    mov ecx, 32
    mov al, 0x55
    rep stosb
    mov BP, [mapping]
    vmovdqu V0, [values]
%if KIND == 0
    OP [BP], V1, V0
%else
    mov T, BP
    byte_mask_store
%endif
    mov S, BP
    xor ecx, ecx
.check_store:
    cmp ecx, VECTOR_BYTES
    mov al, 0x55
    jae .compare_store
    mov edx, [pattern]
    mov eax, ecx
%if ELEMENT == 8
    shr eax, 3
%elif ELEMENT == 4
    shr eax, 2
%endif
    bt edx, eax
    mov al, 0x55
    jnc .compare_store
    mov al, [values + C]
.compare_store:
    cmp [S + C], al
    jne fail
    inc ecx
    cmp ecx, 32
    jb .check_store
    inc dword [pattern]
    cmp dword [pattern], (1 << LANES)
    jb .pattern
    jmp pass

%if KIND == 0
check_mask_load:
    xor ecx, ecx
.byte:
    xor eax, eax
    cmp ecx, VECTOR_BYTES
    jae .compare
    mov edx, [pattern]
    mov eax, ecx
%if ELEMENT == 8
    shr eax, 3
%else
    shr eax, 2
%endif
    bt edx, eax
    mov eax, 0
    jnc .compare
    mov al, [values + C]
.compare:
    cmp [result + C], al
    jne fail
    inc ecx
    cmp ecx, 32
    jb .byte
    ret
%endif

%elif KIND == 2
%if INDEX64 && ELEMENT == 4
%define G0 xmm0
%define G1 xmm1
%else
%define G0 V0
%define G1 V1
%endif
%if WIDE && (INDEX64 || ELEMENT == 4)
%define GI ymm2
%else
%define GI xmm2
%endif
%assign G_LANES (16 + 16 * WIDE) / ELEMENT
%if INDEX64 && ELEMENT == 4
%assign G_LANES (16 + 16 * WIDE) / 8
%endif
    mov BP, [mapping]
    cmp byte [scenario], 'v'
    jne .gather_base_ready
    mov T, BP
    mov S, values
    mov ecx, 32
    rep movsb
    mov dword [pattern], 0
.gather_pattern:
    mov T, mask_buffer
    mov edx, [pattern]
    mov ecx, G_LANES
.gather_mask:
%if ELEMENT == 8
    mov eax, 0x80000000
    stosd
%endif
    mov eax, 0x7fffffff
    test edx, 1
    jz .gather_mask_ready
    mov eax, 0x80000000
.gather_mask_ready:
    stosd
    shr edx, 1
    loop .gather_mask
.gather_base_ready:
    vmovdqu ymm0, [initial_values]
    vmovdqu G1, [partial_mask]
    cmp byte [scenario], 'v'
    jne .gather_mask_loaded
    vmovdqu G1, [mask_buffer]
.gather_mask_loaded:
    mov T, index_buffer
    mov ecx, G_LANES
    xor edx, edx
.index:
    mov eax, edx
    cmp byte [scenario], 'v'
    je .index_ready
    mov A, [boundary]
    sub A, BP
    cmp byte [scenario], 't'
    je .valid_first
    cmp byte [scenario], 'p'
    je .valid_first
    cmp byte [scenario], 'h'
    jne .index_ready
    cmp edx, (G_LANES - 2) * ELEMENT
    jne .index_ready
    sub A, ELEMENT
    jmp .index_ready
.valid_first:
    test edx, edx
    jnz .index_ready
    sub A, ELEMENT
.index_ready:
%if SCALE == 2
    sar A, 1
%elif SCALE == 4
    sar A, 2
%elif SCALE == 8
    sar A, 3
%endif
%if INDEX64
%if __BITS__ == 64
    stosq
%else
    push edx
    cdq
    stosd
    mov eax, edx
    stosd
    pop edx
%endif
%else
    stosd
%endif
    add edx, ELEMENT
    loop .index
    vmovdqu GI, [index_buffer]
    cmp byte [scenario], 'z'
    jne .gather_not_zero
    vpxor G1, G1, G1
    jmp .gather
.gather_not_zero:
    cmp byte [scenario], 'r'
    je .gather_fault
    cmp byte [scenario], 'p'
    je .gather_partial
    cmp byte [scenario], 'h'
    je .gather_high_partial
    cmp byte [scenario], 'R'
    jne .gather
    vmovdqu G1, [full_mask]
    jmp .gather_fault
.gather_partial:
    vmovdqu G1, [two_selected]
    jmp .gather_fault
.gather_high_partial:
    vmovdqu G1, [high_selected]
.gather_fault:
    mov byte [expected_fault], 1
    mov PTR [expected_pc], .gather
.gather:
    OP G0, [BP + GI * SCALE], G1
    cmp byte [expected_fault], 1
    jne .gather_result
    cmp byte [scenario], 'p'
    je .gather_result
    cmp byte [scenario], 'h'
    jne fail
.gather_result:
    vmovdqu [result], ymm0
    vmovdqu [mask_buffer], ymm1
    mov ecx, 32 / 4
    mov S, mask_buffer
.mask_cleared:
    cmp dword [S], 0
    jne fail
    add S, 4
    loop .mask_cleared
    cmp byte [scenario], 'v'
    jne .gather_nonvalue
    xor ecx, ecx
.gather_check_byte:
    mov eax, ecx
%if ELEMENT == 8
    shr eax, 3
%else
    shr eax, 2
%endif
    mov edx, [pattern]
    bt edx, eax
    mov al, 0x55
    jnc .gather_compare_byte
    mov al, [values + C]
.gather_compare_byte:
    cmp [result + C], al
    jne fail
    inc ecx
    cmp ecx, G_LANES * ELEMENT
    jb .gather_check_byte
%if G_LANES * ELEMENT < 32
.gather_unused:
    cmp byte [result + C], 0
    jne fail
    inc ecx
    cmp ecx, 32
    jb .gather_unused
%endif
    inc dword [pattern]
    cmp dword [pattern], 1 << G_LANES
    jb .gather_pattern
    jmp pass
.gather_nonvalue:
    cmp byte [scenario], 'h'
    jne .gather_first
    cmp dword [result + (G_LANES - 2) * ELEMENT], 0x55555555
    jne fail
    cmp dword [result + (G_LANES - 1) * ELEMENT], 0x34343434
    jne fail
    jmp .gather_upper
.gather_first:
    mov eax, [values]
    cmp byte [scenario], 'v'
    je .gather_value
    mov eax, 0x55555555
.gather_value:
    cmp [result], eax
    jne fail
    cmp byte [scenario], 'p'
    jne .gather_upper
    cmp dword [result + ELEMENT], 0x34343434
    jne fail
.gather_upper:
%if !WIDE || (INDEX64 && ELEMENT == 4)
    mov S, result + 16
    mov ecx, 4
.upper_zero:
    cmp dword [S], 0
    jne fail
    add S, 4
    loop .upper_zero
%endif
    jmp pass

%elif KIND == 3
    mov BP, [boundary]
    cmp byte [scenario], 'v'
    je .convert_wide
    sub BP, 8
    OP xmm0, [BP]
    vmovdqu [result], ymm0
    cmp dword [result], 0
    jne fail
    cmp dword [result + 4], 0x40080000
    jne fail
    cmp dword [result + 8], 0
    jne fail
    cmp dword [result + 12], 0x40100000
    jne fail
    mov S, result + 16
    mov ecx, 4
.convert_upper:
    cmp dword [S], 0
    jne fail
    add S, 4
    loop .convert_upper
    jmp pass
.convert_wide:
    sub BP, 16
    OP ymm0, [BP]
    vmovdqu [result], ymm0
    mov S, result
    mov T, conversion_expected
    mov ecx, 32
    repe cmpsb
    jne fail
    jmp pass

%elif KIND == 4
    mov BP, [boundary]
    vmovdqu ymm0, [values]
    cmp byte [scenario], 'v'
    je .scalar_store
    mov byte [expected_fault], 1
    mov PTR [expected_pc], .scalar_store
.scalar_store:
    vmovss [BP], xmm0
    cmp byte [expected_fault], 1
    je fail
    mov eax, [values]
    cmp [BP], eax
    jne fail
    cmp dword [BP + 4], 0x55555555
    jne fail
    jmp pass

%elif KIND == 6
    mov BP, [boundary]
    sub BP, VECTOR_BYTES
    cmp byte [scenario], 'v'
    je .vector_read
    cmp byte [scenario], 't'
    je .vector_read
    add BP, VECTOR_BYTES / 2
    mov byte [expected_fault], 1
    cmp byte [scenario], 'w'
    je .vector_store
.vector_read:
    mov PTR [expected_pc], .vector_load
.vector_load:
    vmovdqu V0, [BP]
    cmp byte [expected_fault], 1
    je fail
    vmovdqu [result], ymm0
    mov S, result
    mov ecx, VECTOR_BYTES / 4
.vector_value:
    cmp dword [S], 0x55555555
    jne fail
    add S, 4
    loop .vector_value
.vector_store:
    vmovdqu V0, [values]
    mov PTR [expected_pc], .vector_write
.vector_write:
    vmovdqu [BP], V0
    cmp byte [expected_fault], 1
    je fail
    mov S, BP
    mov T, values
    mov ecx, VECTOR_BYTES
    repe cmpsb
    jne fail
    jmp pass

%elif KIND == 5
    mov BP, [state_address]
    mov eax, 4
    xor edx, edx
    cmp byte [scenario], 'u'
    jne .state_mask
    mov eax, 1
.state_mask:
    cmp byte [scenario], 'v'
    je .state
    cmp byte [scenario], 'u'
    je .state
    cmp byte [scenario], 'i'
    je .state
    mov byte [expected_fault], 1
    mov PTR [expected_pc], .state
.state:
    OP [BP]
    cmp byte [expected_fault], 1
    je fail
    jmp pass
%endif

section .data
align 8
action:
%if __BITS__ == 64
    dq segv_handler, 0x04000004, restorer, 0
%else
    dd segv_handler, 0x04000004, restorer
    dq 0
%endif
align 32
values: dd 0x11111111, 0x22222222, 0x33333333, 0x44444444
        dd 0x66666666, 0x77777777, 0x12345678, 0x76543210
initial_values: times 32 db 0x55
partial_mask:
%if ELEMENT == 8
    dd 0, 0x80000000
    times 24 db 0
%elif ELEMENT == 1
    db 0x80
    times 31 db 0
%else
    dd 0x80000000
    times 28 db 0
%endif
full_mask: times 32 db 0xff
two_selected:
    times 2 * ELEMENT db 0xff
    times 32 - 2 * ELEMENT db 0
%if KIND == 2
high_selected:
    times (G_LANES - 2) * ELEMENT db 0
    times 2 * ELEMENT db 0xff
    times 32 - G_LANES * ELEMENT db 0
%endif
%ifdef INTEGER_INPUT
conversion_input: dd 1, 2, 3, 4
%else
conversion_input: dd 0x3f800000, 0x40000000, 0x40400000, 0x40800000
%endif
conversion_expected: dq 0x3ff0000000000000, 0x4000000000000000
                     dq 0x4008000000000000, 0x4010000000000000

section .bss
align 32
mask_buffer: resb 32
index_buffer: resb 32
result: resb 32
mapping: resq 1
boundary: resq 1
page_offset: resq 1
expected_address: resq 1
expected_pc: resq 1
state_address: resq 1
pattern: resd 1
scenario: resb 1
expected_fault: resb 1
