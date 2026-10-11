#include "dispatch.h"
#include "latx-x86-env-offsets.h"

.text
.align 2
.global lat_native_x86_dispatch_jirl
.type lat_native_x86_dispatch_jirl, @function
lat_native_x86_dispatch_jirl:
    srli.d $t0, $r21, LAT_NATIVE_X86_JMP_CACHE_BITS
    xor $t0, $r21, $t0
    bstrpick.d $t0, $t0, LAT_NATIVE_X86_JMP_CACHE_BITS - 1, 0
    alsl.d $t0, $t0, $fp, 3
    ld.d $t1, $t0, 0
    bne $t1, $r21, .Ldispatch_jirl_miss
    ld.d $t0, $t0, 8
    jr $t0

.Ldispatch_jirl_miss:
    addi.d $sp, $sp, -96
    st.d $ra, $sp, 0
    st.d $a2, $sp, 8
    st.d $a3, $sp, 16
    st.d $a4, $sp, 24
    st.d $a5, $sp, 32
    st.d $a6, $sp, 40
    st.d $t5, $sp, 48
    st.d $t6, $sp, 56
    st.d $t7, $sp, 64
    st.d $t8, $sp, 72
    st.d $r21, $sp, 80
    move $a0, $r21
    bl lat_native_x86_dispatch_lookup
    ld.d $r21, $sp, 80
    ld.d $t8, $sp, 72
    ld.d $t7, $sp, 64
    ld.d $t6, $sp, 56
    ld.d $t5, $sp, 48
    ld.d $a6, $sp, 40
    ld.d $a5, $sp, 32
    ld.d $a4, $sp, 24
    ld.d $a3, $sp, 16
    ld.d $a2, $sp, 8
    ld.d $ra, $sp, 0
    addi.d $sp, $sp, 96
    jr $a0
.size lat_native_x86_dispatch_jirl, .-lat_native_x86_dispatch_jirl

.align 2
.global lat_native_x86_dispatch_env_eip
.type lat_native_x86_dispatch_env_eip, @function
lat_native_x86_dispatch_env_eip:
    ld.d $r21, $s8, LATC_X86_ENV_EXCEPTION_NEXT_EIP_OFFSET
    b lat_native_x86_dispatch_jirl
.size lat_native_x86_dispatch_env_eip, .-lat_native_x86_dispatch_env_eip

.align 2
.global lat_native_x86_syscall
.type lat_native_x86_syscall, @function
lat_native_x86_syscall:
    bl lat_native_x86_syscall_impl
    ld.d $a0, $s8, LATC_X86_ENV_EXCEPTION_NEXT_EIP_OFFSET
    bl lat_native_x86_dispatch_lookup

    ld.d $s0, $s8, LATC_X86_ENV_RAX_OFFSET
    ld.d $s1, $s8, LATC_X86_ENV_RCX_OFFSET
    ld.d $s2, $s8, LATC_X86_ENV_RDX_OFFSET
    ld.d $s3, $s8, LATC_X86_ENV_RBX_OFFSET
    ld.d $s4, $s8, LATC_X86_ENV_RSP_OFFSET
    ld.d $s5, $s8, LATC_X86_ENV_RBP_OFFSET
    ld.d $s6, $s8, LATC_X86_ENV_RSI_OFFSET
    ld.d $s7, $s8, LATC_X86_ENV_RDI_OFFSET
    ld.d $a2, $s8, LATC_X86_ENV_R8_OFFSET
    ld.d $a3, $s8, LATC_X86_ENV_R9_OFFSET
    ld.d $a4, $s8, LATC_X86_ENV_R10_OFFSET
    ld.d $a5, $s8, LATC_X86_ENV_R11_OFFSET
    ld.d $t5, $s8, LATC_X86_ENV_R12_OFFSET
    ld.d $t6, $s8, LATC_X86_ENV_R13_OFFSET
    ld.d $t7, $s8, LATC_X86_ENV_R14_OFFSET
    ld.d $t8, $s8, LATC_X86_ENV_R15_OFFSET
    ld.d $a6, $s8, LATC_X86_ENV_EFLAGS_OFFSET

    fld.d $f0, $s8, LATC_X86_ENV_FPREG0_OFFSET
    fld.d $f1, $s8, LATC_X86_ENV_FPREG1_OFFSET
    fld.d $f2, $s8, LATC_X86_ENV_FPREG2_OFFSET
    fld.d $f3, $s8, LATC_X86_ENV_FPREG3_OFFSET
    fld.d $f4, $s8, LATC_X86_ENV_FPREG4_OFFSET
    fld.d $f5, $s8, LATC_X86_ENV_FPREG5_OFFSET
    fld.d $f6, $s8, LATC_X86_ENV_FPREG6_OFFSET
    fld.d $f7, $s8, LATC_X86_ENV_FPREG7_OFFSET

    addi.d $t0, $s8, LATC_X86_ENV_XMM13_OFFSET
    xvld $xr16, $t0, LATC_X86_ENV_XMM0_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr17, $t0, LATC_X86_ENV_XMM1_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr18, $t0, LATC_X86_ENV_XMM2_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr19, $t0, LATC_X86_ENV_XMM3_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr20, $t0, LATC_X86_ENV_XMM4_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr21, $t0, LATC_X86_ENV_XMM5_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr22, $t0, LATC_X86_ENV_XMM6_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr23, $t0, LATC_X86_ENV_XMM7_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr24, $t0, LATC_X86_ENV_XMM8_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr25, $t0, LATC_X86_ENV_XMM9_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr26, $t0, LATC_X86_ENV_XMM10_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr27, $t0, LATC_X86_ENV_XMM11_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr28, $t0, LATC_X86_ENV_XMM12_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr29, $t0, 0
    xvld $xr30, $t0, LATC_X86_ENV_XMM14_OFFSET-LATC_X86_ENV_XMM13_OFFSET
    xvld $xr31, $t0, LATC_X86_ENV_XMM15_OFFSET-LATC_X86_ENV_XMM13_OFFSET

    ld.d $r21, $s8, LATC_X86_ENV_EXCEPTION_NEXT_EIP_OFFSET
    jr $a0
.size lat_native_x86_syscall, .-lat_native_x86_syscall
