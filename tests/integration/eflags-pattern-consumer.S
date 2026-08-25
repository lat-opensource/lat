.equ __NR_exit, 60

.section .text

/* CMP+SETcc is the only consumer, so CMP flags can be eliminated. */
.global cmp_setcc_elide
.type cmp_setcc_elide, @function
cmp_setcc_elide:
    mov $1, %eax
    cmp $1, %eax
    sete %dil
    xor $1, %dil
    mov $__NR_exit, %eax
    syscall
.size cmp_setcc_elide, .-cmp_setcc_elide

/* TEST+CMOVcc is the only consumer, so TEST flags can be eliminated. */
.global test_cmov_elide
.type test_cmov_elide, @function
test_cmov_elide:
    mov $0, %rax
    mov $1, %rdi
    mov $0, %rsi
    test %rax, %rax
    cmove %rdi, %rsi
    lea -1(%rsi), %rdi
    mov $__NR_exit, %eax
    syscall
.size test_cmov_elide, .-test_cmov_elide

/* ADC still consumes CF after the native SETcc condition. */
.global cmp_setcc_keep_cf
.type cmp_setcc_keep_cf, @function
cmp_setcc_keep_cf:
    clc
    mov $0, %eax
    cmp $1, %eax
    setb %cl
    mov $0, %edx
    adc $0, %edx
    movzx %cl, %edi
    xor %edx, %edi
    mov $__NR_exit, %eax
    syscall
.size cmp_setcc_keep_cf, .-cmp_setcc_keep_cf

/* LAHF still consumes TEST flags after the native CMOVcc condition. */
.global test_cmov_keep_flags
.type test_cmov_keep_flags, @function
test_cmov_keep_flags:
    mov $1, %eax
    cmp $0, %eax
    mov $0, %rbx
    mov $1, %rdi
    mov $0, %rsi
    test %rbx, %rbx
    cmove %rdi, %rsi
    lahf
    movzx %ah, %edi
    and $0x41, %edi
    xor $0x40, %edi
    lea -1(%rsi), %edx
    or %edx, %edi
    mov $__NR_exit, %eax
    syscall
.size test_cmov_keep_flags, .-test_cmov_keep_flags

/* CMP+SBB consumes native carry; neither CMP nor SBB flags escape. */
.global cmp_sbb_elide
.type cmp_sbb_elide, @function
cmp_sbb_elide:
    mov $0, %edi
    mov $0, %eax
    cmp $1, %eax
    sbb %edi, %edi
    lea 1(%rdi), %rdi
    mov $__NR_exit, %eax
    syscall
.size cmp_sbb_elide, .-cmp_sbb_elide

/* SETE still consumes ZF produced by the transformed SBB. */
.global cmp_sbb_keep_output_zf
.type cmp_sbb_keep_output_zf, @function
cmp_sbb_keep_output_zf:
    mov $0, %edi
    mov $2, %eax
    cmp $1, %eax
    sbb %edi, %edi
    sete %cl
    movzx %cl, %edi
    xor $1, %edi
    mov $__NR_exit, %eax
    syscall
.size cmp_sbb_keep_output_zf, .-cmp_sbb_keep_output_zf
