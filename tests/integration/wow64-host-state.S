/* SPDX-License-Identifier: GPL-2.0-or-later */
    .text
    .align 2
    .global wow64_check_host_fprs
    .type wow64_check_host_fprs, @function
/* Call an API operation with distinct values in all native callee-saved FPRs.
 * a0 = callback, a1 = opaque, a2 = pointer receiving its status. */
wow64_check_host_fprs:
    .cfi_startproc
    addi.d $sp, $sp, -80
    .cfi_def_cfa_offset 80
    st.d $ra, $sp, 64
    .cfi_offset 1, -16
    st.d $a2, $sp, 72
    fst.d $f24, $sp, 0
    fst.d $f25, $sp, 8
    fst.d $f26, $sp, 16
    fst.d $f27, $sp, 24
    fst.d $f28, $sp, 32
    fst.d $f29, $sp, 40
    fst.d $f30, $sp, 48
    fst.d $f31, $sp, 56
    li.d $t0, 101
    movgr2fr.d $f24, $t0
    li.d $t0, 102
    movgr2fr.d $f25, $t0
    li.d $t0, 103
    movgr2fr.d $f26, $t0
    li.d $t0, 104
    movgr2fr.d $f27, $t0
    li.d $t0, 105
    movgr2fr.d $f28, $t0
    li.d $t0, 106
    movgr2fr.d $f29, $t0
    li.d $t0, 107
    movgr2fr.d $f30, $t0
    li.d $t0, 108
    movgr2fr.d $f31, $t0
    move $t0, $a0
    move $a0, $a1
    jirl $ra, $t0, 0
    ld.d $t0, $sp, 72
    st.w $a0, $t0, 0
    li.d $a0, 1
    movfr2gr.d $t0, $f24
    li.d $t1, 101
    bne $t0, $t1, 1f
    movfr2gr.d $t0, $f25
    li.d $t1, 102
    bne $t0, $t1, 1f
    movfr2gr.d $t0, $f26
    li.d $t1, 103
    bne $t0, $t1, 1f
    movfr2gr.d $t0, $f27
    li.d $t1, 104
    bne $t0, $t1, 1f
    movfr2gr.d $t0, $f28
    li.d $t1, 105
    bne $t0, $t1, 1f
    movfr2gr.d $t0, $f29
    li.d $t1, 106
    bne $t0, $t1, 1f
    movfr2gr.d $t0, $f30
    li.d $t1, 107
    bne $t0, $t1, 1f
    movfr2gr.d $t0, $f31
    li.d $t1, 108
    bne $t0, $t1, 1f
    move $a0, $zero
1:
    fld.d $f24, $sp, 0
    fld.d $f25, $sp, 8
    fld.d $f26, $sp, 16
    fld.d $f27, $sp, 24
    fld.d $f28, $sp, 32
    fld.d $f29, $sp, 40
    fld.d $f30, $sp, 48
    fld.d $f31, $sp, 56
    ld.d $ra, $sp, 64
    .cfi_restore 1
    addi.d $sp, $sp, 80
    .cfi_def_cfa_offset 0
    jr $ra
    .cfi_endproc
    .size wow64_check_host_fprs, .-wow64_check_host_fprs
    .section .note.GNU-stack,"",@progbits
