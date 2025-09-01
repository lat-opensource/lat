#ifndef _TRANSLATE_BD_H_
#define _TRANSLATE_BD_H_

#include "translate.h"
#include "common.h"
#include "env.h"
#include "ir1-bd.h"
#include "ir2.h"
#include "la-append.h"
#include "ir2-relocate.h"
#include "macro-inst.h"

#include "aot.h"

//#define LATX_DEBUG_SOFTFPU

#define TRANS_FUNC_BD(name) glue(glue(translate_, name), _bd)
#define TRANS_FUNC_DEF_BD(name) \
bool TRANS_FUNC_BD(name)(IR1_INST * pir1)
#define TRANS_FUNC_GEN_REAL_BD(opcode, function) \
[glue(ND_INS_, opcode)] = function
#define TRANS_FUNC_GEN_BD(opcode, function) \
TRANS_FUNC_GEN_REAL_BD(opcode, TRANS_FUNC_BD(function))

#define TRANS_FPU_WRAP_GEN_NO_PROLOGUE_BD(function)    \
bool translate_##function##_wrap_bd(IR1_INST *pir1)    \
{                                                   \
    if (option_softfpu) {                           \
        return translate_##function##_softfpu_bd(pir1);\
    }                                               \
    return translate_##function##_bd(pir1);              \
}

#define TRANS_FPU_WRAP_GEN_BD(function)                \
bool translate_##function##_wrap_bd(IR1_INST *pir1)    \
{                                                   \
    if (option_softfpu == 2) {                      \
        return translate_##function##_softfpu_bd(pir1);\
    } else if (option_softfpu == 1) {               \
        bool ret;                                   \
        gen_softfpu_helper_prologue_bd(pir1);          \
        ret = translate_##function##_softfpu_bd(pir1); \
        gen_softfpu_helper_epilogue_bd(pir1);          \
        return ret;                                 \
                                                    \
    }                                               \
    return translate_##function##_bd(pir1);              \
}

#ifdef CONFIG_LATX_IMM_REG
void save_imm_cache(void);
void restore_imm_cache(void);
#else
#define save_imm_cache()
#define restore_imm_cache()
#endif

#ifdef LATX_DEBUG_SOFTFPU
#define TRANS_FPU_WRAP_GEN_DEBUG(function)          \
bool translate_##function##_wrap_bd(IR1_INST *pir1)    \
{                                                   \
    if (0) {                                        \
        return translate_##function##_softfpu_bd(pir1);\
    }                                               \
    return translate_##function_bd(pir1);              \
}
#endif

#include "insts-pattern.h"

TRANS_FUNC_DEF_BD(add);
TRANS_FUNC_DEF_BD(lock_add);
TRANS_FUNC_DEF_BD(push);
TRANS_FUNC_DEF_BD(pop);
TRANS_FUNC_DEF_BD(or);
TRANS_FUNC_DEF_BD(lock_or);
TRANS_FUNC_DEF_BD(adc);
TRANS_FUNC_DEF_BD(lock_adc);
TRANS_FUNC_DEF_BD(sbb);
TRANS_FUNC_DEF_BD(lock_sbb);
TRANS_FUNC_DEF_BD(and);
TRANS_FUNC_DEF_BD(lock_and);
TRANS_FUNC_DEF_BD(daa);
TRANS_FUNC_DEF_BD(sub);
TRANS_FUNC_DEF_BD(lock_sub);
TRANS_FUNC_DEF_BD(das);
TRANS_FUNC_DEF_BD(xor);
TRANS_FUNC_DEF_BD(lock_xor);
TRANS_FUNC_DEF_BD(aaa);
TRANS_FUNC_DEF_BD(cmp);
TRANS_FUNC_DEF_BD(aas);
TRANS_FUNC_DEF_BD(inc);
TRANS_FUNC_DEF_BD(lock_inc);
TRANS_FUNC_DEF_BD(dec);
TRANS_FUNC_DEF_BD(lock_dec);
TRANS_FUNC_DEF_BD(pushaw);
TRANS_FUNC_DEF_BD(pushal);
TRANS_FUNC_DEF_BD(popaw);
TRANS_FUNC_DEF_BD(popal);
TRANS_FUNC_DEF_BD(popcnt);
TRANS_FUNC_DEF_BD(imul);
TRANS_FUNC_DEF_BD(ins);
// TRANS_FUNC_DEF_BD(outs);
TRANS_FUNC_DEF_BD(jcc);
TRANS_FUNC_DEF_BD(test);
TRANS_FUNC_DEF_BD(xchg);
TRANS_FUNC_DEF_BD(mov);
TRANS_FUNC_DEF_BD(lea);
TRANS_FUNC_DEF_BD(cbw);
TRANS_FUNC_DEF_BD(cwde);
TRANS_FUNC_DEF_BD(cdqe);
TRANS_FUNC_DEF_BD(cwd);
TRANS_FUNC_DEF_BD(cdq);
TRANS_FUNC_DEF_BD(cqo);
// TRANS_FUNC_DEF_BD(call_far);
TRANS_FUNC_DEF_BD(pushf);
TRANS_FUNC_DEF_BD(popf);
TRANS_FUNC_DEF_BD(sahf);
TRANS_FUNC_DEF_BD(lahf);
TRANS_FUNC_DEF_BD(movs);
TRANS_FUNC_DEF_BD(cmps);
TRANS_FUNC_DEF_BD(stos);
TRANS_FUNC_DEF_BD(lods);
TRANS_FUNC_DEF_BD(scas);
TRANS_FUNC_DEF_BD(ret);
TRANS_FUNC_DEF_BD(retf);
TRANS_FUNC_DEF_BD(enter);
TRANS_FUNC_DEF_BD(leave);
TRANS_FUNC_DEF_BD(int_3);
TRANS_FUNC_DEF_BD(int);
TRANS_FUNC_DEF_BD(iret);
TRANS_FUNC_DEF_BD(iretq);
TRANS_FUNC_DEF_BD(aam);
TRANS_FUNC_DEF_BD(aad);
TRANS_FUNC_DEF_BD(xlat);
TRANS_FUNC_DEF_BD(loopnz);
TRANS_FUNC_DEF_BD(loopz);
TRANS_FUNC_DEF_BD(loop);
TRANS_FUNC_DEF_BD(jrcxz);
TRANS_FUNC_DEF_BD(in);
TRANS_FUNC_DEF_BD(out);
TRANS_FUNC_DEF_BD(call);
TRANS_FUNC_DEF_BD(jmp);
TRANS_FUNC_DEF_BD(ljmp);
TRANS_FUNC_DEF_BD(hlt);
TRANS_FUNC_DEF_BD(cmc);
TRANS_FUNC_DEF_BD(clc);
TRANS_FUNC_DEF_BD(stc);
TRANS_FUNC_DEF_BD(cld);
TRANS_FUNC_DEF_BD(std);
TRANS_FUNC_DEF_BD(syscall);
TRANS_FUNC_DEF_BD(ud2);
TRANS_FUNC_DEF_BD(nop);
TRANS_FUNC_DEF_BD(endbr);

TRANS_FUNC_DEF_BD(rdtsc);
TRANS_FUNC_DEF_BD(cmovcc);
TRANS_FUNC_DEF_BD(setcc);
TRANS_FUNC_DEF_BD(cpuid);
TRANS_FUNC_DEF_BD(btx_llsc);
TRANS_FUNC_DEF_BD(btx);
TRANS_FUNC_DEF_BD(blsr);
TRANS_FUNC_DEF_BD(shld);
TRANS_FUNC_DEF_BD(shrd);
TRANS_FUNC_DEF_BD(sarx);
TRANS_FUNC_DEF_BD(shlx);
TRANS_FUNC_DEF_BD(shrx);
TRANS_FUNC_DEF_BD(cmpxchg);
TRANS_FUNC_DEF_BD(lock_cmpxchg);
TRANS_FUNC_DEF_BD(movzx);
TRANS_FUNC_DEF_BD(tzcnt);
TRANS_FUNC_DEF_BD(bsf);
TRANS_FUNC_DEF_BD(movsx);
TRANS_FUNC_DEF_BD(xadd);
TRANS_FUNC_DEF_BD(lock_xadd);
TRANS_FUNC_DEF_BD(movnti);
TRANS_FUNC_DEF_BD(bswap);
TRANS_FUNC_DEF_BD(rol);
TRANS_FUNC_DEF_BD(ror);
TRANS_FUNC_DEF_BD(rcl);
TRANS_FUNC_DEF_BD(rcr);
TRANS_FUNC_DEF_BD(shl);
TRANS_FUNC_DEF_BD(shr);
TRANS_FUNC_DEF_BD(sal);
TRANS_FUNC_DEF_BD(sar);
TRANS_FUNC_DEF_BD(not);
TRANS_FUNC_DEF_BD(lock_not);
TRANS_FUNC_DEF_BD(neg);
TRANS_FUNC_DEF_BD(lock_neg);
TRANS_FUNC_DEF_BD(mul);
TRANS_FUNC_DEF_BD(mulx);
TRANS_FUNC_DEF_BD(div);
TRANS_FUNC_DEF_BD(idiv);
TRANS_FUNC_DEF_BD(rdtscp);
TRANS_FUNC_DEF_BD(prefetch);
TRANS_FUNC_DEF_BD(prefetchw);
TRANS_FUNC_DEF_BD(movups);
TRANS_FUNC_DEF_BD(movupd);
TRANS_FUNC_DEF_BD(movss);
TRANS_FUNC_DEF_BD(movsd);
TRANS_FUNC_DEF_BD(movhlps);
TRANS_FUNC_DEF_BD(movlps);
TRANS_FUNC_DEF_BD(movlpd);
TRANS_FUNC_DEF_BD(movsldup);
TRANS_FUNC_DEF_BD(movddup);
TRANS_FUNC_DEF_BD(unpcklps);
TRANS_FUNC_DEF_BD(unpcklpd);
TRANS_FUNC_DEF_BD(unpckhps);
TRANS_FUNC_DEF_BD(unpckhpd);
TRANS_FUNC_DEF_BD(movlhps);
TRANS_FUNC_DEF_BD(movhps);
TRANS_FUNC_DEF_BD(movhpd);
TRANS_FUNC_DEF_BD(movshdup);
TRANS_FUNC_DEF_BD(prefetchnta);
TRANS_FUNC_DEF_BD(prefetcht0);
TRANS_FUNC_DEF_BD(prefetcht1);
TRANS_FUNC_DEF_BD(prefetcht2);
TRANS_FUNC_DEF_BD(movaps);
TRANS_FUNC_DEF_BD(movapd);
TRANS_FUNC_DEF_BD(cvtpi2ps);
TRANS_FUNC_DEF_BD(cvtpi2pd);
TRANS_FUNC_DEF_BD(cvtsi2ss);
TRANS_FUNC_DEF_BD(cvtsi2sd);
TRANS_FUNC_DEF_BD(movntps);
TRANS_FUNC_DEF_BD(movntpd);
TRANS_FUNC_DEF_BD(cvttsx2si);
TRANS_FUNC_DEF_BD(cvtps2pi);
TRANS_FUNC_DEF_BD(cvttps2pi);
TRANS_FUNC_DEF_BD(cvtpd2pi);
TRANS_FUNC_DEF_BD(cvtsx2si);
TRANS_FUNC_DEF_BD(cvttpd2pi);
#ifdef CONFIG_LATX_XCOMISX_OPT
TRANS_FUNC_DEF_BD(xcomisx);
#endif
TRANS_FUNC_DEF_BD(ucomiss);
TRANS_FUNC_DEF_BD(ucomisd);
TRANS_FUNC_DEF_BD(comiss);
TRANS_FUNC_DEF_BD(comisd);
TRANS_FUNC_DEF_BD(movmskps);
TRANS_FUNC_DEF_BD(movmskpd);
TRANS_FUNC_DEF_BD(sqrtps);
TRANS_FUNC_DEF_BD(sqrtpd);
TRANS_FUNC_DEF_BD(sqrtss);
TRANS_FUNC_DEF_BD(sqrtsd);
TRANS_FUNC_DEF_BD(rsqrtps);
TRANS_FUNC_DEF_BD(rsqrtss);
TRANS_FUNC_DEF_BD(rcpps);
TRANS_FUNC_DEF_BD(rcpss);
TRANS_FUNC_DEF_BD(andps);
TRANS_FUNC_DEF_BD(andpd);
TRANS_FUNC_DEF_BD(andnps);
TRANS_FUNC_DEF_BD(andnpd);
TRANS_FUNC_DEF_BD(orps);
TRANS_FUNC_DEF_BD(orpd);
TRANS_FUNC_DEF_BD(xorps);
TRANS_FUNC_DEF_BD(xorpd);
TRANS_FUNC_DEF_BD(addps);
TRANS_FUNC_DEF_BD(addpd);
TRANS_FUNC_DEF_BD(addss);
TRANS_FUNC_DEF_BD(addsd);
TRANS_FUNC_DEF_BD(mulps);
TRANS_FUNC_DEF_BD(mulpd);
TRANS_FUNC_DEF_BD(mulss);
TRANS_FUNC_DEF_BD(mulsd);
TRANS_FUNC_DEF_BD(cvtps2pd);
TRANS_FUNC_DEF_BD(cvtpd2ps);
TRANS_FUNC_DEF_BD(cvtss2sd);
TRANS_FUNC_DEF_BD(cvtsd2ss);
TRANS_FUNC_DEF_BD(cvtdq2ps);
TRANS_FUNC_DEF_BD(cvtps2dq);
TRANS_FUNC_DEF_BD(cvtpd2dq);
TRANS_FUNC_DEF_BD(cvttpx2dq);
TRANS_FUNC_DEF_BD(subps);
TRANS_FUNC_DEF_BD(subpd);
TRANS_FUNC_DEF_BD(subss);
TRANS_FUNC_DEF_BD(subsd);
TRANS_FUNC_DEF_BD(minps);
TRANS_FUNC_DEF_BD(minpd);
TRANS_FUNC_DEF_BD(minss);
TRANS_FUNC_DEF_BD(minsd);
TRANS_FUNC_DEF_BD(divps);
TRANS_FUNC_DEF_BD(divpd);
TRANS_FUNC_DEF_BD(divss);
TRANS_FUNC_DEF_BD(divsd);
TRANS_FUNC_DEF_BD(maxps);
TRANS_FUNC_DEF_BD(maxpd);
TRANS_FUNC_DEF_BD(maxss);
TRANS_FUNC_DEF_BD(maxsd);
TRANS_FUNC_DEF_BD(punpcklbw);
TRANS_FUNC_DEF_BD(punpcklwd);
TRANS_FUNC_DEF_BD(punpckldq);
TRANS_FUNC_DEF_BD(packsswb);
TRANS_FUNC_DEF_BD(pcmpgtb);
TRANS_FUNC_DEF_BD(pcmpgtw);
TRANS_FUNC_DEF_BD(pcmpgtd);
TRANS_FUNC_DEF_BD(pcmpgtq);
TRANS_FUNC_DEF_BD(packuswb);
TRANS_FUNC_DEF_BD(punpckhbw);
TRANS_FUNC_DEF_BD(punpckhwd);
TRANS_FUNC_DEF_BD(punpckhdq);
TRANS_FUNC_DEF_BD(packssdw);
TRANS_FUNC_DEF_BD(punpcklqdq);
TRANS_FUNC_DEF_BD(punpckhqdq);
TRANS_FUNC_DEF_BD(movd);
TRANS_FUNC_DEF_BD(movq);
TRANS_FUNC_DEF_BD(movdqa);
TRANS_FUNC_DEF_BD(movdqu);
TRANS_FUNC_DEF_BD(pshufw);
TRANS_FUNC_DEF_BD(pshufd);
TRANS_FUNC_DEF_BD(pshufhw);
TRANS_FUNC_DEF_BD(pshuflw);
TRANS_FUNC_DEF_BD(pcmpeqb);
TRANS_FUNC_DEF_BD(pcmpeqw);
TRANS_FUNC_DEF_BD(pcmpeqd);
TRANS_FUNC_DEF_BD(emms);
// TRANS_FUNC_DEF_BD(xave);
TRANS_FUNC_DEF_BD(lfence);
TRANS_FUNC_DEF_BD(mfence);
TRANS_FUNC_DEF_BD(sfence);
TRANS_FUNC_DEF_BD(clflush);
TRANS_FUNC_DEF_BD(clflushopt);
TRANS_FUNC_DEF_BD(bsr);
TRANS_FUNC_DEF_BD(cmpeqps);
TRANS_FUNC_DEF_BD(cmpltps);
TRANS_FUNC_DEF_BD(cmpleps);
TRANS_FUNC_DEF_BD(cmpunordps);
TRANS_FUNC_DEF_BD(cmpneqps);
TRANS_FUNC_DEF_BD(cmpnltps);
TRANS_FUNC_DEF_BD(cmpnleps);
TRANS_FUNC_DEF_BD(cmpordps);
TRANS_FUNC_DEF_BD(cmpeqpd);
TRANS_FUNC_DEF_BD(cmpltpd);
TRANS_FUNC_DEF_BD(cmplepd);
TRANS_FUNC_DEF_BD(cmpunordpd);
TRANS_FUNC_DEF_BD(cmpneqpd);
TRANS_FUNC_DEF_BD(cmpnltpd);
TRANS_FUNC_DEF_BD(cmpnlepd);
TRANS_FUNC_DEF_BD(cmpordpd);
TRANS_FUNC_DEF_BD(cmpeqss);
TRANS_FUNC_DEF_BD(cmpltss);
TRANS_FUNC_DEF_BD(cmpless);
TRANS_FUNC_DEF_BD(cmpunordss);
TRANS_FUNC_DEF_BD(cmpneqss);
TRANS_FUNC_DEF_BD(cmpnltss);
TRANS_FUNC_DEF_BD(cmpnless);
TRANS_FUNC_DEF_BD(cmpordss);
TRANS_FUNC_DEF_BD(cmpeqsd);
TRANS_FUNC_DEF_BD(cmpltsd);
TRANS_FUNC_DEF_BD(cmplesd);
TRANS_FUNC_DEF_BD(cmpunordsd);
TRANS_FUNC_DEF_BD(cmpneqsd);
TRANS_FUNC_DEF_BD(cmpnltsd);
TRANS_FUNC_DEF_BD(cmpnlesd);
TRANS_FUNC_DEF_BD(cmpordsd);
TRANS_FUNC_DEF_BD(cmppd);
TRANS_FUNC_DEF_BD(cmpps);
TRANS_FUNC_DEF_BD(cmpsd);
TRANS_FUNC_DEF_BD(cmpss);
TRANS_FUNC_DEF_BD(pinsrw);
TRANS_FUNC_DEF_BD(pextrw);
TRANS_FUNC_DEF_BD(shufps);
TRANS_FUNC_DEF_BD(shufpd);
TRANS_FUNC_DEF_BD(cmpxchg8b);
TRANS_FUNC_DEF_BD(cmpxchg16b);
TRANS_FUNC_DEF_BD(addsubpd);
TRANS_FUNC_DEF_BD(addsubps);
TRANS_FUNC_DEF_BD(psrlw);
TRANS_FUNC_DEF_BD(psrld);
TRANS_FUNC_DEF_BD(psrlq);
TRANS_FUNC_DEF_BD(paddq);
TRANS_FUNC_DEF_BD(pmullw);
TRANS_FUNC_DEF_BD(movq2dq);
TRANS_FUNC_DEF_BD(movdq2q);
TRANS_FUNC_DEF_BD(pmovmskb);
TRANS_FUNC_DEF_BD(psubusb);
TRANS_FUNC_DEF_BD(psubusw);
TRANS_FUNC_DEF_BD(pminub);
TRANS_FUNC_DEF_BD(pand);
TRANS_FUNC_DEF_BD(paddusb);
TRANS_FUNC_DEF_BD(paddusw);
TRANS_FUNC_DEF_BD(pmaxub);
TRANS_FUNC_DEF_BD(pandn);
TRANS_FUNC_DEF_BD(pavgb);
TRANS_FUNC_DEF_BD(psraw);
TRANS_FUNC_DEF_BD(psrad);
TRANS_FUNC_DEF_BD(pavgw);
TRANS_FUNC_DEF_BD(pmulhuw);
TRANS_FUNC_DEF_BD(pmulhw);
TRANS_FUNC_DEF_BD(cvtdq2pd);
TRANS_FUNC_DEF_BD(movntq);
TRANS_FUNC_DEF_BD(movntdq);
TRANS_FUNC_DEF_BD(psubsb);
TRANS_FUNC_DEF_BD(psubsw);
TRANS_FUNC_DEF_BD(pminsw);
TRANS_FUNC_DEF_BD(por);
TRANS_FUNC_DEF_BD(paddsb);
TRANS_FUNC_DEF_BD(paddsw);
TRANS_FUNC_DEF_BD(pmaxsw);
TRANS_FUNC_DEF_BD(pxor);
TRANS_FUNC_DEF_BD(lddqu);
TRANS_FUNC_DEF_BD(psllw);
TRANS_FUNC_DEF_BD(pslld);
TRANS_FUNC_DEF_BD(psllq);
TRANS_FUNC_DEF_BD(pmuludq);
TRANS_FUNC_DEF_BD(pmaddwd);
TRANS_FUNC_DEF_BD(psadbw);
TRANS_FUNC_DEF_BD(maskmovq);
TRANS_FUNC_DEF_BD(maskmovdqu);
TRANS_FUNC_DEF_BD(psubb);
TRANS_FUNC_DEF_BD(psubw);
TRANS_FUNC_DEF_BD(psubd);
TRANS_FUNC_DEF_BD(psubq);
TRANS_FUNC_DEF_BD(paddb);
TRANS_FUNC_DEF_BD(paddw);
TRANS_FUNC_DEF_BD(paddd);
TRANS_FUNC_DEF_BD(psrldq);
TRANS_FUNC_DEF_BD(pslldq);
TRANS_FUNC_DEF_BD(ldmxcsr);
TRANS_FUNC_DEF_BD(stmxcsr);
TRANS_FUNC_DEF_BD(movsxd);
TRANS_FUNC_DEF_BD(pause);
TRANS_FUNC_DEF_BD(haddpd);
TRANS_FUNC_DEF_BD(haddps);
TRANS_FUNC_DEF_BD(hsubpd);
TRANS_FUNC_DEF_BD(hsubps);

/* ssse3 */
TRANS_FUNC_DEF_BD(psignb);
TRANS_FUNC_DEF_BD(psignw);
TRANS_FUNC_DEF_BD(psignd);
TRANS_FUNC_DEF_BD(pabsb);
TRANS_FUNC_DEF_BD(pabsw);
TRANS_FUNC_DEF_BD(pabsd);
TRANS_FUNC_DEF_BD(palignr);
TRANS_FUNC_DEF_BD(pshufb);
TRANS_FUNC_DEF_BD(pmulhrsw);
TRANS_FUNC_DEF_BD(pmaddubsw);
TRANS_FUNC_DEF_BD(phsubw);
TRANS_FUNC_DEF_BD(phsubd);
TRANS_FUNC_DEF_BD(phsubsw);
TRANS_FUNC_DEF_BD(phaddw);
TRANS_FUNC_DEF_BD(phaddd);
TRANS_FUNC_DEF_BD(phaddsw);

/* sse 4.1 fp */
TRANS_FUNC_DEF_BD(dpps);
TRANS_FUNC_DEF_BD(dppd);
TRANS_FUNC_DEF_BD(blendps);
TRANS_FUNC_DEF_BD(blendpd);
TRANS_FUNC_DEF_BD(blendvps);
TRANS_FUNC_DEF_BD(blendvpd);
TRANS_FUNC_DEF_BD(roundps);
TRANS_FUNC_DEF_BD(roundss);
TRANS_FUNC_DEF_BD(roundpd);
TRANS_FUNC_DEF_BD(roundsd);
TRANS_FUNC_DEF_BD(insertps);
TRANS_FUNC_DEF_BD(extractps);

/* sse 4.1 int */
TRANS_FUNC_DEF_BD(mpsadbw);
TRANS_FUNC_DEF_BD(phminposuw);
TRANS_FUNC_DEF_BD(pmulld);
TRANS_FUNC_DEF_BD(pmuldq);
TRANS_FUNC_DEF_BD(pblendvb);
TRANS_FUNC_DEF_BD(pblendw);
TRANS_FUNC_DEF_BD(pminsb);
TRANS_FUNC_DEF_BD(pminuw);
TRANS_FUNC_DEF_BD(pminsd);
TRANS_FUNC_DEF_BD(pminud);
TRANS_FUNC_DEF_BD(pmaxsb);
TRANS_FUNC_DEF_BD(pmaxuw);
TRANS_FUNC_DEF_BD(pmaxsd);
TRANS_FUNC_DEF_BD(pmaxud);
TRANS_FUNC_DEF_BD(pinsrb);
TRANS_FUNC_DEF_BD(pinsrd);
TRANS_FUNC_DEF_BD(pinsrq);
TRANS_FUNC_DEF_BD(pextrb);
TRANS_FUNC_DEF_BD(pextrd);
TRANS_FUNC_DEF_BD(pextrq);
TRANS_FUNC_DEF_BD(pmovsxbw);
TRANS_FUNC_DEF_BD(pmovzxbw);
TRANS_FUNC_DEF_BD(pmovsxbd);
TRANS_FUNC_DEF_BD(pmovzxbd);
TRANS_FUNC_DEF_BD(pmovsxbq);
TRANS_FUNC_DEF_BD(pmovzxbq);
TRANS_FUNC_DEF_BD(pmovsxwd);
TRANS_FUNC_DEF_BD(pmovzxwd);
TRANS_FUNC_DEF_BD(pmovsxwq);
TRANS_FUNC_DEF_BD(pmovzxwq);
TRANS_FUNC_DEF_BD(pmovsxdq);
TRANS_FUNC_DEF_BD(pmovzxdq);
TRANS_FUNC_DEF_BD(ptest);
TRANS_FUNC_DEF_BD(pcmpeqq);
TRANS_FUNC_DEF_BD(packusdw);
TRANS_FUNC_DEF_BD(movntdqa);

TRANS_FUNC_DEF_BD(callnext);
TRANS_FUNC_DEF_BD(callthunk);
TRANS_FUNC_DEF_BD(callin);
TRANS_FUNC_DEF_BD(jmpin);
#ifndef CONFIG_LATX_DECODE_DEBUG
bool translate_libfunc(IR1_INST *pir1);
#endif
// /* byhand functions */
// TRANS_FUNC_DEF_BD(add_byhand);
// TRANS_FUNC_DEF_BD(or_byhand);
// TRANS_FUNC_DEF_BD(adc_byhand);
// TRANS_FUNC_DEF_BD(and_byhand);
// TRANS_FUNC_DEF_BD(sub_byhand);
// TRANS_FUNC_DEF_BD(xor_byhand);
// TRANS_FUNC_DEF_BD(cmp_byhand);

// TRANS_FUNC_DEF_BD(neg_byhand);
// TRANS_FUNC_DEF_BD(mov_byhand);
// TRANS_FUNC_DEF_BD(movsx_byhand);
// TRANS_FUNC_DEF_BD(movzx_byhand);
// TRANS_FUNC_DEF_BD(test_byhand);
// TRANS_FUNC_DEF_BD(inc_byhand);
// TRANS_FUNC_DEF_BD(dec_byhand);
// TRANS_FUNC_DEF_BD(rol_byhand);
// TRANS_FUNC_DEF_BD(ror_byhand);
// TRANS_FUNC_DEF_BD(shl_byhand);
// TRANS_FUNC_DEF_BD(sal_byhand);
// TRANS_FUNC_DEF_BD(sar_byhand);
// TRANS_FUNC_DEF_BD(shr_byhand);
// TRANS_FUNC_DEF_BD(not_byhand);
// TRANS_FUNC_DEF_BD(mul_byhand);
// TRANS_FUNC_DEF_BD(div_byhand);
// TRANS_FUNC_DEF_BD(imul_byhand);
// TRANS_FUNC_DEF_BD(cmpxchg_byhand);

// /* fpu */
TRANS_FUNC_DEF_BD(wait);
TRANS_FUNC_DEF_BD(f2xm1);
TRANS_FUNC_DEF_BD(fabs);
TRANS_FUNC_DEF_BD(fadd);
TRANS_FUNC_DEF_BD(faddp);
TRANS_FUNC_DEF_BD(fbld);
TRANS_FUNC_DEF_BD(fbstp);
TRANS_FUNC_DEF_BD(fchs);
TRANS_FUNC_DEF_BD(fcmovnb);
TRANS_FUNC_DEF_BD(fcmovb);
TRANS_FUNC_DEF_BD(fcmovnu);
TRANS_FUNC_DEF_BD(fcmovu);
TRANS_FUNC_DEF_BD(fcmove);
TRANS_FUNC_DEF_BD(fcmovne);
TRANS_FUNC_DEF_BD(fcmovbe);
TRANS_FUNC_DEF_BD(fcmovnbe);
TRANS_FUNC_DEF_BD(fcom);
TRANS_FUNC_DEF_BD(fcomi);
TRANS_FUNC_DEF_BD(fcomip);
TRANS_FUNC_DEF_BD(fcomp);
TRANS_FUNC_DEF_BD(fcompp);
TRANS_FUNC_DEF_BD(fcos);
TRANS_FUNC_DEF_BD(fdecstp);
TRANS_FUNC_DEF_BD(fdiv);
TRANS_FUNC_DEF_BD(fdivp);
TRANS_FUNC_DEF_BD(fdivr);
TRANS_FUNC_DEF_BD(fdivrp);
TRANS_FUNC_DEF_BD(ffree);
TRANS_FUNC_DEF_BD(ffreep);
TRANS_FUNC_DEF_BD(fiadd);
TRANS_FUNC_DEF_BD(ficom);
TRANS_FUNC_DEF_BD(ficomp);
TRANS_FUNC_DEF_BD(fidiv);
TRANS_FUNC_DEF_BD(fidivr);
TRANS_FUNC_DEF_BD(fild);
TRANS_FUNC_DEF_BD(fimul);
TRANS_FUNC_DEF_BD(fincstp);
TRANS_FUNC_DEF_BD(fist);
TRANS_FUNC_DEF_BD(fistp);
TRANS_FUNC_DEF_BD(fisttp);
TRANS_FUNC_DEF_BD(fisub);
TRANS_FUNC_DEF_BD(fisubr);
TRANS_FUNC_DEF_BD(fld1);
TRANS_FUNC_DEF_BD(fld);
TRANS_FUNC_DEF_BD(fldcw);
TRANS_FUNC_DEF_BD(fldenv);
TRANS_FUNC_DEF_BD(fldl2e);
TRANS_FUNC_DEF_BD(fldl2t);
TRANS_FUNC_DEF_BD(fldlg2);
TRANS_FUNC_DEF_BD(fldln2);
TRANS_FUNC_DEF_BD(fldpi);
TRANS_FUNC_DEF_BD(fldz);
TRANS_FUNC_DEF_BD(fmul);
TRANS_FUNC_DEF_BD(fmulp);
TRANS_FUNC_DEF_BD(fnclex);
TRANS_FUNC_DEF_BD(fninit);
TRANS_FUNC_DEF_BD(fnop);
TRANS_FUNC_DEF_BD(fnsave);
TRANS_FUNC_DEF_BD(fnstcw);
TRANS_FUNC_DEF_BD(fnstenv);
TRANS_FUNC_DEF_BD(fnstsw);
TRANS_FUNC_DEF_BD(fpatan);
TRANS_FUNC_DEF_BD(fprem1);
TRANS_FUNC_DEF_BD(fprem);
TRANS_FUNC_DEF_BD(fptan);
TRANS_FUNC_DEF_BD(frndint);
TRANS_FUNC_DEF_BD(frstor);
TRANS_FUNC_DEF_BD(fscale);
// TRANS_FUNC_DEF_BD(fsetpm);
TRANS_FUNC_DEF_BD(fsin);
TRANS_FUNC_DEF_BD(fsincos);
TRANS_FUNC_DEF_BD(fsqrt);
TRANS_FUNC_DEF_BD(fst);
TRANS_FUNC_DEF_BD(fstp);
TRANS_FUNC_DEF_BD(fsub);
TRANS_FUNC_DEF_BD(fsubp);
TRANS_FUNC_DEF_BD(fsubr);
TRANS_FUNC_DEF_BD(fsubrp);
TRANS_FUNC_DEF_BD(ftst);
TRANS_FUNC_DEF_BD(fucom);
TRANS_FUNC_DEF_BD(fucomi);
TRANS_FUNC_DEF_BD(fucomip);
TRANS_FUNC_DEF_BD(fucomp);
TRANS_FUNC_DEF_BD(fucompp);
TRANS_FUNC_DEF_BD(fxam);
TRANS_FUNC_DEF_BD(fxch);
TRANS_FUNC_DEF_BD(fxrstor);
TRANS_FUNC_DEF_BD(fxsave);
TRANS_FUNC_DEF_BD(fxtract);
TRANS_FUNC_DEF_BD(fyl2x);
TRANS_FUNC_DEF_BD(fyl2xp1);

/* fpu wraps */
TRANS_FUNC_DEF_BD(wait_wrap);
TRANS_FUNC_DEF_BD(f2xm1_wrap);
TRANS_FUNC_DEF_BD(fabs_wrap);
TRANS_FUNC_DEF_BD(fadd_wrap);
TRANS_FUNC_DEF_BD(faddp_wrap);
TRANS_FUNC_DEF_BD(fbld_wrap);
TRANS_FUNC_DEF_BD(fbstp_wrap);
TRANS_FUNC_DEF_BD(fchs_wrap);
TRANS_FUNC_DEF_BD(fcmovb_wrap);
TRANS_FUNC_DEF_BD(fcmovbe_wrap);
TRANS_FUNC_DEF_BD(fcmove_wrap);
TRANS_FUNC_DEF_BD(fcmovnb_wrap);
TRANS_FUNC_DEF_BD(fcmovnbe_wrap);
TRANS_FUNC_DEF_BD(fcmovne_wrap);
TRANS_FUNC_DEF_BD(fcmovnu_wrap);
TRANS_FUNC_DEF_BD(fcmovu_wrap);
TRANS_FUNC_DEF_BD(fcom_wrap);
TRANS_FUNC_DEF_BD(fcomi_wrap);
TRANS_FUNC_DEF_BD(fcomip_wrap);
TRANS_FUNC_DEF_BD(fcomp_wrap);
TRANS_FUNC_DEF_BD(fcompp_wrap);
TRANS_FUNC_DEF_BD(fcos_wrap);
TRANS_FUNC_DEF_BD(fdecstp_wrap);
TRANS_FUNC_DEF_BD(fdiv_wrap);
TRANS_FUNC_DEF_BD(fdivp_wrap);
TRANS_FUNC_DEF_BD(fdivr_wrap);
TRANS_FUNC_DEF_BD(fdivrp_wrap);
TRANS_FUNC_DEF_BD(ffree_wrap);
TRANS_FUNC_DEF_BD(ffreep_wrap);
TRANS_FUNC_DEF_BD(fiadd_wrap);
TRANS_FUNC_DEF_BD(ficom_wrap);
TRANS_FUNC_DEF_BD(ficomp_wrap);
TRANS_FUNC_DEF_BD(fidiv_wrap);
TRANS_FUNC_DEF_BD(fidivr_wrap);
TRANS_FUNC_DEF_BD(fild_wrap);
TRANS_FUNC_DEF_BD(fimul_wrap);
TRANS_FUNC_DEF_BD(fincstp_wrap);
TRANS_FUNC_DEF_BD(fist_wrap);
TRANS_FUNC_DEF_BD(fistp_wrap);
TRANS_FUNC_DEF_BD(fisttp_wrap);
TRANS_FUNC_DEF_BD(fisub_wrap);
TRANS_FUNC_DEF_BD(fisubr_wrap);
TRANS_FUNC_DEF_BD(fld1_wrap);
TRANS_FUNC_DEF_BD(fld_wrap);
TRANS_FUNC_DEF_BD(fldcw_wrap);
TRANS_FUNC_DEF_BD(fldenv_wrap);
TRANS_FUNC_DEF_BD(fldl2e_wrap);
TRANS_FUNC_DEF_BD(fldl2t_wrap);
TRANS_FUNC_DEF_BD(fldlg2_wrap);
TRANS_FUNC_DEF_BD(fldln2_wrap);
TRANS_FUNC_DEF_BD(fldpi_wrap);
TRANS_FUNC_DEF_BD(fldz_wrap);
TRANS_FUNC_DEF_BD(fmul_wrap);
TRANS_FUNC_DEF_BD(fmulp_wrap);
TRANS_FUNC_DEF_BD(fnclex_wrap);
TRANS_FUNC_DEF_BD(fninit_wrap);
TRANS_FUNC_DEF_BD(fnop_wrap);
TRANS_FUNC_DEF_BD(fnsave_wrap);
TRANS_FUNC_DEF_BD(fnstcw_wrap);
TRANS_FUNC_DEF_BD(fnstenv_wrap);
TRANS_FUNC_DEF_BD(fnstsw_wrap);
TRANS_FUNC_DEF_BD(fpatan_wrap);
TRANS_FUNC_DEF_BD(fprem1_wrap);
TRANS_FUNC_DEF_BD(fprem_wrap);
TRANS_FUNC_DEF_BD(fptan_wrap);
TRANS_FUNC_DEF_BD(frndint_wrap);
TRANS_FUNC_DEF_BD(frstor_wrap);
TRANS_FUNC_DEF_BD(fscale_wrap);
// TRANS_FUNC_DEF_BD(fsetpm_wrap);
TRANS_FUNC_DEF_BD(fsin_wrap);
TRANS_FUNC_DEF_BD(fsincos_wrap);
TRANS_FUNC_DEF_BD(fsqrt_wrap);
TRANS_FUNC_DEF_BD(fst_wrap);
TRANS_FUNC_DEF_BD(fstp_wrap);
TRANS_FUNC_DEF_BD(fsub_wrap);
TRANS_FUNC_DEF_BD(fsubp_wrap);
TRANS_FUNC_DEF_BD(fsubr_wrap);
TRANS_FUNC_DEF_BD(fsubrp_wrap);
TRANS_FUNC_DEF_BD(ftst_wrap);
TRANS_FUNC_DEF_BD(fucom_wrap);
TRANS_FUNC_DEF_BD(fucomi_wrap);
TRANS_FUNC_DEF_BD(fucomip_wrap);
TRANS_FUNC_DEF_BD(fucomp_wrap);
TRANS_FUNC_DEF_BD(fucompp_wrap);
TRANS_FUNC_DEF_BD(fxam_wrap);
TRANS_FUNC_DEF_BD(fxch_wrap);
TRANS_FUNC_DEF_BD(fxrstor_wrap);
TRANS_FUNC_DEF_BD(fxsave_wrap);
TRANS_FUNC_DEF_BD(fxtract_wrap);
TRANS_FUNC_DEF_BD(fyl2x_wrap);
TRANS_FUNC_DEF_BD(fyl2xp1_wrap);

/* sha */
TRANS_FUNC_DEF_BD(sha1msg1);
TRANS_FUNC_DEF_BD(sha1msg2);
TRANS_FUNC_DEF_BD(sha1nexte);
TRANS_FUNC_DEF_BD(sha1rnds4);
TRANS_FUNC_DEF_BD(sha256msg1);
TRANS_FUNC_DEF_BD(sha256msg2);
TRANS_FUNC_DEF_BD(sha256rnds2);

TRANS_FUNC_DEF_BD(andn);
TRANS_FUNC_DEF_BD(movbe);
TRANS_FUNC_DEF_BD(rorx);
TRANS_FUNC_DEF_BD(blsi);
TRANS_FUNC_DEF_BD(pcmpestri);
TRANS_FUNC_DEF_BD(pcmpestrm);
TRANS_FUNC_DEF_BD(pcmpistri);
TRANS_FUNC_DEF_BD(pcmpistrm);
TRANS_FUNC_DEF_BD(aesdec);
TRANS_FUNC_DEF_BD(aesdeclast);
TRANS_FUNC_DEF_BD(aesenc);
TRANS_FUNC_DEF_BD(aesenclast);
TRANS_FUNC_DEF_BD(aesimc);
TRANS_FUNC_DEF_BD(aeskeygenassist);

TRANS_FUNC_DEF_BD(pext);
TRANS_FUNC_DEF_BD(pdep);
TRANS_FUNC_DEF_BD(bextr);
TRANS_FUNC_DEF_BD(blsmsk);
TRANS_FUNC_DEF_BD(bzhi);
TRANS_FUNC_DEF_BD(lzcnt);
TRANS_FUNC_DEF_BD(adcx);
TRANS_FUNC_DEF_BD(adox);
TRANS_FUNC_DEF_BD(crc32);
TRANS_FUNC_DEF_BD(salc);
TRANS_FUNC_DEF_BD(pclmulqdq);

void tr_init(void *tb);
void tr_fini(bool check_the_extension); /* default TRUE */

void tr_disasm(struct TranslationBlock *tb, int max_insns);
void etb_add_succ(void* etb,int depth);
int tr_translate_tb(struct TranslationBlock *tb);
int tr_ir2_generate(struct TranslationBlock *tb);
int label_dispose(TranslationBlock *tb, TRANSLATION_DATA *lat_ctx);
int tr_ir2_assemble(const void *code_start_addr, const IR2_INST *pir2);
#if defined(CONFIG_LATX_FLAG_REDUCTION) && \
    defined(CONFIG_LATX_FLAG_REDUCTION_EXTEND)
int8 get_etb_type_bd(IR1_INST *pir1);
#endif

IR1_INST *get_ir1_list(struct TranslationBlock *tb, ADDRX pc, int max_insns);

extern ADDR context_switch_native_to_bt_ret_0;
extern ADDR context_switch_native_to_bt;
extern ADDR ss_match_fail_native;

/* target_latx_host()
 * ---------------------------------------
 * |  tr_disasm()
 * |  -----------------------------------
 * |  |  ir1_disasm()
 * |  -----------------------------------
 * |  tr_translate_tb()
 * |  -----------------------------------
 * |  |  tr_init()
 * |  |  tr_ir2_generate()
 * |  |  --------------------------------
 * |  |  |  tr_init_for_each_ir1_in_tb()
 * |  |  |  ir1_translate(pir1)
 * |  |  --------------------------------
 * |  |  tr_ir2_optimize()
 * |  |  tr_ir2_assemble()
 * |  |  tr_fini()
 * |  -----------------------------------
 * --------------------------------------- */

void tr_skip_eflag_calculation(int usedef_bits);
void tr_fpu_push(void);
void tr_fpu_pop(void);
void tr_fpu_inc(void);
void tr_fpu_dec(void);
void tr_fpu_enable_top_mode(void);
void tr_fpu_disable_top_mode(void);

void tr_fpu_load_tag_to_env_bd(IR2_OPND fpu_tag);
void tr_fpu_store_tag_to_mem_bd(IR2_OPND mem_opnd, int mem_imm);

extern int GPR_USEDEF_TO_SAVE;
extern int FPR_USEDEF_TO_SAVE;
extern int XMM_USEDEF_TO_SAVE;

struct lat_lock{
	int lock;
} __attribute__ ((aligned (64)));;
extern struct lat_lock lat_lock[16];

void tr_set_running_of_cs(bool value);
void tr_save_gpr_to_env(uint8 gpr_to_save);
void tr_load_gpr_from_env(uint8 gpr_to_load);
void tr_save_xmm_to_env(uint8 xmm_to_save);
void tr_load_xmm_from_env(uint8 xmm_to_load);
void tr_save_registers_to_env(uint8 gpr_to_save, uint8 fpr_to_save,
                              uint8 xmm_to_save, uint8 vreg_to_save);
void tr_load_registers_from_env(uint8 gpr_to_load, uint8 fpr_to_load,
                                uint8 xmm_to_load, uint8 vreg_to_load);
#ifdef TARGET_X86_64
void tr_save_xmm64_to_env(uint8 xmm_to_save);
void tr_load_xmm64_from_env(uint8 xmm_to_load);
void tr_save_x64_8_registers_to_env(uint8 gpr_to_save, uint8 xmm_to_save);
void tr_load_x64_8_registers_from_env(uint8 gpr_to_load, uint8 xmm_to_load);
#endif
void tr_save_fcsr_to_env(void);
void tr_load_fcsr_from_env(void);
void update_fcsr_by_sw_bd(IR2_OPND sw);

void tr_gen_call_to_helper(ADDR, enum aot_rel_kind);
void convert_fpregs_64_to_x80(void);
void convert_fpregs_x80_to_64(void);
void helper_raise_int_bd(void);
void helper_raise_syscall_bd(void);

bool si12_overflow(long si12);

/* Loongarch V1.1 */
int have_scq_bd(void);

/* operand conversion */
IR2_OPND convert_mem_bd(IR1_OPND_BD *opnd1, int *host_off);
IR2_OPND mem_imm_add_disp_bd(IR2_OPND, int *, int);
IR2_OPND convert_mem_no_offset_bd(IR1_OPND_BD *);
void convert_mem_to_specific_gpr_bd(IR1_OPND_BD *, IR2_OPND, int);
IR2_OPND convert_mem_to_itemp_bd(IR1_OPND_BD *opnd0);
IR2_OPND convert_gpr_opnd_bd(IR1_OPND_BD *opnd1, EXTENSION_MODE em);
IR2_OPND load_freg128_from_ir1_bd(IR1_OPND_BD *opnd1);
IR2_OPND load_freg256_from_ir1_bd(IR1_OPND_BD * opnd1);
void set_high128_xreg_to_zero_bd(IR2_OPND opnd2);
void store_freg256_to_ir1_mem_bd(IR2_OPND opnd2, IR1_OPND_BD * opnd1);
void load_freg256_from_ir1_mem_bd(IR2_OPND opnd2, IR1_OPND_BD * opnd1);

#ifdef CONFIG_LATX_AOT
/* TODO */
void load_ireg_from_host_addr(IR2_OPND opnd2, uint64 value);
void load_ireg_from_guest_addr(IR2_OPND opnd2, uint64 value);
#endif

void load_ireg_from_ir1_mem_bd(IR2_OPND opnd2, IR1_OPND_BD *opnd1,
                                   EXTENSION_MODE em, bool is_xmm_hi);
IR2_OPND load_ireg_from_ir1_bd(IR1_OPND_BD *, EXTENSION_MODE, bool is_xmm_hi);
void load_ireg_from_ir1_2_bd(IR2_OPND, IR1_OPND_BD *, EXTENSION_MODE, bool is_xmm_hi);
void store_ireg_to_ir1_seg_bd(IR2_OPND seg_value_opnd, IR1_OPND_BD *opnd1);
void store_ireg_to_ir1_bd(IR2_OPND, IR1_OPND_BD *, bool is_xmm_hi);

IR2_OPND load_ireg_from_ir2_mem_bd(IR2_OPND mem_opnd, int mem_imm, int mem_size,
                                   EXTENSION_MODE em, bool is_xmm_hi);
void store_ireg_to_ir2_mem_bd(IR2_OPND value_opnd, IR2_OPND mem_opnd,
                                  int mem_imm, int mem_size, bool is_xmm_hi);

void load_ireg_from_cf_opnd_bd(IR2_OPND *opnd2);

/* load to freg */
IR2_OPND load_freg_from_ir1_1_bd(IR1_OPND_BD *opnd1, bool is_xmm_hi, uint32_t options);
void load_freg_from_ir1_2_bd(IR2_OPND opnd2, IR1_OPND_BD *opnd1, uint32_t options);
void store_freg_to_ir1_bd(IR2_OPND, IR1_OPND_BD *, bool is_xmm_hi, bool is_convert);
void store_64_bit_freg_to_ir1_80_bit_mem_bd(IR2_OPND, IR2_OPND, int);
void store_freg128_to_ir1_mem_bd(IR2_OPND opnd2, IR1_OPND_BD *opnd1);
void load_freg128_from_ir1_mem_bd(IR2_OPND opnd2, IR1_OPND_BD *opnd1);
void load_64_bit_freg_from_ir1_80_bit_mem_bd(IR2_OPND opnd2,
                                                 IR2_OPND mem_opnd, int mem_imm);

/* set/clear lsenv->mode_trans_mmx_fputo to transfer to MMX/FPU mode */
void transfer_to_fpu_mode_bd(void);
void transfer_to_mmx_mode_bd(void);

/* load two singles from ir1 pack */
void load_singles_from_ir1_pack_bd(IR2_OPND single0, IR2_OPND single1,
                                IR1_OPND_BD *opnd1, bool is_xmm_hi);
/* store two single into a pack */
void store_singles_to_ir2_pack_bd(IR2_OPND single0, IR2_OPND single1,
                               IR2_OPND pack);

/* fcsr */
void update_sw_by_fcsr_bd(IR2_OPND sw_opnd);
void update_fcsr_by_cw_bd(IR2_OPND cw);
IR2_OPND set_fpu_fcsr_rounding_field_by_x86_bd(void);
void set_fpu_rounding_mode_bd(IR2_OPND rm);

int generate_native_rotate_fpu_by(void *code_buf);
void generate_context_switch_bt_to_native(void *code_buf);
void generate_context_switch_native_to_bt(void);

void generate_eflag_calculation_bd(IR2_OPND, IR2_OPND, IR2_OPND, IR1_INST *, bool);

#ifdef CONFIG_LATX_XCOMISX_OPT
void generate_xcomisx_bd(IR2_OPND, IR2_OPND, bool, bool, uint8_t);
#endif

/* extern ADDR tb_look_up_native; */
void tr_generate_exit_tb_bd(IR1_INST *branch, int succ_id);
#ifdef CONFIG_LATX_XCOMISX_OPT
void tr_generate_exit_stub_tb_bd(IR1_INST *branch, int succ_id, void *func, IR1_INST *stub);
#endif
void tr_generate_goto_tb(void);                          /* TODO */

/* rotate fpu */
/* native_rotate_fpu_by(step, return_address) */
extern ADDR native_rotate_fpu_by;
extern ADDR indirect_jmp_glue;
extern ADDR parallel_indirect_jmp_glue;
void rotate_fpu_to_top(int top);
void rotate_fpu_by(int step);
void rotate_fpu_to_bias(int bias);

void tr_gen_call_to_helper1(ADDR func, int use_fp, enum aot_rel_kind);
void tr_gen_call_to_helper2(ADDR, IR2_OPND, int, enum aot_rel_kind);
void tr_gen_call_to_helper_xgetbv(void);
void tr_gen_call_to_helper_vfll(ADDR, IR2_OPND, IR2_OPND, int);
void tr_gen_call_to_helper_pcmpxstrx(ADDR, int, int, int);
void tr_gen_call_to_helper_cvttpd2pi(ADDR, int, int);
void tr_gen_call_to_helper_pclmulqdq(ADDR, int, int, int, int ,int );
void tr_gen_call_to_helper_aes(ADDR, int, int, int);
void tr_load_top_from_env(void);
void tr_gen_top_mode_init(void);

IR2_OPND tr_lat_spin_lock(IR2_OPND mem_addr, int imm);
void tr_lat_spin_unlock(IR2_OPND lat_lock_addr);

void gen_softfpu_helper_prologue_bd(IR1_INST *pir1);
void gen_softfpu_helper_epilogue_bd(IR1_INST *pir1);
void update_fcsr_rm_bd(IR2_OPND control_word, IR2_OPND fcsr);

bool ir1_need_reserve_h128_bd(IR1_INST *ir1);
IR2_OPND save_h128_of_ymm_bd(IR1_INST *ir1);
void restore_h128_of_ymm_bd(IR1_INST *ir1, IR2_OPND temp);
void gen_test_page_flag(IR2_OPND mem_opnd, int mem_imm, uint32_t flag);

void clear_h32_bd(IR2_OPND *opnd);

#include "qemu-def.h"

#define NONE            0
#define IS_CONVERT      1
#define IS_XMM_HI       (1 << 2)
#define IS_INTEGER      (1 << 3)

bool if_reduce_proepo(IR1_OPCODE_BD opcode);
unsigned long tb_checksum(const uint8_t * start, size_t len);
extern bool need_trace;

#endif