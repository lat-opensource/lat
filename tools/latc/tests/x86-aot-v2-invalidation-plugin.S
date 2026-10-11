.section .text.latc_invalidation,"ax",@progbits
.global latc_invalidation_entry
.type latc_invalidation_entry,@function
latc_invalidation_entry:
    mov $60, %eax
    xor %edi, %edi
    syscall
1:
    jmp 1b
.size latc_invalidation_entry, .-latc_invalidation_entry

.p2align 12
.global latc_invalidation_value
.type latc_invalidation_value,@function
latc_invalidation_value:
    mov $17, %eax
    ret
.size latc_invalidation_value, .-latc_invalidation_value

/* Put the immediate of one instruction on both sides of a guest page. */
.p2align 12
.skip 4092
.global latc_invalidation_cross_page
.type latc_invalidation_cross_page,@function
latc_invalidation_cross_page:
    mov $17, %eax
    ret
.size latc_invalidation_cross_page, .-latc_invalidation_cross_page
.p2align 12

.section .note.GNU-stack,"",@progbits
