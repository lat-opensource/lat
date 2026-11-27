/* SPDX-License-Identifier: GPL-2.0-or-later */
    .text
/* Guest XMM8-XMM15 reuse the native callee-saved f24-f31. Preserve the
 * native ABI at each call that loads guest floating-point registers. */
    .macro wow64_preserve_fprs name, target
    .align 2
    .global \name
    .hidden \name
    .type \name, @function
\name:
    .cfi_startproc
    addi.d $sp, $sp, -80
    .cfi_def_cfa_offset 80
    st.d $ra, $sp, 64
    .cfi_offset 1, -16
    fst.d $f24, $sp, 0
    .cfi_offset 56, -80
    fst.d $f25, $sp, 8
    .cfi_offset 57, -72
    fst.d $f26, $sp, 16
    .cfi_offset 58, -64
    fst.d $f27, $sp, 24
    .cfi_offset 59, -56
    fst.d $f28, $sp, 32
    .cfi_offset 60, -48
    fst.d $f29, $sp, 40
    .cfi_offset 61, -40
    fst.d $f30, $sp, 48
    .cfi_offset 62, -32
    fst.d $f31, $sp, 56
    .cfi_offset 63, -24
    bl \target
    fld.d $f24, $sp, 0
    .cfi_restore 56
    fld.d $f25, $sp, 8
    .cfi_restore 57
    fld.d $f26, $sp, 16
    .cfi_restore 58
    fld.d $f27, $sp, 24
    .cfi_restore 59
    fld.d $f28, $sp, 32
    .cfi_restore 60
    fld.d $f29, $sp, 40
    .cfi_restore 61
    fld.d $f30, $sp, 48
    .cfi_restore 62
    fld.d $f31, $sp, 56
    .cfi_restore 63
    ld.d $ra, $sp, 64
    .cfi_restore 1
    addi.d $sp, $sp, 80
    .cfi_def_cfa_offset 0
    jr $ra
    .cfi_endproc
    .size \name, .-\name
    .endm

    wow64_preserve_fprs lat_wow64_cpu_exec, cpu_exec
    wow64_preserve_fprs lat_wow64_init_fpu_regs, latx_init_fpu_regs
    .section .note.GNU-stack,"",@progbits
