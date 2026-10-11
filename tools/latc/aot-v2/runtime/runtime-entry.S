.text
.align 2

.macro LAT_AOT_RUNTIME_ENTRY name, slot
    .global \name
    .type \name, @function
\name:
    la.local $t0, lat_aot_runtime_target_slots_v2
    ld.d $t0, $t0, (\slot * 8)
    beqz $t0, 1f
    jr $t0
1:
    break 0
    .size \name, .-\name
.endm

LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_epilogue_ret_id_1, 0
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_epilogue_ret_id_0, 1
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_jirl_epilogue_ret_id_1, 2
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_jirl_epilogue_ret_id_0, 3
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_epilogue_ret_0, 4
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_update_mxcsr_status, 5
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fxsave, 6
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fxrstor, 7
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fpregs_x80_to_64, 8
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fpregs_64_to_x80, 9
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_update_fp_status, 10
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_cpuid, 11
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_raise_illop, 12
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_raise_gpf, 13
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_pcmpistri_xmm, 14
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_pcmpistrm_xmm, 15
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_eflagtf, 16
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_log2, 17
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_pow, 18
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sin, 19
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_cos, 20
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_atan2, 21
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_logb, 22
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sincos, 23
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fpatan, 24
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fptan, 25
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fprem, 26
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fprem1, 27
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_frndint, 28
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_f2xm1, 29
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fxtract, 30
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fyl2x, 31
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fyl2xp1, 32
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fsincos, 33
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fsin, 34
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fcos, 35
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fbld_st0, 36
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_fbst_st0, 37
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_aesimc_xmm, 38
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_aeskeygenassist_xmm, 39
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_aesdec_xmm, 40
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_aesdeclast_xmm, 41
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_aesenc_xmm, 42
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_aesenclast_xmm, 43
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sha1nexte, 44
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sha1msg1, 45
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sha1msg2, 46
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sha256msg1, 47
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sha256msg2, 48
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sha1rnds4_f0, 49
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sha1rnds4_f1, 50
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sha1rnds4_f2, 51
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sha1rnds4_f3, 52
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_sha256rnds2_xmm0, 53
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_raise_int, 54
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_raise_trapop, 55
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_raise_into, 56
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_raise_bound, 57
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_xgetbv, 58
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_kzt_get_alternate, 59
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_xsave, 60
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_xsaveopt, 61
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_xrstor, 62
LAT_AOT_RUNTIME_ENTRY lat_aot_runtime_xsetbv, 63

.section .note.GNU-stack,"",@progbits
