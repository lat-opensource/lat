/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

    .text
    .align  2
    .global latx_kzt_callback_cpu_loop
    .type   latx_kzt_callback_cpu_loop, @function

/*
 * Run one complete native-to-guest callback while preserving the native
 * caller's callee-saved floating-point registers.  Guest XMM8-XMM15 reuse
 * f24-f31, so preserving them here keeps the cost off the normal TB dispatch
 * path and pays it only when a KZT wrapper actually invokes guest code.
 */
latx_kzt_callback_cpu_loop:
    .cfi_startproc
    addi.d  $sp, $sp, -80
    .cfi_def_cfa_offset 80
    st.d    $ra, $sp, 64
    .cfi_offset 1, -16

    fst.d   $f24, $sp, 0
    fst.d   $f25, $sp, 8
    fst.d   $f26, $sp, 16
    fst.d   $f27, $sp, 24
    fst.d   $f28, $sp, 32
    fst.d   $f29, $sp, 40
    fst.d   $f30, $sp, 48
    fst.d   $f31, $sp, 56

    bl      cpu_loop

    fld.d   $f24, $sp, 0
    fld.d   $f25, $sp, 8
    fld.d   $f26, $sp, 16
    fld.d   $f27, $sp, 24
    fld.d   $f28, $sp, 32
    fld.d   $f29, $sp, 40
    fld.d   $f30, $sp, 48
    fld.d   $f31, $sp, 56

    ld.d    $ra, $sp, 64
    .cfi_restore 1
    addi.d  $sp, $sp, 80
    .cfi_def_cfa_offset 0
    jirl    $zero, $ra, 0
    .cfi_endproc

    .size   latx_kzt_callback_cpu_loop, .-latx_kzt_callback_cpu_loop
