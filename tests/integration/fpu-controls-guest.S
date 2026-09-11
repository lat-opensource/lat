/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Freestanding x86_64 guest for floating-point control-state tests.
 *
 * The cases check x87/MXCSR rounding-control ownership across FNINIT, FLDCW,
 * XRSTOR, AVX arithmetic, and SSE conversions.  Native FCSR is deliberately
 * primed where needed so a failure identifies stale shared-FCSR control state
 * rather than an unrelated arithmetic result.
 */

#ifndef TEST_CASE
#error TEST_CASE must select an FPU control test
#endif

.equ __NR_write, 1
.equ __NR_exit, 60

.section .text
.global _start
.type _start, @function
_start:
    fninit

#if TEST_CASE == 1
    /* MXCSR round-up must survive FNINIT. */
    fldcw x87_round_up(%rip)
    ldmxcsr mxcsr_round_up(%rip)
    fninit

    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0
    movss %xmm0, report(%rip)

#elif TEST_CASE == 2
    /* x87 RC=round-up must be read from env->fpuc by x87 prepare. */
    fldcw x87_round_up(%rip)
    flds one(%rip)
    fadds half_ulp(%rip)
    fstps report(%rip)

#elif TEST_CASE == 3
    /*
     * Establish the SSE domain, then change native RM through FLDCW.
     * The next SSE instruction must invalidate the TB-local domain and
     * restore MXCSR's round-up mode.
     */
    ldmxcsr mxcsr_round_up(%rip)
    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0

    fldcw x87_round_down(%rip)

    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0
    movss %xmm0, report(%rip)

#elif TEST_CASE == 4
    /*
     * Establish the SSE domain, then FLDENV changes native RM through the
     * restored x87 control word.  The following SSE operation must re-prepare
     * from MXCSR round-up.
     */
    ldmxcsr mxcsr_round_up(%rip)
    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0

    fldenv fldenv_area(%rip)

    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0
    movss %xmm0, report(%rip)

#elif TEST_CASE == 5
    /* FRSTOR must restore the x87 rounding mode into native FCSR. */
    fldcw x87_round_up(%rip)
    fnsave frstor_area(%rip)
    movw $0x077f, frstor_area(%rip)
    fldcw x87_round_up(%rip)
    frstor frstor_area(%rip)

    flds one(%rip)
    fadds half_ulp(%rip)
    fstps report(%rip)

#elif TEST_CASE == 6
    /* XRSTOR with RFBM=3 must restore the x87 rounding mode. */
    fldcw x87_round_up(%rip)
    movl $3, %eax
    xorl %edx, %edx
    xsave xsave_area(%rip)
    movw $0x077f, xsave_area(%rip)
    movl $3, %eax
    xorl %edx, %edx
    xrstor xsave_area(%rip)

    flds one(%rip)
    fadds half_ulp(%rip)
    fstps report(%rip)

#elif TEST_CASE == 7
    /* RFBM=1 restores only the x87 component. */
    fldcw x87_round_up(%rip)
    movl $1, %eax
    xorl %edx, %edx
    xsave xsave_area(%rip)
    movw $0x077f, xsave_area(%rip)
    fldcw x87_round_up(%rip)
    movl $1, %eax
    xorl %edx, %edx
    xrstor xsave_area(%rip)

    flds one(%rip)
    fadds half_ulp(%rip)
    fstps report(%rip)

#elif TEST_CASE == 8
    /* RFBM=2 restores only the SSE component and its MXCSR. */
    ldmxcsr mxcsr_round_up(%rip)
    movl $2, %eax
    xorl %edx, %edx
    xsave xsave_area(%rip)
    movl $0x00003f80, xsave_area+24(%rip)
    ldmxcsr mxcsr_round_up(%rip)
    movl $2, %eax
    xorl %edx, %edx
    xrstor xsave_area(%rip)

    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0
    movss %xmm0, report(%rip)

#elif TEST_CASE == 9
    /* RFBM=3 restores x87 and SSE; both must use round-down afterwards. */
    fldcw x87_round_up(%rip)
    ldmxcsr mxcsr_round_up(%rip)
    movl $3, %eax
    xorl %edx, %edx
    xsave xsave_area(%rip)
    movw $0x077f, xsave_area(%rip)
    movl $0x00003f80, xsave_area+24(%rip)
    fldcw x87_round_up(%rip)
    ldmxcsr mxcsr_round_up(%rip)
    movl $3, %eax
    xorl %edx, %edx
    xrstor xsave_area(%rip)

    flds one(%rip)
    fadds half_ulp(%rip)
    fstps report(%rip)
    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0
    movss %xmm0, report+4(%rip)

#elif TEST_CASE == 10
    /* VADDSS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovss one(%rip), %xmm0
    vaddss half_ulp(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 11
    /* VADDPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovups packed_one(%rip), %xmm0
    vaddps packed_half(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 12
    /* VADDSD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovsd one_d(%rip), %xmm0
    vaddsd half_ulp_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 13
    /* VADDPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovupd packed_one_d(%rip), %xmm0
    vaddpd packed_half_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 14
    /* VSUBSS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovss one(%rip), %xmm0
    vsubss neg_half_ulp(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 15
    /* VSUBPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovups packed_one(%rip), %xmm0
    vsubps packed_neg_half(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 16
    /* VSUBSD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovsd one_d(%rip), %xmm0
    vsubsd neg_half_ulp_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 17
    /* VSUBPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovupd packed_one_d(%rip), %xmm0
    vsubpd packed_neg_half_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 18
    /* VMULSS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovss one_point25(%rip), %xmm0
    vmulss next_one(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 19
    /* VMULPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovups packed_one_point25(%rip), %xmm0
    vmulps packed_next_one(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 20
    /* VMULSD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovsd one_point25_d(%rip), %xmm0
    vmulsd next_one_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 21
    /* VMULPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovupd packed_one_point25_d(%rip), %xmm0
    vmulpd packed_next_one_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 22
    /* VDIVSS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovss div_float_num(%rip), %xmm0
    vdivss div_float_den(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 23
    /* VDIVPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovups packed_div_float_num(%rip), %xmm0
    vdivps packed_div_float_den(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 24
    /* VDIVSD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovsd div_double_num(%rip), %xmm0
    vdivsd div_double_den(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 25
    /* VDIVPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovupd packed_div_double_num(%rip), %xmm0
    vdivpd packed_div_double_den(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 26
    /* VSQRTSS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovss sqrt_two(%rip), %xmm0
    vsqrtss %xmm0, %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 27
    /* VSQRTPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovups packed_sqrt_two(%rip), %xmm0
    vsqrtps %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 28
    /* VSQRTSD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovsd sqrt_three_d(%rip), %xmm0
    vsqrtsd %xmm0, %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 29
    /* VSQRTPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovupd packed_sqrt_three_d(%rip), %xmm0
    vsqrtpd %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 30
    /* VADDSUBPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovups packed_one(%rip), %xmm0
    vaddsubps packed_neg_half(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 31
    /* VADDSUBPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovupd packed_one_d(%rip), %xmm0
    vaddsubpd packed_neg_half_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 32
    /* VHADDPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovups hadd_src1_s(%rip), %xmm0
    vhaddps packed_zero_s(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 33
    /* VHADDPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovupd hadd_src1_d(%rip), %xmm0
    vhaddpd packed_zero_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 34
    /* VHSUBPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovups hsub_src1_s(%rip), %xmm0
    vhsubps packed_zero_s(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 35
    /* VHSUBPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovupd hsub_src1_d(%rip), %xmm0
    vhsubpd packed_zero_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 36
    /* VFMADD132SS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovss one(%rip), %xmm0
    vfmadd132ss half_ulp(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 37
    /* VFMADD132PS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovups packed_one(%rip), %xmm0
    vfmadd132ps packed_half(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 38
    /* VFMADD132SD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovsd one_d(%rip), %xmm0
    vfmadd132sd half_ulp_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 39
    /* VFMADD132PD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovupd packed_one_d(%rip), %xmm0
    vfmadd132pd packed_half_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 40
    /* VFMSUB132SS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovss one(%rip), %xmm0
    vmovss neg_half_ulp(%rip), %xmm1
    vfmsub132ss one(%rip), %xmm1, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 41
    /* VFMSUB132SD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovsd one_d(%rip), %xmm0
    vmovsd neg_half_ulp_d(%rip), %xmm1
    vfmsub132sd one_d(%rip), %xmm1, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 42
    /* VFNMADD132SS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovss one(%rip), %xmm0
    vmovss half_ulp(%rip), %xmm1
    vfnmadd132ss negative_one_s(%rip), %xmm1, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 43
    /* VFNMADD132SD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovsd one_d(%rip), %xmm0
    vmovsd half_ulp_d(%rip), %xmm1
    vfnmadd132sd negative_one_d(%rip), %xmm1, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 44
    /* VFNMSUB132SS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovss one(%rip), %xmm0
    vmovss neg_half_ulp(%rip), %xmm1
    vfnmsub132ss negative_one_s(%rip), %xmm1, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 45
    /* VFNMSUB132SD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovsd one_d(%rip), %xmm0
    vmovsd neg_half_ulp_d(%rip), %xmm1
    vfnmsub132sd negative_one_d(%rip), %xmm1, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 46
    /* VDPPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovups dpps_src1_s(%rip), %xmm0
    vdpps $0xf1, dpps_src2_s(%rip), %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

#elif TEST_CASE == 47
    /* VDPPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    vmovupd dppd_src1_d(%rip), %xmm0
    vdppd $0x31, dppd_src2_d(%rip), %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

#elif TEST_CASE == 48
    /* HADDPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    movups hadd_src1_s(%rip), %xmm0
    movups packed_zero_s(%rip), %xmm1
    haddps %xmm1, %xmm0
    movss %xmm0, report(%rip)

#elif TEST_CASE == 49
    /* HADDPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    movupd hadd_src1_d(%rip), %xmm0
    movupd packed_zero_d(%rip), %xmm1
    haddpd %xmm1, %xmm0
    movsd %xmm0, report(%rip)

#elif TEST_CASE == 50
    /* HSUBPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    movups hsub_src1_s(%rip), %xmm0
    movups packed_zero_s(%rip), %xmm1
    hsubps %xmm1, %xmm0
    movss %xmm0, report(%rip)

#elif TEST_CASE == 51
    /* HSUBPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    movupd hsub_src1_d(%rip), %xmm0
    movupd packed_zero_d(%rip), %xmm1
    hsubpd %xmm1, %xmm0
    movsd %xmm0, report(%rip)

#elif TEST_CASE == 52
    /* DPPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    movups dpps_src1_s(%rip), %xmm0
    movups dpps_src2_s(%rip), %xmm1
    dpps $0xf1, %xmm1, %xmm0
    movss %xmm0, report(%rip)

#elif TEST_CASE == 53
    /* DPPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    movupd dppd_src1_d(%rip), %xmm0
    movupd dppd_src2_d(%rip), %xmm1
    dppd $0x31, %xmm1, %xmm0
    movsd %xmm0, report(%rip)

#elif TEST_CASE == 54
    /* ADDSUBPS must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    movups packed_one(%rip), %xmm0
    movups packed_neg_half(%rip), %xmm1
    addsubps %xmm1, %xmm0
    movss %xmm0, report(%rip)

#elif TEST_CASE == 55
    /* ADDSUBPD must use MXCSR's round-up mode. */
    ldmxcsr mxcsr_round_up(%rip)
    movupd packed_one_d(%rip), %xmm0
    movupd packed_neg_half_d(%rip), %xmm1
    addsubpd %xmm1, %xmm0
    movsd %xmm0, report(%rip)

#elif TEST_CASE == 56
    /* CVTDQ2PS must follow MXCSR.RC for an exactly halfway integer. */
    ldmxcsr mxcsr_round_nearest(%rip)
    movups cvt_dq_input(%rip), %xmm0
    cvtdq2ps %xmm0, %xmm0
    movss %xmm0, report(%rip)

    ldmxcsr mxcsr_round_up(%rip)
    movups cvt_dq_input(%rip), %xmm0
    cvtdq2ps %xmm0, %xmm0
    movss %xmm0, report+4(%rip)

#elif TEST_CASE == 57
    /* CVTSD2SS must follow MXCSR.RC when narrowing double to float. */
    ldmxcsr mxcsr_round_nearest(%rip)
    movsd cvt_sd_halfway(%rip), %xmm0
    cvtsd2ss %xmm0, %xmm0
    movss %xmm0, report(%rip)

    ldmxcsr mxcsr_round_up(%rip)
    movsd cvt_sd_halfway(%rip), %xmm0
    cvtsd2ss %xmm0, %xmm0
    movss %xmm0, report+4(%rip)

#elif TEST_CASE == 58
    /* CVTSS2SI must follow MXCSR.RC for a non-integral source. */
    ldmxcsr mxcsr_round_nearest(%rip)
    movss cvt_ss_one_point_five(%rip), %xmm0
    cvtss2si %xmm0, %eax
    movl %eax, report(%rip)

    ldmxcsr mxcsr_round_down(%rip)
    movss cvt_ss_one_point_five(%rip), %xmm0
    cvtss2si %xmm0, %eax
    movl %eax, report+4(%rip)

#elif TEST_CASE == 59
    /* CVTTSS2SI is fixed round-toward-zero and must ignore MXCSR.RC. */
    ldmxcsr mxcsr_round_nearest(%rip)
    movss cvt_ss_one_point_seven_five(%rip), %xmm0
    cvttss2si %xmm0, %eax
    movl %eax, report(%rip)

    ldmxcsr mxcsr_round_up(%rip)
    movss cvt_ss_one_point_seven_five(%rip), %xmm0
    cvttss2si %xmm0, %eax
    movl %eax, report+4(%rip)

#elif TEST_CASE == 60
    /* CVTPS2DQ must follow MXCSR.RC for a non-integral source. */
    ldmxcsr mxcsr_round_nearest(%rip)
    movups cvt_packed_ss_one_point_five(%rip), %xmm0
    cvtps2dq %xmm0, %xmm0
    movd %xmm0, %eax
    movl %eax, report(%rip)

    ldmxcsr mxcsr_round_down(%rip)
    movups cvt_packed_ss_one_point_five(%rip), %xmm0
    cvtps2dq %xmm0, %xmm0
    movd %xmm0, %eax
    movl %eax, report+4(%rip)

#elif TEST_CASE == 61
    /* CVTPD2DQ must follow MXCSR.RC for a non-integral source. */
    ldmxcsr mxcsr_round_nearest(%rip)
    movupd cvt_packed_d_one_point_five(%rip), %xmm0
    cvtpd2dq %xmm0, %xmm0
    movd %xmm0, %eax
    movl %eax, report(%rip)

    ldmxcsr mxcsr_round_down(%rip)
    movupd cvt_packed_d_one_point_five(%rip), %xmm0
    cvtpd2dq %xmm0, %xmm0
    movd %xmm0, %eax
    movl %eax, report+4(%rip)

#elif TEST_CASE == 62
    /* CVTPD2PS must follow MXCSR.RC when narrowing double to float. */
    ldmxcsr mxcsr_round_nearest(%rip)
    movupd cvt_packed_d_halfway(%rip), %xmm0
    cvtpd2ps %xmm0, %xmm0
    movss %xmm0, report(%rip)

    ldmxcsr mxcsr_round_up(%rip)
    movupd cvt_packed_d_halfway(%rip), %xmm0
    cvtpd2ps %xmm0, %xmm0
    movss %xmm0, report+4(%rip)

#elif TEST_CASE == 63
    /* CVTSI2SS must follow MXCSR.RC for a large integer source. */
    ldmxcsr mxcsr_round_nearest(%rip)
    movl $0x01000001, %eax
    cvtsi2ss %eax, %xmm0
    movss %xmm0, report(%rip)

    ldmxcsr mxcsr_round_up(%rip)
    movl $0x01000001, %eax
    cvtsi2ss %eax, %xmm0
    movss %xmm0, report+4(%rip)

#elif TEST_CASE == 64
    /* VCVTPD2PS must follow MXCSR.RC when narrowing double to float. */
    ldmxcsr mxcsr_round_nearest(%rip)
    vmovupd cvt_packed_d_halfway(%rip), %xmm0
    vcvtpd2ps %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

    ldmxcsr mxcsr_round_up(%rip)
    vmovupd cvt_packed_d_halfway(%rip), %xmm0
    vcvtpd2ps %xmm0, %xmm0
    vmovss %xmm0, report+4(%rip)

#elif TEST_CASE == 65
    /* VCVTDQ2PS must follow MXCSR.RC for an exactly halfway integer. */
    ldmxcsr mxcsr_round_nearest(%rip)
    vmovups cvt_dq_input(%rip), %xmm0
    vcvtdq2ps %xmm0, %xmm0
    vmovss %xmm0, report(%rip)

    ldmxcsr mxcsr_round_up(%rip)
    vmovups cvt_dq_input(%rip), %xmm0
    vcvtdq2ps %xmm0, %xmm0
    vmovss %xmm0, report+4(%rip)

#elif TEST_CASE == 66
    /* VCVTPS2DQ must follow MXCSR.RC for a non-integral source. */
    ldmxcsr mxcsr_round_nearest(%rip)
    vmovups cvt_packed_ss_one_point_five(%rip), %xmm0
    vcvtps2dq %xmm0, %xmm0
    vmovd %xmm0, %eax
    movl %eax, report(%rip)

    ldmxcsr mxcsr_round_down(%rip)
    vmovups cvt_packed_ss_one_point_five(%rip), %xmm0
    vcvtps2dq %xmm0, %xmm0
    vmovd %xmm0, %eax
    movl %eax, report+4(%rip)

#elif TEST_CASE == 67
    /* VCVTPD2DQ must follow MXCSR.RC for a non-integral source. */
    ldmxcsr mxcsr_round_nearest(%rip)
    vmovupd cvt_packed_d_one_point_five(%rip), %xmm0
    vcvtpd2dq %xmm0, %xmm0
    vmovd %xmm0, %eax
    movl %eax, report(%rip)

    ldmxcsr mxcsr_round_down(%rip)
    vmovupd cvt_packed_d_one_point_five(%rip), %xmm0
    vcvtpd2dq %xmm0, %xmm0
    vmovd %xmm0, %eax
    movl %eax, report+4(%rip)

#elif TEST_CASE == 68
    /* 64-bit VCVTSI2SD must follow MXCSR.RC when the integer is inexact. */
    ldmxcsr mxcsr_round_nearest(%rip)
    movq cvt_si2sd_input(%rip), %rax
    vcvtsi2sd %rax, %xmm0, %xmm0
    vmovsd %xmm0, report(%rip)

    ldmxcsr mxcsr_round_up(%rip)
    movq cvt_si2sd_input(%rip), %rax
    vcvtsi2sd %rax, %xmm0, %xmm0
    vmovsd %xmm0, report+8(%rip)

#elif TEST_CASE == 69 || TEST_CASE == 70 || TEST_CASE == 71
    /*
     * Prime the TB-local SSE RM cache before crossing the helper boundary.
     * With LATX_ROUNDING_OPT=1, FNSAVE/FSAVE reset host RM to nearest;
     * FRSTOR loads round-down.  Neither may change subsequent SSE round-up.
     * Keep the measured sequence straight-line to exercise the same TB.
     */
#if TEST_CASE == 71
    /* Create a valid empty x87 image before priming the SSE cache. */
    fnsave frstor_area(%rip)
    movw $0x077f, frstor_area(%rip)
#endif
    ldmxcsr mxcsr_round_up(%rip)
    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0

#if TEST_CASE == 69
    fnsave frstor_area(%rip)
#elif TEST_CASE == 70
    /* FSAVE encodes FWAIT followed by FNSAVE. */
    fsave frstor_area(%rip)
#else
    frstor frstor_area(%rip)
#endif

    movss one(%rip), %xmm0
    addss half_ulp(%rip), %xmm0
    movss %xmm0, report(%rip)

#else
#error unsupported TEST_CASE
#endif

    /* Cases 9, 12, 13, 16, 17, 20, 21, 24, 25, 28, 29, 31, 33, 35, 38, 39, 41, 43, 45, 47, 49, 51, 53, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, and 67 report 8 bytes; case 68 reports 16 bytes; all other cases report 4. */
    movl $__NR_write, %eax
    movl $1, %edi
    leaq report(%rip), %rsi
#if TEST_CASE == 9 || TEST_CASE == 12 || TEST_CASE == 13 || \
    TEST_CASE == 16 || TEST_CASE == 17 || TEST_CASE == 20 || TEST_CASE == 21 || \
    TEST_CASE == 24 || TEST_CASE == 25 || TEST_CASE == 28 || TEST_CASE == 29 || \
    TEST_CASE == 31 || TEST_CASE == 33 || TEST_CASE == 35 || \
    TEST_CASE == 38 || TEST_CASE == 39 || TEST_CASE == 41 || \
    TEST_CASE == 43 || TEST_CASE == 45 || TEST_CASE == 47 || \
    TEST_CASE == 49 || TEST_CASE == 51 || TEST_CASE == 53 || TEST_CASE == 55 || \
    TEST_CASE == 56 || TEST_CASE == 57 || TEST_CASE == 58 || TEST_CASE == 59 || \
    TEST_CASE == 60 || TEST_CASE == 61 || TEST_CASE == 62 || TEST_CASE == 63 || \
    TEST_CASE == 64 || TEST_CASE == 65 || TEST_CASE == 66 || TEST_CASE == 67
    movl $8, %edx
#elif TEST_CASE == 68
    movl $16, %edx
#else
    movl $4, %edx
#endif
    syscall

    xorl %edi, %edi
    movl $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .rodata
.align 2
x87_round_up:
    .word 0x0b7f
x87_round_down:
    .word 0x077f
mxcsr_round_up:
    .long 0x00005f80
mxcsr_round_nearest:
    .long 0x00001f80
mxcsr_round_down:
    .long 0x00003f80
one:
    .long 0x3f800000
negative_one_s:
    .long 0xbf800000
one_point25:
    .long 0x3fa00000
next_one:
    .long 0x3f800001
cvt_dq_input:
    .long 0x01000001, 0x00000000, 0x00000000, 0x00000000
cvt_sd_halfway:
    .quad 0x3ff0000010000000
cvt_ss_one_point_five:
    .long 0x3fc00000
cvt_ss_one_point_seven_five:
    .long 0x3fe00000
cvt_packed_ss_one_point_five:
    .long 0x3fc00000, 0x00000000, 0x00000000, 0x00000000
cvt_packed_d_one_point_five:
    .quad 0x3ff8000000000000, 0x0000000000000000
cvt_packed_d_halfway:
    .quad 0x3ff0000010000000, 0x0000000000000000
cvt_si2sd_input:
    .quad 0x0020000000000001
half_ulp:
    .long 0x33800000
neg_half_ulp:
    .long 0xb3800000
packed_one:
    .long 0x3f800000, 0x00000000, 0x3f800000, 0x00000000
packed_half:
    .long 0x33800000, 0x00000000, 0x00000000, 0x00000000
one_d:
    .quad 0x3ff0000000000000
negative_one_d:
    .quad 0xbff0000000000000
one_point25_d:
    .quad 0x3ff4000000000000
next_one_d:
    .quad 0x3ff0000000000001
half_ulp_d:
    .quad 0x3ca0000000000000
neg_half_ulp_d:
    .quad 0xbca0000000000000
packed_one_d:
    .quad 0x3ff0000000000000, 0x3ff0000000000000
packed_half_d:
    .quad 0x3ca0000000000000, 0x0000000000000000
packed_neg_half:
    .long 0xb3800000, 0x00000000, 0x00000000, 0x00000000
packed_neg_half_d:
    .quad 0xbca0000000000000, 0x0000000000000000
packed_one_point25:
    .long 0x3fa00000, 0x00000000, 0x3fa00000, 0x00000000
packed_next_one:
    .long 0x3f800001, 0x3f800000, 0x3f800000, 0x3f800000
packed_one_point25_d:
    .quad 0x3ff4000000000000, 0x3ff4000000000000
packed_next_one_d:
    .quad 0x3ff0000000000001, 0x3ff0000000000000
div_float_num:
    .long 0x3d800000
div_float_den:
    .long 0x3fc80000
packed_div_float_num:
    .long 0x3d800000, 0x3d800000, 0x3d800000, 0x3d800000
packed_div_float_den:
    .long 0x3fc80000, 0x3fc80000, 0x3fc80000, 0x3fc80000
div_double_num:
    .quad 0x3fb0000000000000
div_double_den:
    .quad 0x3fc8000000000000
packed_div_double_num:
    .quad 0x3fb0000000000000, 0x3fb0000000000000
packed_div_double_den:
    .quad 0x3fc8000000000000, 0x3fc8000000000000
sqrt_two:
    .long 0x40000000
packed_sqrt_two:
    .long 0x40000000, 0x40000000, 0x40000000, 0x40000000
sqrt_three_d:
    .quad 0x4008000000000000
packed_sqrt_three_d:
    .quad 0x4008000000000000, 0x4008000000000000
hadd_src1_s:
    .long 0x3f800000, 0x33800000, 0x00000000, 0x00000000
hsub_src1_s:
    .long 0x3f800000, 0xb3800000, 0x00000000, 0x00000000
packed_zero_s:
    .long 0x00000000, 0x00000000, 0x00000000, 0x00000000
hadd_src1_d:
    .quad 0x3ff0000000000000, 0x3ca0000000000000
hsub_src1_d:
    .quad 0x3ff0000000000000, 0xbca0000000000000
packed_zero_d:
    .quad 0x0000000000000000, 0x0000000000000000
dpps_src1_s:
    .long 0x3f800000, 0x3f800000, 0x00000000, 0x00000000
dpps_src2_s:
    .long 0x3f800000, 0x33800000, 0x00000000, 0x00000000
dppd_src1_d:
    .quad 0x3ff0000000000000, 0x3ff0000000000000
dppd_src2_d:
    .quad 0x3ff0000000000000, 0x3ca0000000000000
fldenv_area:
    .long 0x0000077f
    .long 0x00000000
    .long 0x0000ffff
    .zero 16

.section .bss
.align 2
report:
    .zero 8
.p2align 4
frstor_area:
    .zero 108
.p2align 6
xsave_area:
    .zero 1024
