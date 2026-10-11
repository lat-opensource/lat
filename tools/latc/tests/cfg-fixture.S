.text
.global _start
.type _start,@function
_start:
    call callee
    test %rax, %rax
    jz 1f
    call *%rax
1:
    mov $60, %eax
    xor %edi, %edi
    syscall
.size _start, .-_start

.type callee,@function
callee:
    xor %eax, %eax
    ret
.size callee, .-callee

.type jump_source,@function
jump_source:
    jmp jump_target_mid
.size jump_source, .-jump_source

.type jump_container,@function
jump_container:
    nop
    nop
jump_target_mid:
    ret
.size jump_container, .-jump_container

.type jump_table_source,@function
jump_table_source:
    cmp $7, %edi
    ja jump_table_default
    lea jump_table(%rip), %rdx
    movslq (%rdx,%rdi,4), %rax
    add %rdx, %rax
    jmp *%rax
jump_table_case0:
    mov $10, %eax
    ret
jump_table_case2:
    mov $12, %eax
    ret
jump_table_case4:
    mov $14, %eax
    ret
jump_table_case6:
    mov $16, %eax
    ret
jump_table_case7:
    mov $17, %eax
    ret
jump_table_default:
    xor %eax, %eax
    ret
.size jump_table_source, .-jump_table_source

.type jump_table_cold,@function
jump_table_cold:
    nop
jump_table_cold_case:
    mov $11, %eax
    ret
jump_table_cold_case3:
    mov $13, %eax
    ret
jump_table_cold_case5:
    mov $15, %eax
    ret
.size jump_table_cold, .-jump_table_cold

.type computed_stride_source,@function
computed_stride_source:
    and $3, %edi
    shl $4, %rdi
    lea computed_stride_case0(%rip), %rax
    add %rax, %rdi
    jmp *%rdi
    .p2align 4
computed_stride_case0:
    ret
    .p2align 4
computed_stride_case1:
    ret
    .p2align 4
computed_stride_case2:
    ret
    .p2align 4
computed_stride_case3:
    ret
.size computed_stride_source, .-computed_stride_source

.section .rodata
.align 4
jump_table:
    .long jump_table_case0 - jump_table
    .long jump_table_cold_case - jump_table
    .long jump_table_case2 - jump_table
    .long jump_table_cold_case3 - jump_table
    .long jump_table_case4 - jump_table
    .long jump_table_cold_case5 - jump_table
    .long jump_table_case6 - jump_table
    .long jump_table_case7 - jump_table
.text

.type jump_plt_source,@function
jump_plt_source:
    jmp synthetic_plt_second
.size jump_plt_source, .-jump_plt_source

.type fallthrough_check,@function
fallthrough_check:
    test %edi, %edi
    jne callee
.size fallthrough_check, .-fallthrough_check
.p2align 3

.type fallthrough_next,@function
fallthrough_next:
    ret
.size fallthrough_next, .-fallthrough_next

.type zero_sized,@function
zero_sized:
    ret

.type zero_sized_next,@function
zero_sized_next:
    ret
.size zero_sized_next, .-zero_sized_next

/* Neither entry has STT_FUNC or unwind metadata.  The array discovers the
 * second entry, whose direct jump must pull the first into the CFG closure. */
init_array_direct_target:
    ret

init_array_only:
    jmp init_array_direct_target

.type init_array_boundary,@function
init_array_boundary:
    ret
.size init_array_boundary, .-init_array_boundary

/* Deliberately omit STT_FUNC symbols, as in a static glibc .plt section. */
.section .plt,"ax",@progbits
synthetic_plt_first:
    jmp *synthetic_plt_target(%rip)
    xchg %ax, %ax
synthetic_plt_second:
    jmp *synthetic_plt_target(%rip)
    xchg %ax, %ax

.data
.align 8
synthetic_plt_target:
    .quad callee

.section .init_array,"aw",@init_array
    .quad init_array_only

.section .note.GNU-stack,"",@progbits
