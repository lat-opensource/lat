/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The address and lazy flags are decoded by the common TB restore code. */
#ifdef CONFIG_LATX_WOW64_PE
typedef LatWow64PeFaultContext LatxFaultContext;
#define UC_GR(uc) ((uc)->gpr)

static void restore_extcontext(CPUX86State *env, const LatxFaultContext *uc)
{
    if (!option_softfpu && env->mode_fpu) {
        env->fpstt = uc->ftop & 7;
    }
    env->fcsr = uc->fcsr;
    for (int i = 0; i < 8; i++) {
        env->fpregs[i].mmx.MMX_Q(0) = uc->vector[reg_fpr_map[i]][0];
    }
    for (int i = 0; i < CPU_NB_REGS; i++) {
        const uint64_t *src = uc->vector[reg_xmm_map[i]];
        env->xmm_regs[i].ZMM_Q(0) = src[0];
        env->xmm_regs[i].ZMM_Q(1) = src[1];
        env->xmm_regs[i].ZMM_Q(2) = option_enable_lasx ? src[2] : env->ymmh_regs[i]._q[0];
        env->xmm_regs[i].ZMM_Q(3) = option_enable_lasx ? src[3] : env->ymmh_regs[i]._q[1];
    }
    env->eflags = uc->eflags | UC_GR(uc)[reg_statics_map[S_EFLAGS]];
}
#else
typedef ucontext_t LatxFaultContext;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
static void restore_extcontext(CPUX86State *env, ucontext_t *uc)
{
    int i, nb_xmm_regs;
#ifdef TARGET_X86_64
    nb_xmm_regs = 16;
#else
    nb_xmm_regs = 8;
#endif

    /* save eflag */
#ifdef __loongarch__
    unsigned int *eflag_p = &uc->uc_mcontext.__reserved;
    /*
     * WARNING: the offset of __fregs in uc->uc_mcontext is incorrent,
     * we fix the issue by adding a extra offset, the solution is only
     * temporary!
     */
    eflag_p = eflag_p + 8;
    env->eflags = *eflag_p;
    env->eflags |= uc->uc_mcontext.__gregs[reg_statics_map[S_EFLAGS]];

    /* save fcsr */
    env->fcsr = UC_FCSR(uc);
#endif

    /* save fpr */
    for (i = 0; i < 8; i++) {
        env->fpregs[i].mmx.MMX_Q(0) = UC_FREG(uc)[reg_fpr_map[i] + 1].__val64[0];
    }

    /* save sse */
    for (i = 0; i < nb_xmm_regs; i++) {
        env->xmm_regs[i].ZMM_Q(0) = UC_FREG(uc)[reg_xmm_map[i] + 1].__val64[0];
        env->xmm_regs[i].ZMM_Q(1) = UC_FREG(uc)[reg_xmm_map[i] + 1].__val64[1];
        env->xmm_regs[i].ZMM_Q(2) = UC_FREG(uc)[reg_xmm_map[i] + 1].__val64[2];
        env->xmm_regs[i].ZMM_Q(3) = UC_FREG(uc)[reg_xmm_map[i] + 1].__val64[3];
    }
}
#else /* CONFIG_LOONGARCH_NEW_WORLD */
static void restore_extcontext(CPUX86State *env, ucontext_t *uc)
{
    int i, nb_xmm_regs;
#ifdef TARGET_X86_64
    nb_xmm_regs = 16;
#else
    nb_xmm_regs = 8;
#endif

    struct extctx_layout extctx;
    memset(&extctx, 0, sizeof(extctx));
    parse_extcontext(uc, &extctx);

#ifdef CONFIG_LATX_WOW64
    if (lat_wow64_provider_plugin_active && !option_softfpu && env->mode_fpu) {
        env->fpstt = UC_GET_FTOP(&extctx, uint32_t) & 7;
    }
#endif
    /* save fcsr */
    env->fcsr = UC_GET_FCSR(&extctx, int);

    /* save fpr */
    for (i = 0; i < 8; i++) {
      env->fpregs[i].mmx.MMX_Q(0) =
          UC_GET_FPR(&extctx, reg_fpr_map[i], uint64_t);
    }

    /* save sse */
    for (i = 0; i < nb_xmm_regs; i++) {
      env->xmm_regs[i].ZMM_Q(0) =
          UC_GET_LSX(&extctx, reg_xmm_map[i], 0, uint64_t);
      env->xmm_regs[i].ZMM_Q(1) =
          UC_GET_LSX(&extctx, reg_xmm_map[i], 1, uint64_t);
#ifdef CONFIG_LATX_AVX_OPT
      if (!option_enable_lasx) {
        env->xmm_regs[i].ZMM_Q(2) = env->ymmh_regs[i]._q[0];
        env->xmm_regs[i].ZMM_Q(3) = env->ymmh_regs[i]._q[1];
      } else
#endif
      {
        env->xmm_regs[i].ZMM_Q(2) =
            UC_GET_LSX(&extctx, reg_xmm_map[i], 2, uint64_t);
        env->xmm_regs[i].ZMM_Q(3) =
            UC_GET_LSX(&extctx, reg_xmm_map[i], 3, uint64_t);
      }
    }
    /* eflags */
    env->eflags = UC_GET_EFLAGS(&extctx, uint32_t);
    env->eflags |= UC_GR(uc)[reg_statics_map[S_EFLAGS]];
}
#endif

#endif

void restore_state_to_opc(CPUX86State *env, TranslationBlock *tb,
                          target_ulong *data)
{
    int cc_op = data[1] & 0xffff;
    uint16_t ymmh_zero_pending = data[1] >> 16;

    /* env->eip = data[0] - tb->cs_base; */
    env->eip = data[0];
    if (cc_op != CC_OP_DYNAMIC) {
        env->cc_op = cc_op;
    }

    LatxFaultContext *uc = env->puc;
    if (!uc)
        return;

    /*
     * Update gpr to env 15, 18, 19, 26, 27, 28, 29, 30
     * TODO: update fp if any
     */
    env->regs[R_EAX] = UC_GR(uc)[reg_gpr_map[eax_index]];
    env->regs[R_EBX] = UC_GR(uc)[reg_gpr_map[ebx_index]];
    env->regs[R_ECX] = UC_GR(uc)[reg_gpr_map[ecx_index]];
    env->regs[R_EDX] = UC_GR(uc)[reg_gpr_map[edx_index]];
    env->regs[R_ESP] = UC_GR(uc)[reg_gpr_map[esp_index]];
    env->regs[R_EBP] = UC_GR(uc)[reg_gpr_map[ebp_index]];
    env->regs[R_ESI] = UC_GR(uc)[reg_gpr_map[esi_index]];
    env->regs[R_EDI] = UC_GR(uc)[reg_gpr_map[edi_index]];
#ifdef TARGET_X86_64
    env->regs[R_R8]  = UC_GR(uc)[reg_gpr_map[r8_index]];
    env->regs[R_R9]  = UC_GR(uc)[reg_gpr_map[r9_index]];
    env->regs[R_R10] = UC_GR(uc)[reg_gpr_map[r10_index]];
    env->regs[R_R11] = UC_GR(uc)[reg_gpr_map[r11_index]];
    env->regs[R_R12] = UC_GR(uc)[reg_gpr_map[r12_index]];
    env->regs[R_R13] = UC_GR(uc)[reg_gpr_map[r13_index]];
    env->regs[R_R14] = UC_GR(uc)[reg_gpr_map[r14_index]];
    env->regs[R_R15] = UC_GR(uc)[reg_gpr_map[r15_index]];
#endif
    restore_extcontext(env, uc);
    /* The saved host registers can still contain stale upper halves for
     * completed VEX.128 instructions whose clears were deferred. */
    if (option_enable_lasx) {
        for (int i = 0; i < CPU_NB_REGS; ++i) {
            if (ymmh_zero_pending & (UINT16_C(1) << i)) {
                env->xmm_regs[i].ZMM_Q(2) = 0;
                env->xmm_regs[i].ZMM_Q(3) = 0;
            }
        }
    }
}
