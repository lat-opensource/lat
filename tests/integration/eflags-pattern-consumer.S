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

/* CMP+JE is native and neither successor consumes its flags. */
.global cmp_je_elide
.type cmp_je_elide, @function
cmp_je_elide:
    mov $1, %eax
    cmp $1, %eax
    je .Lcmp_je_taken
    xor %edi, %edi
    jmp .Lcmp_je_exit
.Lcmp_je_taken:
    xor %edi, %edi
.Lcmp_je_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_je_elide, .-cmp_je_elide

/* CMP+JNE is native and neither successor consumes its flags. */
.global cmp_jne_elide
.type cmp_jne_elide, @function
cmp_jne_elide:
    mov $1, %eax
    cmp $2, %eax
    jne .Lcmp_jne_taken
    xor %edi, %edi
    jmp .Lcmp_jne_exit
.Lcmp_jne_taken:
    xor %edi, %edi
.Lcmp_jne_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_jne_elide, .-cmp_jne_elide

/* CMP+JB is native and neither successor consumes its flags. */
.global cmp_jb_elide
.type cmp_jb_elide, @function
cmp_jb_elide:
    mov $0, %eax
    cmp $1, %eax
    jb .Lcmp_jb_taken
    xor %edi, %edi
    jmp .Lcmp_jb_exit
.Lcmp_jb_taken:
    xor %edi, %edi
.Lcmp_jb_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_jb_elide, .-cmp_jb_elide

/* CMP+JAE is native and neither successor consumes its flags. */
.global cmp_jae_elide
.type cmp_jae_elide, @function
cmp_jae_elide:
    mov $1, %eax
    cmp $1, %eax
    jae .Lcmp_jae_taken
    xor %edi, %edi
    jmp .Lcmp_jae_exit
.Lcmp_jae_taken:
    xor %edi, %edi
.Lcmp_jae_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_jae_elide, .-cmp_jae_elide

/* TEST+JE is native and neither successor consumes its flags. */
.global test_je_elide
.type test_je_elide, @function
test_je_elide:
    xor %eax, %eax
    test %eax, %eax
    je .Ltest_je_taken
    xor %edi, %edi
    jmp .Ltest_je_exit
.Ltest_je_taken:
    xor %edi, %edi
.Ltest_je_exit:
    mov $__NR_exit, %eax
    syscall
.size test_je_elide, .-test_je_elide

/* TEST+JNE is native and neither successor consumes its flags. */
.global test_jne_elide
.type test_jne_elide, @function
test_jne_elide:
    mov $1, %eax
    test %eax, %eax
    jne .Ltest_jne_taken
    xor %edi, %edi
    jmp .Ltest_jne_exit
.Ltest_jne_taken:
    xor %edi, %edi
.Ltest_jne_exit:
    mov $__NR_exit, %eax
    syscall
.size test_jne_elide, .-test_jne_elide

/* The taken successor still consumes ZF produced by CMP. */
.global cmp_je_keep_zf
.type cmp_je_keep_zf, @function
cmp_je_keep_zf:
    mov $1, %eax
    cmp $1, %eax
    je .Lcmp_je_keep_taken
    mov $1, %edi
    jmp .Lcmp_je_keep_exit
.Lcmp_je_keep_taken:
    lahf
    movzx %ah, %edi
    and $0x40, %edi
    xor $0x40, %edi
.Lcmp_je_keep_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_je_keep_zf, .-cmp_je_keep_zf

/* The taken successor still consumes ZF produced by TEST. */
.global test_jne_keep_zf
.type test_jne_keep_zf, @function
test_jne_keep_zf:
    mov $1, %eax
    test %eax, %eax
    jne .Ltest_jne_keep_taken
    mov $1, %edi
    jmp .Ltest_jne_keep_exit
.Ltest_jne_keep_taken:
    lahf
    movzx %ah, %edi
    and $0x40, %edi
.Ltest_jne_keep_exit:
    mov $__NR_exit, %eax
    syscall
.size test_jne_keep_zf, .-test_jne_keep_zf

/* CMP+JBE is native and neither successor consumes its flags. */
.global cmp_jbe_elide
.type cmp_jbe_elide, @function
cmp_jbe_elide:
    mov $1, %eax
    cmp $1, %eax
    jbe .Lcmp_jbe_taken
    xor %edi, %edi
    jmp .Lcmp_jbe_exit
.Lcmp_jbe_taken:
    xor %edi, %edi
.Lcmp_jbe_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_jbe_elide, .-cmp_jbe_elide

/* CMP+JA is native and neither successor consumes its flags. */
.global cmp_ja_elide
.type cmp_ja_elide, @function
cmp_ja_elide:
    mov $2, %eax
    cmp $1, %eax
    ja .Lcmp_ja_taken
    xor %edi, %edi
    jmp .Lcmp_ja_exit
.Lcmp_ja_taken:
    xor %edi, %edi
.Lcmp_ja_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_ja_elide, .-cmp_ja_elide

/* CMP+JL is native and neither successor consumes its flags. */
.global cmp_jl_elide
.type cmp_jl_elide, @function
cmp_jl_elide:
    mov $-1, %eax
    cmp $0, %eax
    jl .Lcmp_jl_taken
    xor %edi, %edi
    jmp .Lcmp_jl_exit
.Lcmp_jl_taken:
    xor %edi, %edi
.Lcmp_jl_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_jl_elide, .-cmp_jl_elide

/* CMP+JGE is native and neither successor consumes its flags. */
.global cmp_jge_elide
.type cmp_jge_elide, @function
cmp_jge_elide:
    xor %eax, %eax
    cmp $0, %eax
    jge .Lcmp_jge_taken
    xor %edi, %edi
    jmp .Lcmp_jge_exit
.Lcmp_jge_taken:
    xor %edi, %edi
.Lcmp_jge_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_jge_elide, .-cmp_jge_elide

/* CMP+JLE is native and neither successor consumes its flags. */
.global cmp_jle_elide
.type cmp_jle_elide, @function
cmp_jle_elide:
    xor %eax, %eax
    cmp $0, %eax
    jle .Lcmp_jle_taken
    xor %edi, %edi
    jmp .Lcmp_jle_exit
.Lcmp_jle_taken:
    xor %edi, %edi
.Lcmp_jle_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_jle_elide, .-cmp_jle_elide

/* CMP+JG is native and neither successor consumes its flags. */
.global cmp_jg_elide
.type cmp_jg_elide, @function
cmp_jg_elide:
    mov $1, %eax
    cmp $0, %eax
    jg .Lcmp_jg_taken
    xor %edi, %edi
    jmp .Lcmp_jg_exit
.Lcmp_jg_taken:
    xor %edi, %edi
.Lcmp_jg_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_jg_elide, .-cmp_jg_elide

/* TEST+JS is native and neither successor consumes its flags. */
.global test_js_elide
.type test_js_elide, @function
test_js_elide:
    mov $-1, %eax
    test %eax, %eax
    js .Ltest_js_taken
    xor %edi, %edi
    jmp .Ltest_js_exit
.Ltest_js_taken:
    xor %edi, %edi
.Ltest_js_exit:
    mov $__NR_exit, %eax
    syscall
.size test_js_elide, .-test_js_elide

/* TEST+JNS is native and neither successor consumes its flags. */
.global test_jns_elide
.type test_jns_elide, @function
test_jns_elide:
    mov $1, %eax
    test %eax, %eax
    jns .Ltest_jns_taken
    xor %edi, %edi
    jmp .Ltest_jns_exit
.Ltest_jns_taken:
    xor %edi, %edi
.Ltest_jns_exit:
    mov $__NR_exit, %eax
    syscall
.size test_jns_elide, .-test_jns_elide

/* TEST+JLE is native and neither successor consumes its flags. */
.global test_jle_elide
.type test_jle_elide, @function
test_jle_elide:
    xor %eax, %eax
    test %eax, %eax
    jle .Ltest_jle_taken
    xor %edi, %edi
    jmp .Ltest_jle_exit
.Ltest_jle_taken:
    xor %edi, %edi
.Ltest_jle_exit:
    mov $__NR_exit, %eax
    syscall
.size test_jle_elide, .-test_jle_elide

/* TEST+JG is native and neither successor consumes its flags. */
.global test_jg_elide
.type test_jg_elide, @function
test_jg_elide:
    mov $1, %eax
    test %eax, %eax
    jg .Ltest_jg_taken
    xor %edi, %edi
    jmp .Ltest_jg_exit
.Ltest_jg_taken:
    xor %edi, %edi
.Ltest_jg_exit:
    mov $__NR_exit, %eax
    syscall
.size test_jg_elide, .-test_jg_elide

/* TEST clears OF, so JNO is always taken without reading EFLAGS. */
.global test_jno_elide
.type test_jno_elide, @function
test_jno_elide:
    mov $1, %eax
    test %eax, %eax
    jno .Ltest_jno_taken
    mov $1, %edi
    jmp .Ltest_jno_exit
.Ltest_jno_taken:
    xor %edi, %edi
.Ltest_jno_exit:
    mov $__NR_exit, %eax
    syscall
.size test_jno_elide, .-test_jno_elide

/* TEST clears OF, so JO is never taken without reading EFLAGS. */
.global test_jo_elide
.type test_jo_elide, @function
test_jo_elide:
    mov $1, %eax
    test %eax, %eax
    jo .Ltest_jo_bad
    xor %edi, %edi
    jmp .Ltest_jo_exit
.Ltest_jo_bad:
    mov $1, %edi
.Ltest_jo_exit:
    mov $__NR_exit, %eax
    syscall
.size test_jo_elide, .-test_jo_elide

/* TEST clears CF, so JB is never taken without reading EFLAGS. */
.global test_jb_elide
.type test_jb_elide, @function
test_jb_elide:
    mov $1, %eax
    test %eax, %eax
    jb .Ltest_jb_bad
    xor %edi, %edi
    jmp .Ltest_jb_exit
.Ltest_jb_bad:
    mov $1, %edi
.Ltest_jb_exit:
    mov $__NR_exit, %eax
    syscall
.size test_jb_elide, .-test_jb_elide

/* TEST clears CF, so JBE is equivalent to JE. */
.global test_jbe_elide
.type test_jbe_elide, @function
test_jbe_elide:
    xor %eax, %eax
    test %eax, %eax
    jbe .Ltest_jbe_taken
    mov $1, %edi
    jmp .Ltest_jbe_exit
.Ltest_jbe_taken:
    xor %edi, %edi
.Ltest_jbe_exit:
    mov $__NR_exit, %eax
    syscall
.size test_jbe_elide, .-test_jbe_elide

/* TEST clears CF, so JA is equivalent to JNE. */
.global test_ja_elide
.type test_ja_elide, @function
test_ja_elide:
    mov $1, %eax
    test %eax, %eax
    ja .Ltest_ja_taken
    mov $1, %edi
    jmp .Ltest_ja_exit
.Ltest_ja_taken:
    xor %edi, %edi
.Ltest_ja_exit:
    mov $__NR_exit, %eax
    syscall
.size test_ja_elide, .-test_ja_elide

/* TEST clears CF, so JAE is always taken without reading EFLAGS. */
.global test_jae_elide
.type test_jae_elide, @function
test_jae_elide:
    mov $1, %eax
    test %eax, %eax
    jae .Ltest_jae_taken
    mov $1, %edi
    jmp .Ltest_jae_exit
.Ltest_jae_taken:
    xor %edi, %edi
.Ltest_jae_exit:
    mov $__NR_exit, %eax
    syscall
.size test_jae_elide, .-test_jae_elide

/* BT+JB reads the tested bit directly; its CF does not escape. */
.global bt_jb_elide
.type bt_jb_elide, @function
bt_jb_elide:
    mov $1, %eax
    bt $0, %eax
    jb .Lbt_jb_taken
    mov $1, %edi
    jmp .Lbt_jb_exit
.Lbt_jb_taken:
    xor %edi, %edi
.Lbt_jb_exit:
    mov $__NR_exit, %eax
    syscall
.size bt_jb_elide, .-bt_jb_elide

/* BT+JAE reads the tested bit directly; its CF does not escape. */
.global bt_jae_elide
.type bt_jae_elide, @function
bt_jae_elide:
    xor %eax, %eax
    bt $0, %eax
    jae .Lbt_jae_taken
    mov $1, %edi
    jmp .Lbt_jae_exit
.Lbt_jae_taken:
    xor %edi, %edi
.Lbt_jae_exit:
    mov $__NR_exit, %eax
    syscall
.size bt_jae_elide, .-bt_jae_elide

/* SUB+JNE compares the original operands directly; its flags do not escape. */
.global sub_jne_elide
.type sub_jne_elide, @function
sub_jne_elide:
    mov $2, %eax
    sub $1, %eax
    jne .Lsub_jne_taken
    mov $1, %edi
    jmp .Lsub_jne_exit
.Lsub_jne_taken:
    xor %edi, %edi
.Lsub_jne_exit:
    mov $__NR_exit, %eax
    syscall
.size sub_jne_elide, .-sub_jne_elide

/* SUB+JL compares signed operands directly; its flags do not escape. */
.global sub_jl_elide
.type sub_jl_elide, @function
sub_jl_elide:
    xor %eax, %eax
    sub $1, %eax
    jl .Lsub_jl_taken
    mov $1, %edi
    jmp .Lsub_jl_exit
.Lsub_jl_taken:
    xor %edi, %edi
.Lsub_jl_exit:
    mov $__NR_exit, %eax
    syscall
.size sub_jl_elide, .-sub_jl_elide

/* A locked SUB falls back to the ordinary EFLAGS path. */
.global lock_sub_jne_keep_flags
.type lock_sub_jne_keep_flags, @function
lock_sub_jne_keep_flags:
    lock subl $1, lock_sub_value(%rip)
    jne .Llock_sub_taken
    mov $1, %edi
    jmp .Llock_sub_exit
.Llock_sub_taken:
    xor %edi, %edi
.Llock_sub_exit:
    mov $__NR_exit, %eax
    syscall
.size lock_sub_jne_keep_flags, .-lock_sub_jne_keep_flags

/* A nonzero SHR count supplies JNE natively; its flags do not escape. */
.global shr_jne_elide
.type shr_jne_elide, @function
shr_jne_elide:
    mov $2, %eax
    shr $1, %eax
    jne .Lshr_jne_taken
    mov $1, %edi
    jmp .Lshr_jne_exit
.Lshr_jne_taken:
    xor %edi, %edi
.Lshr_jne_exit:
    mov $__NR_exit, %eax
    syscall
.size shr_jne_elide, .-shr_jne_elide

/* A masked-zero SHR count preserves the flags produced by CMP. */
.global shr_zero_jne_keep_flags
.type shr_zero_jne_keep_flags, @function
shr_zero_jne_keep_flags:
    mov $1, %eax
    cmp $0, %eax
    shr $32, %eax
    jne .Lshr_zero_taken
    mov $1, %edi
    jmp .Lshr_zero_exit
.Lshr_zero_taken:
    xor %edi, %edi
.Lshr_zero_exit:
    mov $__NR_exit, %eax
    syscall
.size shr_zero_jne_keep_flags, .-shr_zero_jne_keep_flags

/* AND+JNE tests the result directly; its flags do not escape. */
.global and_jne_elide
.type and_jne_elide, @function
and_jne_elide:
    mov $3, %eax
    and $1, %eax
    jne .Land_jne_taken
    mov $1, %edi
    jmp .Land_jne_exit
.Land_jne_taken:
    xor %edi, %edi
.Land_jne_exit:
    mov $__NR_exit, %eax
    syscall
.size and_jne_elide, .-and_jne_elide

/* A locked AND falls back to the ordinary EFLAGS path. */
.global lock_and_jne_keep_flags
.type lock_and_jne_keep_flags, @function
lock_and_jne_keep_flags:
    lock andl $1, lock_and_value(%rip)
    jne .Llock_and_taken
    mov $1, %edi
    jmp .Llock_and_exit
.Llock_and_taken:
    xor %edi, %edi
.Llock_and_exit:
    mov $__NR_exit, %eax
    syscall
.size lock_and_jne_keep_flags, .-lock_and_jne_keep_flags

/* COMISD+JE uses the native floating-point condition. */
.global comisd_je_elide
.type comisd_je_elide, @function
comisd_je_elide:
    movq fp_one_double(%rip), %xmm0
    movapd %xmm0, %xmm1
    comisd %xmm1, %xmm0
    je .Lcomisd_je_taken
    mov $1, %edi
    jmp .Lcomisd_je_exit
.Lcomisd_je_taken:
    xor %edi, %edi
.Lcomisd_je_exit:
    mov $__NR_exit, %eax
    syscall
.size comisd_je_elide, .-comisd_je_elide

/* COMISS+JNE uses the native floating-point condition. */
.global comiss_jne_elide
.type comiss_jne_elide, @function
comiss_jne_elide:
    movd fp_one_single(%rip), %xmm0
    movd fp_two_single(%rip), %xmm1
    comiss %xmm1, %xmm0
    jne .Lcomiss_jne_taken
    mov $1, %edi
    jmp .Lcomiss_jne_exit
.Lcomiss_jne_taken:
    xor %edi, %edi
.Lcomiss_jne_exit:
    mov $__NR_exit, %eax
    syscall
.size comiss_jne_elide, .-comiss_jne_elide

/* UCOMISD+JB handles unordered input through the native condition. */
.global ucomisd_jb_nan_elide
.type ucomisd_jb_nan_elide, @function
ucomisd_jb_nan_elide:
    movq fp_qnan_double(%rip), %xmm0
    movq fp_one_double(%rip), %xmm1
    ucomisd %xmm1, %xmm0
    jb .Lucomisd_jb_taken
    mov $1, %edi
    jmp .Lucomisd_jb_exit
.Lucomisd_jb_taken:
    xor %edi, %edi
.Lucomisd_jb_exit:
    mov $__NR_exit, %eax
    syscall
.size ucomisd_jb_nan_elide, .-ucomisd_jb_nan_elide

/* UCOMISS+JNE uses the native floating-point condition. */
.global ucomiss_jne_elide
.type ucomiss_jne_elide, @function
ucomiss_jne_elide:
    movd fp_one_single(%rip), %xmm0
    movd fp_two_single(%rip), %xmm1
    ucomiss %xmm1, %xmm0
    jne .Lucomiss_jne_taken
    mov $1, %edi
    jmp .Lucomiss_jne_exit
.Lucomiss_jne_taken:
    xor %edi, %edi
.Lucomiss_jne_exit:
    mov $__NR_exit, %eax
    syscall
.size ucomiss_jne_elide, .-ucomiss_jne_elide

/* A successor LAHF still requires the flags produced by UCOMISD. */
.global ucomisd_je_keep_zf
.type ucomisd_je_keep_zf, @function
ucomisd_je_keep_zf:
    movq fp_one_double(%rip), %xmm0
    movapd %xmm0, %xmm1
    ucomisd %xmm1, %xmm0
    je .Lucomisd_keep_taken
    mov $1, %edi
    jmp .Lucomisd_keep_exit
.Lucomisd_keep_taken:
    lahf
    movzx %ah, %edi
    and $0x40, %edi
    xor $0x40, %edi
.Lucomisd_keep_exit:
    mov $__NR_exit, %eax
    syscall
.size ucomisd_je_keep_zf, .-ucomisd_je_keep_zf

/* A MOV may separate CMP from its native JNE consumer. */
.global cmp_xx_jne_elide
.type cmp_xx_jne_elide, @function
cmp_xx_jne_elide:
    mov $2, %eax
    cmp $1, %eax
    mov $1, %eax
    jne .Lcmp_xx_jne_taken
    mov $1, %edi
    jmp .Lcmp_xx_jne_exit
.Lcmp_xx_jne_taken:
    xor %edi, %edi
.Lcmp_xx_jne_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_xx_jne_elide, .-cmp_xx_jne_elide

/* A MOV may separate TEST from its native JE consumer. */
.global test_xx_je_elide
.type test_xx_je_elide, @function
test_xx_je_elide:
    xor %eax, %eax
    test %eax, %eax
    mov $1, %eax
    je .Ltest_xx_je_taken
    mov $1, %edi
    jmp .Ltest_xx_je_exit
.Ltest_xx_je_taken:
    xor %edi, %edi
.Ltest_xx_je_exit:
    mov $__NR_exit, %eax
    syscall
.size test_xx_je_elide, .-test_xx_je_elide

/* A MOV may separate BT from its native JB consumer. */
.global bt_xx_jb_elide
.type bt_xx_jb_elide, @function
bt_xx_jb_elide:
    mov $1, %eax
    bt $0, %eax
    mov $0, %eax
    jb .Lbt_xx_jb_taken
    mov $1, %edi
    jmp .Lbt_xx_jb_exit
.Lbt_xx_jb_taken:
    xor %edi, %edi
.Lbt_xx_jb_exit:
    mov $__NR_exit, %eax
    syscall
.size bt_xx_jb_elide, .-bt_xx_jb_elide

/* MOVAPD may separate UCOMISD from its native JE consumer. */
.global ucomisd_xx_je_elide
.type ucomisd_xx_je_elide, @function
ucomisd_xx_je_elide:
    movq fp_one_double(%rip), %xmm0
    movapd %xmm0, %xmm1
    ucomisd %xmm1, %xmm0
    movq fp_qnan_double(%rip), %xmm0
    je .Lucomisd_xx_je_taken
    mov $1, %edi
    jmp .Lucomisd_xx_je_exit
.Lucomisd_xx_je_taken:
    xor %edi, %edi
.Lucomisd_xx_je_exit:
    mov $__NR_exit, %eax
    syscall
.size ucomisd_xx_je_elide, .-ucomisd_xx_je_elide

/* A non-adjacent successor LAHF still requires CMP flags. */
.global cmp_xx_je_keep_zf
.type cmp_xx_je_keep_zf, @function
cmp_xx_je_keep_zf:
    mov $1, %eax
    cmp $1, %eax
    mov $2, %eax
    je .Lcmp_xx_keep_taken
    mov $1, %edi
    jmp .Lcmp_xx_keep_exit
.Lcmp_xx_keep_taken:
    lahf
    movzx %ah, %edi
    and $0x40, %edi
    xor $0x40, %edi
.Lcmp_xx_keep_exit:
    mov $__NR_exit, %eax
    syscall
.size cmp_xx_je_keep_zf, .-cmp_xx_je_keep_zf

/* NEG+CMOVS uses the result sign directly; NEG flags do not escape. */
.global neg_cmovs_elide
.type neg_cmovs_elide, @function
neg_cmovs_elide:
    mov $1, %eax
    mov $7, %ebx
    xor %ecx, %ecx
    neg %eax
    cmovs %ebx, %ecx
    sub $7, %ecx
    mov %ecx, %edi
    mov $__NR_exit, %eax
    syscall
.size neg_cmovs_elide, .-neg_cmovs_elide

/* A locked NEG falls back to the ordinary EFLAGS CMOV path. */
.global lock_neg_cmovs_keep_flags
.type lock_neg_cmovs_keep_flags, @function
lock_neg_cmovs_keep_flags:
    mov $7, %ebx
    xor %ecx, %ecx
    lock negl lock_neg_value(%rip)
    cmovs %ebx, %ecx
    sub $7, %ecx
    mov %ecx, %edi
    mov $__NR_exit, %eax
    syscall
.size lock_neg_cmovs_keep_flags, .-lock_neg_cmovs_keep_flags

/* A later LAHF still requires the flags produced by NEG. */
.global neg_cmovs_keep_sf
.type neg_cmovs_keep_sf, @function
neg_cmovs_keep_sf:
    mov $1, %eax
    mov $7, %ebx
    xor %ecx, %ecx
    neg %eax
    cmovs %ebx, %ecx
    lahf
    movzx %ah, %edi
    and $0x80, %edi
    xor $0x80, %edi
    mov $__NR_exit, %eax
    syscall
.size neg_cmovs_keep_sf, .-neg_cmovs_keep_sf

/* UCOMISD+SETA uses a native floating-point comparison. */
.global ucomisd_seta_elide
.type ucomisd_seta_elide, @function
ucomisd_seta_elide:
    movq fp_one_double(%rip), %xmm0
    movq fp_zero_double(%rip), %xmm1
    ucomisd %xmm1, %xmm0
    seta %dil
    xor $1, %dil
    mov $__NR_exit, %eax
    syscall
.size ucomisd_seta_elide, .-ucomisd_seta_elide

/* A later LAHF still requires the flags produced by UCOMISD+SETA. */
.global ucomisd_seta_keep_zf
.type ucomisd_seta_keep_zf, @function
ucomisd_seta_keep_zf:
    movq fp_one_double(%rip), %xmm0
    movapd %xmm0, %xmm1
    ucomisd %xmm1, %xmm0
    seta %dil
    lahf
    movzx %ah, %edi
    and $0x40, %edi
    xor $0x40, %edi
    mov $__NR_exit, %eax
    syscall
.size ucomisd_seta_keep_zf, .-ucomisd_seta_keep_zf

.section .data
.align 8
lock_sub_value:
    .long 2
lock_and_value:
    .long 3
lock_neg_value:
    .long 1
fp_one_double:
    .quad 0x3ff0000000000000
fp_zero_double:
    .quad 0x0000000000000000
fp_qnan_double:
    .quad 0x7ff8000000000000
fp_one_single:
    .long 0x3f800000
fp_two_single:
    .long 0x40000000
