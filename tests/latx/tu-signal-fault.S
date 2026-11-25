        .text
        .p2align 12
        .globl cross_entry
cross_entry:
#ifdef __x86_64__
        movl %edi, %ecx
#else
        movl 4(%esp), %ecx
#endif
        xorl %eax, %eax
        jmp cross_member
        .p2align 12
cross_member:
        incl %eax
        decl %ecx
        jnz cross_branch
        jmp probe_read
        .p2align 12
cross_branch:
        subl $1, %eax
        jmp cross_member
        .p2align 12
probe_read:
        movl $1, %edx
        .globl probe_fault, probe_after
probe_fault:
        movl (%edx), %edx
probe_after:
        movl $3, %eax
        ret
        .p2align 12
        .section .note.GNU-stack,"",@progbits
