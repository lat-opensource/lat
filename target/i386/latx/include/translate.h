#ifndef _TRANSLATE_H_
#define _TRANSLATE_H_

#include "common.h"
#include "env.h"
#include "ir1.h"
#include "ir2.h"
#include "la-append.h"
#include "ir2-relocate.h"
#include "macro-inst.h"

#include "aot.h"

//#define LATX_DEBUG_SOFTFPU

#define TRANS_FUNC(name) glue(translate_, name)
#define TRANS_FUNC_DEF(name) \
bool TRANS_FUNC(name)(IR1_INST * pir1)
#define TRANS_FUNC_GEN_REAL(opcode, function) \
[glue(dt_X86_INS_, opcode)] = function
#define TRANS_FUNC_GEN(opcode, function) \
TRANS_FUNC_GEN_REAL(opcode, TRANS_FUNC(function))

#define TRANS_FPU_WRAP_GEN_NO_PROLOGUE(function)    \
bool translate_##function##_wrap(IR1_INST *pir1)    \
{                                                   \
    if (option_softfpu) {                           \
        return translate_##function##_softfpu(pir1);\
    }                                               \
    return translate_##function(pir1);              \
}

#define TRANS_FPU_WRAP_GEN(function)                \
bool translate_##function##_wrap(IR1_INST *pir1)    \
{                                                   \
    if (option_softfpu == 2) {                      \
        return translate_##function##_softfpu(pir1);\
    } else if (option_softfpu == 1) {               \
        bool ret;                                   \
        gen_softfpu_helper_prologue(pir1);          \
        ret = translate_##function##_softfpu(pir1); \
        gen_softfpu_helper_epilogue(pir1);          \
        return ret;                                 \
                                                    \
    }                                               \
    return translate_##function(pir1);              \
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
bool translate_##function##_wrap(IR1_INST *pir1)    \
{                                                   \
    if (0) {                                        \
        return translate_##function##_softfpu(pir1);\
    }                                               \
    return translate_##function(pir1);              \
}
#endif

#include "insts-pattern.h"

TRANS_FUNC_DEF(invalid);
TRANS_FUNC_DEF(add);
TRANS_FUNC_DEF(lock_add);
TRANS_FUNC_DEF(push);
TRANS_FUNC_DEF(pop);
TRANS_FUNC_DEF(or);
TRANS_FUNC_DEF(lock_or);
TRANS_FUNC_DEF(adc);
TRANS_FUNC_DEF(lock_adc);
TRANS_FUNC_DEF(sbb);
TRANS_FUNC_DEF(lock_sbb);
TRANS_FUNC_DEF(and);
TRANS_FUNC_DEF(lock_and);
TRANS_FUNC_DEF(daa);
TRANS_FUNC_DEF(sub);
TRANS_FUNC_DEF(lock_sub);
TRANS_FUNC_DEF(das);
TRANS_FUNC_DEF(xor);
TRANS_FUNC_DEF(lock_xor);
TRANS_FUNC_DEF(aaa);
TRANS_FUNC_DEF(cmp);
TRANS_FUNC_DEF(aas);
TRANS_FUNC_DEF(inc);
TRANS_FUNC_DEF(lock_inc);
TRANS_FUNC_DEF(dec);
TRANS_FUNC_DEF(lock_dec);
TRANS_FUNC_DEF(pushaw);
TRANS_FUNC_DEF(pushal);
TRANS_FUNC_DEF(popaw);
TRANS_FUNC_DEF(popal);
TRANS_FUNC_DEF(popcnt);
TRANS_FUNC_DEF(imul);
TRANS_FUNC_DEF(ins);
TRANS_FUNC_DEF(outs);
TRANS_FUNC_DEF(jcc);
TRANS_FUNC_DEF(test);
TRANS_FUNC_DEF(xchg);
TRANS_FUNC_DEF(mov);
TRANS_FUNC_DEF(lea);
TRANS_FUNC_DEF(cbw);
TRANS_FUNC_DEF(cwde);
TRANS_FUNC_DEF(cdqe);
TRANS_FUNC_DEF(cwd);
TRANS_FUNC_DEF(cdq);
TRANS_FUNC_DEF(cqo);
TRANS_FUNC_DEF(call_far);
TRANS_FUNC_DEF(pushf);
TRANS_FUNC_DEF(popf);
TRANS_FUNC_DEF(sahf);
TRANS_FUNC_DEF(lahf);
TRANS_FUNC_DEF(movs);
TRANS_FUNC_DEF(cmps);
TRANS_FUNC_DEF(stos);
TRANS_FUNC_DEF(lods);
TRANS_FUNC_DEF(scas);
TRANS_FUNC_DEF(ret);
TRANS_FUNC_DEF(retf);
TRANS_FUNC_DEF(enter);
TRANS_FUNC_DEF(leave);
TRANS_FUNC_DEF(int_3);
TRANS_FUNC_DEF(int);
TRANS_FUNC_DEF(iret);
TRANS_FUNC_DEF(iretq);
TRANS_FUNC_DEF(aam);
TRANS_FUNC_DEF(aad);
TRANS_FUNC_DEF(xlat);
TRANS_FUNC_DEF(loopnz);
TRANS_FUNC_DEF(loopz);
TRANS_FUNC_DEF(loop);
TRANS_FUNC_DEF(jcxz);
TRANS_FUNC_DEF(jecxz);
TRANS_FUNC_DEF(jrcxz);
TRANS_FUNC_DEF(in);
TRANS_FUNC_DEF(out);
TRANS_FUNC_DEF(call);
TRANS_FUNC_DEF(jmp);
TRANS_FUNC_DEF(ljmp);
TRANS_FUNC_DEF(hlt);
TRANS_FUNC_DEF(cmc);
TRANS_FUNC_DEF(clc);
TRANS_FUNC_DEF(stc);
TRANS_FUNC_DEF(cld);
TRANS_FUNC_DEF(std);
TRANS_FUNC_DEF(syscall);
TRANS_FUNC_DEF(ud2);
TRANS_FUNC_DEF(nop);
TRANS_FUNC_DEF(endbr32);
TRANS_FUNC_DEF(endbr64);

TRANS_FUNC_DEF(rdtsc);
TRANS_FUNC_DEF(cmovcc);
TRANS_FUNC_DEF(setcc);
TRANS_FUNC_DEF(cpuid);
TRANS_FUNC_DEF(btx_llsc);
TRANS_FUNC_DEF(btx);
TRANS_FUNC_DEF(blsr);
TRANS_FUNC_DEF(shld);
TRANS_FUNC_DEF(shrd);
TRANS_FUNC_DEF(sarx);
TRANS_FUNC_DEF(shlx);
TRANS_FUNC_DEF(shrx);
TRANS_FUNC_DEF(cmpxchg);
TRANS_FUNC_DEF(lock_cmpxchg);
TRANS_FUNC_DEF(movzx);
TRANS_FUNC_DEF(tzcnt);
TRANS_FUNC_DEF(bsf);
TRANS_FUNC_DEF(movsx);
TRANS_FUNC_DEF(xadd);
TRANS_FUNC_DEF(lock_xadd);
TRANS_FUNC_DEF(movnti);
TRANS_FUNC_DEF(bswap);
TRANS_FUNC_DEF(rol);
TRANS_FUNC_DEF(ror);
TRANS_FUNC_DEF(rcl);
TRANS_FUNC_DEF(rcr);
TRANS_FUNC_DEF(shl);
TRANS_FUNC_DEF(shr);
TRANS_FUNC_DEF(sal);
TRANS_FUNC_DEF(sar);
TRANS_FUNC_DEF(not);
TRANS_FUNC_DEF(lock_not);
TRANS_FUNC_DEF(neg);
TRANS_FUNC_DEF(lock_neg);
TRANS_FUNC_DEF(mul);
TRANS_FUNC_DEF(mulx);
TRANS_FUNC_DEF(div);
TRANS_FUNC_DEF(idiv);
TRANS_FUNC_DEF(rdtscp);
TRANS_FUNC_DEF(prefetch);
TRANS_FUNC_DEF(prefetchw);
TRANS_FUNC_DEF(movups);
TRANS_FUNC_DEF(movupd);
TRANS_FUNC_DEF(movss);
TRANS_FUNC_DEF(movsd);
TRANS_FUNC_DEF(movhlps);
TRANS_FUNC_DEF(movlps);
TRANS_FUNC_DEF(movlpd);
TRANS_FUNC_DEF(movsldup);
TRANS_FUNC_DEF(movddup);
TRANS_FUNC_DEF(unpcklps);
TRANS_FUNC_DEF(unpcklpd);
TRANS_FUNC_DEF(unpckhps);
TRANS_FUNC_DEF(unpckhpd);
TRANS_FUNC_DEF(movlhps);
TRANS_FUNC_DEF(movhps);
TRANS_FUNC_DEF(movhpd);
TRANS_FUNC_DEF(movshdup);
TRANS_FUNC_DEF(prefetchnta);
TRANS_FUNC_DEF(prefetcht0);
TRANS_FUNC_DEF(prefetcht1);
TRANS_FUNC_DEF(prefetcht2);
TRANS_FUNC_DEF(movaps);
TRANS_FUNC_DEF(movapd);
TRANS_FUNC_DEF(cvtpi2ps);
TRANS_FUNC_DEF(cvtpi2pd);
TRANS_FUNC_DEF(cvtsi2ss);
TRANS_FUNC_DEF(cvtsi2sd);
TRANS_FUNC_DEF(movntps);
TRANS_FUNC_DEF(movntpd);
TRANS_FUNC_DEF(cvttsx2si);
TRANS_FUNC_DEF(cvtps2pi);
TRANS_FUNC_DEF(cvttps2pi);
TRANS_FUNC_DEF(cvtpd2pi);
TRANS_FUNC_DEF(cvtsx2si);
TRANS_FUNC_DEF(cvttpd2pi);
#ifdef CONFIG_LATX_XCOMISX_OPT
TRANS_FUNC_DEF(xcomisx);
#endif
TRANS_FUNC_DEF(ucomiss);
TRANS_FUNC_DEF(ucomisd);
TRANS_FUNC_DEF(comiss);
TRANS_FUNC_DEF(comisd);
TRANS_FUNC_DEF(movmskps);
TRANS_FUNC_DEF(movmskpd);
TRANS_FUNC_DEF(sqrtps);
TRANS_FUNC_DEF(sqrtpd);
TRANS_FUNC_DEF(sqrtss);
TRANS_FUNC_DEF(sqrtsd);
TRANS_FUNC_DEF(rsqrtps);
TRANS_FUNC_DEF(rsqrtss);
TRANS_FUNC_DEF(rcpps);
TRANS_FUNC_DEF(rcpss);
TRANS_FUNC_DEF(andps);
TRANS_FUNC_DEF(andpd);
TRANS_FUNC_DEF(andnps);
TRANS_FUNC_DEF(andnpd);
TRANS_FUNC_DEF(orps);
TRANS_FUNC_DEF(orpd);
TRANS_FUNC_DEF(xorps);
TRANS_FUNC_DEF(xorpd);
TRANS_FUNC_DEF(addps);
TRANS_FUNC_DEF(addpd);
TRANS_FUNC_DEF(addss);
TRANS_FUNC_DEF(addsd);
TRANS_FUNC_DEF(mulps);
TRANS_FUNC_DEF(mulpd);
TRANS_FUNC_DEF(mulss);
TRANS_FUNC_DEF(mulsd);
TRANS_FUNC_DEF(cvtps2pd);
TRANS_FUNC_DEF(cvtpd2ps);
TRANS_FUNC_DEF(cvtss2sd);
TRANS_FUNC_DEF(cvtsd2ss);
TRANS_FUNC_DEF(cvtdq2ps);
TRANS_FUNC_DEF(cvtps2dq);
TRANS_FUNC_DEF(cvtpd2dq);
TRANS_FUNC_DEF(cvttpx2dq);
TRANS_FUNC_DEF(subps);
TRANS_FUNC_DEF(subpd);
TRANS_FUNC_DEF(subss);
TRANS_FUNC_DEF(subsd);
TRANS_FUNC_DEF(minps);
TRANS_FUNC_DEF(minpd);
TRANS_FUNC_DEF(minss);
TRANS_FUNC_DEF(minsd);
TRANS_FUNC_DEF(divps);
TRANS_FUNC_DEF(divpd);
TRANS_FUNC_DEF(divss);
TRANS_FUNC_DEF(divsd);
TRANS_FUNC_DEF(maxps);
TRANS_FUNC_DEF(maxpd);
TRANS_FUNC_DEF(maxss);
TRANS_FUNC_DEF(maxsd);
TRANS_FUNC_DEF(punpcklbw);
TRANS_FUNC_DEF(punpcklwd);
TRANS_FUNC_DEF(punpckldq);
TRANS_FUNC_DEF(packsswb);
TRANS_FUNC_DEF(pcmpgtb);
TRANS_FUNC_DEF(pcmpgtw);
TRANS_FUNC_DEF(pcmpgtd);
TRANS_FUNC_DEF(pcmpgtq);
TRANS_FUNC_DEF(packuswb);
TRANS_FUNC_DEF(punpckhbw);
TRANS_FUNC_DEF(punpckhwd);
TRANS_FUNC_DEF(punpckhdq);
TRANS_FUNC_DEF(packssdw);
TRANS_FUNC_DEF(punpcklqdq);
TRANS_FUNC_DEF(punpckhqdq);
TRANS_FUNC_DEF(movd);
TRANS_FUNC_DEF(movq);
TRANS_FUNC_DEF(movdqa);
TRANS_FUNC_DEF(movdqu);
TRANS_FUNC_DEF(pshufw);
TRANS_FUNC_DEF(pshufd);
TRANS_FUNC_DEF(pshufhw);
TRANS_FUNC_DEF(pshuflw);
TRANS_FUNC_DEF(pcmpeqb);
TRANS_FUNC_DEF(pcmpeqw);
TRANS_FUNC_DEF(pcmpeqd);
TRANS_FUNC_DEF(emms);
TRANS_FUNC_DEF(xave);
TRANS_FUNC_DEF(lfence);
TRANS_FUNC_DEF(mfence);
TRANS_FUNC_DEF(sfence);
TRANS_FUNC_DEF(clflush);
TRANS_FUNC_DEF(clflushopt);
TRANS_FUNC_DEF(bsr);
TRANS_FUNC_DEF(cmpeqps);
TRANS_FUNC_DEF(cmpltps);
TRANS_FUNC_DEF(cmpleps);
TRANS_FUNC_DEF(cmpunordps);
TRANS_FUNC_DEF(cmpneqps);
TRANS_FUNC_DEF(cmpnltps);
TRANS_FUNC_DEF(cmpnleps);
TRANS_FUNC_DEF(cmpordps);
TRANS_FUNC_DEF(cmpeqpd);
TRANS_FUNC_DEF(cmpltpd);
TRANS_FUNC_DEF(cmplepd);
TRANS_FUNC_DEF(cmpunordpd);
TRANS_FUNC_DEF(cmpneqpd);
TRANS_FUNC_DEF(cmpnltpd);
TRANS_FUNC_DEF(cmpnlepd);
TRANS_FUNC_DEF(cmpordpd);
TRANS_FUNC_DEF(cmpeqss);
TRANS_FUNC_DEF(cmpltss);
TRANS_FUNC_DEF(cmpless);
TRANS_FUNC_DEF(cmpunordss);
TRANS_FUNC_DEF(cmpneqss);
TRANS_FUNC_DEF(cmpnltss);
TRANS_FUNC_DEF(cmpnless);
TRANS_FUNC_DEF(cmpordss);
TRANS_FUNC_DEF(cmpeqsd);
TRANS_FUNC_DEF(cmpltsd);
TRANS_FUNC_DEF(cmplesd);
TRANS_FUNC_DEF(cmpunordsd);
TRANS_FUNC_DEF(cmpneqsd);
TRANS_FUNC_DEF(cmpnltsd);
TRANS_FUNC_DEF(cmpnlesd);
TRANS_FUNC_DEF(cmpordsd);
TRANS_FUNC_DEF(cmppd);
TRANS_FUNC_DEF(cmpps);
TRANS_FUNC_DEF(cmpsd);
TRANS_FUNC_DEF(cmpss);
TRANS_FUNC_DEF(pinsrw);
TRANS_FUNC_DEF(pextrw);
TRANS_FUNC_DEF(shufps);
TRANS_FUNC_DEF(shufpd);
TRANS_FUNC_DEF(cmpxchg8b);
TRANS_FUNC_DEF(cmpxchg16b);
TRANS_FUNC_DEF(addsubpd);
TRANS_FUNC_DEF(addsubps);
TRANS_FUNC_DEF(psrlw);
TRANS_FUNC_DEF(psrld);
TRANS_FUNC_DEF(psrlq);
TRANS_FUNC_DEF(paddq);
TRANS_FUNC_DEF(pmullw);
TRANS_FUNC_DEF(movq2dq);
TRANS_FUNC_DEF(movdq2q);
TRANS_FUNC_DEF(pmovmskb);
TRANS_FUNC_DEF(psubusb);
TRANS_FUNC_DEF(psubusw);
TRANS_FUNC_DEF(pminub);
TRANS_FUNC_DEF(pand);
TRANS_FUNC_DEF(paddusb);
TRANS_FUNC_DEF(paddusw);
TRANS_FUNC_DEF(pmaxub);
TRANS_FUNC_DEF(pandn);
TRANS_FUNC_DEF(pavgb);
TRANS_FUNC_DEF(psraw);
TRANS_FUNC_DEF(psrad);
TRANS_FUNC_DEF(pavgw);
TRANS_FUNC_DEF(pmulhuw);
TRANS_FUNC_DEF(pmulhw);
TRANS_FUNC_DEF(cvtdq2pd);
TRANS_FUNC_DEF(movntq);
TRANS_FUNC_DEF(movntdq);
TRANS_FUNC_DEF(psubsb);
TRANS_FUNC_DEF(psubsw);
TRANS_FUNC_DEF(pminsw);
TRANS_FUNC_DEF(por);
TRANS_FUNC_DEF(paddsb);
TRANS_FUNC_DEF(paddsw);
TRANS_FUNC_DEF(pmaxsw);
TRANS_FUNC_DEF(pxor);
TRANS_FUNC_DEF(lddqu);
TRANS_FUNC_DEF(psllw);
TRANS_FUNC_DEF(pslld);
TRANS_FUNC_DEF(psllq);
TRANS_FUNC_DEF(pmuludq);
TRANS_FUNC_DEF(pmaddwd);
TRANS_FUNC_DEF(psadbw);
TRANS_FUNC_DEF(maskmovq);
TRANS_FUNC_DEF(maskmovdqu);
TRANS_FUNC_DEF(psubb);
TRANS_FUNC_DEF(psubw);
TRANS_FUNC_DEF(psubd);
TRANS_FUNC_DEF(psubq);
TRANS_FUNC_DEF(paddb);
TRANS_FUNC_DEF(paddw);
TRANS_FUNC_DEF(paddd);
TRANS_FUNC_DEF(psrldq);
TRANS_FUNC_DEF(pslldq);
TRANS_FUNC_DEF(ldmxcsr);
TRANS_FUNC_DEF(stmxcsr);
TRANS_FUNC_DEF(movsxd);
TRANS_FUNC_DEF(pause);
TRANS_FUNC_DEF(haddpd);
TRANS_FUNC_DEF(haddps);
TRANS_FUNC_DEF(hsubpd);
TRANS_FUNC_DEF(hsubps);

/* ssse3 */
TRANS_FUNC_DEF(psignb);
TRANS_FUNC_DEF(psignw);
TRANS_FUNC_DEF(psignd);
TRANS_FUNC_DEF(pabsb);
TRANS_FUNC_DEF(pabsw);
TRANS_FUNC_DEF(pabsd);
TRANS_FUNC_DEF(palignr);
TRANS_FUNC_DEF(pshufb);
TRANS_FUNC_DEF(pmulhrsw);
TRANS_FUNC_DEF(pmaddubsw);
TRANS_FUNC_DEF(phsubw);
TRANS_FUNC_DEF(phsubd);
TRANS_FUNC_DEF(phsubsw);
TRANS_FUNC_DEF(phaddw);
TRANS_FUNC_DEF(phaddd);
TRANS_FUNC_DEF(phaddsw);

/* sse 4.1 fp */
TRANS_FUNC_DEF(dpps);
TRANS_FUNC_DEF(dppd);
TRANS_FUNC_DEF(blendps);
TRANS_FUNC_DEF(blendpd);
TRANS_FUNC_DEF(blendvps);
TRANS_FUNC_DEF(blendvpd);
TRANS_FUNC_DEF(roundps);
TRANS_FUNC_DEF(roundss);
TRANS_FUNC_DEF(roundpd);
TRANS_FUNC_DEF(roundsd);
TRANS_FUNC_DEF(insertps);
TRANS_FUNC_DEF(extractps);

/* sse 4.1 int */
TRANS_FUNC_DEF(mpsadbw);
TRANS_FUNC_DEF(phminposuw);
TRANS_FUNC_DEF(pmulld);
TRANS_FUNC_DEF(pmuldq);
TRANS_FUNC_DEF(pblendvb);
TRANS_FUNC_DEF(pblendw);
TRANS_FUNC_DEF(pminsb);
TRANS_FUNC_DEF(pminuw);
TRANS_FUNC_DEF(pminsd);
TRANS_FUNC_DEF(pminud);
TRANS_FUNC_DEF(pmaxsb);
TRANS_FUNC_DEF(pmaxuw);
TRANS_FUNC_DEF(pmaxsd);
TRANS_FUNC_DEF(pmaxud);
TRANS_FUNC_DEF(pinsrb);
TRANS_FUNC_DEF(pinsrd);
TRANS_FUNC_DEF(pinsrq);
TRANS_FUNC_DEF(pextrb);
TRANS_FUNC_DEF(pextrd);
TRANS_FUNC_DEF(pextrq);
TRANS_FUNC_DEF(pmovsxbw);
TRANS_FUNC_DEF(pmovzxbw);
TRANS_FUNC_DEF(pmovsxbd);
TRANS_FUNC_DEF(pmovzxbd);
TRANS_FUNC_DEF(pmovsxbq);
TRANS_FUNC_DEF(pmovzxbq);
TRANS_FUNC_DEF(pmovsxwd);
TRANS_FUNC_DEF(pmovzxwd);
TRANS_FUNC_DEF(pmovsxwq);
TRANS_FUNC_DEF(pmovzxwq);
TRANS_FUNC_DEF(pmovsxdq);
TRANS_FUNC_DEF(pmovzxdq);
TRANS_FUNC_DEF(ptest);
TRANS_FUNC_DEF(pcmpeqq);
TRANS_FUNC_DEF(packusdw);
TRANS_FUNC_DEF(movntdqa);

TRANS_FUNC_DEF(callnext);
TRANS_FUNC_DEF(callthunk);
TRANS_FUNC_DEF(callin);
TRANS_FUNC_DEF(jmpin);
TRANS_FUNC_DEF(libfunc);

/* byhand functions */
TRANS_FUNC_DEF(add_byhand);
TRANS_FUNC_DEF(or_byhand);
TRANS_FUNC_DEF(adc_byhand);
TRANS_FUNC_DEF(and_byhand);
TRANS_FUNC_DEF(sub_byhand);
TRANS_FUNC_DEF(xor_byhand);
TRANS_FUNC_DEF(cmp_byhand);

TRANS_FUNC_DEF(neg_byhand);
TRANS_FUNC_DEF(mov_byhand);
TRANS_FUNC_DEF(movsx_byhand);
TRANS_FUNC_DEF(movzx_byhand);
TRANS_FUNC_DEF(test_byhand);
TRANS_FUNC_DEF(inc_byhand);
TRANS_FUNC_DEF(dec_byhand);
TRANS_FUNC_DEF(rol_byhand);
TRANS_FUNC_DEF(ror_byhand);
TRANS_FUNC_DEF(shl_byhand);
TRANS_FUNC_DEF(sal_byhand);
TRANS_FUNC_DEF(sar_byhand);
TRANS_FUNC_DEF(shr_byhand);
TRANS_FUNC_DEF(not_byhand);
TRANS_FUNC_DEF(mul_byhand);
TRANS_FUNC_DEF(div_byhand);
TRANS_FUNC_DEF(imul_byhand);
TRANS_FUNC_DEF(cmpxchg_byhand);

/* fpu */
TRANS_FUNC_DEF(wait);
TRANS_FUNC_DEF(f2xm1);
TRANS_FUNC_DEF(fabs);
TRANS_FUNC_DEF(fadd);
TRANS_FUNC_DEF(faddp);
TRANS_FUNC_DEF(fbld);
TRANS_FUNC_DEF(fbstp);
TRANS_FUNC_DEF(fchs);
TRANS_FUNC_DEF(fcmovcc);
TRANS_FUNC_DEF(fcom);
TRANS_FUNC_DEF(fcomi);
TRANS_FUNC_DEF(fcomip);
TRANS_FUNC_DEF(fcomp);
TRANS_FUNC_DEF(fcompp);
TRANS_FUNC_DEF(fcos);
TRANS_FUNC_DEF(fdecstp);
TRANS_FUNC_DEF(fdiv);
TRANS_FUNC_DEF(fdivp);
TRANS_FUNC_DEF(fdivr);
TRANS_FUNC_DEF(fdivrp);
TRANS_FUNC_DEF(ffree);
TRANS_FUNC_DEF(ffreep);
TRANS_FUNC_DEF(fiadd);
TRANS_FUNC_DEF(ficom);
TRANS_FUNC_DEF(ficomp);
TRANS_FUNC_DEF(fidiv);
TRANS_FUNC_DEF(fidivr);
TRANS_FUNC_DEF(fild);
TRANS_FUNC_DEF(fimul);
TRANS_FUNC_DEF(fincstp);
TRANS_FUNC_DEF(fist);
TRANS_FUNC_DEF(fistp);
TRANS_FUNC_DEF(fisttp);
TRANS_FUNC_DEF(fisub);
TRANS_FUNC_DEF(fisubr);
TRANS_FUNC_DEF(fld1);
TRANS_FUNC_DEF(fld);
TRANS_FUNC_DEF(fldcw);
TRANS_FUNC_DEF(fldenv);
TRANS_FUNC_DEF(fldl2e);
TRANS_FUNC_DEF(fldl2t);
TRANS_FUNC_DEF(fldlg2);
TRANS_FUNC_DEF(fldln2);
TRANS_FUNC_DEF(fldpi);
TRANS_FUNC_DEF(fldz);
TRANS_FUNC_DEF(fmul);
TRANS_FUNC_DEF(fmulp);
TRANS_FUNC_DEF(fnclex);
TRANS_FUNC_DEF(fninit);
TRANS_FUNC_DEF(fnop);
TRANS_FUNC_DEF(fnsave);
TRANS_FUNC_DEF(fnstcw);
TRANS_FUNC_DEF(fnstenv);
TRANS_FUNC_DEF(fnstsw);
TRANS_FUNC_DEF(fpatan);
TRANS_FUNC_DEF(fprem1);
TRANS_FUNC_DEF(fprem);
TRANS_FUNC_DEF(fptan);
TRANS_FUNC_DEF(frndint);
TRANS_FUNC_DEF(frstor);
TRANS_FUNC_DEF(fscale);
TRANS_FUNC_DEF(fsetpm);
TRANS_FUNC_DEF(fsin);
TRANS_FUNC_DEF(fsincos);
TRANS_FUNC_DEF(fsqrt);
TRANS_FUNC_DEF(fst);
TRANS_FUNC_DEF(fstp);
TRANS_FUNC_DEF(fsub);
TRANS_FUNC_DEF(fsubp);
TRANS_FUNC_DEF(fsubr);
TRANS_FUNC_DEF(fsubrp);
TRANS_FUNC_DEF(ftst);
TRANS_FUNC_DEF(fucom);
TRANS_FUNC_DEF(fucomi);
TRANS_FUNC_DEF(fucomip);
TRANS_FUNC_DEF(fucomp);
TRANS_FUNC_DEF(fucompp);
TRANS_FUNC_DEF(fxam);
TRANS_FUNC_DEF(fxch);
TRANS_FUNC_DEF(fxrstor);
TRANS_FUNC_DEF(fxsave);
TRANS_FUNC_DEF(fxtract);
TRANS_FUNC_DEF(fyl2x);
TRANS_FUNC_DEF(fyl2xp1);

/* fpu wraps */
TRANS_FUNC_DEF(wait_wrap);
TRANS_FUNC_DEF(f2xm1_wrap);
TRANS_FUNC_DEF(fabs_wrap);
TRANS_FUNC_DEF(fadd_wrap);
TRANS_FUNC_DEF(faddp_wrap);
TRANS_FUNC_DEF(fbld_wrap);
TRANS_FUNC_DEF(fbstp_wrap);
TRANS_FUNC_DEF(fchs_wrap);
TRANS_FUNC_DEF(fcmovcc_wrap);
TRANS_FUNC_DEF(fcom_wrap);
TRANS_FUNC_DEF(fcomi_wrap);
TRANS_FUNC_DEF(fcomip_wrap);
TRANS_FUNC_DEF(fcomp_wrap);
TRANS_FUNC_DEF(fcompp_wrap);
TRANS_FUNC_DEF(fcos_wrap);
TRANS_FUNC_DEF(fdecstp_wrap);
TRANS_FUNC_DEF(fdiv_wrap);
TRANS_FUNC_DEF(fdivp_wrap);
TRANS_FUNC_DEF(fdivr_wrap);
TRANS_FUNC_DEF(fdivrp_wrap);
TRANS_FUNC_DEF(ffree_wrap);
TRANS_FUNC_DEF(ffreep_wrap);
TRANS_FUNC_DEF(fiadd_wrap);
TRANS_FUNC_DEF(ficom_wrap);
TRANS_FUNC_DEF(ficomp_wrap);
TRANS_FUNC_DEF(fidiv_wrap);
TRANS_FUNC_DEF(fidivr_wrap);
TRANS_FUNC_DEF(fild_wrap);
TRANS_FUNC_DEF(fimul_wrap);
TRANS_FUNC_DEF(fincstp_wrap);
TRANS_FUNC_DEF(fist_wrap);
TRANS_FUNC_DEF(fistp_wrap);
TRANS_FUNC_DEF(fisttp_wrap);
TRANS_FUNC_DEF(fisub_wrap);
TRANS_FUNC_DEF(fisubr_wrap);
TRANS_FUNC_DEF(fld1_wrap);
TRANS_FUNC_DEF(fld_wrap);
TRANS_FUNC_DEF(fldcw_wrap);
TRANS_FUNC_DEF(fldenv_wrap);
TRANS_FUNC_DEF(fldl2e_wrap);
TRANS_FUNC_DEF(fldl2t_wrap);
TRANS_FUNC_DEF(fldlg2_wrap);
TRANS_FUNC_DEF(fldln2_wrap);
TRANS_FUNC_DEF(fldpi_wrap);
TRANS_FUNC_DEF(fldz_wrap);
TRANS_FUNC_DEF(fmul_wrap);
TRANS_FUNC_DEF(fmulp_wrap);
TRANS_FUNC_DEF(fnclex_wrap);
TRANS_FUNC_DEF(fninit_wrap);
TRANS_FUNC_DEF(fnop_wrap);
TRANS_FUNC_DEF(fnsave_wrap);
TRANS_FUNC_DEF(fnstcw_wrap);
TRANS_FUNC_DEF(fnstenv_wrap);
TRANS_FUNC_DEF(fnstsw_wrap);
TRANS_FUNC_DEF(fpatan_wrap);
TRANS_FUNC_DEF(fprem1_wrap);
TRANS_FUNC_DEF(fprem_wrap);
TRANS_FUNC_DEF(fptan_wrap);
TRANS_FUNC_DEF(frndint_wrap);
TRANS_FUNC_DEF(frstor_wrap);
TRANS_FUNC_DEF(fscale_wrap);
TRANS_FUNC_DEF(fsetpm_wrap);
TRANS_FUNC_DEF(fsin_wrap);
TRANS_FUNC_DEF(fsincos_wrap);
TRANS_FUNC_DEF(fsqrt_wrap);
TRANS_FUNC_DEF(fst_wrap);
TRANS_FUNC_DEF(fstp_wrap);
TRANS_FUNC_DEF(fsub_wrap);
TRANS_FUNC_DEF(fsubp_wrap);
TRANS_FUNC_DEF(fsubr_wrap);
TRANS_FUNC_DEF(fsubrp_wrap);
TRANS_FUNC_DEF(ftst_wrap);
TRANS_FUNC_DEF(fucom_wrap);
TRANS_FUNC_DEF(fucomi_wrap);
TRANS_FUNC_DEF(fucomip_wrap);
TRANS_FUNC_DEF(fucomp_wrap);
TRANS_FUNC_DEF(fucompp_wrap);
TRANS_FUNC_DEF(fxam_wrap);
TRANS_FUNC_DEF(fxch_wrap);
TRANS_FUNC_DEF(fxrstor_wrap);
TRANS_FUNC_DEF(fxsave_wrap);
TRANS_FUNC_DEF(fxtract_wrap);
TRANS_FUNC_DEF(fyl2x_wrap);
TRANS_FUNC_DEF(fyl2xp1_wrap);

/* sha */
TRANS_FUNC_DEF(sha1msg1);
TRANS_FUNC_DEF(sha1msg2);
TRANS_FUNC_DEF(sha1nexte);
TRANS_FUNC_DEF(sha1rnds4);
TRANS_FUNC_DEF(sha256msg1);
TRANS_FUNC_DEF(sha256msg2);
TRANS_FUNC_DEF(sha256rnds2);

TRANS_FUNC_DEF(vpaddx);
#ifdef CONFIG_LATX_AVX_OPT
/* f16c */
TRANS_FUNC_DEF(vcvtph2ps);
TRANS_FUNC_DEF(vcvtps2ph);

/* avx */
TRANS_FUNC_DEF(vpclmulqdq);
TRANS_FUNC_DEF(vtestpd);
TRANS_FUNC_DEF(vtestps);
TRANS_FUNC_DEF(vzeroall);
TRANS_FUNC_DEF(vpmulhrsw);
TRANS_FUNC_DEF(vphminposuw);
TRANS_FUNC_DEF(vphsubsw);
TRANS_FUNC_DEF(vphsubd);
TRANS_FUNC_DEF(vphsubw);
TRANS_FUNC_DEF(vpmaddubsw);
TRANS_FUNC_DEF(vpmaddwd);
TRANS_FUNC_DEF(vpmulhuw);
TRANS_FUNC_DEF(vpmulhw);
TRANS_FUNC_DEF(vpsignw);
TRANS_FUNC_DEF(vpsignd);
TRANS_FUNC_DEF(vpsignb);
TRANS_FUNC_DEF(vrcpss);
TRANS_FUNC_DEF(vrcpps);
TRANS_FUNC_DEF(vrsqrtss);
TRANS_FUNC_DEF(vrsqrtps);
TRANS_FUNC_DEF(vpermilpd);
TRANS_FUNC_DEF(vpermilps);
TRANS_FUNC_DEF(vpermpx);
TRANS_FUNC_DEF(vpsravd);
TRANS_FUNC_DEF(vpsllvq);
TRANS_FUNC_DEF(vpsllvd);
TRANS_FUNC_DEF(vpmaskmovx);
TRANS_FUNC_DEF(vpshufd);
TRANS_FUNC_DEF(vpmuldq);
TRANS_FUNC_DEF(vpinsrx);
TRANS_FUNC_DEF(vpminxx);
TRANS_FUNC_DEF(vpmaxxx);
TRANS_FUNC_DEF(vpalignr);

TRANS_FUNC_DEF(vmovsd);
TRANS_FUNC_DEF(vmovntdqa);
TRANS_FUNC_DEF(vmovlhps);
TRANS_FUNC_DEF(vmovhpd);
TRANS_FUNC_DEF(vmovhps);
TRANS_FUNC_DEF(vmovhlps);
TRANS_FUNC_DEF(vmovlpd);
TRANS_FUNC_DEF(vmovlps);
TRANS_FUNC_DEF(vmovq);
TRANS_FUNC_DEF(vmovapd);
TRANS_FUNC_DEF(vmovaps);
TRANS_FUNC_DEF(vmovddup);
TRANS_FUNC_DEF(vmovdqa);
TRANS_FUNC_DEF(vmovdqu);
TRANS_FUNC_DEF(vmovmskpd);
TRANS_FUNC_DEF(vmovmskps);
TRANS_FUNC_DEF(vmovntdq);
TRANS_FUNC_DEF(vmovntpd);
TRANS_FUNC_DEF(vmovntps);
TRANS_FUNC_DEF(vmovshdup);
TRANS_FUNC_DEF(vmovsldup);
TRANS_FUNC_DEF(vmovss);
//TRANS_FUNC_DEF(vmovsd);
TRANS_FUNC_DEF(vmovd);
//TRANS_FUNC_DEF(vmovq);
TRANS_FUNC_DEF(vlddqu);
TRANS_FUNC_DEF(vmovupd);
TRANS_FUNC_DEF(vmovups);
TRANS_FUNC_DEF(vmaskmovpx);
TRANS_FUNC_DEF(vpmovmskb);
TRANS_FUNC_DEF(vaddpd);
TRANS_FUNC_DEF(vaddps);
TRANS_FUNC_DEF(vaddsd);
TRANS_FUNC_DEF(vaddss);
TRANS_FUNC_DEF(vsubpd);
TRANS_FUNC_DEF(vsubps);
TRANS_FUNC_DEF(vsubsd);
TRANS_FUNC_DEF(vsubss);
TRANS_FUNC_DEF(vmulpd);
TRANS_FUNC_DEF(vmulps);
TRANS_FUNC_DEF(vmulsd);
TRANS_FUNC_DEF(vmulss);
TRANS_FUNC_DEF(vdivpd);
TRANS_FUNC_DEF(vdivps);
TRANS_FUNC_DEF(vdivsd);
TRANS_FUNC_DEF(vdivss);
TRANS_FUNC_DEF(vsqrtpd);
TRANS_FUNC_DEF(vsqrtps);
TRANS_FUNC_DEF(vsqrtsd);
TRANS_FUNC_DEF(vsqrtss);
TRANS_FUNC_DEF(vaddsubpd);
TRANS_FUNC_DEF(vaddsubps);
TRANS_FUNC_DEF(vhaddpd);
TRANS_FUNC_DEF(vhaddps);
TRANS_FUNC_DEF(vhsubpd);
TRANS_FUNC_DEF(vhsubps);
TRANS_FUNC_DEF(vandnpd);
TRANS_FUNC_DEF(vandnps);
TRANS_FUNC_DEF(vandpd);
TRANS_FUNC_DEF(vandps);
TRANS_FUNC_DEF(vorps);
TRANS_FUNC_DEF(vorpd);
TRANS_FUNC_DEF(vxorpd);
TRANS_FUNC_DEF(vxorps);
TRANS_FUNC_DEF(vmaxpx);
TRANS_FUNC_DEF(vmaxsx);
TRANS_FUNC_DEF(vminpx);
TRANS_FUNC_DEF(vminsx);
TRANS_FUNC_DEF(vblendpd);
TRANS_FUNC_DEF(vblendps);
TRANS_FUNC_DEF(vblendvpd);
TRANS_FUNC_DEF(vblendvps);
TRANS_FUNC_DEF(vbroadcastf128);
TRANS_FUNC_DEF(vbroadcasti128);
TRANS_FUNC_DEF(vbroadcastsd);
TRANS_FUNC_DEF(vbroadcastss);
TRANS_FUNC_DEF(vextractf128);
TRANS_FUNC_DEF(vextracti128);
TRANS_FUNC_DEF(vextractps);
TRANS_FUNC_DEF(vinsertf128);
TRANS_FUNC_DEF(vinserti128);
TRANS_FUNC_DEF(vinsertps);
TRANS_FUNC_DEF(vshufpd);
TRANS_FUNC_DEF(vshufps);
TRANS_FUNC_DEF(vunpckhpd);
TRANS_FUNC_DEF(vunpckhps);
TRANS_FUNC_DEF(vunpcklpd);
TRANS_FUNC_DEF(vunpcklps);
TRANS_FUNC_DEF(vpslldq);
TRANS_FUNC_DEF(vpsllx);
TRANS_FUNC_DEF(vpsrax);
TRANS_FUNC_DEF(vpsrldq);
TRANS_FUNC_DEF(vpsrlx);
TRANS_FUNC_DEF(vpcmpeqx);
TRANS_FUNC_DEF(vpcmpgtx);
TRANS_FUNC_DEF(vucomisd);
TRANS_FUNC_DEF(vucomiss);
TRANS_FUNC_DEF(vcomisd);
TRANS_FUNC_DEF(vcomiss);
TRANS_FUNC_DEF(vpabsx);
TRANS_FUNC_DEF(vpackusxx);
TRANS_FUNC_DEF(vpand);
TRANS_FUNC_DEF(vpandn);
TRANS_FUNC_DEF(vpblendd);
TRANS_FUNC_DEF(vpblendvb);
TRANS_FUNC_DEF(vpblendw);
TRANS_FUNC_DEF(vperm2f128);
TRANS_FUNC_DEF(vperm2i128);
TRANS_FUNC_DEF(vpermd);
TRANS_FUNC_DEF(vpermq);
TRANS_FUNC_DEF(vpextrx);
TRANS_FUNC_DEF(vpminux);
TRANS_FUNC_DEF(vpmovsxxx);
TRANS_FUNC_DEF(vpmovzxxx);
TRANS_FUNC_DEF(vpmullx);
TRANS_FUNC_DEF(vpor);
TRANS_FUNC_DEF(vpshufb);
TRANS_FUNC_DEF(vpsubx);
TRANS_FUNC_DEF(vptest);
TRANS_FUNC_DEF(vpunpckhxx);
TRANS_FUNC_DEF(vpunpcklxx);
TRANS_FUNC_DEF(vpxor);
TRANS_FUNC_DEF(vcvtdq2pd);
TRANS_FUNC_DEF(vcvtdq2ps);
TRANS_FUNC_DEF(vcvtpd2dq);
TRANS_FUNC_DEF(vcvtpd2ps);
TRANS_FUNC_DEF(vcvtps2dq);
TRANS_FUNC_DEF(vcvtps2pd);
TRANS_FUNC_DEF(vcvtsd2ss);
TRANS_FUNC_DEF(vcvtsi2sd);
TRANS_FUNC_DEF(vcvtsi2ss);
TRANS_FUNC_DEF(vcvtss2sd);
TRANS_FUNC_DEF(vcvttpd2dq);
TRANS_FUNC_DEF(vcvttps2dq);
TRANS_FUNC_DEF(vfmaddxxxpd);
TRANS_FUNC_DEF(vfmaddxxxps);
TRANS_FUNC_DEF(vfmaddxxxsd);
TRANS_FUNC_DEF(vfmaddxxxss);
TRANS_FUNC_DEF(vfmaddsubxxxpd);
TRANS_FUNC_DEF(vfmaddsubxxxps);
TRANS_FUNC_DEF(vfmsubaddxxxpd);
TRANS_FUNC_DEF(vfmsubaddxxxps);
TRANS_FUNC_DEF(vfmsubxxxpd);
TRANS_FUNC_DEF(vfmsubxxxps);
TRANS_FUNC_DEF(vfmsubxxxsd);
TRANS_FUNC_DEF(vfmsubxxxss);
TRANS_FUNC_DEF(vfnmaddxxxpd);
TRANS_FUNC_DEF(vfnmaddxxxps);
TRANS_FUNC_DEF(vfnmaddxxxsd);
TRANS_FUNC_DEF(vfnmaddxxxss);
TRANS_FUNC_DEF(vfnmsubxxxpd);
TRANS_FUNC_DEF(vfnmsubxxxps);
TRANS_FUNC_DEF(vfnmsubxxxsd);
TRANS_FUNC_DEF(vfnmsubxxxss);
TRANS_FUNC_DEF(vcmppd);
TRANS_FUNC_DEF(vcmpeqpd);
TRANS_FUNC_DEF(vcmpltpd);
TRANS_FUNC_DEF(vcmplepd);
TRANS_FUNC_DEF(vcmpunordpd);
TRANS_FUNC_DEF(vcmpneqpd);
TRANS_FUNC_DEF(vcmpnltpd);
TRANS_FUNC_DEF(vcmpnlepd);
TRANS_FUNC_DEF(vcmpordpd);
TRANS_FUNC_DEF(vcmpeq_uqpd);
TRANS_FUNC_DEF(vcmpngepd);
TRANS_FUNC_DEF(vcmpngtpd);
TRANS_FUNC_DEF(vcmpfalsepd);
TRANS_FUNC_DEF(vcmpneq_oqpd);
TRANS_FUNC_DEF(vcmpgepd);
TRANS_FUNC_DEF(vcmpgtpd);
TRANS_FUNC_DEF(vcmptruepd);
TRANS_FUNC_DEF(vcmpeq_ospd);
TRANS_FUNC_DEF(vcmplt_oqpd);
TRANS_FUNC_DEF(vcmple_oqpd);
TRANS_FUNC_DEF(vcmpunord_spd);
TRANS_FUNC_DEF(vcmpneq_uspd);
TRANS_FUNC_DEF(vcmpnlt_uqpd);
TRANS_FUNC_DEF(vcmpnle_uqpd);
TRANS_FUNC_DEF(vcmpord_spd);
TRANS_FUNC_DEF(vcmpeq_uspd);
TRANS_FUNC_DEF(vcmpnge_uqpd);
TRANS_FUNC_DEF(vcmpngt_uqpd);
TRANS_FUNC_DEF(vcmpfalse_ospd);
TRANS_FUNC_DEF(vcmpneq_ospd);
TRANS_FUNC_DEF(vcmpge_oqpd);
TRANS_FUNC_DEF(vcmpgt_oqpd);
TRANS_FUNC_DEF(vcmptrue_uspd);
TRANS_FUNC_DEF(vcmpps);
TRANS_FUNC_DEF(vcmpeqps);
TRANS_FUNC_DEF(vcmpltps);
TRANS_FUNC_DEF(vcmpleps);
TRANS_FUNC_DEF(vcmpunordps);
TRANS_FUNC_DEF(vcmpneqps);
TRANS_FUNC_DEF(vcmpnltps);
TRANS_FUNC_DEF(vcmpnleps);
TRANS_FUNC_DEF(vcmpordps);
TRANS_FUNC_DEF(vcmpeq_uqps);
TRANS_FUNC_DEF(vcmpngeps);
TRANS_FUNC_DEF(vcmpngtps);
TRANS_FUNC_DEF(vcmpfalseps);
TRANS_FUNC_DEF(vcmpneq_oqps);
TRANS_FUNC_DEF(vcmpgeps);
TRANS_FUNC_DEF(vcmpgtps);
TRANS_FUNC_DEF(vcmptrueps);
TRANS_FUNC_DEF(vcmpeq_osps);
TRANS_FUNC_DEF(vcmplt_oqps);
TRANS_FUNC_DEF(vcmple_oqps);
TRANS_FUNC_DEF(vcmpunord_sps);
TRANS_FUNC_DEF(vcmpneq_usps);
TRANS_FUNC_DEF(vcmpnlt_uqps);
TRANS_FUNC_DEF(vcmpnle_uqps);
TRANS_FUNC_DEF(vcmpord_sps);
TRANS_FUNC_DEF(vcmpeq_usps);
TRANS_FUNC_DEF(vcmpnge_uqps);
TRANS_FUNC_DEF(vcmpngt_uqps);
TRANS_FUNC_DEF(vcmpfalse_osps);
TRANS_FUNC_DEF(vcmpneq_osps);
TRANS_FUNC_DEF(vcmpge_oqps);
TRANS_FUNC_DEF(vcmpgt_oqps);
TRANS_FUNC_DEF(vcmptrue_usps);
TRANS_FUNC_DEF(vcmpsd);
TRANS_FUNC_DEF(vcmpeqsd);
TRANS_FUNC_DEF(vcmpltsd);
TRANS_FUNC_DEF(vcmplesd);
TRANS_FUNC_DEF(vcmpunordsd);
TRANS_FUNC_DEF(vcmpneqsd);
TRANS_FUNC_DEF(vcmpnltsd);
TRANS_FUNC_DEF(vcmpnlesd);
TRANS_FUNC_DEF(vcmpordsd);
TRANS_FUNC_DEF(vcmpeq_uqsd);
TRANS_FUNC_DEF(vcmpngesd);
TRANS_FUNC_DEF(vcmpngtsd);
TRANS_FUNC_DEF(vcmpfalsesd);
TRANS_FUNC_DEF(vcmpneq_oqsd);
TRANS_FUNC_DEF(vcmpgesd);
TRANS_FUNC_DEF(vcmpgtsd);
TRANS_FUNC_DEF(vcmptruesd);
TRANS_FUNC_DEF(vcmpeq_ossd);
TRANS_FUNC_DEF(vcmplt_oqsd);
TRANS_FUNC_DEF(vcmple_oqsd);
TRANS_FUNC_DEF(vcmpunord_ssd);
TRANS_FUNC_DEF(vcmpneq_ussd);
TRANS_FUNC_DEF(vcmpnlt_uqsd);
TRANS_FUNC_DEF(vcmpnle_uqsd);
TRANS_FUNC_DEF(vcmpord_ssd);
TRANS_FUNC_DEF(vcmpeq_ussd);
TRANS_FUNC_DEF(vcmpnge_uqsd);
TRANS_FUNC_DEF(vcmpngt_uqsd);
TRANS_FUNC_DEF(vcmpfalse_ossd);
TRANS_FUNC_DEF(vcmpneq_ossd);
TRANS_FUNC_DEF(vcmpge_oqsd);
TRANS_FUNC_DEF(vcmpgt_oqsd);
TRANS_FUNC_DEF(vcmptrue_ussd);
TRANS_FUNC_DEF(vcmpss);
TRANS_FUNC_DEF(vcmpeqss);
TRANS_FUNC_DEF(vcmpltss);
TRANS_FUNC_DEF(vcmpless);
TRANS_FUNC_DEF(vcmpunordss);
TRANS_FUNC_DEF(vcmpneqss);
TRANS_FUNC_DEF(vcmpnltss);
TRANS_FUNC_DEF(vcmpnless);
TRANS_FUNC_DEF(vcmpordss);
TRANS_FUNC_DEF(vcmpeq_uqss);
TRANS_FUNC_DEF(vcmpngess);
TRANS_FUNC_DEF(vcmpngtss);
TRANS_FUNC_DEF(vcmpfalsess);
TRANS_FUNC_DEF(vcmpneq_oqss);
TRANS_FUNC_DEF(vcmpgess);
TRANS_FUNC_DEF(vcmpgtss);
TRANS_FUNC_DEF(vcmptruess);
TRANS_FUNC_DEF(vcmpeq_osss);
TRANS_FUNC_DEF(vcmplt_oqss);
TRANS_FUNC_DEF(vcmple_oqss);
TRANS_FUNC_DEF(vcmpunord_sss);
TRANS_FUNC_DEF(vcmpneq_usss);
TRANS_FUNC_DEF(vcmpnlt_uqss);
TRANS_FUNC_DEF(vcmpnle_uqss);
TRANS_FUNC_DEF(vcmpord_sss);
TRANS_FUNC_DEF(vcmpeq_usss);
TRANS_FUNC_DEF(vcmpnge_uqss);
TRANS_FUNC_DEF(vcmpngt_uqss);
TRANS_FUNC_DEF(vcmpfalse_osss);
TRANS_FUNC_DEF(vcmpneq_osss);
TRANS_FUNC_DEF(vcmpge_oqss);
TRANS_FUNC_DEF(vcmpgt_oqss);
TRANS_FUNC_DEF(vcmptrue_usss);
TRANS_FUNC_DEF(vpbroadcastq);
TRANS_FUNC_DEF(vpaddq);
TRANS_FUNC_DEF(vzeroupper);

//TRANS_FUNC_DEF(vpinsrb);
//TRANS_FUNC_DEF(vpinsrq);
TRANS_FUNC_DEF(xgetbv);
TRANS_FUNC_DEF(xsetbv);
TRANS_FUNC_DEF(xsave);
TRANS_FUNC_DEF(xsaveopt);
TRANS_FUNC_DEF(xrstor);
TRANS_FUNC_DEF(xgetbv_wrap);
TRANS_FUNC_DEF(xsetbv_wrap);
TRANS_FUNC_DEF(xsave_wrap);
TRANS_FUNC_DEF(xsaveopt_wrap);
TRANS_FUNC_DEF(xrstor_wrap);
#endif

TRANS_FUNC_DEF(andn);
TRANS_FUNC_DEF(movbe);
TRANS_FUNC_DEF(rorx);
//TRANS_FUNC_DEF(vpmaxsd);
//TRANS_FUNC_DEF(vpminsd);
TRANS_FUNC_DEF(blsi);

#ifdef CONFIG_LATX_AVX_OPT
TRANS_FUNC_DEF(vpcmpestri);
#else
TRANS_FUNC_DEF(pcmpestri);
#endif

#ifdef CONFIG_LATX_AVX_OPT
TRANS_FUNC_DEF(vpackssxx);
TRANS_FUNC_DEF(vpshufhw);
TRANS_FUNC_DEF(vpshuflw);
TRANS_FUNC_DEF(vpavgb);
TRANS_FUNC_DEF(vpavgw);
TRANS_FUNC_DEF(vdppd);
TRANS_FUNC_DEF(vdpps);
TRANS_FUNC_DEF(vmpsadbw);
TRANS_FUNC_DEF(vphaddw);
TRANS_FUNC_DEF(vphaddd);
TRANS_FUNC_DEF(vphaddsw);
TRANS_FUNC_DEF(vpsadbw);
TRANS_FUNC_DEF(vroundps);
TRANS_FUNC_DEF(vroundss);
TRANS_FUNC_DEF(vroundpd);
TRANS_FUNC_DEF(vroundsd);
#endif
TRANS_FUNC_DEF(pcmpestrm);

#ifdef CONFIG_LATX_AVX_OPT
TRANS_FUNC_DEF(vpcmpistri);
#else
TRANS_FUNC_DEF(pcmpistri);
#endif

TRANS_FUNC_DEF(pcmpistrm);
#ifdef CONFIG_LATX_AVX_OPT
TRANS_FUNC_DEF(vpcmpestrm);
TRANS_FUNC_DEF(vpcmpistrm);
#endif
TRANS_FUNC_DEF(aesdec);
TRANS_FUNC_DEF(aesdeclast);
TRANS_FUNC_DEF(aesenc);
TRANS_FUNC_DEF(aesenclast);
TRANS_FUNC_DEF(aesimc);
TRANS_FUNC_DEF(aeskeygenassist);

#ifdef CONFIG_LATX_AVX_OPT
TRANS_FUNC_DEF(vaesdec);
TRANS_FUNC_DEF(vaesdeclast);
TRANS_FUNC_DEF(vaesenc);
TRANS_FUNC_DEF(vaesenclast);
TRANS_FUNC_DEF(vaesimc);
TRANS_FUNC_DEF(vaeskeygenassist);
TRANS_FUNC_DEF(vpsrlvd);
TRANS_FUNC_DEF(vpsrlvq);
TRANS_FUNC_DEF(vpgatherdd);
TRANS_FUNC_DEF(vpgatherqd);
TRANS_FUNC_DEF(vpgatherdq);
TRANS_FUNC_DEF(vpgatherqq);
#endif

TRANS_FUNC_DEF(pext);
TRANS_FUNC_DEF(pdep);
TRANS_FUNC_DEF(bextr);
TRANS_FUNC_DEF(blsmsk);
TRANS_FUNC_DEF(bzhi);
TRANS_FUNC_DEF(lzcnt);
TRANS_FUNC_DEF(adcx);
TRANS_FUNC_DEF(adox);

#ifdef CONFIG_LATX_AVX_OPT
TRANS_FUNC_DEF(vpbroadcastd);
TRANS_FUNC_DEF(vpbroadcastb);
TRANS_FUNC_DEF(vpbroadcastw);
#endif

TRANS_FUNC_DEF(crc32);
TRANS_FUNC_DEF(salc);
TRANS_FUNC_DEF(pclmulqdq);

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
int8 get_etb_type(IR1_INST *pir1);
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

void tr_fpu_load_tag_to_env(IR2_OPND fpu_tag);
void tr_fpu_store_tag_to_mem(IR2_OPND mem_opnd, int mem_imm);

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
void update_fcsr_by_sw(IR2_OPND sw);

void tr_gen_call_to_helper(ADDR, enum aot_rel_kind);
void convert_fpregs_64_to_x80(void);
void convert_fpregs_x80_to_64(void);
void helper_raise_int(void);
void helper_raise_illop(void);
void helper_raise_trapop(void);
void helper_raise_gpf(void);
void helper_raise_syscall(void);

bool si12_overflow(long si12);

/* Loongarch V1.1 */
int have_scq(void);

/* operand conversion */
IR2_OPND convert_mem(IR1_OPND *, int *);
IR2_OPND mem_imm_add_disp(IR2_OPND, int *, int);
IR2_OPND convert_mem_no_offset(IR1_OPND *);
void convert_mem_to_specific_gpr(IR1_OPND *, IR2_OPND, int);
IR2_OPND convert_mem_to_itemp(IR1_OPND *opnd0);
IR2_OPND convert_gpr_opnd(IR1_OPND *, EXTENSION_MODE);
IR2_OPND load_freg128_from_ir1(IR1_OPND *);
void load_imm_to_ir1_opnd_gpr(IR1_OPND *opnd0, uint64_t imm);
IR2_OPND load_freg256_from_ir1(IR1_OPND *opnd1);
void set_high128_xreg_to_zero(IR2_OPND opnd);
void store_freg256_to_ir1_mem(IR2_OPND opnd2,IR1_OPND *opnd1);
void load_freg256_from_ir1_mem(IR2_OPND opnd2,IR1_OPND *opnd1);

#ifdef CONFIG_LATX_AOT
/* TODO */
void load_ireg_from_host_addr(IR2_OPND opnd2, uint64 value);
void load_ireg_from_guest_addr(IR2_OPND opnd2, uint64 value);
#endif

void load_ireg_from_ir1_mem(IR2_OPND opnd2, IR1_OPND *opnd1,
                                   EXTENSION_MODE em, bool is_xmm_hi);
IR2_OPND load_ireg_from_ir1(IR1_OPND *, EXTENSION_MODE, bool is_xmm_hi);
void load_ireg_from_ir1_2(IR2_OPND, IR1_OPND *, EXTENSION_MODE, bool is_xmm_hi);
void store_ireg_to_ir1_seg(IR2_OPND seg_value_opnd, IR1_OPND *opnd1);
void store_ireg_to_ir1(IR2_OPND, IR1_OPND *, bool is_xmm_hi);

IR2_OPND load_ireg_from_ir2_mem(IR2_OPND mem_opnd, int mem_imm, int mem_size,
                                   EXTENSION_MODE em, bool is_xmm_hi);
void store_ireg_to_ir2_mem(IR2_OPND value_opnd, IR2_OPND mem_opnd,
                                  int mem_imm, int mem_size, bool is_xmm_hi);

void load_ireg_from_cf_opnd(IR2_OPND *a);

/* load to freg */
IR2_OPND load_freg_from_ir1_1(IR1_OPND *opnd1, bool is_xmm_hi, uint32_t options);
void load_freg_from_ir1_2(IR2_OPND opnd2, IR1_OPND *opnd1, uint32_t options);
void store_freg_to_ir1(IR2_OPND, IR1_OPND *, bool is_xmm_hi, bool is_convert);
void store_64_bit_freg_to_ir1_80_bit_mem(IR2_OPND, IR2_OPND, int);
void store_freg128_to_ir1_mem(IR2_OPND opnd2, IR1_OPND *opnd1);
void load_freg128_from_ir1_mem(IR2_OPND opnd2, IR1_OPND *opnd1);
void load_64_bit_freg_from_ir1_80_bit_mem(IR2_OPND opnd2,
                                                 IR2_OPND mem_opnd, int mem_imm);

/* set/clear lsenv->mode_trans_mmx_fputo to transfer to MMX/FPU mode */
void transfer_to_mmx_mode(void);
void transfer_to_fpu_mode(void);

/* load two singles from ir1 pack */
void load_singles_from_ir1_pack(IR2_OPND single0, IR2_OPND single1,
                                       IR1_OPND *opnd1, bool is_xmm_hi);
/* store two single into a pack */
void store_singles_to_ir2_pack(IR2_OPND single0, IR2_OPND single1,
                                      IR2_OPND pack);

/* fcsr */
void update_sw_by_fcsr(IR2_OPND sw_opnd);
void update_fcsr_by_cw(IR2_OPND cw);
IR2_OPND set_fpu_fcsr_rounding_field_by_x86(void);
void set_fpu_rounding_mode(IR2_OPND rm);

int generate_native_rotate_fpu_by(void *code_buf);
void generate_context_switch_bt_to_native(void *code_buf);
void generate_context_switch_native_to_bt(void);

void generate_eflag_calculation(IR2_OPND, IR2_OPND, IR2_OPND, IR1_INST *, bool);

#ifdef CONFIG_LATX_XCOMISX_OPT
void generate_xcomisx(IR2_OPND, IR2_OPND, bool, bool, uint8_t);
#endif

/* extern ADDR tb_look_up_native; */

void tr_generate_exit_tb(IR1_INST *branch, int succ_id);
#ifdef CONFIG_LATX_XCOMISX_OPT
void tr_generate_exit_stub_tb(IR1_INST *branch, int succ_id, void *func, IR1_INST *stub);
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

void gen_softfpu_helper_prologue(IR1_INST *pir1);
void gen_softfpu_helper_epilogue(IR1_INST *pir1);
void update_fcsr_rm(IR2_OPND control_word, IR2_OPND fcsr);

bool ir1_need_reserve_h128(IR1_INST *ir1);
IR2_OPND save_h128_of_ymm(IR1_INST *ir1);
void restore_h128_of_ymm(IR1_INST *ir1, IR2_OPND temp);
void gen_test_page_flag(IR2_OPND mem_opnd, int mem_imm, uint32_t flag);

void clear_h32(IR2_OPND *opnd);

#include "qemu-def.h"

#define NONE            0
#define IS_CONVERT      1
#define IS_XMM_HI       (1 << 2)
#define IS_INTEGER      (1 << 3)

#endif
