#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "lsenv.h"
#include "wrapper.h"
#include "complext.h"
#include "myalign.h"
#ifdef TARGET_X86_64
#endif
/*
    R_EAX = 0,
    R_ECX = 1,
    R_EDX = 2,
    R_EBX = 3,
    R_ESP = 4,
    R_EBP = 5,
    R_ESI = 6,
    R_EDI = 7,
    R_R8 = 8,
    R_R9 = 9,
    R_R10 = 10,
    R_R11 = 11,
    R_R12 = 12,
    R_R13 = 13,
    R_R14 = 14,
    R_R15 = 15,
*/
extern void* my__IO_2_1_stdin_ ;
extern void* my__IO_2_1_stdout_;
extern void* my__IO_2_1_stderr_;

#ifdef TARGET_X86_64
#ifdef CONFIG_LATX_DEBUG
#define DEBUG_LOG \
    if (kzt_call_log) { \
        Dl_info dl_info; \
        int ret = dladdr((const void *)fcn, &dl_info); \
        if (ret != -1) { \
            printf_kzt_call(LOG_INFO, "pid %d %llx call %s(%p,%p,%p,%p,%p,%p,%p,%p,%p,%p,%p,%p) = 0x%lx from %s\n",  \
                getpid(), \
                (unsigned long long)pthread_self(), \
                dl_info.dli_sname, \
                (void *)R_RDI, \
                (void *)R_RSI, \
                (void*)R_RDX, \
                (void*)R_RCX, \
                (void*)R_R8, \
                (void*)R_R9, \
                *(void**)(R_RSP + 8), \
                *(void**)(R_RSP + 16), \
                *(void**)(R_RSP + 24), \
                *(void**)(R_RSP + 32), \
                *(void**)(R_RSP + 40), \
                *(void**)(R_RSP + 48), \
                R_RAX, \
                dl_info.dli_fname); \
        } else { \
            fprintf(stderr, "call dladdr err 0x%lx ret =%d \n", fcn, ret); \
        } \
    }
#else
#define DEBUG_LOG (void)0;
#endif
#define __CPU CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
#define R_RAX cpu->regs[R_EAX]
#define R_RDI cpu->regs[R_EDI]
#define R_RSI cpu->regs[R_ESI]
#define R_RDX cpu->regs[R_EDX]
#define R_RCX cpu->regs[R_ECX]
#define R_R8 cpu->regs[R_R8]
#define R_R9 cpu->regs[R_R9]
#define R_RSP cpu->regs[R_ESP]
#define R_XMMD(r) (*(double*)&cpu->xmm_regs->ZMM_D(r))
#define R_XMMS(r) (*(float*)&cpu->xmm_regs->ZMM_S(r))
#define ST0val (cpu->fpregs[cpu->fpstt].d)
#else
#define __CPU CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
#define R_RAX cpu->regs[R_EAX]
#define R_RDI cpu->regs[R_EAX]
#define R_RSI cpu->regs[R_EAX]
#define R_RDX cpu->regs[R_EAX]
#define R_RCX cpu->regs[R_EAX]
#define R_R8 cpu->regs[R_EAX]
#define R_R9 cpu->regs[R_EAX]
#define R_RSP cpu->regs[R_EAX]

#endif

typedef uintptr_t (*vFE_t)(void);
typedef int64_t (*iFii_t)(int64_t, int64_t);
typedef void* (*pFpiL_t)(void*, int64_t, uintptr_t);
typedef void* (*pFppl_t)(void*, void*, intptr_t);
typedef void* (*pFppL_t)(void*, void*, uintptr_t);
typedef void* (*pFppiL_t)(void*, void*, int64_t, uintptr_t);
typedef void* (*pFppuL_t)(void*, void*, uint64_t, uintptr_t);
typedef void (*vFpLLp_t)(void*, uintptr_t, uintptr_t, void*);

typedef int64_t (*iFippi_t)(int64_t, void*, void*, int64_t);
typedef void (*vFpppi_t)(void*, void*, void*, int64_t);
typedef int64_t (*iFippp_t)(int64_t, void*, void*, void*);
typedef void* (*pFppiiiiiip_t)(void*, void*, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, void*);
typedef void (*vFpppuuuu_t)(void*, void*, void*, uint64_t, uint64_t, uint64_t, uint64_t);
typedef void (*vFppiip_t)(void*, void*, int64_t, int64_t, void*);
typedef void (*vFppiiip_t)(void*, void*, int64_t, int64_t, int64_t, void*);
typedef void (*vFppii_t)(void*, void*, int64_t, int64_t);

void iFippi(uintptr_t fcn) { __CPU; iFippi_t fn = (iFippi_t)fcn; R_RAX=(int64_t)fn((int64_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (int64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpppi(uintptr_t fcn) { __CPU; vFpppi_t fn = (vFpppi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFippp(uintptr_t fcn) { __CPU; iFippp_t fn = (iFippp_t)fcn; R_RAX=(int64_t)fn((int64_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFppiiiiiip(uintptr_t fcn) {__CPU; pFppiiiiiip_t fn = (pFppiiiiiip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX, (int64_t)R_R8, (int64_t)R_R9, *(int64_t*)(R_RSP + 8), *(int64_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFpppuuuu(uintptr_t fcn) { __CPU; vFpppuuuu_t fn = (vFpppuuuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint64_t)R_RCX, (uint64_t)R_R8, (uint64_t)R_R9, *(uint64_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFppiip(uintptr_t fcn) { __CPU; vFppiip_t fn = (vFppiip_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFppiiip(uintptr_t fcn) { __CPU; vFppiiip_t fn = (vFppiiip_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX, (int64_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFppii(uintptr_t fcn) { __CPU; vFppii_t fn = (vFppii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFE(uintptr_t fcn) { __CPU; vFE_t fn = (vFE_t)fcn; fn(); DEBUG_LOG; (void)cpu; }
void pFppL(uintptr_t fcn) { __CPU; pFppL_t fn = (pFppL_t)fcn; R_RAX = (uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }

typedef void (*vFv_t)(void);
typedef void (*vFi_t)(int32_t);
typedef void (*vFu_t)(uint32_t);
typedef void (*vFU_t)(uint64_t);
typedef void (*vFf_t)(float);
typedef void (*vFd_t)(double);
typedef void (*vFl_t)(intptr_t);
typedef void (*vFp_t)(void*);
typedef int32_t (*iFv_t)(void);
typedef int32_t (*iFi_t)(int32_t);
typedef int32_t (*iFu_t)(uint32_t);
typedef int32_t (*iFU_t)(uint64_t);
typedef int32_t (*iFp_t)(void*);
typedef uint16_t (*WFi_t)(int32_t);
typedef uint32_t (*uFv_t)(void);
typedef uint32_t (*uFi_t)(int32_t);
typedef uint32_t (*uFu_t)(uint32_t);
typedef uint32_t (*uFl_t)(intptr_t);
typedef uint32_t (*uFp_t)(void*);
typedef uint64_t (*UFv_t)(void);
typedef uint64_t (*UFu_t)(uint32_t);
typedef intptr_t (*lFu_t)(uint32_t);
typedef intptr_t (*lFp_t)(void*);
typedef uintptr_t (*LFv_t)(void);
typedef uintptr_t (*LFp_t)(void*);
typedef void* (*pFv_t)(void);
typedef void* (*pFi_t)(int32_t);
typedef void* (*pFu_t)(uint32_t);
typedef void* (*pFL_t)(uintptr_t);
typedef void* (*pFp_t)(void*);
typedef void (*vFii_t)(int32_t, int32_t);
typedef void (*vFiI_t)(int32_t, int64_t);
typedef void (*vFiu_t)(int32_t, uint32_t);
typedef void (*vFiU_t)(int32_t, uint64_t);
typedef void (*vFif_t)(int32_t, float);
typedef void (*vFid_t)(int32_t, double);
typedef void (*vFip_t)(int32_t, void*);
typedef void (*vFui_t)(uint32_t, int32_t);
typedef void (*vFuu_t)(uint32_t, uint32_t);
typedef void (*vFuU_t)(uint32_t, uint64_t);
typedef void (*vFuf_t)(uint32_t, float);
typedef void (*vFud_t)(uint32_t, double);
typedef void (*vFul_t)(uint32_t, intptr_t);
typedef void (*vFup_t)(uint32_t, void*);
typedef void* (*pFupi_t)(uint32_t, void*, int32_t);
typedef void (*vFUi_t)(uint64_t, int32_t);
typedef void (*vFfi_t)(float, int32_t);
typedef void (*vFff_t)(float, float);
typedef void (*vFdd_t)(double, double);
typedef void (*vFlp_t)(intptr_t, void*);
typedef void (*vFpi_t)(void*, int32_t);
typedef void (*vFpl_t)(void*, intptr_t);
typedef void (*vFpL_t)(void*, uintptr_t);
typedef void (*vFpp_t)(void*, void*);
typedef void (*vFppuu_t)(void*, void*, uint32_t, uint32_t);
typedef int32_t (*iFEp_t)(void*);
typedef int32_t (*iFiu_t)(int32_t, uint32_t);
typedef int32_t (*iFip_t)(int32_t, void*);
typedef int32_t (*iFui_t)(uint32_t, int32_t);
typedef int32_t (*iFuu_t)(uint32_t, uint32_t);
typedef int32_t (*iFuU_t)(uint32_t, uint64_t);
typedef int32_t (*iFup_t)(uint32_t, void*);
typedef int32_t (*iFpi_t)(void*, int32_t);
typedef int32_t (*iFpu_t)(void*, uint32_t);
typedef int32_t (*iFpl_t)(void*, intptr_t);
typedef int32_t (*iFpL_t)(void*, uintptr_t);
typedef int32_t (*iFpp_t)(void*, void*);
typedef uint32_t (*uFiu_t)(int32_t, uint32_t);
typedef uint32_t (*uFuu_t)(uint32_t, uint32_t);
typedef uint32_t (*uFup_t)(uint32_t, void*);
typedef uint32_t (*uFpU_t)(void*, uint64_t);
typedef uint32_t (*uFpp_t)(void*, void*);
typedef uint64_t (*UFuu_t)(uint32_t, uint32_t);
typedef uintptr_t (*LFpi_t)(void*, int32_t);
typedef void* (*pFEp_t)(void*);
typedef void* (*pFii_t)(int32_t, int32_t);
typedef void* (*pFiu_t)(int32_t, uint32_t);
typedef void* (*pFui_t)(uint32_t, int32_t);
typedef void* (*pFuu_t)(uint32_t, uint32_t);
typedef void* (*pFpi_t)(void*, int32_t);
typedef void* (*pFpu_t)(void*, uint32_t);
typedef void* (*pFpL_t)(void*, uintptr_t);
typedef void* (*pFpp_t)(void*, void*);
typedef void* (*pFpppuupp_t)(void*, void*, void*, uint32_t, uint32_t, void*, void*);
typedef void (*vFEpp_t)(void*, void*);
typedef void (*vFiii_t)(int32_t, int32_t, int32_t);
typedef void (*vFiiu_t)(int32_t, int32_t, uint32_t);
typedef void (*vFiif_t)(int32_t, int32_t, float);
typedef void (*vFiip_t)(int32_t, int32_t, void*);
typedef void (*vFiII_t)(int32_t, int64_t, int64_t);
typedef void (*vFiui_t)(int32_t, uint32_t, int32_t);
typedef void (*vFiuu_t)(int32_t, uint32_t, uint32_t);
typedef void (*vFiuU_t)(int32_t, uint32_t, uint64_t);
typedef void (*vFiup_t)(int32_t, uint32_t, void*);
typedef void (*vFiUU_t)(int32_t, uint64_t, uint64_t);
typedef void (*vFiff_t)(int32_t, float, float);
typedef void (*vFidd_t)(int32_t, double, double);
typedef void (*vFill_t)(int32_t, intptr_t, intptr_t);
typedef void (*vFilp_t)(int32_t, intptr_t, void*);
typedef void (*vFipi_t)(int32_t, void*, int32_t);
typedef void (*vFipu_t)(int32_t, void*, uint32_t);
typedef void (*vFipp_t)(int32_t, void*, void*);
typedef void (*vFuii_t)(uint32_t, int32_t, int32_t);
typedef void (*vFuiI_t)(uint32_t, int32_t, int64_t);
typedef void (*vFuiu_t)(uint32_t, int32_t, uint32_t);
typedef void (*vFuiU_t)(uint32_t, int32_t, uint64_t);
typedef void (*vFuif_t)(uint32_t, int32_t, float);
typedef void (*vFuid_t)(uint32_t, int32_t, double);
typedef void (*vFuip_t)(uint32_t, int32_t, void*);
typedef void (*vFuui_t)(uint32_t, uint32_t, int32_t);
typedef void (*vFuuu_t)(uint32_t, uint32_t, uint32_t);
typedef void (*vFuuU_t)(uint32_t, uint32_t, uint64_t);
typedef void (*vFuuf_t)(uint32_t, uint32_t, float);
typedef void (*vFuud_t)(uint32_t, uint32_t, double);
typedef void (*vFuup_t)(uint32_t, uint32_t, void*);
typedef void (*vFuff_t)(uint32_t, float, float);
typedef void (*vFudd_t)(uint32_t, double, double);
typedef void (*vFull_t)(uint32_t, intptr_t, intptr_t);
typedef void (*vFulp_t)(uint32_t, intptr_t, void*);
typedef void (*vFupp_t)(uint32_t, void*, void*);
typedef void (*vFfff_t)(float, float, float);
typedef void (*vFddd_t)(double, double, double);
typedef void (*vFlip_t)(intptr_t, int32_t, void*);
typedef void (*vFlll_t)(intptr_t, intptr_t, intptr_t);
typedef void (*vFllp_t)(intptr_t, intptr_t, void*);
typedef void (*vFlpp_t)(intptr_t, void*, void*);
typedef void (*vFLpp_t)(uintptr_t, void*, void*);
typedef void (*vFpiu_t)(void*, int32_t, uint32_t);
typedef void (*vFpif_t)(void*, int32_t, float);
typedef void (*vFpid_t)(void*, int32_t, double);
typedef void (*vFpip_t)(void*, int32_t, void*);
typedef void (*vFpui_t)(void*, uint32_t, int32_t);
typedef void (*vFpuU_t)(void*, uint32_t, uint64_t);
typedef void (*vFplp_t)(void*, intptr_t, void*);
typedef void (*vFppi_t)(void*, void*, int32_t);
typedef void (*vFppu_t)(void*, void*, uint32_t);
typedef void (*vFppp_t)(void*, void*, void*);
typedef int32_t (*iFiip_t)(int32_t, int32_t, void*);
typedef int32_t (*iFipp_t)(int32_t, void*, void*);
typedef int32_t (*iFuip_t)(uint32_t, int32_t, void*);
typedef int32_t (*iFuup_t)(uint32_t, uint32_t, void*);
typedef int32_t (*iFuUu_t)(uint32_t, uint64_t, uint32_t);
typedef int32_t (*iFuff_t)(uint32_t, float, float);
typedef int32_t (*iFpii_t)(void*, int32_t, int32_t);
typedef int32_t (*iFpiL_t)(void*, int32_t, uintptr_t);
typedef int32_t (*iFpip_t)(void*, int32_t, void*);
typedef int32_t (*iFpui_t)(void*, uint32_t, int32_t);
typedef int32_t (*iFpuu_t)(void*, uint32_t, uint32_t);
typedef int32_t (*iFpuU_t)(void*, uint32_t, uint64_t);
typedef int32_t (*iFpup_t)(void*, uint32_t, void*);
typedef int32_t (*iFplp_t)(void*, intptr_t, void*);
typedef int32_t (*iFpLi_t)(void*, uintptr_t, int32_t);
typedef int32_t (*iFpLp_t)(void*, uintptr_t, void*);
typedef int32_t (*iFppi_t)(void*, void*, int32_t);
typedef int32_t (*iFppu_t)(void*, void*, uint32_t);
typedef int32_t (*iFppL_t)(void*, void*, uintptr_t);
typedef int32_t (*iFppp_t)(void*, void*, void*);
typedef uint32_t (*uFipu_t)(int32_t, void*, uint32_t);
typedef uint32_t (*uFuip_t)(uint32_t, int32_t, void*);
typedef uint32_t (*uFuuu_t)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (*uFuup_t)(uint32_t, uint32_t, void*);
typedef uint32_t (*uFupp_t)(uint32_t, void*, void*);
typedef uint32_t (*uFpLp_t)(void*, uintptr_t, void*);
typedef float (*fFull_t)(uint32_t, intptr_t, intptr_t);
typedef uintptr_t (*LFpii_t)(void*, int32_t, int32_t);
typedef void* (*pFEiV_t)(int32_t, void*);
typedef void* (*pFEpi_t)(void*, int32_t);
typedef void* (*pFEpp_t)(void*, void*);
typedef void* (*pFEpV_t)(void*, void*);
typedef void* (*pFuii_t)(uint32_t, int32_t, int32_t);
typedef void* (*pFpii_t)(void*, int32_t, int32_t);
typedef void* (*pFpip_t)(void*, int32_t, void*);
typedef void* (*pFpuu_t)(void*, uint32_t, uint32_t);
typedef void* (*pFpuL_t)(void*, uint32_t, uintptr_t);
typedef void* (*pFpup_t)(void*, uint32_t, void*);
typedef uint32_t (*uFpip_t)(void*, int32_t, void*);
typedef int32_t (*iFpppiuu_t)(void*, void*, void*, int32_t, uint32_t, uint32_t);
typedef int32_t (*iFpppiuwu_t)(void*, void*, void*, int32_t, uint32_t, int16_t, uint32_t);
typedef int16_t (*wFp_t)(void*);
typedef int32_t (*iFppuuiiuupi_t)(void*, void*, uint32_t, uint32_t, int32_t, int32_t, uint32_t, uint32_t, void*, int32_t);
typedef void* (*pFpiip_t)(void*, int32_t, int32_t, void*);
typedef int32_t (*iFpupiiiipppppp_t)(void*, uint32_t, void*, int32_t, int32_t, int32_t, int32_t, void*, void*, void*, void*, void*, void*);
typedef uint32_t (*uFpipp_t)(void*, int32_t, void*, void*);
typedef void (*vFpiup_t)(void*, int32_t, uint32_t, void*);
typedef void (*vFppiiuuuiupup_t)(void*, void*, int32_t, int32_t, uint32_t, uint32_t, uint32_t, int32_t, uint32_t, void*, uint32_t, void*);
typedef int32_t (*iFppupp_t)(void*, void*, uint32_t, void*, void*);
typedef void (*vFpu_t)(void*, uint32_t);
typedef void (*vFEppp_t)(void*, void*, void*);
typedef void* (*pFppi_t)(void*, void*, int32_t);
typedef void* (*pFppu_t)(void*, void*, uint32_t);
typedef void* (*pFppp_t)(void*, void*, void*);
typedef void (*vFEipp_t)(int32_t, void*, void*);
typedef void (*vFEpip_t)(void*, int32_t, void*);
typedef void (*vFiiii_t)(int32_t, int32_t, int32_t, int32_t);
typedef void (*vFiiip_t)(int32_t, int32_t, int32_t, void*);
typedef void (*vFiill_t)(int32_t, int32_t, intptr_t, intptr_t);
typedef void (*vFiIII_t)(int32_t, int64_t, int64_t, int64_t);
typedef void (*vFiuiu_t)(int32_t, uint32_t, int32_t, uint32_t);
typedef void (*vFiuip_t)(int32_t, uint32_t, int32_t, void*);
typedef void (*vFiuuu_t)(int32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFiuup_t)(int32_t, uint32_t, uint32_t, void*);
typedef void (*vFiulp_t)(int32_t, uint32_t, intptr_t, void*);
typedef void (*vFiupu_t)(int32_t, uint32_t, void*, uint32_t);
typedef void (*vFiUUU_t)(int32_t, uint64_t, uint64_t, uint64_t);
typedef void (*vFifff_t)(int32_t, float, float, float);
typedef void (*vFiddd_t)(int32_t, double, double, double);
typedef void (*vFilil_t)(int32_t, intptr_t, int32_t, intptr_t);
typedef void (*vFilip_t)(int32_t, intptr_t, int32_t, void*);
typedef void (*vFiluU_t)(int32_t, intptr_t, uint32_t, uint64_t);
typedef void (*vFilpu_t)(int32_t, intptr_t, void*, uint32_t);
typedef void (*vFilpp_t)(int32_t, intptr_t, void*, void*);
typedef void (*vFipup_t)(int32_t, void*, uint32_t, void*);
typedef void (*vFipll_t)(int32_t, void*, intptr_t, intptr_t);
typedef void (*vFippi_t)(int32_t, void*, void*, int32_t);
typedef void (*vFippu_t)(int32_t, void*, void*, uint32_t);
typedef void (*vFippp_t)(int32_t, void*, void*, void*);
typedef void (*vFuiii_t)(uint32_t, int32_t, int32_t, int32_t);
typedef void (*vFuiiu_t)(uint32_t, int32_t, int32_t, uint32_t);
typedef void (*vFuiip_t)(uint32_t, int32_t, int32_t, void*);
typedef void (*vFuiII_t)(uint32_t, int32_t, int64_t, int64_t);
typedef void (*vFuiui_t)(uint32_t, int32_t, uint32_t, int32_t);
typedef void (*vFuiuu_t)(uint32_t, int32_t, uint32_t, uint32_t);
typedef void (*vFuiup_t)(uint32_t, int32_t, uint32_t, void*);
typedef void (*vFuiUU_t)(uint32_t, int32_t, uint64_t, uint64_t);
typedef void (*vFuifi_t)(uint32_t, int32_t, float, int32_t);
typedef void (*vFuiff_t)(uint32_t, int32_t, float, float);
typedef void (*vFuidd_t)(uint32_t, int32_t, double, double);
typedef void (*vFuill_t)(uint32_t, int32_t, intptr_t, intptr_t);
typedef void (*vFuilp_t)(uint32_t, int32_t, intptr_t, void*);
typedef void (*vFuipi_t)(uint32_t, int32_t, void*, int32_t);
typedef void (*vFuipu_t)(uint32_t, int32_t, void*, uint32_t);
typedef void (*vFuipp_t)(uint32_t, int32_t, void*, void*);
typedef void (*vFuuii_t)(uint32_t, uint32_t, int32_t, int32_t);
typedef void (*vFuuiu_t)(uint32_t, uint32_t, int32_t, uint32_t);
typedef void (*vFuuil_t)(uint32_t, uint32_t, int32_t, intptr_t);
typedef void (*vFuuip_t)(uint32_t, uint32_t, int32_t, void*);
typedef void (*vFuuui_t)(uint32_t, uint32_t, uint32_t, int32_t);
typedef void (*vFuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuuuf_t)(uint32_t, uint32_t, uint32_t, float);
typedef void (*vFuuud_t)(uint32_t, uint32_t, uint32_t, double);
typedef void (*vFuuul_t)(uint32_t, uint32_t, uint32_t, intptr_t);
typedef void (*vFuuup_t)(uint32_t, uint32_t, uint32_t, void*);
typedef void (*vFuulp_t)(uint32_t, uint32_t, intptr_t, void*);
typedef void (*vFuupi_t)(uint32_t, uint32_t, void*, int32_t);
typedef void (*vFuupp_t)(uint32_t, uint32_t, void*, void*);
typedef void (*vFuUii_t)(uint32_t, uint64_t, int32_t, int32_t);
typedef void (*vFuUip_t)(uint32_t, uint64_t, int32_t, void*);
typedef void (*vFufff_t)(uint32_t, float, float, float);
typedef void (*vFuddd_t)(uint32_t, double, double, double);
typedef void (*vFulil_t)(uint32_t, intptr_t, int32_t, intptr_t);
typedef void (*vFulip_t)(uint32_t, intptr_t, int32_t, void*);
typedef void (*vFuluU_t)(uint32_t, intptr_t, uint32_t, uint64_t);
typedef void (*vFullp_t)(uint32_t, intptr_t, intptr_t, void*);
typedef void (*vFulpi_t)(uint32_t, intptr_t, void*, int32_t);
typedef void (*vFulpu_t)(uint32_t, intptr_t, void*, uint32_t);
typedef void (*vFulpp_t)(uint32_t, intptr_t, void*, void*);
typedef void (*vFupii_t)(uint32_t, void*, int32_t, int32_t);
typedef void (*vFuppi_t)(uint32_t, void*, void*, int32_t);
typedef void (*vFffff_t)(float, float, float, float);
typedef void (*vFdddd_t)(double, double, double, double);
typedef void (*vFllll_t)(intptr_t, intptr_t, intptr_t, intptr_t);
typedef void (*vFpiii_t)(void*, int32_t, int32_t, int32_t);
typedef void (*vFpipp_t)(void*, int32_t, void*, void*);
typedef void (*vFpupp_t)(void*, uint32_t, void*, void*);
typedef void (*vFpdii_t)(void*, double, int32_t, int32_t);
typedef void (*vFpddd_t)(void*, double, double, double);
typedef void (*vFplpp_t)(void*, intptr_t, void*, void*);
typedef void (*vFppip_t)(void*, void*, int32_t, void*);
typedef void (*vFpppu_t)(void*, void*, void*, uint32_t);
typedef void (*vFpppp_t)(void*, void*, void*, void*);
typedef int32_t (*iFEppp_t)(void*, void*, void*);
typedef int32_t (*iFiiup_t)(int32_t, int32_t, uint32_t, void*);
typedef int32_t (*iFuuff_t)(uint32_t, uint32_t, float, float);
typedef int32_t (*iFpiiL_t)(void*, int32_t, int32_t, uintptr_t);
typedef int32_t (*iFpiip_t)(void*, int32_t, int32_t, void*);
typedef int32_t (*iFpiipp_t)(void*, int32_t, int32_t, void*, void*);
typedef int32_t (*iFpiippp_t)(void*, int32_t, int32_t, void*, void*, void*);
typedef int32_t (*iFpipppp_t)(void*, int32_t, void*, void*, void*, void*);
typedef void* (*pFppuuppp_t)(void*, void*, uint32_t, uint32_t, void*, void*, void*);
typedef int32_t (*iFpppiipiiu_t)(void*, void*, void*, int32_t, int32_t, void*, int32_t, int32_t, uint32_t);
typedef void (*vFpippiipi_t)(void*, int32_t, void*, void*, int32_t, int32_t, void*, int32_t);
typedef int32_t (*iFpipllipppppp_t)(void*, int32_t, void*, intptr_t, intptr_t, int32_t, void*, void*, void*, void*, void*, void*);
typedef int32_t (*iFpiippiiipip_t)(void*, int32_t, int32_t, void*, void*, int32_t, int32_t, int32_t, void*, int32_t, void*);
typedef int32_t (*iFpipLpiiip_t)(void*, int32_t, void*, uintptr_t, void*, int32_t, int32_t, int32_t, void*);
typedef int32_t (*iFpiipiiipip_t)(void*, int32_t, int32_t, void*, int32_t, int32_t, int32_t, void*, int32_t, void*);
typedef int32_t (*iFpipipip_t)(void*, int32_t, void*, int32_t, void*, int32_t, void*);
typedef int32_t (*iFpipppppppppp_t)(void*, int32_t, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef int32_t (*iFpiipip_t)(void*, int32_t, int32_t, void*, int32_t, void*);
typedef int32_t (*iFpipip_t)(void*, int32_t, void*, int32_t, void*);
typedef int32_t (*iFpippddiidd_t)(void*, int32_t, void*, void*, double, double, int32_t, int32_t, double, double);
typedef int32_t (*iFpiup_t)(void*, int32_t, uint32_t, void*);
typedef int32_t (*iFpipi_t)(void*, int32_t, void*, int32_t);
typedef int32_t (*iFpipp_t)(void*, int32_t, void*, void*);
typedef int32_t (*iFpuuu_t)(void*, uint32_t, uint32_t, uint32_t);
typedef int32_t (*iFpuup_t)(void*, uint32_t, uint32_t, void*);
typedef int32_t (*iFpuLL_t)(void*, uint32_t, uintptr_t, uintptr_t);
typedef int32_t (*iFpuLp_t)(void*, uint32_t, uintptr_t, void*);
typedef int32_t (*iFpLip_t)(void*, uintptr_t, int32_t, void*);
typedef int32_t (*iFppii_t)(void*, void*, int32_t, int32_t);
typedef int32_t (*iFppiL_t)(void*, void*, int32_t, uintptr_t);
typedef int32_t (*iFppip_t)(void*, void*, int32_t, void*);
typedef int32_t (*iFppuu_t)(void*, void*, uint32_t, uint32_t);
typedef int32_t (*iFppup_t)(void*, void*, uint32_t, void*);
typedef int32_t (*iFpplp_t)(void*, void*, intptr_t, void*);
typedef int32_t (*iFppLp_t)(void*, void*, uintptr_t, void*);
typedef int32_t (*iFpppi_t)(void*, void*, void*, int32_t);
typedef int32_t (*iFpppL_t)(void*, void*, void*, uintptr_t);
typedef int32_t (*iFpppp_t)(void*, void*, void*, void*);
typedef uint32_t (*uFuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (*uFpuip_t)(void*, uint32_t, int32_t, void*);
typedef uintptr_t (*LFpCii_t)(void*, uint8_t, int32_t, int32_t);
typedef void* (*pFEpip_t)(void*, int32_t, void*);
typedef void* (*pFillu_t)(int32_t, intptr_t, intptr_t, uint32_t);
typedef void* (*pFuiii_t)(uint32_t, int32_t, int32_t, int32_t);
typedef void* (*pFulli_t)(uint32_t, intptr_t, intptr_t, int32_t);
typedef void* (*pFullu_t)(uint32_t, intptr_t, intptr_t, uint32_t);
typedef void* (*pFlfff_t)(intptr_t, float, float, float);
typedef void* (*pFpiii_t)(void*, int32_t, int32_t, int32_t);
typedef void* (*pFpipp_t)(void*, int32_t, void*, void*);
typedef void* (*pFplpp_t)(void*, intptr_t, void*, void*);
typedef void* (*pFppip_t)(void*, void*, int32_t, void*);
typedef void* (*pFppup_t)(void*, void*, uint32_t, void*);
typedef void* (*pFpppi_t)(void*, void*, void*, int32_t);
typedef void* (*pFpppp_t)(void*, void*, void*, void*);
typedef void (*vFiiiii_t)(int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFiiiiu_t)(int32_t, int32_t, int32_t, int32_t, uint32_t);
typedef void (*vFiiuii_t)(int32_t, int32_t, uint32_t, int32_t, int32_t);
typedef void (*vFiiuup_t)(int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFiillu_t)(int32_t, int32_t, intptr_t, intptr_t, uint32_t);
typedef void (*vFiilll_t)(int32_t, int32_t, intptr_t, intptr_t, intptr_t);
typedef void (*vFiipll_t)(int32_t, int32_t, void*, intptr_t, intptr_t);
typedef void (*vFiIIII_t)(int32_t, int64_t, int64_t, int64_t, int64_t);
typedef void (*vFiuiip_t)(int32_t, uint32_t, int32_t, int32_t, void*);
typedef void (*vFiuipi_t)(int32_t, uint32_t, int32_t, void*, int32_t);
typedef void (*vFiuuuu_t)(int32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFiulpp_t)(int32_t, uint32_t, intptr_t, void*, void*);
typedef void (*vFiuppu_t)(int32_t, uint32_t, void*, void*, uint32_t);
typedef void (*vFiUUUU_t)(int32_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef void (*vFiffff_t)(int32_t, float, float, float, float);
typedef void (*vFidddd_t)(int32_t, double, double, double, double);
typedef void (*vFilill_t)(int32_t, intptr_t, int32_t, intptr_t, intptr_t);
typedef void (*vFilipi_t)(int32_t, intptr_t, int32_t, void*, int32_t);
typedef void (*vFilipl_t)(int32_t, intptr_t, int32_t, void*, intptr_t);
typedef void (*vFillpu_t)(int32_t, intptr_t, intptr_t, void*, uint32_t);
typedef void (*vFipipu_t)(int32_t, void*, int32_t, void*, uint32_t);
typedef void (*vFipipp_t)(int32_t, void*, int32_t, void*, void*);
typedef void (*vFipupi_t)(int32_t, void*, uint32_t, void*, int32_t);
typedef void (*vFiplli_t)(int32_t, void*, intptr_t, intptr_t, int32_t);
typedef void (*vFiplll_t)(int32_t, void*, intptr_t, intptr_t, intptr_t);
typedef void (*vFuiiii_t)(uint32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuiiiu_t)(uint32_t, int32_t, int32_t, int32_t, uint32_t);
typedef void (*vFuiiip_t)(uint32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuiifi_t)(uint32_t, int32_t, int32_t, float, int32_t);
typedef void (*vFuiill_t)(uint32_t, int32_t, int32_t, intptr_t, intptr_t);
typedef void (*vFuiilp_t)(uint32_t, int32_t, int32_t, intptr_t, void*);
typedef void (*vFuiIII_t)(uint32_t, int32_t, int64_t, int64_t, int64_t);
typedef void (*vFuiuii_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t);
typedef void (*vFuiuiu_t)(uint32_t, int32_t, uint32_t, int32_t, uint32_t);
typedef void (*vFuiuip_t)(uint32_t, int32_t, uint32_t, int32_t, void*);
typedef void (*vFuiuuu_t)(uint32_t, int32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuiuup_t)(uint32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuiull_t)(uint32_t, int32_t, uint32_t, intptr_t, intptr_t);
typedef void (*vFuiupi_t)(uint32_t, int32_t, uint32_t, void*, int32_t);
typedef void (*vFuiUUU_t)(uint32_t, int32_t, uint64_t, uint64_t, uint64_t);
typedef void (*vFuifff_t)(uint32_t, int32_t, float, float, float);
typedef void (*vFuiddd_t)(uint32_t, int32_t, double, double, double);
typedef void (*vFuipip_t)(uint32_t, int32_t, void*, int32_t, void*);
typedef void (*vFuipup_t)(uint32_t, int32_t, void*, uint32_t, void*);
typedef void (*vFuippp_t)(uint32_t, int32_t, void*, void*, void*);
typedef void (*vFuuiii_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuiiu_t)(uint32_t, uint32_t, int32_t, int32_t, uint32_t);
typedef void (*vFuuiui_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t);
typedef void (*vFuuiuu_t)(uint32_t, uint32_t, int32_t, uint32_t, uint32_t);
typedef void (*vFuuiup_t)(uint32_t, uint32_t, int32_t, uint32_t, void*);
typedef void (*vFuuipi_t)(uint32_t, uint32_t, int32_t, void*, int32_t);
typedef void (*vFuuipu_t)(uint32_t, uint32_t, int32_t, void*, uint32_t);
typedef void (*vFuuipp_t)(uint32_t, uint32_t, int32_t, void*, void*);
typedef void (*vFuuuii_t)(uint32_t, uint32_t, uint32_t, int32_t, int32_t);
typedef void (*vFuuuiu_t)(uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
typedef void (*vFuuuip_t)(uint32_t, uint32_t, uint32_t, int32_t, void*);
typedef void (*vFuuuui_t)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t);
typedef void (*vFuuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuuuup_t)(uint32_t, uint32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuull_t)(uint32_t, uint32_t, uint32_t, intptr_t, intptr_t);
typedef void (*vFuuulp_t)(uint32_t, uint32_t, uint32_t, intptr_t, void*);
typedef void (*vFuulll_t)(uint32_t, uint32_t, intptr_t, intptr_t, intptr_t);
typedef void (*vFuullp_t)(uint32_t, uint32_t, intptr_t, intptr_t, void*);
typedef void (*vFuulpp_t)(uint32_t, uint32_t, intptr_t, void*, void*);
typedef void (*vFuupii_t)(uint32_t, uint32_t, void*, int32_t, int32_t);
typedef void (*vFuffff_t)(uint32_t, float, float, float, float);
typedef void (*vFudddd_t)(uint32_t, double, double, double, double);
typedef void (*vFulill_t)(uint32_t, intptr_t, int32_t, intptr_t, intptr_t);
typedef void (*vFullip_t)(uint32_t, intptr_t, intptr_t, int32_t, void*);
typedef void (*vFullpp_t)(uint32_t, intptr_t, intptr_t, void*, void*);
typedef void (*vFupupi_t)(uint32_t, void*, uint32_t, void*, int32_t);
typedef void (*vFupupp_t)(uint32_t, void*, uint32_t, void*, void*);
typedef void (*vFuppip_t)(uint32_t, void*, void*, int32_t, void*);
typedef void (*vFfffff_t)(float, float, float, float, float);
typedef void (*vFddddp_t)(double, double, double, double, void*);
typedef void (*vFpilpp_t)(void*, int32_t, intptr_t, void*, void*);
typedef void (*vFpipii_t)(void*, int32_t, void*, int32_t, int32_t);
typedef void (*vFpuipp_t)(void*, uint32_t, int32_t, void*, void*);
typedef void (*vFpddii_t)(void*, double, double, int32_t, int32_t);
typedef void (*vFppiii_t)(void*, void*, int32_t, int32_t, int32_t);
typedef void (*vFpppii_t)(void*, void*, void*, int32_t, int32_t);
typedef void (*vFppppu_t)(void*, void*, void*, void*, uint32_t);
typedef void (*vFppppp_t)(void*, void*, void*, void*, void*);
typedef int32_t (*iFEpppp_t)(void*, void*, void*, void*);
typedef int32_t (*iFpiii_t)(void*, int32_t, int32_t, int32_t);
typedef int32_t (*iFpiiii_t)(void*, int32_t, int32_t, int32_t, int32_t);
typedef int32_t (*iFpiiip_t)(void*, int32_t, int32_t, int32_t, void*);
typedef int32_t (*iFpiiuu_t)(void*, int32_t, int32_t, uint32_t, uint32_t);
typedef int32_t (*iFpiipi_t)(void*, int32_t, int32_t, void*, int32_t);
typedef int32_t (*iFpippp_t)(void*, int32_t, void*, void*, void*);
typedef int32_t (*iFpuuLL_t)(void*, uint32_t, uint32_t, uintptr_t, uintptr_t);
typedef int32_t (*iFpCupp_t)(void*, uint8_t, uint32_t, void*, void*);
typedef int32_t (*iFppiip_t)(void*, void*, int32_t, int32_t, void*);
typedef int32_t (*iFppiup_t)(void*, void*, int32_t, uint32_t, void*);
typedef int32_t (*iFppipi_t)(void*, void*, int32_t, void*, int32_t);
typedef int32_t (*iFppipp_t)(void*, void*, int32_t, void*, void*);
typedef int32_t (*iFpppii_t)(void*, void*, void*, int32_t, int32_t);
typedef int32_t (*iFpppiL_t)(void*, void*, void*, int32_t, uintptr_t);
typedef int32_t (*iFpppip_t)(void*, void*, void*, int32_t, void*);
typedef int32_t (*iFppppi_t)(void*, void*, void*, void*, int32_t);
typedef int32_t (*iFppppp_t)(void*, void*, void*, void*, void*);
typedef int64_t (*IFppIII_t)(void*, void*, int64_t, int64_t, int64_t);
typedef uint32_t (*uFuiiiu_t)(uint32_t, int32_t, int32_t, int32_t, uint32_t);
typedef uint32_t (*uFppilp_t)(void*, void*, int32_t, intptr_t, void*);
typedef uint64_t (*UFuiiii_t)(uint32_t, int32_t, int32_t, int32_t, int32_t);
typedef void* (*pFuiipp_t)(uint32_t, int32_t, int32_t, void*, void*);
typedef void* (*pFpiiuu_t)(void*, int32_t, int32_t, uint32_t, uint32_t);
typedef void* (*pFpippp_t)(void*, int32_t, void*, void*, void*);
typedef void* (*pFppipi_t)(void*, void*, int32_t, void*, int32_t);
typedef void* (*pFppipp_t)(void*, void*, int32_t, void*, void*);
typedef void* (*pFppuuu_t)(void*, void*, uint32_t, uint32_t, uint32_t);
typedef void* (*pFppuup_t)(void*, void*, uint32_t, uint32_t, void*);
typedef void* (*pFppLLp_t)(void*, void*, uintptr_t, uintptr_t, void*);
typedef void* (*pFpppip_t)(void*, void*, void*, int32_t, void*);
typedef void* (*pFpppuu_t)(void*, void*, void*, uint32_t, uint32_t);
typedef void* (*pFppppp_t)(void*, void*, void*, void*, void*);
typedef void (*vFiiiiii_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFiiiuil_t)(int32_t, int32_t, int32_t, uint32_t, int32_t, intptr_t);
typedef void (*vFiiilpi_t)(int32_t, int32_t, int32_t, intptr_t, void*, int32_t);
typedef void (*vFiiuiil_t)(int32_t, int32_t, uint32_t, int32_t, int32_t, intptr_t);
typedef void (*vFiiuilp_t)(int32_t, int32_t, uint32_t, int32_t, intptr_t, void*);
typedef void (*vFiiuulp_t)(int32_t, int32_t, uint32_t, uint32_t, intptr_t, void*);
typedef void (*vFiililp_t)(int32_t, int32_t, intptr_t, int32_t, intptr_t, void*);
typedef void (*vFiiplli_t)(int32_t, int32_t, void*, intptr_t, intptr_t, int32_t);
typedef void (*vFiiplll_t)(int32_t, int32_t, void*, intptr_t, intptr_t, intptr_t);
typedef void (*vFiuippp_t)(int32_t, uint32_t, int32_t, void*, void*, void*);
typedef void (*vFiffiff_t)(int32_t, float, float, int32_t, float, float);
typedef void (*vFiddidd_t)(int32_t, double, double, int32_t, double, double);
typedef void (*vFililuU_t)(int32_t, intptr_t, int32_t, intptr_t, uint32_t, uint64_t);
typedef void (*vFililll_t)(int32_t, intptr_t, int32_t, intptr_t, intptr_t, intptr_t);
typedef void (*vFilipli_t)(int32_t, intptr_t, int32_t, void*, intptr_t, int32_t);
typedef void (*vFiliplu_t)(int32_t, intptr_t, int32_t, void*, intptr_t, uint32_t);
typedef void (*vFillill_t)(int32_t, intptr_t, intptr_t, int32_t, intptr_t, intptr_t);
typedef void (*vFipiplp_t)(int32_t, void*, int32_t, void*, intptr_t, void*);
typedef void (*vFipllli_t)(int32_t, void*, intptr_t, intptr_t, intptr_t, int32_t);
typedef void (*vFuiiiii_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuiiiil_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, intptr_t);
typedef void (*vFuiiilp_t)(uint32_t, int32_t, int32_t, int32_t, intptr_t, void*);
typedef void (*vFuiiuii_t)(uint32_t, int32_t, int32_t, uint32_t, int32_t, int32_t);
typedef void (*vFuiiuup_t)(uint32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuiIIII_t)(uint32_t, int32_t, int64_t, int64_t, int64_t, int64_t);
typedef void (*vFuiuiii_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t);
typedef void (*vFuiuiil_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, intptr_t);
typedef void (*vFuiuiip_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, void*);
typedef void (*vFuiuiuu_t)(uint32_t, int32_t, uint32_t, int32_t, uint32_t, uint32_t);
typedef void (*vFuiuuip_t)(uint32_t, int32_t, uint32_t, uint32_t, int32_t, void*);
typedef void (*vFuiuuuu_t)(uint32_t, int32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuiuulp_t)(uint32_t, int32_t, uint32_t, uint32_t, intptr_t, void*);
typedef void (*vFuiupii_t)(uint32_t, int32_t, uint32_t, void*, int32_t, int32_t);
typedef void (*vFuiupiu_t)(uint32_t, int32_t, uint32_t, void*, int32_t, uint32_t);
typedef void (*vFuiUUUU_t)(uint32_t, int32_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef void (*vFuiffff_t)(uint32_t, int32_t, float, float, float, float);
typedef void (*vFuidddd_t)(uint32_t, int32_t, double, double, double, double);
typedef void (*vFuuiiii_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuiiiu_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t);
typedef void (*vFuuiuii_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t);
typedef void (*vFuuiuiu_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, uint32_t);
typedef void (*vFuuiuup_t)(uint32_t, uint32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuuiup_t)(uint32_t, uint32_t, uint32_t, int32_t, uint32_t, void*);
typedef void (*vFuuuipi_t)(uint32_t, uint32_t, uint32_t, int32_t, void*, int32_t);
typedef void (*vFuuuipp_t)(uint32_t, uint32_t, uint32_t, int32_t, void*, void*);
typedef void (*vFuuuuii_t)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t);
typedef void (*vFuuuuip_t)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t, void*);
typedef void (*vFuuuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuuuuff_t)(uint32_t, uint32_t, uint32_t, uint32_t, float, float);
typedef void (*vFuuuppi_t)(uint32_t, uint32_t, uint32_t, void*, void*, int32_t);
typedef void (*vFuuuppp_t)(uint32_t, uint32_t, uint32_t, void*, void*, void*);
typedef void (*vFuuffff_t)(uint32_t, uint32_t, float, float, float, float);
typedef void (*vFuudddd_t)(uint32_t, uint32_t, double, double, double, double);
typedef void (*vFuulppp_t)(uint32_t, uint32_t, intptr_t, void*, void*, void*);
typedef void (*vFuupupp_t)(uint32_t, uint32_t, void*, uint32_t, void*, void*);
typedef void (*vFuffiip_t)(uint32_t, float, float, int32_t, int32_t, void*);
typedef void (*vFufffff_t)(uint32_t, float, float, float, float, float);
typedef void (*vFuddiip_t)(uint32_t, double, double, int32_t, int32_t, void*);
typedef void (*vFuliluU_t)(uint32_t, intptr_t, int32_t, intptr_t, uint32_t, uint64_t);
typedef void (*vFulilli_t)(uint32_t, intptr_t, int32_t, intptr_t, intptr_t, int32_t);
typedef void (*vFulilll_t)(uint32_t, intptr_t, int32_t, intptr_t, intptr_t, intptr_t);
typedef void (*vFullill_t)(uint32_t, intptr_t, intptr_t, int32_t, intptr_t, intptr_t);
typedef void (*vFulplup_t)(uint32_t, intptr_t, void*, intptr_t, uint32_t, void*);
typedef void (*vFupupip_t)(uint32_t, void*, uint32_t, void*, int32_t, void*);
typedef void (*vFuppppu_t)(uint32_t, void*, void*, void*, void*, uint32_t);
typedef void (*vFffffff_t)(float, float, float, float, float, float);
typedef void (*vFdddddd_t)(double, double, double, double, double, double);
typedef void (*vFpdddii_t)(void*, double, double, double, int32_t, int32_t);
typedef void (*vFppiiii_t)(void*, void*, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppupii_t)(void*, void*, uint32_t, void*, int32_t, int32_t);
typedef int32_t (*iFEppppp_t)(void*, void*, void*, void*, void*);
typedef int32_t (*iFiiiiip_t)(int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef int32_t (*iFipuufp_t)(int32_t, void*, uint32_t, uint32_t, float, void*);
typedef int32_t (*iFLppipp_t)(uintptr_t, void*, void*, int32_t, void*, void*);
typedef int32_t (*iFpiiiii_t)(void*, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef int32_t (*iFpiiipp_t)(void*, int32_t, int32_t, int32_t, void*, void*);
typedef int32_t (*iFppiiii_t)(void*, void*, int32_t, int32_t, int32_t, int32_t);
typedef int32_t (*iFppiiuu_t)(void*, void*, int32_t, int32_t, uint32_t, uint32_t);
typedef int32_t (*iFppIppp_t)(void*, void*, int64_t, void*, void*, void*);
typedef int32_t (*iFppuiii_t)(void*, void*, uint32_t, int32_t, int32_t, int32_t);
typedef int32_t (*iFppiiiL_t)(void*, void*, int32_t, int32_t, int32_t, uintptr_t);
typedef int32_t (*iFppuupp_t)(void*, void*, uint32_t, uint32_t, void*, void*);
typedef int32_t (*iFppupip_t)(void*, void*, uint32_t, void*, int32_t, void*);
typedef int32_t (*iFpppipi_t)(void*, void*, void*, int32_t, void*, int32_t);
typedef int32_t (*iFpppipp_t)(void*, void*, void*, int32_t, void*, void*);
typedef int32_t (*iFppppii_t)(void*, void*, void*, void*, int32_t, int32_t);
typedef int32_t (*iFpppppi_t)(void*, void*, void*, void*, void*, int32_t);
typedef int32_t (*iFpppppL_t)(void*, void*, void*, void*, void*, uintptr_t);
typedef int32_t (*iFpppppp_t)(void*, void*, void*, void*, void*, void*);
typedef void* (*pFpippip_t)(void*, int32_t, void*, void*, int32_t, void*);
typedef void* (*pFpppppp_t)(void*, void*, void*, void*, void*, void*);
typedef void (*vFiiiiiip_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFiiiiuup_t)(int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFiiuilil_t)(int32_t, int32_t, uint32_t, int32_t, intptr_t, int32_t, intptr_t);
typedef void (*vFiiffffp_t)(int32_t, int32_t, float, float, float, float, void*);
typedef void (*vFiipllli_t)(int32_t, int32_t, void*, intptr_t, intptr_t, intptr_t, int32_t);
typedef void (*vFiuulipi_t)(int32_t, uint32_t, uint32_t, intptr_t, int32_t, void*, int32_t);
typedef void (*vFililluU_t)(int32_t, intptr_t, int32_t, intptr_t, intptr_t, uint32_t, uint64_t);
typedef void (*vFilipliu_t)(int32_t, intptr_t, int32_t, void*, intptr_t, int32_t, uint32_t);
typedef void (*vFilulipi_t)(int32_t, intptr_t, uint32_t, intptr_t, int32_t, void*, int32_t);
typedef void (*vFuiiiiii_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuiiiuip_t)(uint32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, void*);
typedef void (*vFuiiiuup_t)(uint32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuiiliip_t)(uint32_t, int32_t, int32_t, intptr_t, int32_t, int32_t, void*);
typedef void (*vFuiililp_t)(uint32_t, int32_t, int32_t, intptr_t, int32_t, intptr_t, void*);
typedef void (*vFuiuiiii_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuiuiiip_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuiuiiuu_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t, uint32_t);
typedef void (*vFuiupiiu_t)(uint32_t, int32_t, uint32_t, void*, int32_t, int32_t, uint32_t);
typedef void (*vFuilliip_t)(uint32_t, int32_t, intptr_t, intptr_t, int32_t, int32_t, void*);
typedef void (*vFuipiiii_t)(uint32_t, int32_t, void*, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuipffff_t)(uint32_t, int32_t, void*, float, float, float, float);
typedef void (*vFuipdddd_t)(uint32_t, int32_t, void*, double, double, double, double);
typedef void (*vFuuiiiii_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuiiiui_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, int32_t);
typedef void (*vFuuiiiuu_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t);
typedef void (*vFuuiiuup_t)(uint32_t, uint32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuiuiii_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuipppp_t)(uint32_t, uint32_t, int32_t, void*, void*, void*, void*);
typedef void (*vFuuuiiii_t)(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuuiiip_t)(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuuuiuii_t)(uint32_t, uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t);
typedef void (*vFuuuiupi_t)(uint32_t, uint32_t, uint32_t, int32_t, uint32_t, void*, int32_t);
typedef void (*vFuuuuiip_t)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, void*);
typedef void (*vFuuuuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuuuufff_t)(uint32_t, uint32_t, uint32_t, uint32_t, float, float, float);
typedef void (*vFuuuulll_t)(uint32_t, uint32_t, uint32_t, uint32_t, intptr_t, intptr_t, intptr_t);
typedef void (*vFuuuffff_t)(uint32_t, uint32_t, uint32_t, float, float, float, float);
typedef void (*vFuuudddd_t)(uint32_t, uint32_t, uint32_t, double, double, double, double);
typedef void (*vFuuffiip_t)(uint32_t, uint32_t, float, float, int32_t, int32_t, void*);
typedef void (*vFuuddiip_t)(uint32_t, uint32_t, double, double, int32_t, int32_t, void*);
typedef void (*vFuuppppu_t)(uint32_t, uint32_t, void*, void*, void*, void*, uint32_t);
typedef void (*vFuuppppp_t)(uint32_t, uint32_t, void*, void*, void*, void*, void*);
typedef void (*vFuffffff_t)(uint32_t, float, float, float, float, float, float);
typedef void (*vFudddddd_t)(uint32_t, double, double, double, double, double, double);
typedef void (*vFulilluU_t)(uint32_t, intptr_t, int32_t, intptr_t, intptr_t, uint32_t, uint64_t);
typedef void (*vFulillli_t)(uint32_t, intptr_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t);
typedef void (*vFulipulp_t)(uint32_t, intptr_t, int32_t, void*, uint32_t, intptr_t, void*);
typedef void (*vFulpiill_t)(uint32_t, intptr_t, void*, int32_t, int32_t, intptr_t, intptr_t);
typedef void (*vFlipuiip_t)(intptr_t, int32_t, void*, uint32_t, int32_t, int32_t, void*);
typedef void (*vFlliiiip_t)(intptr_t, intptr_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFpipipii_t)(void*, int32_t, void*, int32_t, void*, int32_t, int32_t);
typedef void (*vFpddiidd_t)(void*, double, double, int32_t, int32_t, double, double);
typedef void (*vFppiiipi_t)(void*, void*, int32_t, int32_t, int32_t, void*, int32_t);
typedef void (*vFpppiiii_t)(void*, void*, void*, int32_t, int32_t, int32_t, int32_t);
typedef int32_t (*iFEpppiiu_t)(void*, void*, void*, int32_t, int32_t, uint32_t);
typedef int32_t (*iFEpppppp_t)(void*, void*, void*, void*, void*, void*);
typedef int32_t (*iFiiiiiip_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef int32_t (*iFpiupiii_t)(void*, int32_t, uint32_t, void*, int32_t, int32_t, int32_t);
typedef int32_t (*iFpuppppp_t)(void*, uint32_t, void*, void*, void*, void*, void*);
typedef int32_t (*iFppiiuui_t)(void*, void*, int32_t, int32_t, uint32_t, uint32_t, int32_t);
typedef int32_t (*iFppiipii_t)(void*, void*, int32_t, int32_t, void*, int32_t, int32_t);
typedef int32_t (*iFppipupu_t)(void*, void*, int32_t, void*, uint32_t, void*, uint32_t);
typedef int32_t (*iFppipppp_t)(void*, void*, int32_t, void*, void*, void*, void*);
typedef int32_t (*iFpppiiii_t)(void*, void*, void*, int32_t, int32_t, int32_t, int32_t);
typedef int32_t (*iFpppiiuu_t)(void*, void*, void*, int32_t, int32_t, uint32_t, uint32_t);
typedef int32_t (*iFpppiipi_t)(void*, void*, void*, int32_t, int32_t, void*, int32_t);
typedef int32_t (*iFppppiii_t)(void*, void*, void*, void*, int32_t, int32_t, int32_t);
typedef uint32_t (*uFuippppp_t)(uint32_t, int32_t, void*, void*, void*, void*, void*);
typedef void* (*pFEppppip_t)(void*, void*, void*, void*, int32_t, void*);
typedef void* (*pFppppuuu_t)(void*, void*, void*, void*, uint32_t, uint32_t, uint32_t);
typedef void* (*pFpppppuu_t)(void*, void*, void*, void*, void*, uint32_t, uint32_t);
typedef void* (*pFppppppp_t)(void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFuupupup_t)(uint32_t, uint32_t, void*, uint32_t, void*, uint32_t, void*);
typedef void (*vFiiiiuuip_t)(int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, int32_t, void*);
typedef void (*vFiilliilp_t)(int32_t, int32_t, intptr_t, intptr_t, int32_t, int32_t, intptr_t, void*);
typedef void (*vFililliuU_t)(int32_t, intptr_t, int32_t, intptr_t, intptr_t, int32_t, uint32_t, uint64_t);
typedef void (*vFilillluU_t)(int32_t, intptr_t, int32_t, intptr_t, intptr_t, intptr_t, uint32_t, uint64_t);
typedef void (*vFilipufip_t)(int32_t, intptr_t, int32_t, void*, uint32_t, float, int32_t, void*);
typedef void (*vFuiiiiiii_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuiiiiill_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t);
typedef void (*vFuiiiiuup_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuiuiiiii_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuiuiiiip_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuiuiuuuu_t)(uint32_t, int32_t, uint32_t, int32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuiulplpp_t)(uint32_t, int32_t, uint32_t, intptr_t, void*, intptr_t, void*, void*);
typedef void (*vFuipuliuf_t)(uint32_t, int32_t, void*, uint32_t, intptr_t, int32_t, uint32_t, float);
typedef void (*vFuuiiiiii_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuiiiuip_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, void*);
typedef void (*vFuuiiiuup_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuiiuupp_t)(uint32_t, uint32_t, int32_t, int32_t, uint32_t, uint32_t, void*, void*);
typedef void (*vFuuiuiiii_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuiuiiip_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuuuiiiii_t)(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuuiuiii_t)(uint32_t, uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuuipipp_t)(uint32_t, uint32_t, uint32_t, int32_t, void*, int32_t, void*, void*);
typedef void (*vFuuuuuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuuuuufff_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float);
typedef void (*vFuuufffff_t)(uint32_t, uint32_t, uint32_t, float, float, float, float, float);
typedef void (*vFulilliuU_t)(uint32_t, intptr_t, int32_t, intptr_t, intptr_t, int32_t, uint32_t, uint64_t);
typedef void (*vFulillluU_t)(uint32_t, intptr_t, int32_t, intptr_t, intptr_t, intptr_t, uint32_t, uint64_t);
typedef void (*vFulllplip_t)(uint32_t, intptr_t, intptr_t, intptr_t, void*, intptr_t, int32_t, void*);
typedef void (*vFffffffff_t)(float, float, float, float, float, float, float, float);
typedef void (*vFlipuiuip_t)(intptr_t, int32_t, void*, uint32_t, int32_t, uint32_t, int32_t, void*);
typedef void (*vFppiiipii_t)(void*, void*, int32_t, int32_t, int32_t, void*, int32_t, int32_t);
typedef void (*vFppppiipi_t)(void*, void*, void*, void*, int32_t, int32_t, void*, int32_t);
typedef int32_t (*iFiiiiiiip_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef int32_t (*iFuipuuluf_t)(uint32_t, int32_t, void*, uint32_t, uint32_t, intptr_t, uint32_t, float);
typedef int32_t (*iFullfpppp_t)(uint32_t, intptr_t, intptr_t, float, void*, void*, void*, void*);
typedef int32_t (*iFpLpipppp_t)(void*, uintptr_t, void*, int32_t, void*, void*, void*, void*);
typedef int32_t (*iFppIIIppp_t)(void*, void*, int64_t, int64_t, int64_t, void*, void*, void*);
typedef int32_t (*iFpppiippp_t)(void*, void*, void*, int32_t, int32_t, void*, void*, void*);
typedef int32_t (*iFppppiipi_t)(void*, void*, void*, void*, int32_t, int32_t, void*, int32_t);
typedef int32_t (*iFppppppip_t)(void*, void*, void*, void*, void*, void*, int32_t, void*);
typedef uint32_t (*uFuipppppp_t)(uint32_t, int32_t, void*, void*, void*, void*, void*, void*);
typedef uint32_t (*uFulpppppp_t)(uint32_t, intptr_t, void*, void*, void*, void*, void*, void*);
typedef void* (*pFpppuuLLu_t)(void*, void*, void*, uint32_t, uint32_t, uintptr_t, uintptr_t, uint32_t);
typedef void* (*pFuupupipp_t)(uint32_t, uint32_t, void*, uint32_t, void*, int32_t, void*, void*);
typedef void (*vFiiiiiiiii_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFiiiiiiill_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t);
typedef void (*vFiiiiillli_t)(int32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t);
typedef void (*vFiiilllilp_t)(int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t, intptr_t, void*);
typedef void (*vFilillliuU_t)(int32_t, intptr_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t, uint32_t, uint64_t);
typedef void (*vFuiiiiiiii_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuiiiiiuip_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, void*);
typedef void (*vFuiiiiiuup_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuiiiillli_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t);
typedef void (*vFuiiilliip_t)(uint32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, int32_t, int32_t, void*);
typedef void (*vFuiiillilp_t)(uint32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, int32_t, intptr_t, void*);
typedef void (*vFuiuiiiiip_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuuiiiiiii_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuiuiiiii_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuiuiiiip_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuuiuiiuup_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuuiiiiip_t)(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuuuuuuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuffffffff_t)(uint32_t, float, float, float, float, float, float, float, float);
typedef void (*vFulillliuU_t)(uint32_t, intptr_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t, uint32_t, uint64_t);
typedef void (*vFffuuuufff_t)(float, float, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float);
typedef void (*vFddddddddd_t)(double, double, double, double, double, double, double, double, double);
typedef void (*vFlipuiuiip_t)(intptr_t, int32_t, void*, uint32_t, int32_t, uint32_t, int32_t, int32_t, void*);
typedef void (*vFppiiipiii_t)(void*, void*, int32_t, int32_t, int32_t, void*, int32_t, int32_t, int32_t);
typedef void (*vFpppppippp_t)(void*, void*, void*, void*, void*, int32_t, void*, void*, void*);
typedef int32_t (*iFiiiiiiiip_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef int32_t (*iFiiiipiiip_t)(int32_t, int32_t, int32_t, int32_t, void*, int32_t, int32_t, int32_t, void*);
typedef int32_t (*iFuilpluluf_t)(uint32_t, int32_t, intptr_t, void*, intptr_t, uint32_t, intptr_t, uint32_t, float);
typedef int32_t (*iFdddpppppp_t)(double, double, double, void*, void*, void*, void*, void*, void*);
typedef int32_t (*iFppiuiippL_t)(void*, void*, int32_t, uint32_t, int32_t, int32_t, void*, void*, uintptr_t);
typedef int32_t (*iFpppiiuuii_t)(void*, void*, void*, int32_t, int32_t, uint32_t, uint32_t, int32_t, int32_t);
typedef int32_t (*iFppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFEppiiuuLi_t)(void*, void*, int32_t, int32_t, uint32_t, uint32_t, uintptr_t, int32_t);
typedef void* (*pFEppuippuu_t)(void*, void*, uint32_t, int32_t, void*, void*, uint32_t, uint32_t);
typedef void* (*pFppiiuuuLL_t)(void*, void*, int32_t, int32_t, uint32_t, uint32_t, uint32_t, uintptr_t, uintptr_t);
typedef void (*vFiiiiiiiiii_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFiiiiiiiiiu_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t);
typedef void (*vFiiiiiiiiui_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t);
typedef void (*vFiiillliiip_t)(int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuiiiiiiiii_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuiiiiiiill_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t);
typedef void (*vFuiiiiiiuup_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuiiiillllp_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t, intptr_t, void*);
typedef void (*vFuiuiiiiuup_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuipulipiuf_t)(uint32_t, int32_t, void*, uint32_t, intptr_t, int32_t, void*, int32_t, uint32_t, float);
typedef void (*vFuuiiiiiiii_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuiiiiiuip_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, void*);
typedef void (*vFuuiiiiiuup_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuiuiiiiip_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuuiuiiiuup_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuuuuuuiii_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t);
typedef void (*vFuuuuuuuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuffiiffiip_t)(uint32_t, float, float, int32_t, int32_t, float, float, int32_t, int32_t, void*);
typedef void (*vFuddiiddiip_t)(uint32_t, double, double, int32_t, int32_t, double, double, int32_t, int32_t, void*);
typedef void (*vFffffffffff_t)(float, float, float, float, float, float, float, float, float, float);
typedef int32_t (*iFiiiiiiiiip_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef int32_t (*iFpuupiuiipp_t)(void*, uint32_t, uint32_t, void*, int32_t, uint32_t, int32_t, int32_t, void*, void*);
typedef int32_t (*iFppppiiuuii_t)(void*, void*, void*, void*, int32_t, int32_t, uint32_t, uint32_t, int32_t, int32_t);
typedef void* (*pFppuiipuuii_t)(void*, void*, uint32_t, int32_t, int32_t, void*, uint32_t, uint32_t, int32_t, int32_t);
typedef void* (*pFpppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFiiiiillliip_t)(int32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t, int32_t, void*);
typedef void (*vFiiiiilllilp_t)(int32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t, intptr_t, void*);
typedef void (*vFuiiiiiiiiip_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFuiiiiiiiuip_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, void*);
typedef void (*vFuiiiiiiiuup_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuiiiillliip_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t, int32_t, void*);
typedef void (*vFuiiiilllilp_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t, intptr_t, void*);
typedef void (*vFuiuiiiiiuup_t)(uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuiuiiiiuup_t)(uint32_t, uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuuuuuuuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuuupupppppp_t)(uint32_t, uint32_t, uint32_t, void*, uint32_t, void*, void*, void*, void*, void*, void*);
typedef void (*vFuuffiiffiip_t)(uint32_t, uint32_t, float, float, int32_t, int32_t, float, float, int32_t, int32_t, void*);
typedef void (*vFuufffffffff_t)(uint32_t, uint32_t, float, float, float, float, float, float, float, float, float);
typedef void (*vFuuddiiddiip_t)(uint32_t, uint32_t, double, double, int32_t, int32_t, double, double, int32_t, int32_t, void*);
typedef void (*vFuffffffffff_t)(uint32_t, float, float, float, float, float, float, float, float, float, float);
typedef void (*vFUufffffffff_t)(uint64_t, uint32_t, float, float, float, float, float, float, float, float, float);
typedef void (*vFpipipiipiii_t)(void*, int32_t, void*, int32_t, void*, int32_t, int32_t, void*, int32_t, int32_t, int32_t);
typedef int32_t (*iFEppppiiiiuu_t)(void*, void*, void*, void*, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t);
typedef int32_t (*iFiiiiiiiiiip_t)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef int32_t (*iFppppiiuuiiL_t)(void*, void*, void*, void*, int32_t, int32_t, uint32_t, uint32_t, int32_t, int32_t, uintptr_t);
typedef void* (*pFEppuiipuuii_t)(void*, void*, uint32_t, int32_t, int32_t, void*, uint32_t, uint32_t, int32_t, int32_t);
typedef void (*vFuiiiillliilp_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t, int32_t, int32_t, intptr_t, void*);
typedef void (*vFuuiiiiiiiiui_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t);
typedef void (*vFuuiiiiiiiuip_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, void*);
typedef void (*vFuuiiiiiiiuup_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuuuuuuuuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFffffffffffff_t)(float, float, float, float, float, float, float, float, float, float, float, float);
typedef int32_t (*iFEppppiiiiuui_t)(void*, void*, void*, void*, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, int32_t);
typedef int32_t (*iFpppllipppppp_t)(void*, void*, void*, intptr_t, intptr_t, int32_t, void*, void*, void*, void*, void*, void*);
typedef int32_t (*iFpppppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFEppiiuuuipii_t)(void*, void*, int32_t, int32_t, uint32_t, uint32_t, uint32_t, int32_t, void*, int32_t, int32_t);
typedef void* (*pFppiiuuuiupLp_t)(void*, void*, int32_t, int32_t, uint32_t, uint32_t, uint32_t, int32_t, uint32_t, void*, uintptr_t, void*);
typedef void (*vFuiiiiiiiiiuup_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFuuuuuuuuuuuuu_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFuffffffffffff_t)(uint32_t, float, float, float, float, float, float, float, float, float, float, float, float);
typedef int32_t (*iFddddpppddpppp_t)(double, double, double, double, void*, void*, void*, double, double, void*, void*, void*, void*);
typedef int32_t (*iFpippuuuiipppp_t)(void*, int32_t, void*, void*, uint32_t, uint32_t, uint32_t, int32_t, int32_t, void*, void*, void*, void*);
typedef void (*vFuffiiffiiffiip_t)(uint32_t, float, float, int32_t, int32_t, float, float, int32_t, int32_t, float, float, int32_t, int32_t, void*);
typedef void (*vFuddiiddiiddiip_t)(uint32_t, double, double, int32_t, int32_t, double, double, int32_t, int32_t, double, double, int32_t, int32_t, void*);
typedef void (*vFuiiiiiuiiiiilll_t)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t);
typedef void (*vFuuiiiiuuiiiiiii_t)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFfffffffffffffff_t)(float, float, float, float, float, float, float, float, float, float, float, float, float, float, float);
typedef void* (*pFpppppppppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFuuuiiiiiuiiiiilll_t)(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, intptr_t, intptr_t, intptr_t);
typedef void (*vFppuiiiiipuiiiiiiii_t)(void*, void*, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFpup_t)(void*, uint32_t, void*);
typedef int32_t (*iFO_t)(int32_t);
typedef unsigned __int128 (*HFp_t)(void*);
typedef uint32_t (*uWp_t)(void*);
typedef void* (*pFpuWp_t)(void*, uint32_t, uint16_t, void*);
typedef void (*vFpU_t)(void*, uint64_t);
typedef int32_t (*iFpO_t)(void*, int32_t);
typedef uint32_t (*uFpi_t)(void*, int32_t);
typedef uint32_t (*uFpu_t)(void*, uint32_t);
typedef int32_t (*iFpupuu_t)(void*, uint32_t, void*, uint32_t, uint32_t);
typedef void (*vFpuiip_t)(void*, uint32_t, int32_t, int32_t, void*);
typedef void (*vFpuu_t)(void*, uint32_t, uint32_t);
typedef void (*vFpdd_t)(void*, double, double);
typedef void (*vFpdddddd_t)(void*, double, double, double, double, double, double);
typedef double (*dFp_t)(void*);
typedef void* (*pFdddddd_t)(double, double, double, double, double, double);
typedef void (*vFpd_t)(void*, double);
typedef void* (*pFffff_t)(float, float, float, float);
typedef void (*vFpffff_t)(void*, float, float, float, float);
typedef void (*vFpuuuu_t)(void*, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void* (*pFpfffi_t)(void*, float, float, float, int32_t);
typedef void (*vFpfffi_t)(void*, float, float, float, int32_t);
typedef void (*vFpuupp_t)(void*, uint32_t, uint32_t, void*, void*);
typedef void (*vFpupu_t)(void*, uint32_t, void*, uint32_t);
typedef void (*vFppiiu_t)(void*, void*, int32_t, int32_t, uint32_t);
typedef void (*vFpppppp_t)(void*, void*, void*, void*, void*, void*);
typedef uint32_t (*uFppp_t)(void*, void*, void*);
typedef uint32_t (*uFppLp_t)(void*, void*, uintptr_t, void*);
typedef uint32_t (*uFpppp_t)(void*, void*, void*, void*);
typedef uint32_t (*uFppLpp_t)(void*, void*, uintptr_t, void*, void*);
typedef uintptr_t (*LFpp_t)(void*, void*);
typedef int32_t (*iFpLppp_t)(void*, uintptr_t, void*, void*, void*);
typedef void (*vFpff_t)(void*, float, float);
typedef void (*vFpppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef int32_t (*iFppppppu_t)(void*, void*, void*, void*, void*, void*, uint32_t);
typedef void (*vFppppppp_t)(void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFpppppu_t)(void*, void*, void*, void*, void*, uint32_t);
typedef void* (*pFppppppu_t)(void*, void*, void*, void*, void*, void*, uint32_t);
typedef void* (*pFuuu_t)(uint32_t, uint32_t, uint32_t);
typedef int32_t (*iFpiipppp_t)(void*, int32_t, int32_t, void*, void*, void*, void*);
typedef void (*vFpiiff_t)(void*, int32_t, int32_t, float, float);
typedef void* (*pFip_t)(int32_t, void*);
typedef void* (*pFip_t)(int32_t, void*);
typedef void (*vFpiC_t)(void*, int32_t, uint8_t);
typedef void (*vFpiip_t)(void*, int32_t, int32_t, void*);
typedef void (*vFpiipp_t)(void*, int32_t, int32_t, void*, void*);
typedef void (*vFpiipCpp_t)(void*, int32_t, int32_t, void*, uint8_t, void*, void*);
typedef void (*vFpiiii_t)(void*, int32_t, int32_t, int32_t, int32_t);
typedef uint16_t (*WFp_t)(void*);
typedef void (*vFpW_t)(void*, uint16_t);
typedef void (*vFpppppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFpf_t)(void*, float);
typedef void* (*pFppipppppppppppp_t)(void*, void*, int32_t, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef int32_t (*iFiup_t)(int32_t, uint32_t, void*);
typedef void* (*pFppuip_t)(void*, void*, uint32_t, int32_t, void*);
typedef void (*vFpupiu_t)(void*, uint32_t, void*, int32_t, uint32_t);
typedef void (*vFppui_t)(void*, void*, uint32_t, int32_t);
typedef void (*vFpiiu_t)(void*, int32_t, int32_t, uint32_t);
typedef void (*vFppppii_t)(void*, void*, void*, void*, int32_t, int32_t);
typedef void (*vFppiiiiiiii_t)(void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppiiiiii_t)(void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppiiiiiiiii_t)(void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppiiiiiii_t)(void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFpppiui_t)(void*, void*, void*, int32_t, uint32_t, int32_t);
typedef void (*vFppiiiip_t)(void*, void*, int32_t, int32_t, int32_t, int32_t, void*);
typedef void (*vFppiipii_t)(void*, void*, int32_t, int32_t, void*, int32_t, int32_t);
typedef uint32_t (*uFpupi_t)(void*, uint32_t, void*, int32_t);
typedef float (*fFp_t)(void*);
typedef void* (*pFppippipipipipipip_t)(void*, void*, int32_t, void*, void*, int32_t, void*, int32_t, void*, int32_t, void*, int32_t, void*, int32_t, void*, int32_t, void*);
typedef void* (*pFippu_t)(int32_t, void*, void*, uint32_t);
typedef intptr_t (*lFv_t)(void);
typedef intptr_t (*lFv_t)(void);
typedef void* (*pFddd_t)(double, double, double);
typedef void* (*pFppuuupp_t)(void*, void*, uint32_t, uint32_t, uint32_t, void*, void*);
typedef int32_t (*iFupp_t)(uint32_t, void*, void*);
typedef int32_t (*iFupp_t)(uint32_t, void*, void*);
typedef int32_t (*iFupp_t)(uint32_t, void*, void*);
typedef uint32_t (*uFpii_t)(void*, int32_t, int32_t);
typedef void* (*pFppiu_t)(void*, void*, int32_t, uint32_t);
typedef void* (*pFppiup_t)(void*, void*, int32_t, uint32_t, void*);
typedef void* (*pFppiiu_t)(void*, void*, int32_t, int32_t, uint32_t);
typedef void (*vFppiu_t)(void*, void*, int32_t, uint32_t);
typedef int32_t (*iFpppippp_t)(void*, void*, void*, int32_t, void*, void*, void*);
typedef void (*vFppiff_t)(void*, void*, int32_t, float, float);
typedef int32_t (*iFpippuuii_t)(void*, int32_t, void*, void*, uint32_t, uint32_t, int32_t, int32_t);
typedef void (*vFpifi_t)(void*, int32_t, float, int32_t);
typedef void (*vFppippi_t)(void*, void*, int32_t, void*, void*, int32_t);
typedef void (*vFppuuuu_t)(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void* (*pFpuiippppppppppp_t)(void*, uint32_t, int32_t, int32_t, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFpiuu_t)(void*, int32_t, uint32_t, uint32_t);
typedef void (*vFppppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFppppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef double (*dFpi_t)(void*, int32_t);
typedef double (*dFpu_t)(void*, uint32_t);
typedef void (*vFpdu_t)(void*, double, uint32_t);
typedef void (*vFppuuppuiiiii_t)(void*, void*, uint32_t, uint32_t, void*, void*, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppiipppiiii_t)(void*, void*, int32_t, int32_t, void*, void*, void*, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppuuppiiiiuii_t)(void*, void*, uint32_t, uint32_t, void*, void*, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t);
typedef void (*vFppuppiiu_t)(void*, void*, uint32_t, void*, void*, int32_t, int32_t, uint32_t);
typedef void (*vFppuuppiiiiu_t)(void*, void*, uint32_t, uint32_t, void*, void*, int32_t, int32_t, int32_t, int32_t, uint32_t);
typedef void (*vFppuppiiii_t)(void*, void*, uint32_t, void*, void*, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppipppiii_t)(void*, void*, int32_t, void*, void*, void*, int32_t, int32_t, int32_t);
typedef void (*vFppuippiip_t)(void*, void*, uint32_t, int32_t, void*, void*, int32_t, int32_t, void*);
typedef void (*vFppiippppii_t)(void*, void*, int32_t, int32_t, void*, void*, void*, void*, int32_t, int32_t);
typedef void (*vFppuppuiiii_t)(void*, void*, uint32_t, void*, void*, uint32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppiipppiiiiiii_t)(void*, void*, int32_t, int32_t, void*, void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppiipppiiiii_t)(void*, void*, int32_t, int32_t, void*, void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppipppiip_t)(void*, void*, int32_t, void*, void*, void*, int32_t, int32_t, void*);
typedef void (*vFppuuppiiii_t)(void*, void*, uint32_t, uint32_t, void*, void*, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFppuppiii_t)(void*, void*, uint32_t, void*, void*, int32_t, int32_t, int32_t);
typedef void* (*pFppddu_t)(void*, void*, double, double, uint32_t);
typedef void* (*pFppdd_t)(void*, void*, double, double);
typedef void (*vFpddu_t)(void*, double, double, uint32_t);
typedef void (*vFppdd_t)(void*, void*, double, double);
typedef uint32_t (*uFpupp_t)(void*, uint32_t, void*, void*);
typedef double (*dFpp_t)(void*, void*);
typedef double (*dFppd_t)(void*, void*, double);
typedef double (*dFppu_t)(void*, void*, uint32_t);
typedef void (*vFppd_t)(void*, void*, double);
typedef void (*vFppdu_t)(void*, void*, double, uint32_t);
typedef void* (*pFpppL_t)(void*, void*, void*, uintptr_t);
typedef void (*vFppdddd_t)(void*, void*, double, double, double, double);
typedef void (*vFpddddp_t)(void*, double, double, double, double, void*);
typedef void (*vFppddddu_t)(void*, void*, double, double, double, double, uint32_t);
typedef void (*vFppddddudd_t)(void*, void*, double, double, double, double, uint32_t, double, double);
typedef void (*vFpppdd_t)(void*, void*, void*, double, double);
typedef void (*vFppddpiu_t)(void*, void*, double, double, void*, int32_t, uint32_t);
typedef void (*vFppddp_t)(void*, void*, double, double, void*);
typedef void (*vFdddppp_t)(double, double, double, void*, void*, void*);
typedef void (*vFpdup_t)(void*, double, uint32_t, void*);
typedef void* (*pFudddp_t)(uint32_t, double, double, double, void*);
typedef int32_t (*iFpppu_t)(void*, void*, void*, uint32_t);
typedef void (*vFppipi_t)(void*, void*, int32_t, void*, int32_t);
typedef void (*vFppdp_t)(void*, void*, double, void*);
typedef void (*vFpplp_t)(void*, void*, intptr_t, void*);
typedef void (*vFpppppppppppppppppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFpdu_t)(void*, double, uint32_t);
typedef void (*vFpud_t)(void*, uint32_t, double);
typedef uint32_t (*uFpup_t)(void*, uint32_t, void*);
typedef void (*vFpppuiiii_t)(void*, void*, void*, uint32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFpppui_t)(void*, void*, void*, uint32_t, int32_t);
typedef void (*vFpLpp_t)(void*, uintptr_t, void*, void*);
typedef void (*vFppuuuuuuuu_t)(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void* (*pFuui_t)(uint32_t, uint32_t, int32_t);
typedef void (*vFpuip_t)(void*, uint32_t, int32_t, void*);
typedef int32_t (*iFpppppLp_t)(void*, void*, void*, void*, void*, uintptr_t, void*);
typedef void (*vFpppipppppppppppppp_t)(void*, void*, void*, int32_t, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFpppippppppppppp_t)(void*, void*, void*, int32_t, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFpppppi_t)(void*, void*, void*, void*, void*, int32_t);
typedef int32_t (*iFppuppp_t)(void*, void*, uint32_t, void*, void*, void*);
typedef void (*vFppuii_t)(void*, void*, uint32_t, int32_t, int32_t);
typedef void (*vFpiiipp_t)(void*, int32_t, int32_t, int32_t, void*, void*);
typedef int32_t (*iFppdidd_t)(void*, void*, double, int32_t, double, double);
typedef void (*vFppdidd_t)(void*, void*, double, int32_t, double, double);
typedef void (*vFpuiipp_t)(void*, uint32_t, int32_t, int32_t, void*, void*);
typedef void (*vFppuuu_t)(void*, void*, uint32_t, uint32_t, uint32_t);
typedef void* (*pFippppppppppppppppp_t)(int32_t, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFpppippi_t)(void*, void*, void*, int32_t, void*, void*, int32_t);
typedef void* (*pFppppppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef int32_t (*iFpipppppppppppp_t)(void*, int32_t, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFpppiff_t)(void*, void*, void*, int32_t, float, float);
typedef void (*vFpupppui_t)(void*, uint32_t, void*, void*, void*, uint32_t, int32_t);
typedef uint32_t (*uFpplp_t)(void*, void*, intptr_t, void*);
typedef void (*vFpppuuu_t)(void*, void*, void*, uint32_t, uint32_t, uint32_t);
typedef void (*vFppil_t)(void*, void*, int32_t, intptr_t);
typedef void (*vFpipppp_t)(void*, int32_t, void*, void*, void*, void*);
typedef void* (*pFpLp_t)(void*, uintptr_t, void*);
typedef void (*vFpiipppp_t)(void*, int32_t, int32_t, void*, void*, void*, void*);
typedef void (*vFpipu_t)(void*, int32_t, void*, uint32_t);
typedef int32_t (*iFpiu_t)(void*, int32_t, uint32_t);
typedef void (*vFpiL_t)(void*, int32_t, uintptr_t);
typedef void (*vFppppppppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFpppuii_t)(void*, void*, void*, uint32_t, int32_t, int32_t);
typedef int32_t (*iFppiipp_t)(void*, void*, int32_t, int32_t, void*, void*);
typedef void (*vFpiiiu_t)(void*, int32_t, int32_t, int32_t, uint32_t);
typedef void (*vFpuiiiu_t)(void*, uint32_t, int32_t, int32_t, int32_t, uint32_t);
typedef int32_t (*iFppdd_t)(void*, void*, double, double);
typedef int32_t (*iFppUup_t)(void*, void*, uint64_t, uint32_t, void*);
typedef void* (*pFpLi_t)(void*, uintptr_t, int32_t);
typedef void* (*pFpC_t)(void*, uint8_t);
typedef int32_t (*iFELp_t)(uintptr_t, void*);
typedef int32_t (*iFEpp_t)(void*, void*);
typedef int32_t (*iFEpp_t)(void*, void*);
typedef int32_t (*iFEppuppp_t)(void*, void*, uint32_t, void*, void*, void*);
typedef uintptr_t (*LFEppppppii_t)(void*, void*, void*, void*, void*, void*, int32_t, int32_t);
typedef void* (*pFEi_t)(int32_t);
typedef void* (*pFEpippppppp_t)(void*, int32_t, void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFEpipppppppi_t)(void*, int32_t, void*, void*, void*, void*, void*, void*, void*, int32_t);
typedef void* (*pFEppppppi_t)(void*, void*, void*, void*, void*, void*, int32_t);
typedef void* (*pFEppppppp_t)(void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFEpppppppi_t)(void*, void*, void*, void*, void*, void*, void*, int32_t);
typedef void* (*pFEuV_t)(uint32_t, void*);
typedef void* (*pFppipipipipipipip_t)(void*, void*, int32_t, void*, int32_t, void*, int32_t, void*, int32_t, void*, int32_t, void*, int32_t, void*, int32_t, void*);
typedef uint32_t (*uFEupp_t)(uint32_t, void*, void*);
typedef void (*vFEpA_t)(void*, void*);
typedef void (*vFEpiA_t)(void*, int32_t, void*);
typedef void (*vFEpippp_t)(void*, int32_t, void*, void*, void*);
typedef void (*vFEpppi_t)(void*, void*, void*, int32_t);
typedef void (*vFEpppp_t)(void*, void*, void*, void*);
typedef void (*vFEppppp_t)(void*, void*, void*, void*, void*);
typedef void (*vFEpppppuu_t)(void*, void*, void*, void*, void*, uint32_t, uint32_t);
typedef void (*vFEppV_t)(void*, void*, void*);
typedef void (*vFpiiiiiiiiiiiiiiiiii_t)(void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFpippppppppppp_t)(void*, int32_t, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFiV_t)(int32_t, void*);
typedef void* (*pFLup_t)(uintptr_t, uint32_t, void*);
typedef void (*vFLp_t)(uintptr_t, void*);
typedef void (*vFLup_t)(uintptr_t, uint32_t, void*);
typedef int32_t (*iFL_t)(uintptr_t);
typedef int32_t (*iFpipLpp_t)(void*, int32_t, void*, uintptr_t, void*, void*);
typedef int32_t (*iFppiiu_t)(void*, void*, int32_t, int32_t, uint32_t);
typedef int32_t (*iFpUup_t)(void*, uint64_t, uint32_t, void*);
typedef int32_t (*iFpUU_t)(void*, uint64_t, uint64_t);
typedef int (*iFL_t)(unsigned long);
typedef uint64_t (*UFp_t)(void*);
typedef void* (*pFpll_t)(void*, intptr_t, intptr_t);
typedef void (*vFppiipuu_t)(void*, void*, int32_t, int32_t, void*, uint32_t, uint32_t);
typedef void (*vFpppuu_t)(void*, void*, void*, uint32_t, uint32_t);
typedef void (*vFppupp_t)(void*, void*, uint32_t, void*, void*);
typedef void (*vFpii_t)(void*, int32_t, int32_t);
typedef uintptr_t (*LFpL_t)(void*, uintptr_t);
typedef uint8_t (*CFppp_t)(void*, void*, void*);
typedef int32_t (*iFpCpp_t)(void*, uint8_t, void*, void*);
typedef int32_t (*iFplpp_t)(void*, intptr_t, void*, void*);
typedef int32_t (*iFpLpp_t)(void*, uintptr_t, void*, void*);
typedef int32_t (*iFpLpp_t)(void*, uintptr_t, void*, void*);
typedef int32_t (*iFplupp_t)(void*, intptr_t, uint32_t, void*, void*);
typedef int32_t (*iFppd_t)(void*, void*, double);
typedef int32_t (*iFppd_t)(void*, void*, double);
typedef int32_t (*iFppiupp_t)(void*, void*, int32_t, uint32_t, void*, void*);
typedef int32_t (*iFppLippp_t)(void*, void*, uintptr_t, int32_t, void*, void*, void*);
typedef int32_t (*iFppLpiuppp_t)(void*, void*, uintptr_t, void*, int32_t, uint32_t, void*, void*, void*);
typedef int32_t (*iFppLppp_t)(void*, void*, uintptr_t, void*, void*, void*);
typedef int32_t (*iFpplupp_t)(void*, void*, intptr_t, uint32_t, void*, void*);
typedef int32_t (*iFppLupp_t)(void*, void*, uintptr_t, uint32_t, void*, void*);
typedef int32_t (*iFppppppp_t)(void*, void*, void*, void*, void*, void*, void*);
typedef int32_t (*iFpppupp_t)(void*, void*, void*, uint32_t, void*, void*);
typedef int32_t (*iFppuippp_t)(void*, void*, uint32_t, int32_t, void*, void*, void*);
typedef int32_t (*iFppupupp_t)(void*, void*, uint32_t, void*, uint32_t, void*, void*);
typedef int32_t (*iFpulpp_t)(void*, uint32_t, intptr_t, void*, void*);
typedef int32_t (*iFpuppp_t)(void*, uint32_t, void*, void*, void*);
typedef int32_t (*iFpuppp_t)(void*, uint32_t, void*, void*, void*);
typedef int32_t (*iFpuppp_t)(void*, uint32_t, void*, void*, void*);
typedef int32_t (*iFpwpp_t)(void*, int16_t, void*, void*);
typedef int32_t (*iFpWpp_t)(void*, uint16_t, void*, void*);
typedef uintptr_t (*LFEpppp_t)(void*, void*, void*, void*);
typedef intptr_t (*lFpLp_t)(void*, uintptr_t, void*);
typedef intptr_t (*lFplpp_t)(void*, intptr_t, void*, void*);
typedef intptr_t (*lFpLpp_t)(void*, uintptr_t, void*, void*);
typedef intptr_t (*lFpp_t)(void*, void*);
typedef intptr_t (*lFppLipp_t)(void*, void*, uintptr_t, int32_t, void*, void*);
typedef uintptr_t (*LFppLL_t)(void*, void*, uintptr_t, uintptr_t);
typedef intptr_t (*lFppLpp_t)(void*, void*, uintptr_t, void*, void*);
typedef intptr_t (*lFppp_t)(void*, void*, void*);
typedef uintptr_t (*LFppp_t)(void*, void*, void*);
typedef intptr_t (*lFpppipiipp_t)(void*, void*, void*, int32_t, void*, int32_t, int32_t, void*, void*);
typedef intptr_t (*lFpppippppp_t)(void*, void*, void*, int32_t, void*, void*, void*, void*, void*);
typedef intptr_t (*lFpppLpp_t)(void*, void*, void*, uintptr_t, void*, void*);
typedef intptr_t (*lFpppp_t)(void*, void*, void*, void*);
typedef intptr_t (*lFppupp_t)(void*, void*, uint32_t, void*, void*);
typedef uintptr_t (*LFuui_t)(uint32_t, uint32_t, int32_t);
typedef void* (*pFEiippppppp_t)(int32_t, int32_t, void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFEppApp_t)(void*, void*, void*, void*, void*);
typedef void* (*pFEpppp_t)(void*, void*, void*, void*);
typedef void* (*pFEpppp_t)(void*, void*, void*, void*);
typedef void* (*pFEppppV_t)(void*, void*, void*, void*, void*);
typedef void* (*pFEpppuipV_t)(void*, void*, void*, uint32_t, int32_t, void*, void*);
typedef void* (*pFiiLp_t)(int32_t, int32_t, uintptr_t, void*);
typedef void* (*pFipp_t)(int32_t, void*, void*);
typedef void* (*pFipp_t)(int32_t, void*, void*);
typedef void* (*pFiupppppp_t)(int32_t, uint32_t, void*, void*, void*, void*, void*, void*);
typedef void* (*pFLuppp_t)(uintptr_t, uint32_t, void*, void*, void*);
typedef void* (*pFpiu_t)(void*, int32_t, uint32_t);
typedef void* (*pFplp_t)(void*, intptr_t, void*);
typedef void* (*pFpLpi_t)(void*, uintptr_t, void*, int32_t);
typedef void* (*pFpLpp_t)(void*, uintptr_t, void*, void*);
typedef void* (*pFpLup_t)(void*, uintptr_t, uint32_t, void*);
typedef void* (*pFppiupp_t)(void*, void*, int32_t, uint32_t, void*, void*);
typedef void* (*pFppLp_t)(void*, void*, uintptr_t, void*);
typedef void* (*pFppLp_t)(void*, void*, uintptr_t, void*);
typedef void* (*pFpplppp_t)(void*, void*, intptr_t, void*, void*, void*);
typedef void* (*pFpppppppuipp_t)(void*, void*, void*, void*, void*, void*, void*, uint32_t, int32_t, void*, void*);
typedef void* (*pFpppppppuipppp_t)(void*, void*, void*, void*, void*, void*, void*, uint32_t, int32_t, void*, void*, void*, void*);
typedef void* (*pFpppuipp_t)(void*, void*, void*, uint32_t, int32_t, void*, void*);
typedef void* (*pFpppuipppp_t)(void*, void*, void*, uint32_t, int32_t, void*, void*, void*, void*);
typedef void* (*pFpppupp_t)(void*, void*, void*, uint32_t, void*, void*);
typedef void* (*pFppuippp_t)(void*, void*, uint32_t, int32_t, void*, void*, void*);
typedef void* (*pFppupp_t)(void*, void*, uint32_t, void*, void*);
typedef void* (*pFppuppp_t)(void*, void*, uint32_t, void*, void*, void*);
typedef void* (*pFppWpp_t)(void*, void*, uint16_t, void*, void*);
typedef void* (*pFpupp_t)(void*, uint32_t, void*, void*);
typedef void* (*pFpupp_t)(void*, uint32_t, void*, void*);
typedef void* (*pFpuppp_t)(void*, uint32_t, void*, void*, void*);
typedef void* (*pFpuppp_t)(void*, uint32_t, void*, void*, void*);
typedef void* (*pFpupppp_t)(void*, uint32_t, void*, void*, void*, void*);
typedef void* (*pFpupppppp_t)(void*, uint32_t, void*, void*, void*, void*, void*, void*);
typedef void* (*pFpW_t)(void*, uint16_t);
typedef void* (*pFpWp_t)(void*, uint16_t, void*);
typedef void* (*pFpWppWpp_t)(void*, uint16_t, void*, void*, uint16_t, void*, void*);
typedef void* (*pFpWWW_t)(void*, uint16_t, uint16_t, uint16_t);
typedef void* (*pFup_t)(uint32_t, void*);
typedef void* (*pFuuip_t)(uint32_t, uint32_t, int32_t, void*);
typedef uint32_t (*uFEipipppp_t)(int32_t, void*, int32_t, void*, void*, void*, void*);
typedef uint32_t (*uFEipippppp_t)(int32_t, void*, int32_t, void*, void*, void*, void*, void*);
typedef uint32_t (*uFEppipppp_t)(void*, void*, int32_t, void*, void*, void*, void*);
typedef uint32_t (*uFEpppp_t)(void*, void*, void*, void*);
typedef uint32_t (*uFEppppppippp_t)(void*, void*, void*, void*, void*, void*, int32_t, void*, void*, void*);
typedef uint32_t (*uFEppppppp_t)(void*, void*, void*, void*, void*, void*, void*);
typedef uint32_t (*uFipipp_t)(int32_t, void*, int32_t, void*, void*);
typedef uint32_t (*uFppipp_t)(void*, void*, int32_t, void*, void*);
typedef uint32_t (*uFppippp_t)(void*, void*, int32_t, void*, void*, void*);
typedef uint32_t (*uFppLpLuppp_t)(void*, void*, uintptr_t, void*, uintptr_t, uint32_t, void*, void*, void*);
typedef uint32_t (*uFpppppupp_t)(void*, void*, void*, void*, void*, uint32_t, void*, void*);
typedef uint32_t (*uFppupp_t)(void*, void*, uint32_t, void*, void*);
typedef void (*vFEiippppppp_t)(int32_t, int32_t, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFEiippppV_t)(int32_t, int32_t, void*, void*, void*, void*, void*);
typedef void (*vFEipAippp_t)(int32_t, void*, void*, int32_t, void*, void*, void*);
typedef void (*vFEippp_t)(int32_t, void*, void*, void*);
typedef void (*vFEiupippp_t)(int32_t, uint32_t, void*, int32_t, void*, void*, void*);
typedef void (*vFEpipppp_t)(void*, int32_t, void*, void*, void*, void*);
typedef void (*vFEpippppppp_t)(void*, int32_t, void*, void*, void*, void*, void*, void*, void*);
typedef void (*vFEppiipppp_t)(void*, void*, int32_t, int32_t, void*, void*, void*, void*);
typedef void (*vFEppip_t)(void*, void*, int32_t, void*);
typedef void (*vFEppipA_t)(void*, void*, int32_t, void*, void*);
typedef void (*vFEppipppp_t)(void*, void*, int32_t, void*, void*, void*, void*);
typedef void (*vFEppipV_t)(void*, void*, int32_t, void*, void*);
typedef void (*vFEppLippp_t)(void*, void*, uintptr_t, int32_t, void*, void*, void*);
typedef void (*vFEpppiippp_t)(void*, void*, void*, int32_t, int32_t, void*, void*, void*);
typedef void (*vFEpppiipppp_t)(void*, void*, void*, int32_t, int32_t, void*, void*, void*, void*);
typedef void (*vFEpppppppiippp_t)(void*, void*, void*, void*, void*, void*, void*, int32_t, int32_t, void*, void*, void*);
typedef void (*vFEpppuipV_t)(void*, void*, void*, uint32_t, int32_t, void*, void*);
typedef void (*vFEpuipV_t)(void*, uint32_t, int32_t, void*, void*);
typedef void (*vFLuui_t)(uintptr_t, uint32_t, uint32_t, int32_t);
typedef void (*vFppCuupp_t)(void*, void*, uint8_t, uint32_t, uint32_t, void*, void*);
typedef void (*vFppl_t)(void*, void*, intptr_t);
typedef void (*vFppL_t)(void*, void*, uintptr_t);
typedef int16_t (*wFppp_t)(void*, void*, void*);
typedef uint16_t (*WFppp_t)(void*, void*, void*);
typedef int64_t (*IFv_t)(void);
typedef void* (*pFLL_t)(uintptr_t, uintptr_t);
typedef int32_t (*iFpLL_t)(void*, uintptr_t, uintptr_t);
typedef void* (*pFppI_t)(void*, void*, int64_t);
typedef void* (*pFIp_t)(int64_t, void*);
typedef void* (*pFpUUU_t)(void*, uint64_t, uint64_t, uint64_t);
typedef int32_t (*iFpUpU_t)(void*, uint64_t, void*, uint64_t);
typedef void* (*pFpUU_t)(void*, uint64_t, uint64_t);
typedef int32_t (*iFppUp_t)(void*, void*, uint64_t, void*);
typedef void* (*pFU_t)(uint64_t);
typedef void (*vFEpi_t)(void*, int32_t);
typedef void (*vFUU_t)(uint64_t, uint64_t);
typedef void* (*pFUU_t)(uint64_t, uint64_t);
typedef void* (*pFppU_t)(void*, void*, uint64_t);
typedef void* (*pFpLL_t)(void*, uintptr_t, uintptr_t);
typedef int32_t (*iFppiU_t)(void*, void*, int32_t, uint64_t);
typedef void* (*pFpCu_t)(void*, uint8_t, uint32_t);
typedef void* (*pFppppiii_t)(void*, void*, void*, void*, int32_t, int32_t, int32_t);
typedef int32_t (*iFpLiL_t)(void*, uintptr_t, int32_t, uintptr_t);
typedef void* (*pFpCip_t)(void*, uint8_t, int32_t, void*);
typedef void* (*pFpCL_t)(void*, uint8_t, uintptr_t);
typedef void* (*pFEppiiuuLipii_t)(void*, void*, int32_t, int32_t, uint32_t, uint32_t, uintptr_t, int32_t, void*, int32_t, int32_t);
typedef void* (*pFpCi_t)(void*, uint8_t, int32_t);
typedef int32_t (*iFppl_t)(void*, void*, intptr_t);
typedef int32_t (*iFppilp_t)(void*, void*, int32_t, intptr_t, void*);
typedef void (*vFpppL_t)(void*, void*, void*, uintptr_t);
typedef int32_t (*iFpppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*);
//vulkan
typedef int32_t (*iFpUUU_t)(void*, uint64_t, uint64_t, uint64_t);
typedef void (*vFpUui_t)(void*, uint64_t, uint32_t, int32_t);
typedef void (*vFpiUuupup_t)(void*, int32_t, uint64_t, uint32_t, uint32_t, void*, uint32_t, void*);
typedef void (*vFpUUi_t)(void*, uint64_t, uint64_t, int32_t);
typedef void (*vFppU_t)(void*, void*, uint64_t);
typedef void (*vFpUiUiupi_t)(void*, uint64_t, int32_t, uint64_t, int32_t, uint32_t, void*, int32_t);
typedef void (*vFpupup_t)(void*, uint32_t, void*, uint32_t, void*);
typedef void (*vFpUipup_t)(void*, uint64_t, int32_t, void*, uint32_t, void*);
typedef void (*vFpUUup_t)(void*, uint64_t, uint64_t, uint32_t, void*);
typedef void (*vFpUUiup_t)(void*, uint64_t, uint64_t, int32_t, uint32_t, void*);
typedef void (*vFpUiUiup_t)(void*, uint64_t, int32_t, uint64_t, int32_t, uint32_t, void*);
typedef void (*vFpUiUup_t)(void*, uint64_t, int32_t, uint64_t, uint32_t, void*);
typedef void (*vFpUuuUUUi_t)(void*, uint64_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, int32_t);
typedef void (*vFpUU_t)(void*, uint64_t, uint64_t);
typedef void (*vFpuuuiu_t)(void*, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
typedef void (*vFpUUuu_t)(void*, uint64_t, uint64_t, uint32_t, uint32_t);
typedef void (*vFpUu_t)(void*, uint64_t, uint32_t);
typedef void (*vFpUUUu_t)(void*, uint64_t, uint64_t, uint64_t, uint32_t);
typedef void (*vFpUiuup_t)(void*, uint64_t, int32_t, uint32_t, uint32_t, void*);
typedef void (*vFpUi_t)(void*, uint64_t, int32_t);
typedef void (*vFpUuu_t)(void*, uint64_t, uint32_t, uint32_t);
typedef void (*vFpfff_t)(void*, float, float, float);
typedef void (*vFpuup_t)(void*, uint32_t, uint32_t, void*);
typedef void (*vFpUUUp_t)(void*, uint64_t, uint64_t, uint64_t, void*);
typedef void (*vFpupiiupupup_t)(void*, uint32_t, void*, int32_t, int32_t, uint32_t, void*, uint32_t, void*, uint32_t, void*);
typedef void (*vFpiUu_t)(void*, int32_t, uint64_t, uint32_t);
typedef void (*vFpUup_t)(void*, uint64_t, uint32_t, void*);
typedef int32_t (*iFpUp_t)(void*, uint64_t, void*);
typedef void (*vFpUp_t)(void*, uint64_t, void*);
typedef int32_t (*iFpU_t)(void*, uint64_t);
typedef void (*vFpUpp_t)(void*, uint64_t, void*, void*);
typedef int32_t (*iFpiiiiip_t)(void*, int32_t, int32_t, int32_t, int32_t, int32_t, void*);
typedef int32_t (*iFpUpp_t)(void*, uint64_t, void*, void*);
typedef int32_t (*iFpUuuLpUi_t)(void*, uint64_t, uint32_t, uint32_t, uintptr_t, void*, uint64_t, int32_t);
typedef int32_t (*iFpUUUip_t)(void*, uint64_t, uint64_t, uint64_t, int32_t, void*);
typedef int32_t (*iFpupU_t)(void*, uint32_t, void*, uint64_t);
typedef int32_t (*iFpUi_t)(void*, uint64_t, int32_t);
typedef int32_t (*iFpupiU_t)(void*, uint32_t, void*, int32_t, uint64_t);
typedef void (*vFpuuuuuu_t)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFpuuup_t)(void*, uint32_t, uint32_t, uint32_t, void*);
typedef void (*vFpUUp_t)(void*, uint64_t, uint64_t, void*);
typedef void (*vFpUUUUuu_t)(void*, uint64_t, uint64_t, uint64_t, uint64_t, uint32_t, uint32_t);
typedef uint64_t (*UFpp_t)(void*, void*);
typedef void (*vFpiUUp_t)(void*, int32_t, uint64_t, uint64_t, void*);
typedef int32_t (*iFpiUUU_t)(void*, int32_t, uint64_t, uint64_t, uint64_t);
typedef void (*vFpUUu_t)(void*, uint64_t, uint64_t, uint32_t);
typedef void (*vFpuupppp_t)(void*, uint32_t, uint32_t, void*, void*, void*, void*);
typedef void (*vFpuiiii_t)(void*, uint32_t, int32_t, int32_t, int32_t, int32_t);
typedef void (*vFpiiULipp_t)(void*, int32_t, int32_t, uint64_t, uintptr_t, int32_t, void*, void*);
typedef int32_t (*iFpuUp_t)(void*, uint32_t, uint64_t, void*);
typedef int32_t (*iFpubp_t)(void*, uint32_t, void*, void*);
typedef int32_t (*iFpUUUUp_t)(void*, uint64_t, uint64_t, uint64_t, uint64_t, void*);
typedef void (*vFpUuiu_t)(void*, uint64_t, uint32_t, int32_t, uint32_t);
typedef void (*vFpuuppp_t)(void*, uint32_t, uint32_t, void*, void*, void*);
typedef void (*vFpuuUUuu_t)(void*, uint32_t, uint32_t, uint64_t, uint64_t, uint32_t, uint32_t);
typedef void (*vFpuUUu_t)(void*, uint32_t, uint64_t, uint64_t, uint32_t);
typedef int32_t (*iFpUuupp_t)(void*, uint64_t, uint32_t, uint32_t, void*, void*);
typedef void (*vFpiUuup_t)(void*, int32_t, uint64_t, uint32_t, uint32_t, void*);
typedef void (*vFpuW_t)(void*, uint32_t, uint16_t);
typedef void (*vFpupuuu_t)(void*, uint32_t, void*, uint32_t, uint32_t, uint32_t);
typedef void (*vFpupuuup_t)(void*, uint32_t, void*, uint32_t, uint32_t, uint32_t, void*);
typedef void (*vFpUf_t)(void*, uint64_t, float);
typedef int32_t (*iFpUupp_t)(void*, uint64_t, uint32_t, void*, void*);
typedef void (*vFpupppp_t)(void*, uint32_t, void*, void*, void*, void*);
typedef void (*vFpupiUu_t)(void*, uint32_t, void*, int32_t, uint64_t, uint32_t);
typedef void (*vFpUppp_t)(void*, uint64_t, void*, void*, void*);
typedef int32_t (*iFpupiLpL_t)(void*, uint32_t, void*, int32_t, uintptr_t, void*, uintptr_t);
typedef void (*vFpppppU_t)(void*, void*, void*, void*, void*, uint64_t);
typedef void (*vFpppppuuu_t)(void*, void*, void*, void*, void*, uint32_t, uint32_t, uint32_t);
typedef int32_t (*iFpUuuLp_t)(void*, uint64_t, uint32_t, uint32_t, uintptr_t, void*);
typedef uint64_t (*UFpUui_t)(void*, uint64_t, uint32_t, int32_t);
typedef void (*vFppUUiUUUU_t)(void*, void*, uint64_t, uint64_t, int32_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef void (*vFpUUUUUUUUUUUuuu_t)(void*, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint32_t, uint32_t, uint32_t);
typedef int32_t (*iFpUu_t)(void*, uint64_t, uint32_t);
typedef int32_t (*iFpULp_t)(void*, uint64_t, uintptr_t, void*);
typedef int32_t (*iFpUiUi_t)(void*, uint64_t, int32_t, uint64_t, int32_t);
typedef void (*vFppUu_t)(void*, void*, uint64_t, uint32_t);
typedef void (*vFppUuupp_t)(void*, void*, uint64_t, uint32_t, uint32_t, void*, void*);
typedef void (*vFpUUUi_t)(void*, uint64_t, uint64_t, uint64_t, int32_t);
typedef int32_t (*iFpiU_t)(void*, int32_t, uint64_t);
typedef void (*vFpUuuUup_t)(void*, uint64_t, uint32_t, uint32_t, uint64_t, uint32_t, void*);
typedef int32_t (*iFpuuuuup_t)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, void*);
typedef void (*vFEpiiiupupup_t)(void*, int32_t, int32_t, int32_t, uint32_t, void*, uint32_t, void*, uint32_t, void*);
typedef int32_t (*iFEpUuppp_t)(void*, uint64_t, uint32_t, void*, void*, void*);
typedef void (*vFEpUp_t)(void*, uint64_t, void*);
typedef int32_t (*iFEpUp_t)(void*, uint64_t, void*);
typedef void (*vFEpiiiiipp_t)(void*, int32_t, int32_t, int32_t, int32_t, int32_t, void*, void*);
typedef void (*vFEpupup_t)(void*, uint32_t, void*, uint32_t, void*);
typedef int32_t (*iFEpUppp_t)(void*, uint64_t, void*, void*, void*);
typedef int32_t (*iFEpUup_t)(void*, uint64_t, uint32_t, void*);
typedef int32_t (*iFEpuppp_t)(void*, uint32_t, void*, void*, void*);
typedef int32_t (*iFEpUUuppp_t)(void*, uint64_t, uint64_t, uint32_t, void*, void*, void*);
//endvulkan
//xcbV2
typedef void* (*pFb_t)(void*);
typedef void* (*pFbuu_t)(void*, uint32_t, uint32_t);
typedef void* (*pFbup_t)(void*, uint32_t, void*);
typedef void* (*pFbupuuuuup_t)(void*, uint32_t, void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, void*);
typedef void* (*pFbWWiCpup_t)(void*, uint16_t, uint16_t, int32_t, uint8_t, void*, uint32_t, void*);
typedef void* (*pFbdwwWWui_t)(void*, double, int16_t, int16_t, uint16_t, uint16_t, uint32_t, int32_t);
typedef uint32_t (*uFbuupwwC_t)(void*, uint32_t, uint32_t, void*, int16_t, int16_t, uint8_t);
typedef int32_t (*iFbupppWWu_t)(void*, uint32_t, void*, void*, void*, uint16_t, uint16_t, uint32_t);
typedef void* (*pFbp_t)(void*, void*);
typedef void* (*pFbuuuWWWCCi_t)(void*, uint32_t, uint32_t, uint32_t, uint16_t, uint16_t, uint16_t, uint8_t, uint8_t, int32_t);
typedef void* (*pFbppu_t)(void*, void*, void*, uint32_t);
typedef uint32_t (*uFbuC_t)(void*, uint32_t, uint8_t);
typedef void* (*pFbpu_t)(void*, void*, uint32_t);
typedef void* (*pFbppppuuCC_t)(void*, void*, void*, void*, void*, uint32_t, uint32_t, uint8_t, uint8_t);
typedef uint32_t (*uFbuu_t)(void*, uint32_t, uint32_t);
typedef uint32_t (*uFbuW_t)(void*, uint32_t, uint16_t);
typedef void* (*pFbuuupwwp_t)(void*, uint32_t, uint32_t, uint32_t, void*, int16_t, int16_t, void*);
typedef void* (*pFbpi_t)(void*, void*, int32_t);
typedef uint32_t (*uFbuuuwwu_t)(void*, uint32_t, uint32_t, uint32_t, int16_t, int16_t, uint32_t);
typedef uint32_t (*uFb_t)(void*);
typedef void* (*pFbiiCpWWup_t)(void*, int32_t, int32_t, uint8_t, void*, uint16_t, uint16_t, uint32_t, void*);
typedef uint32_t (*uFbWWWCCCCCCCCWCCCCCC_t)(void*, uint16_t, uint16_t, uint16_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint16_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
typedef uint32_t (*uFbWu_t)(void*, uint16_t, uint32_t);
typedef uint32_t (*uFbWWWWWWp_t)(void*, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, void*);
typedef uint32_t (*uFbWW_t)(void*, uint16_t, uint16_t);
typedef uint32_t (*uFbuuC_t)(void*, uint32_t, uint32_t, uint8_t);
typedef void* (*pFbuuC_t)(void*, uint32_t, uint32_t, uint8_t);
typedef void* (*pFbuuWWCuu_t)(void*, uint32_t, uint32_t, uint16_t, uint16_t, uint8_t, uint32_t, uint32_t);
typedef uint32_t (*uFbu_t)(void*, uint32_t);
typedef void* (*pFbuwwWWuCuu_t)(void*, uint32_t, int16_t, int16_t, uint16_t, uint16_t, uint32_t, uint8_t, uint32_t, uint32_t);
typedef void* (*pFbuuWWWWWWwwCCCuu_t)(void*, uint32_t, uint32_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, int16_t, int16_t, uint8_t, uint8_t, uint8_t, uint32_t, uint32_t);
typedef void* (*pFbpp_t)(void*, void*, void*);
typedef void* (*pFbuWWW_t)(void*, uint32_t, uint16_t, uint16_t, uint16_t);
typedef void* (*pFbC_t)(void*, uint8_t);
typedef void* (*pFbuup_t)(void*, uint32_t, uint32_t, void*);
typedef uint32_t (*uFbCuuuCup_t)(void*, uint8_t, uint32_t, uint32_t, uint32_t, uint8_t, uint32_t, void*);
typedef uint32_t (*uFbuup_t)(void*, uint32_t, uint32_t, void*);
typedef void* (*pFbCuwwWW_t)(void*, uint8_t, uint32_t, int16_t, int16_t, uint16_t, uint16_t);
typedef void* (*pFbu_t)(void*, uint32_t);
typedef void* (*pFbuWp_t)(void*, uint32_t, uint16_t, void*);
typedef int32_t (*iFb_t)(void*);
typedef void* (*pFbuuuuu_t)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void* (*pFbuuuwwwwWW_t)(void*, uint32_t, uint32_t, uint32_t, int16_t, int16_t, int16_t, int16_t, uint16_t, uint16_t);
typedef uint32_t (*uFbCuuu_t)(void*, uint8_t, uint32_t, uint32_t, uint32_t);
typedef void* (*pFbuuuWWWWWWWW_t)(void*, uint32_t, uint32_t, uint32_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t);
typedef uint32_t (*uFbuuup_t)(void*, uint32_t, uint32_t, uint32_t, void*);
typedef uint32_t (*uFbCuuWW_t)(void*, uint8_t, uint32_t, uint32_t, uint16_t, uint16_t);
typedef uint32_t (*uFbCuuwwWWWWuup_t)(void*, uint8_t, uint32_t, uint32_t, int16_t, int16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint32_t, uint32_t, void*);
typedef void (*vFbu_t)(void*, uint32_t);
typedef void (*vFbU_t)(void*, uint64_t);
typedef void* (*pFbpup_t)(void*, void*, uint32_t, void*);
typedef void* (*pFbCuwwWWu_t)(void*, uint8_t, uint32_t, int16_t, int16_t, uint16_t, uint16_t, uint32_t);
typedef void* (*pFbCC_t)(void*, uint8_t, uint8_t);
typedef uint32_t (*uFbCuuuuu_t)(void*, uint8_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void* (*pFbCuWCCuuCW_t)(void*, uint8_t, uint32_t, uint16_t, uint8_t, uint8_t, uint32_t, uint32_t, uint8_t, uint16_t);
typedef void* (*pFbCuWCCC_t)(void*, uint8_t, uint32_t, uint16_t, uint8_t, uint8_t, uint8_t);
typedef void* (*pFbCuuCC_t)(void*, uint8_t, uint32_t, uint32_t, uint8_t, uint8_t);
typedef void* (*pFbCuWCCuuu_t)(void*, uint8_t, uint32_t, uint16_t, uint8_t, uint8_t, uint32_t, uint32_t, uint32_t);
typedef void* (*pFbCuuwwp_t)(void*, uint8_t, uint32_t, uint32_t, int16_t, int16_t, void*);
typedef uint32_t (*uFbCWp_t)(void*, uint8_t, uint16_t, void*);
typedef uint32_t (*uFbuWp_t)(void*, uint32_t, uint16_t, void*);
typedef int32_t (*iFbupp_t)(void*, uint32_t, void*, void*);
typedef void* (*pFbuuup_t)(void*, uint32_t, uint32_t, uint32_t, void*);
typedef void* (*pFbCuuup_t)(void*, uint8_t, uint32_t, uint32_t, uint32_t, void*);
typedef void (*vFbp_t)(void*, void*);
typedef void (*vFb_t)(void*);
typedef void* (*pFbCuuWWwwCCup_t)(void*, uint8_t, uint32_t, uint32_t, uint16_t, uint16_t, int16_t, int16_t, uint8_t, uint8_t, uint32_t, void*);
typedef void* (*pFbuuWW_t)(void*, uint32_t, uint32_t, uint16_t, uint16_t);
typedef uint32_t (*uFbCuup_t)(void*, uint8_t, uint32_t, uint32_t, void*);
typedef void (*vFbi_t)(void*, int32_t);
typedef uint32_t (*uFbipp_t)(void*, int32_t, void*, void*);
typedef uint64_t (*UFbipp_t)(void*, int32_t, void*, void*);
typedef uint32_t (*uFbippup_t)(void*, int32_t, void*, void*, uint32_t, void*);
typedef uint64_t (*UFbippup_t)(void*, int32_t, void*, void*, uint32_t, void*);
typedef void* (*pFbCpWWup_t)(void*, uint8_t, void*, uint16_t, uint16_t, uint32_t, void*);
typedef void* (*pFbCuu_t)(void*, uint8_t, uint32_t, uint32_t);
typedef void* (*pFbpppp_t)(void*, void*, void*, void*, void*);
typedef void* (*pFbCuW_t)(void*, uint8_t, uint32_t, uint16_t);
typedef void* (*pFbUp_t)(void*, uint64_t, void*);
typedef void* (*pFbuuwwWWww_t)(void*, uint32_t, uint32_t, int16_t, int16_t, uint16_t, uint16_t, int16_t, int16_t);
typedef void (*vFEp_t)(void*);
typedef void* (*pFbCu_t)(void*, uint8_t, uint32_t);
typedef int32_t (*iFbppip_t)(void*, void*, void*, int32_t, void*);
typedef int32_t (*iFbpiU_t)(void*, void*, int32_t, uint64_t);
typedef void* (*pFbpWp_t)(void*, void*, uint16_t, void*);
typedef void* (*pFbuuwwu_t)(void*, uint32_t, uint32_t, int16_t, int16_t, uint32_t);
typedef void* (*pFbCCuuwwC_t)(void*, uint8_t, uint8_t, uint32_t, uint32_t, int16_t, int16_t, uint8_t);
typedef uint32_t (*uFbuuu_t)(void*, uint32_t, uint32_t, uint32_t);
typedef void* (*pFbuuu_t)(void*, uint32_t, uint32_t, uint32_t);
typedef int32_t (*iFbpp_t)(void*, void*, void*);
typedef uint8_t (*CFbupp_t)(void*, uint32_t, void*, void*);
typedef uint32_t (*uFbup_t)(void*, uint32_t, void*);
typedef void (*vFpC_t)(void*, uint8_t);
typedef unsigned __int128 (*HFpp_t)(void*, void*);
typedef uint32_t (*uFbuU_t)(void*, uint32_t, uint64_t);
typedef void* (*pFbppU_t)(void*, void*, void*, uint64_t);
typedef void* (*pFbpCpppwwwwwwWW_t)(void*, void*, uint8_t, void*, void*, void*, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t, uint16_t);
typedef uint32_t (*uFbuuWW_t)(void*, uint32_t, uint32_t, uint16_t, uint16_t);
typedef uint32_t (*uFbuuiup_t)(void*, uint32_t, uint32_t, int32_t, uint32_t, void*);
typedef void* (*pFbuuUUU_t)(void*, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t);
typedef void* (*pFbuuuuuwwuuuuUUUup_t)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int16_t, int16_t, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint32_t, void*);
typedef float (*fFbu_t)(void*, uint32_t);
//endxcbV2
typedef void (*vFpLi_t)(void*, uintptr_t, int32_t);
typedef void (*vFpLL_t)(void*, uintptr_t, uintptr_t);
typedef int32_t (*iFiiO_t)(int32_t, int32_t, int32_t);
typedef int32_t (*iFipO_t)(int32_t, void*, int32_t);
typedef int32_t (*iFupL_t)(uint32_t, void*, uintptr_t);
typedef int32_t (*iFppU_t)(void*, void*, uint64_t);
typedef int32_t (*iFpOu_t)(void*, int32_t, uint32_t);
typedef int32_t (*iFpOM_t)(void*, int32_t, ...);
typedef uint32_t (*uFpCi_t)(void*, uint8_t, int32_t);
typedef uint32_t (*uFpui_t)(void*, uint32_t, int32_t);
typedef uint32_t (*uFpuu_t)(void*, uint32_t, uint32_t);
typedef uint32_t (*uFppi_t)(void*, void*, int32_t);
typedef uintptr_t (*LFpLi_t)(void*, uintptr_t, int32_t);
typedef void* (*pFpCC_t)(void*, uint8_t, uint8_t);
typedef void* (*pFpUp_t)(void*, uint64_t, void*);
typedef void* (*pFpOM_t)(void*, int32_t, ...);
typedef void (*vFpuuu_t)(void*, uint32_t, uint32_t, uint32_t);
typedef void (*vFpLLL_t)(void*, uintptr_t, uintptr_t, uintptr_t);
typedef void (*vFppup_t)(void*, void*, uint32_t, void*);
typedef void (*vFppLp_t)(void*, void*, uintptr_t, void*);
typedef int32_t (*iFEpOu_t)(void*, int32_t, uint32_t);
typedef int32_t (*iFipON_t)(int32_t, void*, int32_t, ...);
typedef int32_t (*iFpuiL_t)(void*, uint32_t, int32_t, uintptr_t);
typedef int32_t (*iFpuui_t)(void*, uint32_t, uint32_t, int32_t);
typedef int32_t (*iFpupi_t)(void*, uint32_t, void*, int32_t);
typedef int32_t (*iFpupL_t)(void*, uint32_t, void*, uintptr_t);
typedef int32_t (*iFpupp_t)(void*, uint32_t, void*, void*);
typedef int32_t (*iFpLpi_t)(void*, uintptr_t, void*, int32_t);
typedef uint64_t (*UFpipp_t)(void*, int32_t, void*, void*);
typedef intptr_t (*lFEppp_t)(void*, void*, void*);
typedef void* (*pFpCWp_t)(void*, uint8_t, uint16_t, void*);
typedef void* (*pFpCuW_t)(void*, uint8_t, uint32_t, uint16_t);
typedef void* (*pFpCuu_t)(void*, uint8_t, uint32_t, uint32_t);
typedef void* (*pFpuWp_t)(void*, uint32_t, uint16_t, void*);
typedef void* (*pFpuuC_t)(void*, uint32_t, uint32_t, uint8_t);
typedef void* (*pFpuup_t)(void*, uint32_t, uint32_t, void*);
typedef void* (*pFpupi_t)(void*, uint32_t, void*, int32_t);
typedef void* (*pFppii_t)(void*, void*, int32_t, int32_t);
typedef void* (*pFppWW_t)(void*, void*, uint16_t, uint16_t);
typedef void* (*pFppuW_t)(void*, void*, uint32_t, uint16_t);
typedef void* (*pFppuu_t)(void*, void*, uint32_t, uint32_t);
typedef void* (*pFpppu_t)(void*, void*, void*, uint32_t);
typedef void (*vFpppip_t)(void*, void*, void*, int32_t, void*);
typedef void (*vFppppi_t)(void*, void*, void*, void*, int32_t);
typedef int32_t (*iFpiiiu_t)(void*, int32_t, int32_t, int32_t, uint32_t);
typedef int32_t (*iFpiiiL_t)(void*, int32_t, int32_t, int32_t, uintptr_t);
typedef int32_t (*iFpuuup_t)(void*, uint32_t, uint32_t, uint32_t, void*);
typedef int32_t (*iFpupup_t)(void*, uint32_t, void*, uint32_t, void*);
typedef int32_t (*iFppuup_t)(void*, void*, uint32_t, uint32_t, void*);
typedef void* (*pFpCpup_t)(void*, uint8_t, void*, uint32_t, void*);
typedef void* (*pFpCppp_t)(void*, uint8_t, void*, void*, void*);
typedef void* (*pFpuWWW_t)(void*, uint32_t, uint16_t, uint16_t, uint16_t);
typedef void* (*pFpuuWW_t)(void*, uint32_t, uint32_t, uint16_t, uint16_t);
typedef void* (*pFpuuup_t)(void*, uint32_t, uint32_t, uint32_t, void*);
typedef void* (*pFppLii_t)(void*, void*, uintptr_t, int32_t, int32_t);
typedef void* (*pFpppup_t)(void*, void*, void*, uint32_t, void*);
typedef void* (*pFppppi_t)(void*, void*, void*, void*, int32_t);
typedef void (*vFEpuipp_t)(void*, uint32_t, int32_t, void*, void*);
typedef void (*vFpiiuuu_t)(void*, int32_t, int32_t, uint32_t, uint32_t, uint32_t);
typedef void (*vFpipppi_t)(void*, int32_t, void*, void*, void*, int32_t);
typedef void (*vFppiiuu_t)(void*, void*, int32_t, int32_t, uint32_t, uint32_t);
typedef void (*vFppiipi_t)(void*, void*, int32_t, int32_t, void*, int32_t);
typedef void (*vFpppiii_t)(void*, void*, void*, int32_t, int32_t, int32_t);
typedef int32_t (*iFppipiL_t)(void*, void*, int32_t, void*, int32_t, uintptr_t);
typedef uint32_t (*uFpippup_t)(void*, int32_t, void*, void*, uint32_t, void*);
typedef uint64_t (*UFpippup_t)(void*, int32_t, void*, void*, uint32_t, void*);
typedef intptr_t (*lFEpippp_t)(void*, int32_t, void*, void*, void*);
typedef uintptr_t (*LFpipipi_t)(void*, int32_t, void*, int32_t, void*, int32_t);
typedef void* (*pFpCuuCC_t)(void*, uint8_t, uint32_t, uint32_t, uint8_t, uint8_t);
typedef void* (*pFpCuuWW_t)(void*, uint8_t, uint32_t, uint32_t, uint16_t, uint16_t);
typedef void* (*pFpCuuup_t)(void*, uint8_t, uint32_t, uint32_t, uint32_t, void*);
typedef void* (*pFpuuwwu_t)(void*, uint32_t, uint32_t, int16_t, int16_t, uint32_t);
typedef void* (*pFpuuuuu_t)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void* (*pFpppppu_t)(void*, void*, void*, void*, void*, uint32_t);
typedef void (*vFpppiipi_t)(void*, void*, void*, int32_t, int32_t, void*, int32_t);
typedef void (*vFppppipi_t)(void*, void*, void*, void*, int32_t, void*, int32_t);
typedef int32_t (*iFpiiiiii_t)(void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef int32_t (*iFpiuiipp_t)(void*, int32_t, uint32_t, int32_t, int32_t, void*, void*);
typedef int32_t (*iFpuiuupp_t)(void*, uint32_t, int32_t, uint32_t, uint32_t, void*, void*);
typedef int32_t (*iFpLipipi_t)(void*, uintptr_t, int32_t, void*, int32_t, void*, int32_t);
typedef int32_t (*iFppiiuup_t)(void*, void*, int32_t, int32_t, uint32_t, uint32_t, void*);
typedef int32_t (*iFppiipiL_t)(void*, void*, int32_t, int32_t, void*, int32_t, uintptr_t);
typedef int32_t (*iFppuipiL_t)(void*, void*, uint32_t, int32_t, void*, int32_t, uintptr_t);
typedef void* (*pFpCuwwWW_t)(void*, uint8_t, uint32_t, int16_t, int16_t, uint16_t, uint16_t);
typedef void* (*pFpCuWCCC_t)(void*, uint8_t, uint32_t, uint16_t, uint8_t, uint8_t, uint8_t);
typedef void* (*pFpCuuwwp_t)(void*, uint8_t, uint32_t, uint32_t, int16_t, int16_t, void*);
typedef void* (*pFpCuuuuu_t)(void*, uint8_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void* (*pFpCpWWup_t)(void*, uint8_t, void*, uint16_t, uint16_t, uint32_t, void*);
typedef void* (*pFpuuuwwu_t)(void*, uint32_t, uint32_t, uint32_t, int16_t, int16_t, uint32_t);
typedef void* (*pFpuupwwC_t)(void*, uint32_t, uint32_t, void*, int16_t, int16_t, uint8_t);
typedef void (*vFpippiiuu_t)(void*, int32_t, void*, void*, int32_t, int32_t, uint32_t, uint32_t);
typedef int32_t (*iFpWWipppp_t)(void*, uint16_t, uint16_t, int32_t, void*, void*, void*, void*);
typedef int32_t (*iFpuuupupu_t)(void*, uint32_t, uint32_t, uint32_t, void*, uint32_t, void*, uint32_t);
typedef int32_t (*iFpupppWWu_t)(void*, uint32_t, void*, void*, void*, uint16_t, uint16_t, uint32_t);
typedef int32_t (*iFppiiipip_t)(void*, void*, int32_t, int32_t, int32_t, void*, int32_t, void*);
typedef void* (*pFpCCuuwwC_t)(void*, uint8_t, uint8_t, uint32_t, uint32_t, int16_t, int16_t, uint8_t);
typedef void* (*pFpCuwwWWu_t)(void*, uint8_t, uint32_t, int16_t, int16_t, uint16_t, uint16_t, uint32_t);
typedef void* (*pFpCuuuCup_t)(void*, uint8_t, uint32_t, uint32_t, uint32_t, uint8_t, uint32_t, void*);
typedef void* (*pFpWWiCpup_t)(void*, uint16_t, uint16_t, int32_t, uint8_t, void*, uint32_t, void*);
typedef void* (*pFpuuWWCuu_t)(void*, uint32_t, uint32_t, uint16_t, uint16_t, uint8_t, uint32_t, uint32_t);
typedef void* (*pFpuuuupup_t)(void*, uint32_t, uint32_t, uint32_t, uint32_t, void*, uint32_t, void*);
typedef void* (*pFpuuupwwp_t)(void*, uint32_t, uint32_t, uint32_t, void*, int16_t, int16_t, void*);
typedef void* (*pFpdwwWWui_t)(void*, double, int16_t, int16_t, uint16_t, uint16_t, uint32_t, int32_t);
typedef void* (*pFpppppupp_t)(void*, void*, void*, void*, void*, uint32_t, void*, void*);
typedef void (*vFpipiuiipp_t)(void*, int32_t, void*, int32_t, uint32_t, int32_t, int32_t, void*, void*);
typedef void (*vFpipppiipi_t)(void*, int32_t, void*, void*, void*, int32_t, int32_t, void*, int32_t);
typedef void* (*pFpCuWCCuuu_t)(void*, uint8_t, uint32_t, uint16_t, uint8_t, uint8_t, uint32_t, uint32_t, uint32_t);
typedef void* (*pFpuuwwWWww_t)(void*, uint32_t, uint32_t, int16_t, int16_t, uint16_t, uint16_t, int16_t, int16_t);
typedef void* (*pFpupuuuuup_t)(void*, uint32_t, void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, void*);
typedef void* (*pFpppWWWWWp_t)(void*, void*, void*, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, void*);
typedef int32_t (*iFpCuWCCCCup_t)(void*, uint8_t, uint32_t, uint16_t, uint8_t, uint8_t, uint8_t, uint8_t, uint32_t, void*);
typedef void* (*pFpCuWCCuuCW_t)(void*, uint8_t, uint32_t, uint16_t, uint8_t, uint8_t, uint32_t, uint32_t, uint8_t, uint16_t);
typedef void* (*pFpuwwWWuCuu_t)(void*, uint32_t, int16_t, int16_t, uint16_t, uint16_t, uint32_t, uint8_t, uint32_t, uint32_t);
typedef void* (*pFpuuuwwwwWW_t)(void*, uint32_t, uint32_t, uint32_t, int16_t, int16_t, int16_t, int16_t, uint16_t, uint16_t);
typedef void* (*pFpuuuWWWCCi_t)(void*, uint32_t, uint32_t, uint32_t, uint16_t, uint16_t, uint16_t, uint8_t, uint8_t, int32_t);
typedef void (*vFpipppiiiipi_t)(void*, int32_t, void*, void*, void*, int32_t, int32_t, int32_t, int32_t, void*, int32_t);
typedef int32_t (*iFpppiiuuiiuu_t)(void*, void*, void*, int32_t, int32_t, uint32_t, uint32_t, int32_t, int32_t, uint32_t, uint32_t);
typedef void (*vFpipppiiiipii_t)(void*, int32_t, void*, void*, void*, int32_t, int32_t, int32_t, int32_t, void*, int32_t, int32_t);
typedef void (*vFpippppiiiipi_t)(void*, int32_t, void*, void*, void*, void*, int32_t, int32_t, int32_t, int32_t, void*, int32_t);
typedef int32_t (*iFpCCCWCWCCCWp_t)(void*, uint8_t, uint8_t, uint8_t, uint16_t, uint8_t, uint16_t, uint8_t, uint8_t, uint8_t, uint16_t, void*);
typedef void* (*pFWWiCCCCiipup_t)(uint16_t, uint16_t, int32_t, uint8_t, uint8_t, uint8_t, uint8_t, int32_t, int32_t, void*, uint32_t, void*);
typedef void* (*pFpCuuWWwwCCup_t)(void*, uint8_t, uint32_t, uint32_t, uint16_t, uint16_t, int16_t, int16_t, uint8_t, uint8_t, uint32_t, void*);
typedef void* (*pFpuuuWWWWWWWW_t)(void*, uint32_t, uint32_t, uint32_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t);
typedef void (*vFpipppiiiiiiuu_t)(void*, int32_t, void*, void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t);
typedef void* (*pFpCuuwwWWWWuup_t)(void*, uint8_t, uint32_t, uint32_t, int16_t, int16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint32_t, uint32_t, void*);
typedef void* (*pFpuupppwwwwWWC_t)(void*, uint32_t, uint32_t, void*, void*, void*, int16_t, int16_t, int16_t, int16_t, uint16_t, uint16_t, uint8_t);
typedef int32_t (*iFpppwwWWwwWWpuu_t)(void*, void*, void*, int16_t, int16_t, uint16_t, uint16_t, int16_t, int16_t, uint16_t, uint16_t, void*, uint32_t, uint32_t);
typedef void* (*pFppppppppppppppp_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFpuuWWWWWWwwCCCuu_t)(void*, uint32_t, uint32_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, int16_t, int16_t, uint8_t, uint8_t, uint8_t, uint32_t, uint32_t);
typedef void* (*pFpippppppppppppppppp_t)(void*, int32_t, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void* (*pFpppWWCCpCpCpCWpCpCpC_t)(void*, void*, void*, uint16_t, uint16_t, uint8_t, uint8_t, void*, uint8_t, void*, uint8_t, void*, uint8_t, uint16_t, void*, uint8_t, void*, uint8_t, void*, uint8_t);
typedef int64_t (*IFED_t)(long double);
typedef void* (*pFEpp_t)(void*, void*);
typedef int32_t (*iFEppp_t)(void*, void*, void*);
typedef int32_t (*iFEpppp_t)(void*, void*, void*, void*);

void vFv(uintptr_t fcn) { __CPU; vFv_t fn = (vFv_t)fcn; fn(); DEBUG_LOG; DEBUG_LOG; (void)cpu; }
void vFi(uintptr_t fcn) { __CPU;  vFi_t fn = (vFi_t)fcn; fn((int32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void vFu(uintptr_t fcn) { __CPU;  vFu_t fn = (vFu_t)fcn; fn((uint32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void vFU(uintptr_t fcn) { __CPU;  vFU_t fn = (vFU_t)fcn; fn((uint64_t)R_RDI); DEBUG_LOG; (void)cpu; }
void vFf(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFf_t fn = (vFf_t)fcn; fn(f0); DEBUG_LOG; (void)cpu; }
void vFd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16");  vFd_t fn = (vFd_t)fcn; fn(d0); DEBUG_LOG; (void)cpu; }
void vFl(uintptr_t fcn) { __CPU;  vFl_t fn = (vFl_t)fcn; fn((intptr_t)R_RDI); DEBUG_LOG; (void)cpu; }
void vFp(uintptr_t fcn) { __CPU;  vFp_t fn = (vFp_t)fcn; fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void iFv(uintptr_t fcn) { __CPU;  iFv_t fn = (iFv_t)fcn; R_RAX=(int32_t)fn(); DEBUG_LOG; (void)cpu; }
void iFi(uintptr_t fcn) { __CPU;  iFi_t fn = (iFi_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void iFu(uintptr_t fcn) { __CPU;  iFu_t fn = (iFu_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void iFU(uintptr_t fcn) { __CPU;  iFU_t fn = (iFU_t)fcn; R_RAX=(int32_t)fn((uint64_t)R_RDI); DEBUG_LOG; (void)cpu; }
void iFp(uintptr_t fcn) { __CPU;  iFp_t fn = (iFp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void WFi(uintptr_t fcn) { __CPU;  WFi_t fn = (WFi_t)fcn; R_RAX=(unsigned short)fn((int32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void uFv(uintptr_t fcn) { __CPU;  uFv_t fn = (uFv_t)fcn; R_RAX=(uint32_t)fn(); DEBUG_LOG; (void)cpu; }
void uFi(uintptr_t fcn) { __CPU;  uFi_t fn = (uFi_t)fcn; R_RAX=(uint32_t)fn((int32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void uFu(uintptr_t fcn) { __CPU;  uFu_t fn = (uFu_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void uFl(uintptr_t fcn) { __CPU;  uFl_t fn = (uFl_t)fcn; R_RAX=(uint32_t)fn((intptr_t)R_RDI); DEBUG_LOG; (void)cpu; }
void uFp(uintptr_t fcn) { __CPU;  uFp_t fn = (uFp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void UFv(uintptr_t fcn) { __CPU;  UFv_t fn = (UFv_t)fcn; R_RAX=fn(); DEBUG_LOG; (void)cpu; }
void UFu(uintptr_t fcn) { __CPU;  UFu_t fn = (UFu_t)fcn; R_RAX=fn((uint32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void lFu(uintptr_t fcn) { __CPU;  lFu_t fn = (lFu_t)fcn; R_RAX=(intptr_t)fn((uint32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void lFp(uintptr_t fcn) { __CPU;  lFp_t fn = (lFp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void LFv(uintptr_t fcn) { __CPU;  LFv_t fn = (LFv_t)fcn; R_RAX=(uintptr_t)fn(); DEBUG_LOG; (void)cpu; }
void LFp(uintptr_t fcn) { __CPU;  LFp_t fn = (LFp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFv(uintptr_t fcn) { __CPU;  pFv_t fn = (pFv_t)fcn; R_RAX=(uintptr_t)fn(); DEBUG_LOG; (void)cpu; }
void pFi(uintptr_t fcn) { __CPU;  pFi_t fn = (pFi_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void pFu(uintptr_t fcn) { __CPU;  pFu_t fn = (pFu_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void pFL(uintptr_t fcn) { __CPU;  pFL_t fn = (pFL_t)fcn; R_RAX=(uintptr_t)fn((uintptr_t)R_RDI); DEBUG_LOG; (void)cpu; }
void pFp(uintptr_t fcn) { __CPU;  pFp_t fn = (pFp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFii(uintptr_t fcn) { __CPU;  vFii_t fn = (vFii_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFiI(uintptr_t fcn) { __CPU;  vFiI_t fn = (vFiI_t)fcn; fn((int32_t)R_RDI, (int64_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFiu(uintptr_t fcn) { __CPU;  vFiu_t fn = (vFiu_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFiU(uintptr_t fcn) { __CPU;  vFiU_t fn = (vFiU_t)fcn; fn((int32_t)R_RDI, (uint64_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFif(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFif_t fn = (vFif_t)fcn; fn((int32_t)R_RDI, f0); DEBUG_LOG; (void)cpu; }
void vFid(uintptr_t fcn) { __CPU; register double d0 __asm__("f16");  vFid_t fn = (vFid_t)fcn; fn((int32_t)R_RDI, d0); DEBUG_LOG; (void)cpu; }
void vFip(uintptr_t fcn) { __CPU;  vFip_t fn = (vFip_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void vFui(uintptr_t fcn) { __CPU;  vFui_t fn = (vFui_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFuu(uintptr_t fcn) { __CPU;  vFuu_t fn = (vFuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFuU(uintptr_t fcn) { __CPU;  vFuU_t fn = (vFuU_t)fcn; fn((uint32_t)R_RDI, (uint64_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFuf(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFuf_t fn = (vFuf_t)fcn; fn((uint32_t)R_RDI, f0); DEBUG_LOG; (void)cpu; }
void vFud(uintptr_t fcn) { __CPU; register double d0 __asm__("f16");  vFud_t fn = (vFud_t)fcn; fn((uint32_t)R_RDI, d0); DEBUG_LOG; (void)cpu; }
void vFul(uintptr_t fcn) { __CPU;  vFul_t fn = (vFul_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFup(uintptr_t fcn) { __CPU;  vFup_t fn = (vFup_t)fcn; fn((uint32_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void pFupi(uintptr_t fcn) { __CPU; pFupi_t fn = (pFupi_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (void*)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFUi(uintptr_t fcn) { __CPU;  vFUi_t fn = (vFUi_t)fcn; fn((uint64_t)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFfi(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFfi_t fn = (vFfi_t)fcn; fn(f0, (int32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void vFff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17");  vFff_t fn = (vFff_t)fcn; fn(f0, f1); DEBUG_LOG; (void)cpu; }
void vFdd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17");  vFdd_t fn = (vFdd_t)fcn; fn(d0, d1); DEBUG_LOG; (void)cpu; }
void vFlp(uintptr_t fcn) { __CPU;  vFlp_t fn = (vFlp_t)fcn; fn((intptr_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpi(uintptr_t fcn) { __CPU;  vFpi_t fn = (vFpi_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpl(uintptr_t fcn) { __CPU;  vFpl_t fn = (vFpl_t)fcn; fn((void*)R_RDI, (intptr_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpL(uintptr_t fcn) { __CPU;  vFpL_t fn = (vFpL_t)fcn; fn((void*)R_RDI, (uintptr_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpp(uintptr_t fcn) { __CPU;  vFpp_t fn = (vFpp_t)fcn; fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void vFppuu(uintptr_t fcn) { __CPU; vFppuu_t fn = (vFppuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFEp(uintptr_t fcn) { __CPU;  iFEp_t fn = (iFEp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void iFiu(uintptr_t fcn) { __CPU;  iFiu_t fn = (iFiu_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void iFip(uintptr_t fcn) { __CPU;  iFip_t fn = (iFip_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void iFui(uintptr_t fcn) { __CPU;  iFui_t fn = (iFui_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void iFuu(uintptr_t fcn) { __CPU;  iFuu_t fn = (iFuu_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void iFuU(uintptr_t fcn) { __CPU;  iFuU_t fn = (iFuU_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (uint64_t)R_RSI); DEBUG_LOG; (void)cpu; }
void iFup(uintptr_t fcn) { __CPU;  iFup_t fn = (iFup_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void iFpi(uintptr_t fcn) { __CPU;  iFpi_t fn = (iFpi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void iFpu(uintptr_t fcn) { __CPU;  iFpu_t fn = (iFpu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void iFpl(uintptr_t fcn) { __CPU;  iFpl_t fn = (iFpl_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (intptr_t)R_RSI); DEBUG_LOG; (void)cpu; }
void iFpL(uintptr_t fcn) { __CPU;  iFpL_t fn = (iFpL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI); DEBUG_LOG; (void)cpu; }
void iFpp(uintptr_t fcn) { __CPU;  iFpp_t fn = (iFpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void uFiu(uintptr_t fcn) { __CPU;  uFiu_t fn = (uFiu_t)fcn; R_RAX=(uint32_t)fn((int32_t)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void uFuu(uintptr_t fcn) { __CPU;  uFuu_t fn = (uFuu_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void uFup(uintptr_t fcn) { __CPU;  uFup_t fn = (uFup_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void uFpU(uintptr_t fcn) { __CPU;  uFpU_t fn = (uFpU_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (uint64_t)R_RSI); DEBUG_LOG; (void)cpu; }
void uFpp(uintptr_t fcn) { __CPU;  uFpp_t fn = (uFpp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void UFuu(uintptr_t fcn) { __CPU;  UFuu_t fn = (UFuu_t)fcn; R_RAX=fn((uint32_t)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void LFpi(uintptr_t fcn) { __CPU;  LFpi_t fn = (LFpi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFEp(uintptr_t fcn) { __CPU;  pFEp_t fn = (pFEp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFii(uintptr_t fcn) { __CPU; pFii_t fn = (pFii_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFiu(uintptr_t fcn) { __CPU;  pFiu_t fn = (pFiu_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFui(uintptr_t fcn) { __CPU;  pFui_t fn = (pFui_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFuu(uintptr_t fcn) { __CPU;  pFuu_t fn = (pFuu_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFpi(uintptr_t fcn) { __CPU;  pFpi_t fn = (pFpi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFpu(uintptr_t fcn) { __CPU;  pFpu_t fn = (pFpu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFpL(uintptr_t fcn) { __CPU;  pFpL_t fn = (pFpL_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFpp(uintptr_t fcn) { __CPU;  pFpp_t fn = (pFpp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void pFpppuupp(uintptr_t fcn) { __CPU; pFpppuupp_t fn = (pFpppuupp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFEpp(uintptr_t fcn) { __CPU;  vFEpp_t fn = (vFEpp_t)fcn; fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void vFiii(uintptr_t fcn) { __CPU;  vFiii_t fn = (vFiii_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiiu(uintptr_t fcn) { __CPU;  vFiiu_t fn = (vFiiu_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiif(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFiif_t fn = (vFiif_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, f0); DEBUG_LOG; (void)cpu; }
void vFiip(uintptr_t fcn) { __CPU;  vFiip_t fn = (vFiip_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiII(uintptr_t fcn) { __CPU;  vFiII_t fn = (vFiII_t)fcn; fn((int32_t)R_RDI, (int64_t)R_RSI, (int64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiui(uintptr_t fcn) { __CPU;  vFiui_t fn = (vFiui_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiuu(uintptr_t fcn) { __CPU;  vFiuu_t fn = (vFiuu_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiuU(uintptr_t fcn) { __CPU;  vFiuU_t fn = (vFiuU_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiup(uintptr_t fcn) { __CPU;  vFiup_t fn = (vFiup_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiUU(uintptr_t fcn) { __CPU;  vFiUU_t fn = (vFiUU_t)fcn; fn((int32_t)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17");  vFiff_t fn = (vFiff_t)fcn; fn((int32_t)R_RDI, f0, f1); DEBUG_LOG; (void)cpu; }
void vFidd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17");  vFidd_t fn = (vFidd_t)fcn; fn((int32_t)R_RDI, d0, d1); DEBUG_LOG; (void)cpu; }
void vFill(uintptr_t fcn) { __CPU;  vFill_t fn = (vFill_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFilp(uintptr_t fcn) { __CPU;  vFilp_t fn = (vFilp_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFipi(uintptr_t fcn) { __CPU;  vFipi_t fn = (vFipi_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFipu(uintptr_t fcn) { __CPU;  vFipu_t fn = (vFipu_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFipp(uintptr_t fcn) { __CPU;  vFipp_t fn = (vFipp_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuii(uintptr_t fcn) { __CPU;  vFuii_t fn = (vFuii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuiI(uintptr_t fcn) { __CPU;  vFuiI_t fn = (vFuiI_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuiu(uintptr_t fcn) { __CPU;  vFuiu_t fn = (vFuiu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuiU(uintptr_t fcn) { __CPU;  vFuiU_t fn = (vFuiU_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuif(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFuif_t fn = (vFuif_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, f0); DEBUG_LOG; (void)cpu; }
void vFuid(uintptr_t fcn) { __CPU; register double d0 __asm__("f16");  vFuid_t fn = (vFuid_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, d0); DEBUG_LOG; (void)cpu; }
void vFuip(uintptr_t fcn) { __CPU;  vFuip_t fn = (vFuip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuui(uintptr_t fcn) { __CPU;  vFuui_t fn = (vFuui_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuuu(uintptr_t fcn) { __CPU;  vFuuu_t fn = (vFuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuuU(uintptr_t fcn) { __CPU;  vFuuU_t fn = (vFuuU_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuuf(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFuuf_t fn = (vFuuf_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, f0); DEBUG_LOG; (void)cpu; }
void vFuud(uintptr_t fcn) { __CPU; register double d0 __asm__("f16");  vFuud_t fn = (vFuud_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, d0); DEBUG_LOG; (void)cpu; }
void vFuup(uintptr_t fcn) { __CPU;  vFuup_t fn = (vFuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17");  vFuff_t fn = (vFuff_t)fcn; fn((uint32_t)R_RDI, f0, f1); DEBUG_LOG; (void)cpu; }
void vFudd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17");  vFudd_t fn = (vFudd_t)fcn; fn((uint32_t)R_RDI, d0, d1); DEBUG_LOG; (void)cpu; }
void vFull(uintptr_t fcn) { __CPU;  vFull_t fn = (vFull_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFulp(uintptr_t fcn) { __CPU;  vFulp_t fn = (vFulp_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFupp(uintptr_t fcn) { __CPU;  vFupp_t fn = (vFupp_t)fcn; fn((uint32_t)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFfff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18");  vFfff_t fn = (vFfff_t)fcn; fn(f0, f1, f2); DEBUG_LOG; (void)cpu; }
void vFddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18");  vFddd_t fn = (vFddd_t)fcn; fn(d0, d1, d2); DEBUG_LOG; (void)cpu; }
void vFlip(uintptr_t fcn) { __CPU;  vFlip_t fn = (vFlip_t)fcn; fn((intptr_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFlll(uintptr_t fcn) { __CPU;  vFlll_t fn = (vFlll_t)fcn; fn((intptr_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFllp(uintptr_t fcn) { __CPU;  vFllp_t fn = (vFllp_t)fcn; fn((intptr_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFlpp(uintptr_t fcn) { __CPU;  vFlpp_t fn = (vFlpp_t)fcn; fn((intptr_t)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFLpp(uintptr_t fcn) { __CPU;  vFLpp_t fn = (vFLpp_t)fcn; fn((uintptr_t)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpiu(uintptr_t fcn) { __CPU;  vFpiu_t fn = (vFpiu_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpif(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFpif_t fn = (vFpif_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, f0); DEBUG_LOG; (void)cpu; }
void vFpid(uintptr_t fcn) { __CPU; register double d0 __asm__("f16");  vFpid_t fn = (vFpid_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, d0); DEBUG_LOG; (void)cpu; }
void vFpip(uintptr_t fcn) { __CPU;  vFpip_t fn = (vFpip_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpui(uintptr_t fcn) { __CPU;  vFpui_t fn = (vFpui_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpuU(uintptr_t fcn) { __CPU;  vFpuU_t fn = (vFpuU_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFplp(uintptr_t fcn) { __CPU;  vFplp_t fn = (vFplp_t)fcn; fn((void*)R_RDI, (intptr_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFppi(uintptr_t fcn) { __CPU;  vFppi_t fn = (vFppi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFppu(uintptr_t fcn) { __CPU;  vFppu_t fn = (vFppu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFppp(uintptr_t fcn) { __CPU;  vFppp_t fn = (vFppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFiip(uintptr_t fcn) { __CPU;  iFiip_t fn = (iFiip_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFipp(uintptr_t fcn) { __CPU;  iFipp_t fn = (iFipp_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFuip(uintptr_t fcn) { __CPU;  iFuip_t fn = (iFuip_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFuup(uintptr_t fcn) { __CPU;  iFuup_t fn = (iFuup_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFuUu(uintptr_t fcn) { __CPU;  iFuUu_t fn = (iFuUu_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFuff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17");  iFuff_t fn = (iFuff_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, f0, f1); DEBUG_LOG; (void)cpu; }
void iFpii(uintptr_t fcn) { __CPU;  iFpii_t fn = (iFpii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpiL(uintptr_t fcn) { __CPU;  iFpiL_t fn = (iFpiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpip(uintptr_t fcn) { __CPU;  iFpip_t fn = (iFpip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpui(uintptr_t fcn) { __CPU;  iFpui_t fn = (iFpui_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpuu(uintptr_t fcn) { __CPU;  iFpuu_t fn = (iFpuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpuU(uintptr_t fcn) { __CPU;  iFpuU_t fn = (iFpuU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpup(uintptr_t fcn) { __CPU;  iFpup_t fn = (iFpup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFplp(uintptr_t fcn) { __CPU;  iFplp_t fn = (iFplp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (intptr_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpLi(uintptr_t fcn) { __CPU;  iFpLi_t fn = (iFpLi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpLp(uintptr_t fcn) { __CPU;  iFpLp_t fn = (iFpLp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFppi(uintptr_t fcn) { __CPU;  iFppi_t fn = (iFppi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFppu(uintptr_t fcn) { __CPU;  iFppu_t fn = (iFppu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFppL(uintptr_t fcn) { __CPU;  iFppL_t fn = (iFppL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFppp(uintptr_t fcn) { __CPU;  iFppp_t fn = (iFppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void uFipu(uintptr_t fcn) { __CPU;  uFipu_t fn = (uFipu_t)fcn; R_RAX=(uint32_t)fn((int32_t)R_RDI, (void*)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void uFuip(uintptr_t fcn) { __CPU;  uFuip_t fn = (uFuip_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void uFuuu(uintptr_t fcn) { __CPU;  uFuuu_t fn = (uFuuu_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void uFuup(uintptr_t fcn) { __CPU;  uFuup_t fn = (uFuup_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void uFupp(uintptr_t fcn) { __CPU;  uFupp_t fn = (uFupp_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void uFpLp(uintptr_t fcn) { __CPU;  uFpLp_t fn = (uFpLp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void fFull(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  fFull_t fn = (fFull_t)fcn; f0=(float)fn((uint32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX); (void)cpu; (void)f0; }
void LFpii(uintptr_t fcn) { __CPU;  LFpii_t fn = (LFpii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFEiV(uintptr_t fcn) { __CPU;  pFEiV_t fn = (pFEiV_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFEpi(uintptr_t fcn) { __CPU;  pFEpi_t fn = (pFEpi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFEpp(uintptr_t fcn) { __CPU;  pFEpp_t fn = (pFEpp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void pFEpV(uintptr_t fcn) { __CPU;  pFEpV_t fn = (pFEpV_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFuii(uintptr_t fcn) { __CPU;  pFuii_t fn = (pFuii_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpii(uintptr_t fcn) { __CPU;  pFpii_t fn = (pFpii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpip(uintptr_t fcn) { __CPU;  pFpip_t fn = (pFpip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpuu(uintptr_t fcn) { __CPU;  pFpuu_t fn = (pFpuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpuL(uintptr_t fcn) { __CPU;  pFpuL_t fn = (pFpuL_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFppi(uintptr_t fcn) { __CPU; pFppi_t fn = (pFppi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpup(uintptr_t fcn) { __CPU; pFpup_t fn = (pFpup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void uFpip(uintptr_t fcn) { __CPU; uFpip_t fn = (uFpip_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpppiuu(uintptr_t fcn) { __CPU; iFpppiuu_t fn = (iFpppiuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFpppiuwu(uintptr_t fcn) { __CPU; iFpppiuwu_t fn = (iFpppiuwu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (int16_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void wFp(uintptr_t fcn) { __CPU; wFp_t fn = (wFp_t)fcn; R_RAX=fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void iFppuuiiuupi(uintptr_t fcn) { __CPU; iFppuuiiuupi_t fn = (iFppuuiiuupi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(void**)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void pFpiip(uintptr_t fcn) { __CPU; pFpiip_t fn = (pFpiip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpupiiiipppppp(uintptr_t fcn) { __CPU; iFpupiiiipppppp_t fn = (iFpupiiiipppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void uFpipp(uintptr_t fcn) { __CPU; uFpipp_t fn = (uFpipp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpiup(uintptr_t fcn) { __CPU; vFpiup_t fn = (vFpiup_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppiiuuuiupup(uintptr_t fcn) { __CPU; vFppiiuuuiupup_t fn = (vFppiiuuuiupup_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void pFppu(uintptr_t fcn) { __CPU;  pFppu_t fn = (pFppu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFppupp(uintptr_t fcn) { __CPU; iFppupp_t fn = (iFppupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpu(uintptr_t fcn) { __CPU; vFpu_t fn = (vFpu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFEppp(uintptr_t fcn) { __CPU; vFEppp_t fn = (vFEppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void pFppp(uintptr_t fcn) { __CPU;  pFppp_t fn = (pFppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFEipp(uintptr_t fcn) { __CPU;  vFEipp_t fn = (vFEipp_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFEpip(uintptr_t fcn) { __CPU;  vFEpip_t fn = (vFEpip_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiiii(uintptr_t fcn) { __CPU;  vFiiii_t fn = (vFiiii_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiiip(uintptr_t fcn) { __CPU;  vFiiip_t fn = (vFiiip_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiill(uintptr_t fcn) { __CPU;  vFiill_t fn = (vFiill_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiIII(uintptr_t fcn) { __CPU;  vFiIII_t fn = (vFiIII_t)fcn; fn((int32_t)R_RDI, (int64_t)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiuiu(uintptr_t fcn) { __CPU;  vFiuiu_t fn = (vFiuiu_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiuip(uintptr_t fcn) { __CPU;  vFiuip_t fn = (vFiuip_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiuuu(uintptr_t fcn) { __CPU;  vFiuuu_t fn = (vFiuuu_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiuup(uintptr_t fcn) { __CPU;  vFiuup_t fn = (vFiuup_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiulp(uintptr_t fcn) { __CPU;  vFiulp_t fn = (vFiulp_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiupu(uintptr_t fcn) { __CPU;  vFiupu_t fn = (vFiupu_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiUUU(uintptr_t fcn) { __CPU;  vFiUUU_t fn = (vFiUUU_t)fcn; fn((int32_t)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFifff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18");  vFifff_t fn = (vFifff_t)fcn; fn((int32_t)R_RDI, f0, f1, f2); DEBUG_LOG; (void)cpu; }
void vFiddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18");  vFiddd_t fn = (vFiddd_t)fcn; fn((int32_t)R_RDI, d0, d1, d2); DEBUG_LOG; (void)cpu; }
void vFilil(uintptr_t fcn) { __CPU;  vFilil_t fn = (vFilil_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFilip(uintptr_t fcn) { __CPU;  vFilip_t fn = (vFilip_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiluU(uintptr_t fcn) { __CPU;  vFiluU_t fn = (vFiluU_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (uint32_t)R_RDX, (uint64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFilpu(uintptr_t fcn) { __CPU;  vFilpu_t fn = (vFilpu_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFilpp(uintptr_t fcn) { __CPU;  vFilpp_t fn = (vFilpp_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFipup(uintptr_t fcn) { __CPU;  vFipup_t fn = (vFipup_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFipll(uintptr_t fcn) { __CPU;  vFipll_t fn = (vFipll_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFippi(uintptr_t fcn) { __CPU;  vFippi_t fn = (vFippi_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFippu(uintptr_t fcn) { __CPU;  vFippu_t fn = (vFippu_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFippp(uintptr_t fcn) { __CPU;  vFippp_t fn = (vFippp_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuiii(uintptr_t fcn) { __CPU;  vFuiii_t fn = (vFuiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuiiu(uintptr_t fcn) { __CPU;  vFuiiu_t fn = (vFuiiu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuiip(uintptr_t fcn) { __CPU;  vFuiip_t fn = (vFuiip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuiII(uintptr_t fcn) { __CPU;  vFuiII_t fn = (vFuiII_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuiui(uintptr_t fcn) { __CPU;  vFuiui_t fn = (vFuiui_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuiuu(uintptr_t fcn) { __CPU;  vFuiuu_t fn = (vFuiuu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuiup(uintptr_t fcn) { __CPU;  vFuiup_t fn = (vFuiup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuiUU(uintptr_t fcn) { __CPU;  vFuiUU_t fn = (vFuiUU_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuifi(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFuifi_t fn = (vFuifi_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, f0, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFuiff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17");  vFuiff_t fn = (vFuiff_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, f0, f1); DEBUG_LOG; (void)cpu; }
void vFuidd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17");  vFuidd_t fn = (vFuidd_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, d0, d1); DEBUG_LOG; (void)cpu; }
void vFuill(uintptr_t fcn) { __CPU;  vFuill_t fn = (vFuill_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuilp(uintptr_t fcn) { __CPU;  vFuilp_t fn = (vFuilp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuipi(uintptr_t fcn) { __CPU;  vFuipi_t fn = (vFuipi_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuipu(uintptr_t fcn) { __CPU;  vFuipu_t fn = (vFuipu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuipp(uintptr_t fcn) { __CPU;  vFuipp_t fn = (vFuipp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuuii(uintptr_t fcn) { __CPU;  vFuuii_t fn = (vFuuii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuuiu(uintptr_t fcn) { __CPU;  vFuuiu_t fn = (vFuuiu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuuil(uintptr_t fcn) { __CPU;  vFuuil_t fn = (vFuuil_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuuip(uintptr_t fcn) { __CPU;  vFuuip_t fn = (vFuuip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuuui(uintptr_t fcn) { __CPU;  vFuuui_t fn = (vFuuui_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuuuu(uintptr_t fcn) { __CPU;  vFuuuu_t fn = (vFuuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuuuf(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFuuuf_t fn = (vFuuuf_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, f0); DEBUG_LOG; (void)cpu; }
void vFuuud(uintptr_t fcn) { __CPU; register double d0 __asm__("f16");  vFuuud_t fn = (vFuuud_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, d0); DEBUG_LOG; (void)cpu; }
void vFuuul(uintptr_t fcn) { __CPU; vFuuul_t fn = (vFuuul_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (intptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuuup(uintptr_t fcn) { __CPU;  vFuuup_t fn = (vFuuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuulp(uintptr_t fcn) { __CPU;  vFuulp_t fn = (vFuulp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuupi(uintptr_t fcn) { __CPU;  vFuupi_t fn = (vFuupi_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuupp(uintptr_t fcn) { __CPU;  vFuupp_t fn = (vFuupp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuUii(uintptr_t fcn) { __CPU;  vFuUii_t fn = (vFuUii_t)fcn; fn((uint32_t)R_RDI, (uint64_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuUip(uintptr_t fcn) { __CPU;  vFuUip_t fn = (vFuUip_t)fcn; fn((uint32_t)R_RDI, (uint64_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFufff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18");  vFufff_t fn = (vFufff_t)fcn; fn((uint32_t)R_RDI, f0, f1, f2); DEBUG_LOG; (void)cpu; }
void vFuddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18");  vFuddd_t fn = (vFuddd_t)fcn; fn((uint32_t)R_RDI, d0, d1, d2); DEBUG_LOG; (void)cpu; }
void vFulil(uintptr_t fcn) { __CPU;  vFulil_t fn = (vFulil_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFulip(uintptr_t fcn) { __CPU;  vFulip_t fn = (vFulip_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuluU(uintptr_t fcn) { __CPU;  vFuluU_t fn = (vFuluU_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (uint32_t)R_RDX, (uint64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFullp(uintptr_t fcn) { __CPU;  vFullp_t fn = (vFullp_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFulpi(uintptr_t fcn) { __CPU;  vFulpi_t fn = (vFulpi_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFulpu(uintptr_t fcn) { __CPU;  vFulpu_t fn = (vFulpu_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFulpp(uintptr_t fcn) { __CPU;  vFulpp_t fn = (vFulpp_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFupii(uintptr_t fcn) { __CPU;  vFupii_t fn = (vFupii_t)fcn; fn((uint32_t)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuppi(uintptr_t fcn) { __CPU;  vFuppi_t fn = (vFuppi_t)fcn; fn((uint32_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFffff_t fn = (vFffff_t)fcn; fn(f0, f1, f2, f3); DEBUG_LOG; (void)cpu; }
void vFdddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFdddd_t fn = (vFdddd_t)fcn; fn(d0, d1, d2, d3); DEBUG_LOG; (void)cpu; }
void vFllll(uintptr_t fcn) { __CPU;  vFllll_t fn = (vFllll_t)fcn; fn((intptr_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpiii(uintptr_t fcn) { __CPU;  vFpiii_t fn = (vFpiii_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpipp(uintptr_t fcn) { __CPU;  vFpipp_t fn = (vFpipp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpupp(uintptr_t fcn) { __CPU;  vFpupp_t fn = (vFpupp_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpdii(uintptr_t fcn) { __CPU; register double d0 __asm__("f16");  vFpdii_t fn = (vFpdii_t)fcn; fn((void*)R_RDI, d0, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18");  vFpddd_t fn = (vFpddd_t)fcn; fn((void*)R_RDI, d0, d1, d2); DEBUG_LOG; (void)cpu; }
void vFplpp(uintptr_t fcn) { __CPU;  vFplpp_t fn = (vFplpp_t)fcn; fn((void*)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppip(uintptr_t fcn) { __CPU;  vFppip_t fn = (vFppip_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpppu(uintptr_t fcn) { __CPU;  vFpppu_t fn = (vFpppu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpppp(uintptr_t fcn) { __CPU;  vFpppp_t fn = (vFpppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFEppp(uintptr_t fcn) { __CPU;  iFEppp_t fn = (iFEppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFiiup(uintptr_t fcn) { __CPU;  iFiiup_t fn = (iFiiup_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFuuff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17");  iFuuff_t fn = (iFuuff_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, f0, f1); DEBUG_LOG; (void)cpu; }
void iFpiiL(uintptr_t fcn) { __CPU;  iFpiiL_t fn = (iFpiiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpiip(uintptr_t fcn) { __CPU;  iFpiip_t fn = (iFpiip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpiipp(uintptr_t fcn) { __CPU;  iFpiipp_t fn = (iFpiipp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFpiippp(uintptr_t fcn) { __CPU;  iFpiippp_t fn = (iFpiippp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8,(void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFpiup(uintptr_t fcn) { __CPU;  iFpiup_t fn = (iFpiup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpipi(uintptr_t fcn) { __CPU;  iFpipi_t fn = (iFpipi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpipp(uintptr_t fcn) { __CPU;  iFpipp_t fn = (iFpipp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpuuu(uintptr_t fcn) { __CPU;  iFpuuu_t fn = (iFpuuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpuup(uintptr_t fcn) { __CPU;  iFpuup_t fn = (iFpuup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpuLL(uintptr_t fcn) { __CPU;  iFpuLL_t fn = (iFpuLL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uintptr_t)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpuLp(uintptr_t fcn) { __CPU;  iFpuLp_t fn = (iFpuLp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpLip(uintptr_t fcn) { __CPU;  iFpLip_t fn = (iFpLip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFppii(uintptr_t fcn) { __CPU;  iFppii_t fn = (iFppii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFppiL(uintptr_t fcn) { __CPU;  iFppiL_t fn = (iFppiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFppip(uintptr_t fcn) { __CPU;  iFppip_t fn = (iFppip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFppuu(uintptr_t fcn) { __CPU;  iFppuu_t fn = (iFppuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFppup(uintptr_t fcn) { __CPU;  iFppup_t fn = (iFppup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpplp(uintptr_t fcn) { __CPU;  iFpplp_t fn = (iFpplp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (intptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFppLp(uintptr_t fcn) { __CPU;  iFppLp_t fn = (iFppLp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpppi(uintptr_t fcn) { __CPU;  iFpppi_t fn = (iFpppi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpppL(uintptr_t fcn) { __CPU;  iFpppL_t fn = (iFpppL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpppp(uintptr_t fcn) { __CPU;  iFpppp_t fn = (iFpppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void uFuuuu(uintptr_t fcn) { __CPU;  uFuuuu_t fn = (uFuuuu_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void uFpuip(uintptr_t fcn) { __CPU;  uFpuip_t fn = (uFpuip_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void LFpCii(uintptr_t fcn) { __CPU;  LFpCii_t fn = (LFpCii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFEpip(uintptr_t fcn) { __CPU;  pFEpip_t fn = (pFEpip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void pFillu(uintptr_t fcn) { __CPU;  pFillu_t fn = (pFillu_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFuiii(uintptr_t fcn) { __CPU;  pFuiii_t fn = (pFuiii_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFulli(uintptr_t fcn) { __CPU;  pFulli_t fn = (pFulli_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFullu(uintptr_t fcn) { __CPU;  pFullu_t fn = (pFullu_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFlfff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18");  pFlfff_t fn = (pFlfff_t)fcn; R_RAX=(uintptr_t)fn((intptr_t)R_RDI, f0, f1, f2); DEBUG_LOG; (void)cpu; }
void pFpiii(uintptr_t fcn) { __CPU;  pFpiii_t fn = (pFpiii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpipp(uintptr_t fcn) { __CPU;  pFpipp_t fn = (pFpipp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFplpp(uintptr_t fcn) { __CPU;  pFplpp_t fn = (pFplpp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFppip(uintptr_t fcn) { __CPU;  pFppip_t fn = (pFppip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFppup(uintptr_t fcn) { __CPU;  pFppup_t fn = (pFppup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpppi(uintptr_t fcn) { __CPU;  pFpppi_t fn = (pFpppi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpppp(uintptr_t fcn) { __CPU;  pFpppp_t fn = (pFpppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFiiiii(uintptr_t fcn) { __CPU;  vFiiiii_t fn = (vFiiiii_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiiiiu(uintptr_t fcn) { __CPU;  vFiiiiu_t fn = (vFiiiiu_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiiuii(uintptr_t fcn) { __CPU;  vFiiuii_t fn = (vFiiuii_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiiuup(uintptr_t fcn) { __CPU;  vFiiuup_t fn = (vFiiuup_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFiillu(uintptr_t fcn) { __CPU;  vFiillu_t fn = (vFiillu_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiilll(uintptr_t fcn) { __CPU;  vFiilll_t fn = (vFiilll_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiipll(uintptr_t fcn) { __CPU;  vFiipll_t fn = (vFiipll_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiIIII(uintptr_t fcn) { __CPU;  vFiIIII_t fn = (vFiIIII_t)fcn; fn((int32_t)R_RDI, (int64_t)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX, (int64_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiuiip(uintptr_t fcn) { __CPU;  vFiuiip_t fn = (vFiuiip_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFiuipi(uintptr_t fcn) { __CPU;  vFiuipi_t fn = (vFiuipi_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiuuuu(uintptr_t fcn) { __CPU;  vFiuuuu_t fn = (vFiuuuu_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiulpp(uintptr_t fcn) { __CPU;  vFiulpp_t fn = (vFiulpp_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFiuppu(uintptr_t fcn) { __CPU;  vFiuppu_t fn = (vFiuppu_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiUUUU(uintptr_t fcn) { __CPU;  vFiUUUU_t fn = (vFiUUUU_t)fcn; fn((int32_t)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (uint64_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFiffff_t fn = (vFiffff_t)fcn; fn((int32_t)R_RDI, f0, f1, f2, f3); DEBUG_LOG; (void)cpu; }
void vFidddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFidddd_t fn = (vFidddd_t)fcn; fn((int32_t)R_RDI, d0, d1, d2, d3); DEBUG_LOG; (void)cpu; }
void vFilill(uintptr_t fcn) { __CPU;  vFilill_t fn = (vFilill_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFilipi(uintptr_t fcn) { __CPU;  vFilipi_t fn = (vFilipi_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFilipl(uintptr_t fcn) { __CPU;  vFilipl_t fn = (vFilipl_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (intptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFillpu(uintptr_t fcn) { __CPU;  vFillpu_t fn = (vFillpu_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFipipu(uintptr_t fcn) { __CPU;  vFipipu_t fn = (vFipipu_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFipipp(uintptr_t fcn) { __CPU;  vFipipp_t fn = (vFipipp_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFipupi(uintptr_t fcn) { __CPU;  vFipupi_t fn = (vFipupi_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiplli(uintptr_t fcn) { __CPU;  vFiplli_t fn = (vFiplli_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFiplll(uintptr_t fcn) { __CPU;  vFiplll_t fn = (vFiplll_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiiii(uintptr_t fcn) { __CPU;  vFuiiii_t fn = (vFuiiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiiiu(uintptr_t fcn) { __CPU;  vFuiiiu_t fn = (vFuiiiu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiiip(uintptr_t fcn) { __CPU;  vFuiiip_t fn = (vFuiiip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiifi(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFuiifi_t fn = (vFuiifi_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, f0, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuiill(uintptr_t fcn) { __CPU;  vFuiill_t fn = (vFuiill_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiilp(uintptr_t fcn) { __CPU;  vFuiilp_t fn = (vFuiilp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiIII(uintptr_t fcn) { __CPU;  vFuiIII_t fn = (vFuiIII_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX, (int64_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiuii(uintptr_t fcn) { __CPU;  vFuiuii_t fn = (vFuiuii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiuiu(uintptr_t fcn) { __CPU;  vFuiuiu_t fn = (vFuiuiu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiuip(uintptr_t fcn) { __CPU;  vFuiuip_t fn = (vFuiuip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiuuu(uintptr_t fcn) { __CPU;  vFuiuuu_t fn = (vFuiuuu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiuup(uintptr_t fcn) { __CPU;  vFuiuup_t fn = (vFuiuup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiull(uintptr_t fcn) { __CPU;  vFuiull_t fn = (vFuiull_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiupi(uintptr_t fcn) { __CPU;  vFuiupi_t fn = (vFuiupi_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuiUUU(uintptr_t fcn) { __CPU;  vFuiUUU_t fn = (vFuiUUU_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (uint64_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuifff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18");  vFuifff_t fn = (vFuifff_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, f0, f1, f2); DEBUG_LOG; (void)cpu; }
void vFuiddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18");  vFuiddd_t fn = (vFuiddd_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, d0, d1, d2); DEBUG_LOG; (void)cpu; }
void vFuipip(uintptr_t fcn) { __CPU;  vFuipip_t fn = (vFuipip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuipup(uintptr_t fcn) { __CPU;  vFuipup_t fn = (vFuipup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuippp(uintptr_t fcn) { __CPU;  vFuippp_t fn = (vFuippp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuiii(uintptr_t fcn) { __CPU;  vFuuiii_t fn = (vFuuiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuiiu(uintptr_t fcn) { __CPU;  vFuuiiu_t fn = (vFuuiiu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuiui(uintptr_t fcn) { __CPU;  vFuuiui_t fn = (vFuuiui_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuiuu(uintptr_t fcn) { __CPU;  vFuuiuu_t fn = (vFuuiuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuiup(uintptr_t fcn) { __CPU;  vFuuiup_t fn = (vFuuiup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuipi(uintptr_t fcn) { __CPU;  vFuuipi_t fn = (vFuuipi_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuipu(uintptr_t fcn) { __CPU;  vFuuipu_t fn = (vFuuipu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuipp(uintptr_t fcn) { __CPU;  vFuuipp_t fn = (vFuuipp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuuii(uintptr_t fcn) { __CPU;  vFuuuii_t fn = (vFuuuii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuuiu(uintptr_t fcn) { __CPU;  vFuuuiu_t fn = (vFuuuiu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuuip(uintptr_t fcn) { __CPU;  vFuuuip_t fn = (vFuuuip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuuui(uintptr_t fcn) { __CPU;  vFuuuui_t fn = (vFuuuui_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuuuu(uintptr_t fcn) { __CPU;  vFuuuuu_t fn = (vFuuuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuuup(uintptr_t fcn) { __CPU;  vFuuuup_t fn = (vFuuuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuull(uintptr_t fcn) { __CPU;  vFuuull_t fn = (vFuuull_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuulp(uintptr_t fcn) { __CPU;  vFuuulp_t fn = (vFuuulp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (intptr_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuulll(uintptr_t fcn) { __CPU;  vFuulll_t fn = (vFuulll_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuullp(uintptr_t fcn) { __CPU;  vFuullp_t fn = (vFuullp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuulpp(uintptr_t fcn) { __CPU;  vFuulpp_t fn = (vFuulpp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuupii(uintptr_t fcn) { __CPU;  vFuupii_t fn = (vFuupii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFuffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFuffff_t fn = (vFuffff_t)fcn; fn((uint32_t)R_RDI, f0, f1, f2, f3); DEBUG_LOG; (void)cpu; }
void vFudddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFudddd_t fn = (vFudddd_t)fcn; fn((uint32_t)R_RDI, d0, d1, d2, d3); DEBUG_LOG; (void)cpu; }
void vFulill(uintptr_t fcn) { __CPU;  vFulill_t fn = (vFulill_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFullip(uintptr_t fcn) { __CPU;  vFullip_t fn = (vFullip_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFullpp(uintptr_t fcn) { __CPU;  vFullpp_t fn = (vFullpp_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFupupi(uintptr_t fcn) { __CPU;  vFupupi_t fn = (vFupupi_t)fcn; fn((uint32_t)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFupupp(uintptr_t fcn) { __CPU;  vFupupp_t fn = (vFupupp_t)fcn; fn((uint32_t)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuppip(uintptr_t fcn) { __CPU;  vFuppip_t fn = (vFuppip_t)fcn; fn((uint32_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFfffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20");  vFfffff_t fn = (vFfffff_t)fcn; fn(f0, f1, f2, f3, f4); DEBUG_LOG; (void)cpu; }
void vFddddp(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFddddp_t fn = (vFddddp_t)fcn; fn(d0, d1, d2, d3, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFpilpp(uintptr_t fcn) { __CPU;  vFpilpp_t fn = (vFpilpp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpipii(uintptr_t fcn) { __CPU;  vFpipii_t fn = (vFpipii_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpuipp(uintptr_t fcn) { __CPU;  vFpuipp_t fn = (vFpuipp_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpddii(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17");  vFpddii_t fn = (vFpddii_t)fcn; fn((void*)R_RDI, d0, d1, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFppiii(uintptr_t fcn) { __CPU;  vFppiii_t fn = (vFppiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpppii(uintptr_t fcn) { __CPU;  vFpppii_t fn = (vFpppii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFppppu(uintptr_t fcn) { __CPU;  vFppppu_t fn = (vFppppu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFppppp(uintptr_t fcn) { __CPU;  vFppppp_t fn = (vFppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFEpppp(uintptr_t fcn) { __CPU;  iFEpppp_t fn = (iFEpppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpiii(uintptr_t fcn) { __CPU;  iFpiii_t fn = (iFpiii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpiiii(uintptr_t fcn) { __CPU;  iFpiiii_t fn = (iFpiiii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpiiip(uintptr_t fcn) { __CPU;  iFpiiip_t fn = (iFpiiip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFpiiuu(uintptr_t fcn) { __CPU;  iFpiiuu_t fn = (iFpiiuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpiipi(uintptr_t fcn) { __CPU;  iFpiipi_t fn = (iFpiipi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpippp(uintptr_t fcn) { __CPU;  iFpippp_t fn = (iFpippp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFpipppp(uintptr_t fcn) { __CPU;  iFpipppp_t fn = (iFpipppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFpipipip(uintptr_t fcn) { __CPU; iFpipipip_t fn = (iFpipipip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpipip(uintptr_t fcn) { __CPU; iFpipip_t fn = (iFpipip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFpiipip(uintptr_t fcn) { __CPU; iFpiipip_t fn = (iFpiipip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFpipppppppppp(uintptr_t fcn) { __CPU; iFpipppppppppp_t fn = (iFpipppppppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void iFpiippiiipip(uintptr_t fcn) { __CPU; iFpiippiiipip_t fn = (iFpiippiiipip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void iFpiipiiipip(uintptr_t fcn) { __CPU; iFpiipiiipip_t fn = (iFpiipiiipip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void iFpipllipppppp(uintptr_t fcn) { __CPU; iFpipllipppppp_t fn = (iFpipllipppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void iFpipLpiiip(uintptr_t fcn) { __CPU; iFpipLpiiip_t fn = (iFpipLpiiip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uintptr_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void iFpippddiidd(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); iFpippddiidd_t fn = (iFpippddiidd_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, f0, f1 , (int32_t)R_R8, (int32_t)R_R9, f2, f3); DEBUG_LOG; (void)cpu; }
void iFpuuLL(uintptr_t fcn) { __CPU;  iFpuuLL_t fn = (iFpuuLL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uintptr_t)R_RCX, (uintptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpCupp(uintptr_t fcn) { __CPU;  iFpCupp_t fn = (iFpCupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFppiip(uintptr_t fcn) { __CPU;  iFppiip_t fn = (iFppiip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFppiup(uintptr_t fcn) { __CPU;  iFppiup_t fn = (iFppiup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFppipi(uintptr_t fcn) { __CPU;  iFppipi_t fn = (iFppipi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFppipp(uintptr_t fcn) { __CPU;  iFppipp_t fn = (iFppipp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFpppii(uintptr_t fcn) { __CPU;  iFpppii_t fn = (iFpppii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpppiL(uintptr_t fcn) { __CPU;  iFpppiL_t fn = (iFpppiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (uintptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpppip(uintptr_t fcn) { __CPU;  iFpppip_t fn = (iFpppip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFppppi(uintptr_t fcn) { __CPU;  iFppppi_t fn = (iFppppi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFppppp(uintptr_t fcn) { __CPU;  iFppppp_t fn = (iFppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void IFppIII(uintptr_t fcn) { __CPU;  IFppIII_t fn = (IFppIII_t)fcn; R_RAX=(int64_t)fn((void*)R_RDI, (void*)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX, (int64_t)R_R8); DEBUG_LOG; (void)cpu; }
void uFuiiiu(uintptr_t fcn) { __CPU;  uFuiiiu_t fn = (uFuiiiu_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void UFuiiii(uintptr_t fcn) { __CPU;  UFuiiii_t fn = (UFuiiii_t)fcn; R_RAX=fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void pFuiipp(uintptr_t fcn) { __CPU;  pFuiipp_t fn = (pFuiipp_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFpiiuu(uintptr_t fcn) { __CPU;  pFpiiuu_t fn = (pFpiiuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void pFpippp(uintptr_t fcn) { __CPU;  pFpippp_t fn = (pFpippp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFppipi(uintptr_t fcn) { __CPU;  pFppipi_t fn = (pFppipi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void pFppipp(uintptr_t fcn) { __CPU;  pFppipp_t fn = (pFppipp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFppuuu(uintptr_t fcn) { __CPU;  pFppuuu_t fn = (pFppuuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void pFppuup(uintptr_t fcn) { __CPU;  pFppuup_t fn = (pFppuup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFppuuppp(uintptr_t fcn) { __CPU; pFppuuppp_t fn = (pFppuuppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFppLLp(uintptr_t fcn) { __CPU;  pFppLLp_t fn = (pFppLLp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (uintptr_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFpppip(uintptr_t fcn) { __CPU;  pFpppip_t fn = (pFpppip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFpppuu(uintptr_t fcn) { __CPU;  pFpppuu_t fn = (pFpppuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void pFppppp(uintptr_t fcn) { __CPU;  pFppppp_t fn = (pFppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFiiiiii(uintptr_t fcn) { __CPU;  vFiiiiii_t fn = (vFiiiiii_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFiiiuil(uintptr_t fcn) { __CPU;  vFiiiuil_t fn = (vFiiiuil_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFiiilpi(uintptr_t fcn) { __CPU;  vFiiilpi_t fn = (vFiiilpi_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (void*)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFiiuiil(uintptr_t fcn) { __CPU;  vFiiuiil_t fn = (vFiiuiil_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFiiuilp(uintptr_t fcn) { __CPU;  vFiiuilp_t fn = (vFiiuilp_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (intptr_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFiiuulp(uintptr_t fcn) { __CPU;  vFiiuulp_t fn = (vFiiuulp_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (intptr_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFiililp(uintptr_t fcn) { __CPU;  vFiililp_t fn = (vFiililp_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (intptr_t)R_RDX, (int32_t)R_RCX, (intptr_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFiiplli(uintptr_t fcn) { __CPU;  vFiiplli_t fn = (vFiiplli_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFiiplll(uintptr_t fcn) { __CPU;  vFiiplll_t fn = (vFiiplll_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFiuippp(uintptr_t fcn) { __CPU;  vFiuippp_t fn = (vFiuippp_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFiffiff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFiffiff_t fn = (vFiffiff_t)fcn; fn((int32_t)R_RDI, f0, f1, (int32_t)R_RSI, f2, f3); DEBUG_LOG; (void)cpu; }
void vFiddidd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFiddidd_t fn = (vFiddidd_t)fcn; fn((int32_t)R_RDI, d0, d1, (int32_t)R_RSI, d2, d3); DEBUG_LOG; (void)cpu; }
void vFililuU(uintptr_t fcn) { __CPU;  vFililuU_t fn = (vFililuU_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (uint32_t)R_R8, (uint64_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFililll(uintptr_t fcn) { __CPU;  vFililll_t fn = (vFililll_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFilipli(uintptr_t fcn) { __CPU;  vFilipli_t fn = (vFilipli_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (intptr_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFiliplu(uintptr_t fcn) { __CPU;  vFiliplu_t fn = (vFiliplu_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (intptr_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFillill(uintptr_t fcn) { __CPU;  vFillill_t fn = (vFillill_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (int32_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFipiplp(uintptr_t fcn) { __CPU;  vFipiplp_t fn = (vFipiplp_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (intptr_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFipllli(uintptr_t fcn) { __CPU;  vFipllli_t fn = (vFipllli_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiiiii(uintptr_t fcn) { __CPU;  vFuiiiii_t fn = (vFuiiiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiiiil(uintptr_t fcn) { __CPU;  vFuiiiil_t fn = (vFuiiiil_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiiilp(uintptr_t fcn) { __CPU;  vFuiiilp_t fn = (vFuiiilp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (intptr_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiiuii(uintptr_t fcn) { __CPU;  vFuiiuii_t fn = (vFuiiuii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiiuup(uintptr_t fcn) { __CPU;  vFuiiuup_t fn = (vFuiiuup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiIIII(uintptr_t fcn) { __CPU;  vFuiIIII_t fn = (vFuiIIII_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX, (int64_t)R_R8, (int64_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiuiii(uintptr_t fcn) { __CPU;  vFuiuiii_t fn = (vFuiuiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiuiil(uintptr_t fcn) { __CPU;  vFuiuiil_t fn = (vFuiuiil_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiuiip(uintptr_t fcn) { __CPU;  vFuiuiip_t fn = (vFuiuiip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiuiuu(uintptr_t fcn) { __CPU;  vFuiuiuu_t fn = (vFuiuiuu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiuuip(uintptr_t fcn) { __CPU;  vFuiuuip_t fn = (vFuiuuip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiuuuu(uintptr_t fcn) { __CPU;  vFuiuuuu_t fn = (vFuiuuuu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiuulp(uintptr_t fcn) { __CPU;  vFuiuulp_t fn = (vFuiuulp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (intptr_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiupii(uintptr_t fcn) { __CPU;  vFuiupii_t fn = (vFuiupii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiupiu(uintptr_t fcn) { __CPU;  vFuiupiu_t fn = (vFuiupiu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiUUUU(uintptr_t fcn) { __CPU;  vFuiUUUU_t fn = (vFuiUUUU_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (uint64_t)R_R8, (uint64_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuiffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFuiffff_t fn = (vFuiffff_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, f0, f1, f2, f3); DEBUG_LOG; (void)cpu; }
void vFuidddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFuidddd_t fn = (vFuidddd_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, d0, d1, d2, d3); DEBUG_LOG; (void)cpu; }
void vFuuiiii(uintptr_t fcn) { __CPU;  vFuuiiii_t fn = (vFuuiiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuiiiu(uintptr_t fcn) { __CPU;  vFuuiiiu_t fn = (vFuuiiiu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuiuii(uintptr_t fcn) { __CPU;  vFuuiuii_t fn = (vFuuiuii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuiuiu(uintptr_t fcn) { __CPU;  vFuuiuiu_t fn = (vFuuiuiu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuiuup(uintptr_t fcn) { __CPU;  vFuuiuup_t fn = (vFuuiuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuuiup(uintptr_t fcn) { __CPU;  vFuuuiup_t fn = (vFuuuiup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuuipi(uintptr_t fcn) { __CPU;  vFuuuipi_t fn = (vFuuuipi_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuuipp(uintptr_t fcn) { __CPU;  vFuuuipp_t fn = (vFuuuipp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuuuii(uintptr_t fcn) { __CPU;  vFuuuuii_t fn = (vFuuuuii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuuuip(uintptr_t fcn) { __CPU;  vFuuuuip_t fn = (vFuuuuip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuuuuu(uintptr_t fcn) { __CPU;  vFuuuuuu_t fn = (vFuuuuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuuuff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17");  vFuuuuff_t fn = (vFuuuuff_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, f0, f1); DEBUG_LOG; (void)cpu; }
void vFuuuppi(uintptr_t fcn) { __CPU;  vFuuuppi_t fn = (vFuuuppi_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuuppp(uintptr_t fcn) { __CPU;  vFuuuppp_t fn = (vFuuuppp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuuffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFuuffff_t fn = (vFuuffff_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, f0, f1, f2, f3); DEBUG_LOG; (void)cpu; }
void vFuudddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFuudddd_t fn = (vFuudddd_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, d0, d1, d2, d3); DEBUG_LOG; (void)cpu; }
void vFuulppp(uintptr_t fcn) { __CPU;  vFuulppp_t fn = (vFuulppp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuupupp(uintptr_t fcn) { __CPU;  vFuupupp_t fn = (vFuupupp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuffiip(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17");  vFuffiip_t fn = (vFuffiip_t)fcn; fn((uint32_t)R_RDI, f0, f1, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFufffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20");  vFufffff_t fn = (vFufffff_t)fcn; fn((uint32_t)R_RDI, f0, f1, f2, f3, f4); DEBUG_LOG; (void)cpu; }
void vFuddiip(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17");  vFuddiip_t fn = (vFuddiip_t)fcn; fn((uint32_t)R_RDI, d0, d1, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFuliluU(uintptr_t fcn) { __CPU;  vFuliluU_t fn = (vFuliluU_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (uint32_t)R_R8, (uint64_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFulilli(uintptr_t fcn) { __CPU;  vFulilli_t fn = (vFulilli_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFulilll(uintptr_t fcn) { __CPU;  vFulilll_t fn = (vFulilll_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFullill(uintptr_t fcn) { __CPU;  vFullill_t fn = (vFullill_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (int32_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFulplup(uintptr_t fcn) { __CPU;  vFulplup_t fn = (vFulplup_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (intptr_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFupupip(uintptr_t fcn) { __CPU;  vFupupip_t fn = (vFupupip_t)fcn; fn((uint32_t)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuppppu(uintptr_t fcn) { __CPU;  vFuppppu_t fn = (vFuppppu_t)fcn; fn((uint32_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21");  vFffffff_t fn = (vFffffff_t)fcn; fn(f0, f1, f2, f3, f4, f5); DEBUG_LOG; (void)cpu; }
void vFdddddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19"); register double d4 __asm__("f20"); register double d5 __asm__("f21");  vFdddddd_t fn = (vFdddddd_t)fcn; fn(d0, d1, d2, d3, d4, d5); DEBUG_LOG; (void)cpu; }
void vFpdddii(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18");  vFpdddii_t fn = (vFpdddii_t)fcn; fn((void*)R_RDI, d0, d1, d2, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFppiiii(uintptr_t fcn) { __CPU;  vFppiiii_t fn = (vFppiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFppupii(uintptr_t fcn) { __CPU;  vFppupii_t fn = (vFppupii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFEppppp(uintptr_t fcn) { __CPU;  iFEppppp_t fn = (iFEppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFiiiiip(uintptr_t fcn) { __CPU;  iFiiiiip_t fn = (iFiiiiip_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFipuufp(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  iFipuufp_t fn = (iFipuufp_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, f0, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFLppipp(uintptr_t fcn) { __CPU;  iFLppipp_t fn = (iFLppipp_t)fcn; R_RAX=(int32_t)fn((uintptr_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFpiiiii(uintptr_t fcn) { __CPU;  iFpiiiii_t fn = (iFpiiiii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFpiiipp(uintptr_t fcn) { __CPU;  iFpiiipp_t fn = (iFpiiipp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppiiii(uintptr_t fcn) { __CPU;  iFppiiii_t fn = (iFppiiii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFppiiuu(uintptr_t fcn) { __CPU;  iFppiiuu_t fn = (iFppiiuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFppIppp(uintptr_t fcn) { __CPU;  iFppIppp_t fn = (iFppIppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int64_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppuiii(uintptr_t fcn) { __CPU;  iFppuiii_t fn = (iFppuiii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFppiiiL(uintptr_t fcn) { __CPU;  iFppiiiL_t fn = (iFppiiiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uintptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFppuupp(uintptr_t fcn) { __CPU;  iFppuupp_t fn = (iFppuupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppupip(uintptr_t fcn) { __CPU;  iFppupip_t fn = (iFppupip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFpppipi(uintptr_t fcn) { __CPU;  iFpppipi_t fn = (iFpppipi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFpppiipiiu(uintptr_t fcn) {__CPU; iFpppiipiiu_t fn = (iFpppiipiiu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void iFpppipp(uintptr_t fcn) { __CPU;  iFpppipp_t fn = (iFpppipp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppppii(uintptr_t fcn) { __CPU;  iFppppii_t fn = (iFppppii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFpppppi(uintptr_t fcn) { __CPU;  iFpppppi_t fn = (iFpppppi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFpppppL(uintptr_t fcn) { __CPU;  iFpppppL_t fn = (iFpppppL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uintptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFpppppp(uintptr_t fcn) { __CPU;  iFpppppp_t fn = (iFpppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void pFpippip(uintptr_t fcn) { __CPU;  pFpippip_t fn = (pFpippip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void pFpppppp(uintptr_t fcn) { __CPU;  pFpppppp_t fn = (pFpppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFiiiiiip(uintptr_t fcn) { __CPU;  vFiiiiiip_t fn = (vFiiiiiip_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFiiiiuup(uintptr_t fcn) { __CPU;  vFiiiiuup_t fn = (vFiiiiuup_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFiiuilil(uintptr_t fcn) { __CPU;  vFiiuilil_t fn = (vFiiuilil_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (intptr_t)R_R8, (int32_t)R_R9, *(intptr_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFiiffffp(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFiiffffp_t fn = (vFiiffffp_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, f0, f1, f2, f3, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFiipllli(uintptr_t fcn) { __CPU;  vFiipllli_t fn = (vFiipllli_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFiuulipi(uintptr_t fcn) { __CPU;  vFiuulipi_t fn = (vFiuulipi_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (intptr_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFililluU(uintptr_t fcn) { __CPU;  vFililluU_t fn = (vFililluU_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (uint32_t)R_R9, *(uint64_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFilipliu(uintptr_t fcn) { __CPU;  vFilipliu_t fn = (vFilipliu_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (intptr_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFilulipi(uintptr_t fcn) { __CPU;  vFilulipi_t fn = (vFilulipi_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (uint32_t)R_RDX, (intptr_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuiiiiii(uintptr_t fcn) { __CPU;  vFuiiiiii_t fn = (vFuiiiiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuiiiuip(uintptr_t fcn) { __CPU;  vFuiiiuip_t fn = (vFuiiiuip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuiiiuup(uintptr_t fcn) { __CPU;  vFuiiiuup_t fn = (vFuiiiuup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuiiliip(uintptr_t fcn) { __CPU;  vFuiiliip_t fn = (vFuiiliip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuiililp(uintptr_t fcn) { __CPU;  vFuiililp_t fn = (vFuiililp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuiuiiii(uintptr_t fcn) { __CPU;  vFuiuiiii_t fn = (vFuiuiiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuiuiiip(uintptr_t fcn) { __CPU;  vFuiuiiip_t fn = (vFuiuiiip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuiuiiuu(uintptr_t fcn) { __CPU;  vFuiuiiuu_t fn = (vFuiuiiuu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuiupiiu(uintptr_t fcn) { __CPU;  vFuiupiiu_t fn = (vFuiupiiu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuilliip(uintptr_t fcn) { __CPU;  vFuilliip_t fn = (vFuilliip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuipiiii(uintptr_t fcn) { __CPU;  vFuipiiii_t fn = (vFuipiiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuipffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFuipffff_t fn = (vFuipffff_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, f0, f1, f2, f3); DEBUG_LOG; (void)cpu; }
void vFuipdddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFuipdddd_t fn = (vFuipdddd_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, d0, d1, d2, d3); DEBUG_LOG; (void)cpu; }
void vFuuiiiii(uintptr_t fcn) { __CPU;  vFuuiiiii_t fn = (vFuuiiiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuiiiui(uintptr_t fcn) { __CPU;  vFuuiiiui_t fn = (vFuuiiiui_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuiiiuu(uintptr_t fcn) { __CPU;  vFuuiiiuu_t fn = (vFuuiiiuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuiiuup(uintptr_t fcn) { __CPU;  vFuuiiuup_t fn = (vFuuiiuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuiuiii(uintptr_t fcn) { __CPU;  vFuuiuiii_t fn = (vFuuiuiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuipppp(uintptr_t fcn) { __CPU;  vFuuipppp_t fn = (vFuuipppp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuuiiii(uintptr_t fcn) { __CPU;  vFuuuiiii_t fn = (vFuuuiiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuuiiip(uintptr_t fcn) { __CPU;  vFuuuiiip_t fn = (vFuuuiiip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuuiuii(uintptr_t fcn) { __CPU;  vFuuuiuii_t fn = (vFuuuiuii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuuiupi(uintptr_t fcn) { __CPU;  vFuuuiupi_t fn = (vFuuuiupi_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuuuiip(uintptr_t fcn) { __CPU;  vFuuuuiip_t fn = (vFuuuuiip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuuuuuu(uintptr_t fcn) { __CPU;  vFuuuuuuu_t fn = (vFuuuuuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuuufff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18");  vFuuuufff_t fn = (vFuuuufff_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, f0, f1, f2); DEBUG_LOG; (void)cpu; }
void vFuuuulll(uintptr_t fcn) { __CPU;  vFuuuulll_t fn = (vFuuuulll_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(intptr_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuuffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFuuuffff_t fn = (vFuuuffff_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, f0, f1, f2, f3); DEBUG_LOG; (void)cpu; }
void vFuuudddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFuuudddd_t fn = (vFuuudddd_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, d0, d1, d2, d3); DEBUG_LOG; (void)cpu; }
void vFuuffiip(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17");  vFuuffiip_t fn = (vFuuffiip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, f0, f1, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuddiip(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17");  vFuuddiip_t fn = (vFuuddiip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, d0, d1, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFuuppppu(uintptr_t fcn) { __CPU;  vFuuppppu_t fn = (vFuuppppu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuppppp(uintptr_t fcn) { __CPU;  vFuuppppp_t fn = (vFuuppppp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21");  vFuffffff_t fn = (vFuffffff_t)fcn; fn((uint32_t)R_RDI, f0, f1, f2, f3, f4, f5); DEBUG_LOG; (void)cpu; }
void vFudddddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19"); register double d4 __asm__("f20"); register double d5 __asm__("f21");  vFudddddd_t fn = (vFudddddd_t)fcn; fn((uint32_t)R_RDI, d0, d1, d2, d3, d4, d5); DEBUG_LOG; (void)cpu; }
void vFulilluU(uintptr_t fcn) { __CPU;  vFulilluU_t fn = (vFulilluU_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (uint32_t)R_R9, *(uint64_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFulillli(uintptr_t fcn) { __CPU;  vFulillli_t fn = (vFulillli_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFulipulp(uintptr_t fcn) { __CPU;  vFulipulp_t fn = (vFulipulp_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (intptr_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFulpiill(uintptr_t fcn) { __CPU;  vFulpiill_t fn = (vFulpiill_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9, *(intptr_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFlipuiip(uintptr_t fcn) { __CPU;  vFlipuiip_t fn = (vFlipuiip_t)fcn; fn((intptr_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFlliiiip(uintptr_t fcn) { __CPU;  vFlliiiip_t fn = (vFlliiiip_t)fcn; fn((intptr_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpipipii(uintptr_t fcn) { __CPU;  vFpipipii_t fn = (vFpipipii_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpippiipi(uintptr_t fcn) { __CPU; vFpippiipi_t fn = (vFpippiipi_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFpddiidd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFpddiidd_t fn = (vFpddiidd_t)fcn; fn((void*)R_RDI, d0, d1, (int32_t)R_RSI, (int32_t)R_RDX, d2, d3); DEBUG_LOG; (void)cpu; }
void vFppiiipi(uintptr_t fcn) { __CPU;  vFppiiipi_t fn = (vFppiiipi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpppiiii(uintptr_t fcn) { __CPU;  vFpppiiii_t fn = (vFpppiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFEpppiiu(uintptr_t fcn) { __CPU;  iFEpppiiu_t fn = (iFEpppiiu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFEpppppp(uintptr_t fcn) { __CPU;  iFEpppppp_t fn = (iFEpppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFiiiiiip(uintptr_t fcn) { __CPU;  iFiiiiiip_t fn = (iFiiiiiip_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpiupiii(uintptr_t fcn) { __CPU;  iFpiupiii_t fn = (iFpiupiii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpuppppp(uintptr_t fcn) { __CPU;  iFpuppppp_t fn = (iFpuppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFppiiuui(uintptr_t fcn) { __CPU;  iFppiiuui_t fn = (iFppiiuui_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFppiipii(uintptr_t fcn) { __CPU;  iFppiipii_t fn = (iFppiipii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFppipupu(uintptr_t fcn) { __CPU;  iFppipupu_t fn = (iFppipupu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFppipppp(uintptr_t fcn) { __CPU;  iFppipppp_t fn = (iFppipppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpppiiii(uintptr_t fcn) { __CPU;  iFpppiiii_t fn = (iFpppiiii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpppiiuu(uintptr_t fcn) { __CPU;  iFpppiiuu_t fn = (iFpppiiuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpppiipi(uintptr_t fcn) { __CPU;  iFpppiipi_t fn = (iFpppiipi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFppppiii(uintptr_t fcn) { __CPU;  iFppppiii_t fn = (iFppppiii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void uFuippppp(uintptr_t fcn) { __CPU;  uFuippppp_t fn = (uFuippppp_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFEppppip(uintptr_t fcn) { __CPU;  pFEppppip_t fn = (pFEppppip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void pFppppuuu(uintptr_t fcn) { __CPU;  pFppppuuu_t fn = (pFppppuuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpppppuu(uintptr_t fcn) { __CPU;  pFpppppuu_t fn = (pFpppppuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFppppppp(uintptr_t fcn) { __CPU;  pFppppppp_t fn = (pFppppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFuupupup(uintptr_t fcn) { __CPU;  pFuupupup_t fn = (pFuupupup_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFiiiiuuip(uintptr_t fcn) { __CPU;  vFiiiiuuip_t fn = (vFiiiiuuip_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFiilliilp(uintptr_t fcn) { __CPU;  vFiilliilp_t fn = (vFiilliilp_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(intptr_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFililliuU(uintptr_t fcn) { __CPU;  vFililliuU_t fn = (vFililliuU_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint64_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFilillluU(uintptr_t fcn) { __CPU;  vFilillluU_t fn = (vFilillluU_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint64_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFilipufip(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFilipufip_t fn = (vFilipufip_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (uint32_t)R_R8, f0, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiii(uintptr_t fcn) { __CPU;  vFuiiiiiii_t fn = (vFuiiiiiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuiiiiill(uintptr_t fcn) { __CPU;  vFuiiiiill_t fn = (vFuiiiiill_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(intptr_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuiiiiuup(uintptr_t fcn) { __CPU;  vFuiiiiuup_t fn = (vFuiiiiuup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuiuiiiii(uintptr_t fcn) { __CPU;  vFuiuiiiii_t fn = (vFuiuiiiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuiuiiiip(uintptr_t fcn) { __CPU;  vFuiuiiiip_t fn = (vFuiuiiiip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuiuiuuuu(uintptr_t fcn) { __CPU;  vFuiuiuuuu_t fn = (vFuiuiuuuu_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuiulplpp(uintptr_t fcn) { __CPU;  vFuiulplpp_t fn = (vFuiulplpp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (intptr_t)R_RCX, (void*)R_R8, (intptr_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuipuliuf(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFuipuliuf_t fn = (vFuipuliuf_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (intptr_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), f0); DEBUG_LOG; (void)cpu; }
void vFuuiiiiii(uintptr_t fcn) { __CPU;  vFuuiiiiii_t fn = (vFuuiiiiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuuiiiuip(uintptr_t fcn) { __CPU;  vFuuiiiuip_t fn = (vFuuiiiuip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuuiiiuup(uintptr_t fcn) { __CPU;  vFuuiiiuup_t fn = (vFuuiiiuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuuiiuupp(uintptr_t fcn) { __CPU;  vFuuiiuupp_t fn = (vFuuiiuupp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuuiuiiii(uintptr_t fcn) { __CPU;  vFuuiuiiii_t fn = (vFuuiuiiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuuiuiiip(uintptr_t fcn) { __CPU;  vFuuiuiiip_t fn = (vFuuiuiiip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuuuiiiii(uintptr_t fcn) { __CPU;  vFuuuiiiii_t fn = (vFuuuiiiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuuuiuiii(uintptr_t fcn) { __CPU;  vFuuuiuiii_t fn = (vFuuuiuiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuuuipipp(uintptr_t fcn) { __CPU;  vFuuuipipp_t fn = (vFuuuipipp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuuuuuuuu(uintptr_t fcn) { __CPU;  vFuuuuuuuu_t fn = (vFuuuuuuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuuuuufff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18");  vFuuuuufff_t fn = (vFuuuuufff_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, f0, f1, f2); DEBUG_LOG; (void)cpu; }
void vFuuufffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20");  vFuuufffff_t fn = (vFuuufffff_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, f0, f1, f2, f3, f4); DEBUG_LOG; (void)cpu; }
void vFulilliuU(uintptr_t fcn) { __CPU;  vFulilliuU_t fn = (vFulilliuU_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint64_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFulillluU(uintptr_t fcn) { __CPU;  vFulillluU_t fn = (vFulillluU_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint64_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFulllplip(uintptr_t fcn) { __CPU;  vFulllplip_t fn = (vFulllplip_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, (intptr_t)R_RCX, (void*)R_R8, (intptr_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFffffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21"); register float f6 __asm__("f22"); register float f7 __asm__("f23");  vFffffffff_t fn = (vFffffffff_t)fcn; fn(f0, f1, f2, f3, f4, f5, f6, f7); DEBUG_LOG; (void)cpu; }
void vFlipuiuip(uintptr_t fcn) { __CPU;  vFlipuiuip_t fn = (vFlipuiuip_t)fcn; fn((intptr_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFppiiipii(uintptr_t fcn) { __CPU;  vFppiiipii_t fn = (vFppiiipii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFppppiipi(uintptr_t fcn) { __CPU;  vFppppiipi_t fn = (vFppppiipi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFiiiiiiip(uintptr_t fcn) { __CPU;  iFiiiiiiip_t fn = (iFiiiiiiip_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFuipuuluf(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  iFuipuuluf_t fn = (iFuipuuluf_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (intptr_t)R_R9, *(uint32_t*)(R_RSP + 8), f0); DEBUG_LOG; (void)cpu; }
void iFullfpppp(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  iFullfpppp_t fn = (iFullfpppp_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX, f0, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpLpipppp(uintptr_t fcn) { __CPU;  iFpLpipppp_t fn = (iFpLpipppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFppIIIppp(uintptr_t fcn) { __CPU;  iFppIIIppp_t fn = (iFppIIIppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int64_t)R_RDX, (int64_t)R_RCX, (int64_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFpppiippp(uintptr_t fcn) { __CPU;  iFpppiippp_t fn = (iFpppiippp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFppppiipi(uintptr_t fcn) { __CPU;  iFppppiipi_t fn = (iFppppiipi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFppppppip(uintptr_t fcn) { __CPU;  iFppppppip_t fn = (iFppppppip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void uFuipppppp(uintptr_t fcn) { __CPU;  uFuipppppp_t fn = (uFuipppppp_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void uFulpppppp(uintptr_t fcn) { __CPU;  uFulpppppp_t fn = (uFulpppppp_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFpppuuLLu(uintptr_t fcn) { __CPU;  pFpppuuLLu_t fn = (pFpppuuLLu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uintptr_t)R_R9, *(uintptr_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFuupupipp(uintptr_t fcn) { __CPU;  pFuupupipp_t fn = (pFuupupipp_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFiiiiiiiii(uintptr_t fcn) { __CPU;  vFiiiiiiiii_t fn = (vFiiiiiiiii_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFiiiiiiill(uintptr_t fcn) { __CPU;  vFiiiiiiill_t fn = (vFiiiiiiill_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(intptr_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFiiiiillli(uintptr_t fcn) { __CPU;  vFiiiiillli_t fn = (vFiiiiillli_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9, *(intptr_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFiiilllilp(uintptr_t fcn) { __CPU;  vFiiilllilp_t fn = (vFiiilllilp_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(int32_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFilillliuU(uintptr_t fcn) { __CPU;  vFilillliuU_t fn = (vFilillliuU_t)fcn; fn((int32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(int32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint64_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiiii(uintptr_t fcn) { __CPU;  vFuiiiiiiii_t fn = (vFuiiiiiiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiuip(uintptr_t fcn) { __CPU;  vFuiiiiiuip_t fn = (vFuiiiiiuip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiuup(uintptr_t fcn) { __CPU;  vFuiiiiiuup_t fn = (vFuiiiiiuup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuiiiillli(uintptr_t fcn) { __CPU;  vFuiiiillli_t fn = (vFuiiiillli_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9, *(intptr_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuiiilliip(uintptr_t fcn) { __CPU;  vFuiiilliip_t fn = (vFuiiilliip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuiiillilp(uintptr_t fcn) { __CPU;  vFuiiillilp_t fn = (vFuiiillilp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(int32_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuiuiiiiip(uintptr_t fcn) { __CPU;  vFuiuiiiiip_t fn = (vFuiuiiiiip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuuiiiiiii(uintptr_t fcn) { __CPU;  vFuuiiiiiii_t fn = (vFuuiiiiiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuuiuiiiii(uintptr_t fcn) { __CPU;  vFuuiuiiiii_t fn = (vFuuiuiiiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuuiuiiiip(uintptr_t fcn) { __CPU;  vFuuiuiiiip_t fn = (vFuuiuiiiip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuuiuiiuup(uintptr_t fcn) { __CPU;  vFuuiuiiuup_t fn = (vFuuiuiiuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuuuiiiiip(uintptr_t fcn) { __CPU;  vFuuuiiiiip_t fn = (vFuuuiiiiip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuuuuuuuuu(uintptr_t fcn) { __CPU;  vFuuuuuuuuu_t fn = (vFuuuuuuuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFuffffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21"); register float f6 __asm__("f22"); register float f7 __asm__("f23");  vFuffffffff_t fn = (vFuffffffff_t)fcn; fn((uint32_t)R_RDI, f0, f1, f2, f3, f4, f5, f6, f7); DEBUG_LOG; (void)cpu; }
void vFulillliuU(uintptr_t fcn) { __CPU;  vFulillliuU_t fn = (vFulillliuU_t)fcn; fn((uint32_t)R_RDI, (intptr_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(int32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint64_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFffuuuufff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20");  vFffuuuufff_t fn = (vFffuuuufff_t)fcn; fn(f0, f1, (uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, f2, f3, f4); DEBUG_LOG; (void)cpu; }
void vFddddddddd(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19"); register double d4 __asm__("f20"); register double d5 __asm__("f21"); register double d6 __asm__("f22"); register double d7 __asm__("f23");  vFddddddddd_t fn = (vFddddddddd_t)fcn; fn(d0, d1, d2, d3, d4, d5, d6, d7, *(double*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFlipuiuiip(uintptr_t fcn) { __CPU;  vFlipuiuiip_t fn = (vFlipuiuiip_t)fcn; fn((intptr_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFppiiipiii(uintptr_t fcn) { __CPU;  vFppiiipiii_t fn = (vFppiiipiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFpppppippp(uintptr_t fcn) { __CPU;  vFpppppippp_t fn = (vFpppppippp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void iFiiiiiiiip(uintptr_t fcn) { __CPU;  iFiiiiiiiip_t fn = (iFiiiiiiiip_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void iFiiiipiiip(uintptr_t fcn) { __CPU;  iFiiiipiiip_t fn = (iFiiiipiiip_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void iFuilpluluf(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  iFuilpluluf_t fn = (iFuilpluluf_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (int32_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX, (intptr_t)R_R8, (uint32_t)R_R9, *(intptr_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), f0); DEBUG_LOG; (void)cpu; }
void iFdddpppppp(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18");  iFdddpppppp_t fn = (iFdddpppppp_t)fcn; R_RAX=(int32_t)fn(d0, d1, d2, (void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppiuiippL(uintptr_t fcn) { __CPU;  iFppiuiippL_t fn = (iFppiuiippL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(uintptr_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void iFpppiiuuii(uintptr_t fcn) { __CPU;  iFpppiiuuii_t fn = (iFpppiiuuii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void iFppppppppp(uintptr_t fcn) { __CPU;  iFppppppppp_t fn = (iFppppppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void pFEppiiuuLi(uintptr_t fcn) { __CPU;  pFEppiiuuLi_t fn = (pFEppiiuuLi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uintptr_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFEppuippuu(uintptr_t fcn) { __CPU;  pFEppuippuu_t fn = (pFEppuippuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFppiiuuuLL(uintptr_t fcn) { __CPU;  pFppiiuuuLL_t fn = (pFppiiuuuLL_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uintptr_t*)(R_RSP + 16), *(uintptr_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFiiiiiiiiii(uintptr_t fcn) { __CPU;  vFiiiiiiiiii_t fn = (vFiiiiiiiiii_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFiiiiiiiiiu(uintptr_t fcn) { __CPU;  vFiiiiiiiiiu_t fn = (vFiiiiiiiiiu_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFiiiiiiiiui(uintptr_t fcn) { __CPU;  vFiiiiiiiiui_t fn = (vFiiiiiiiiui_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFiiillliiip(uintptr_t fcn) { __CPU;  vFiiillliiip_t fn = (vFiiillliiip_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (intptr_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiiiii(uintptr_t fcn) { __CPU;  vFuiiiiiiiii_t fn = (vFuiiiiiiiii_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiiill(uintptr_t fcn) { __CPU;  vFuiiiiiiill_t fn = (vFuiiiiiiill_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(intptr_t*)(R_RSP + 24), *(intptr_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiiuup(uintptr_t fcn) { __CPU;  vFuiiiiiiuup_t fn = (vFuiiiiiiuup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuiiiillllp(uintptr_t fcn) { __CPU;  vFuiiiillllp_t fn = (vFuiiiillllp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9, *(intptr_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(intptr_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuiuiiiiuup(uintptr_t fcn) { __CPU;  vFuiuiiiiuup_t fn = (vFuiuiiiiuup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuipulipiuf(uintptr_t fcn) { __CPU; register float f0 __asm__("f16");  vFuipulipiuf_t fn = (vFuipulipiuf_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (intptr_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), f0); DEBUG_LOG; (void)cpu; }
void vFuuiiiiiiii(uintptr_t fcn) { __CPU;  vFuuiiiiiiii_t fn = (vFuuiiiiiiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuuiiiiiuip(uintptr_t fcn) { __CPU;  vFuuiiiiiuip_t fn = (vFuuiiiiiuip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuuiiiiiuup(uintptr_t fcn) { __CPU;  vFuuiiiiiuup_t fn = (vFuuiiiiiuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuuiuiiiiip(uintptr_t fcn) { __CPU;  vFuuiuiiiiip_t fn = (vFuuiuiiiiip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuuiuiiiuup(uintptr_t fcn) { __CPU;  vFuuiuiiiuup_t fn = (vFuuiuiiiuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuuuuuuuiii(uintptr_t fcn) { __CPU;  vFuuuuuuuiii_t fn = (vFuuuuuuuiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuuuuuuuuuu(uintptr_t fcn) { __CPU;  vFuuuuuuuuuu_t fn = (vFuuuuuuuuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuffiiffiip(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFuffiiffiip_t fn = (vFuffiiffiip_t)fcn; fn((uint32_t)R_RDI, f0, f1, (int32_t)R_RSI, (int32_t)R_RDX, f2, f3, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFuddiiddiip(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFuddiiddiip_t fn = (vFuddiiddiip_t)fcn; fn((uint32_t)R_RDI, d0, d1, (int32_t)R_RSI, (int32_t)R_RDX, d2, d3, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFffffffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21"); register float f6 __asm__("f22"); register float f7 __asm__("f23");  vFffffffffff_t fn = (vFffffffffff_t)fcn; fn(f0, f1, f2, f3, f4, f5, f6, f7, *(float*)(R_RSP + 8), *(float*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFiiiiiiiiip(uintptr_t fcn) { __CPU;  iFiiiiiiiiip_t fn = (iFiiiiiiiiip_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void iFpuupiuiipp(uintptr_t fcn) { __CPU;  iFpuupiuiipp_t fn = (iFpuupiuiipp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void iFppppiiuuii(uintptr_t fcn) { __CPU;  iFppppiiuuii_t fn = (iFppppiiuuii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void pFppuiipuuii(uintptr_t fcn) { __CPU;  pFppuiipuuii_t fn = (pFppuiipuuii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void pFpppppppppp(uintptr_t fcn) { __CPU;  pFpppppppppp_t fn = (pFpppppppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFiiiiillliip(uintptr_t fcn) { __CPU;  vFiiiiillliip_t fn = (vFiiiiillliip_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9, *(intptr_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFiiiiilllilp(uintptr_t fcn) { __CPU;  vFiiiiilllilp_t fn = (vFiiiiilllilp_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9, *(intptr_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(intptr_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiiiiip(uintptr_t fcn) { __CPU;  vFuiiiiiiiiip_t fn = (vFuiiiiiiiiip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiiiuip(uintptr_t fcn) { __CPU;  vFuiiiiiiiuip_t fn = (vFuiiiiiiiuip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiiiuup(uintptr_t fcn) { __CPU;  vFuiiiiiiiuup_t fn = (vFuiiiiiiiuup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFuiiiillliip(uintptr_t fcn) { __CPU;  vFuiiiillliip_t fn = (vFuiiiillliip_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9, *(intptr_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFuiiiilllilp(uintptr_t fcn) { __CPU;  vFuiiiilllilp_t fn = (vFuiiiilllilp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9, *(intptr_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(intptr_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFuiuiiiiiuup(uintptr_t fcn) { __CPU;  vFuiuiiiiiuup_t fn = (vFuiuiiiiiuup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFuuiuiiiiuup(uintptr_t fcn) { __CPU;  vFuuiuiiiiuup_t fn = (vFuuiuiiiiuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFuuuuuuuuuuu(uintptr_t fcn) { __CPU;  vFuuuuuuuuuuu_t fn = (vFuuuuuuuuuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFuuupupppppp(uintptr_t fcn) { __CPU;  vFuuupupppppp_t fn = (vFuuupupppppp_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFuuffiiffiip(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19");  vFuuffiiffiip_t fn = (vFuuffiiffiip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, f0, f1, (int32_t)R_RDX, (int32_t)R_RCX, f2, f3, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuufffffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21"); register float f6 __asm__("f22"); register float f7 __asm__("f23");  vFuufffffffff_t fn = (vFuufffffffff_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, f0, f1, f2, f3, f4, f5, f6, f7, *(float*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuuddiiddiip(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19");  vFuuddiiddiip_t fn = (vFuuddiiddiip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, d0, d1, (int32_t)R_RDX, (int32_t)R_RCX, d2, d3, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFuffffffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21"); register float f6 __asm__("f22"); register float f7 __asm__("f23");  vFuffffffffff_t fn = (vFuffffffffff_t)fcn; fn((uint32_t)R_RDI, f0, f1, f2, f3, f4, f5, f6, f7, *(float*)(R_RSP + 8), *(float*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFUufffffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21"); register float f6 __asm__("f22"); register float f7 __asm__("f23");  vFUufffffffff_t fn = (vFUufffffffff_t)fcn; fn((uint64_t)R_RDI, (uint32_t)R_RSI, f0, f1, f2, f3, f4, f5, f6, f7, *(float*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpipipiipiii(uintptr_t fcn) { __CPU;  vFpipipiipiii_t fn = (vFpipipiipiii_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void iFEppppiiiiuu(uintptr_t fcn) { __CPU;  iFEppppiiiiuu_t fn = (iFEppppiiiiuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void iFiiiiiiiiiip(uintptr_t fcn) { __CPU;  iFiiiiiiiiiip_t fn = (iFiiiiiiiiiip_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void iFppppiiuuiiL(uintptr_t fcn) { __CPU;  iFppppiiuuiiL_t fn = (iFppppiiuuiiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(uintptr_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void pFEppuiipuuii(uintptr_t fcn) { __CPU;  pFEppuiipuuii_t fn = (pFEppuiipuuii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFuiiiillliilp(uintptr_t fcn) { __CPU;  vFuiiiillliilp_t fn = (vFuiiiillliilp_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (intptr_t)R_R9, *(intptr_t*)(R_RSP + 8), *(intptr_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(intptr_t*)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFuuiiiiiiiiui(uintptr_t fcn) { __CPU;  vFuuiiiiiiiiui_t fn = (vFuuiiiiiiiiui_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFuuiiiiiiiuip(uintptr_t fcn) { __CPU;  vFuuiiiiiiiuip_t fn = (vFuuiiiiiiiuip_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFuuiiiiiiiuup(uintptr_t fcn) { __CPU;  vFuuiiiiiiiuup_t fn = (vFuuiiiiiiiuup_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFuuuuuuuuuuuu(uintptr_t fcn) { __CPU;  vFuuuuuuuuuuuu_t fn = (vFuuuuuuuuuuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(uint32_t*)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFffffffffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21"); register float f6 __asm__("f22"); register float f7 __asm__("f23");  vFffffffffffff_t fn = (vFffffffffffff_t)fcn; fn(f0, f1, f2, f3, f4, f5, f6, f7, *(float*)(R_RSP + 8), *(float*)(R_RSP + 16), *(float*)(R_RSP + 24), *(float*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void iFEppppiiiiuui(uintptr_t fcn) { __CPU;  iFEppppiiiiuui_t fn = (iFEppppiiiiuui_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void iFpppllipppppp(uintptr_t fcn) { __CPU;  iFpppllipppppp_t fn = (iFpppllipppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (intptr_t)R_RCX, (intptr_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void iFpppppppppppp(uintptr_t fcn) { __CPU;  iFpppppppppppp_t fn = (iFpppppppppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void pFEppiiuuuipii(uintptr_t fcn) { __CPU;  pFEppiiuuuipii_t fn = (pFEppiiuuuipii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void pFppiiuuuiupLp(uintptr_t fcn) { __CPU;  pFppiiuuuiupLp_t fn = (pFppiiuuuiupLp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(uintptr_t*)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiiiiiuup(uintptr_t fcn) { __CPU;  vFuiiiiiiiiiuup_t fn = (vFuiiiiiiiiiuup_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(uint32_t*)(R_RSP + 48), *(void**)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void vFuuuuuuuuuuuuu(uintptr_t fcn) { __CPU;  vFuuuuuuuuuuuuu_t fn = (vFuuuuuuuuuuuuu_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(uint32_t*)(R_RSP + 48), *(uint32_t*)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void vFuffffffffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21"); register float f6 __asm__("f22"); register float f7 __asm__("f23");  vFuffffffffffff_t fn = (vFuffffffffffff_t)fcn; fn((uint32_t)R_RDI, f0, f1, f2, f3, f4, f5, f6, f7, *(float*)(R_RSP + 8), *(float*)(R_RSP + 16), *(float*)(R_RSP + 24), *(float*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void iFddddpppddpppp(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19"); register double d4 __asm__("f20"); register double d5 __asm__("f21");  iFddddpppddpppp_t fn = (iFddddpppddpppp_t)fcn; R_RAX=(int32_t)fn(d0, d1, d2, d3, (void*)R_RDI, (void*)R_RSI, (void*)R_RDX, d4, d5, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpippuuuiipppp(uintptr_t fcn) { __CPU;  iFpippuuuiipppp_t fn = (iFpippuuuiipppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void vFuffiiffiiffiip(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21");  vFuffiiffiiffiip_t fn = (vFuffiiffiiffiip_t)fcn; fn((uint32_t)R_RDI, f0, f1, (int32_t)R_RSI, (int32_t)R_RDX, f2, f3, (int32_t)R_RCX, (int32_t)R_R8, f4, f5, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuddiiddiiddiip(uintptr_t fcn) { __CPU; register double d0 __asm__("f16"); register double d1 __asm__("f17"); register double d2 __asm__("f18"); register double d3 __asm__("f19"); register double d4 __asm__("f20"); register double d5 __asm__("f21");  vFuddiiddiiddiip_t fn = (vFuddiiddiiddiip_t)fcn; fn((uint32_t)R_RDI, d0, d1, (int32_t)R_RSI, (int32_t)R_RDX, d2, d3, (int32_t)R_RCX, (int32_t)R_R8, d4, d5, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFuiiiiiuiiiiilll(uintptr_t fcn) { __CPU;  vFuiiiiiuiiiiilll_t fn = (vFuiiiiiuiiiiilll_t)fcn; fn((uint32_t)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48), *(intptr_t*)(R_RSP + 56), *(intptr_t*)(R_RSP + 64), *(intptr_t*)(R_RSP + 72)); DEBUG_LOG; (void)cpu; }
void vFuuiiiiuuiiiiiii(uintptr_t fcn) { __CPU;  vFuuiiiiuuiiiiiii_t fn = (vFuuiiiiuuiiiiiii_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48), *(int32_t*)(R_RSP + 56), *(int32_t*)(R_RSP + 64), *(int32_t*)(R_RSP + 72)); DEBUG_LOG; (void)cpu; }
void vFfffffffffffffff(uintptr_t fcn) { __CPU; register float f0 __asm__("f16"); register float f1 __asm__("f17"); register float f2 __asm__("f18"); register float f3 __asm__("f19"); register float f4 __asm__("f20"); register float f5 __asm__("f21"); register float f6 __asm__("f22"); register float f7 __asm__("f23");  vFfffffffffffffff_t fn = (vFfffffffffffffff_t)fcn; fn(f0, f1, f2, f3, f4, f5, f6, f7, *(float*)(R_RSP + 8), *(float*)(R_RSP + 16), *(float*)(R_RSP + 24), *(float*)(R_RSP + 32), *(float*)(R_RSP + 40), *(float*)(R_RSP + 48), *(float*)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void pFpppppppppppppppp(uintptr_t fcn) { __CPU;  pFpppppppppppppppp_t fn = (pFpppppppppppppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64), *(void**)(R_RSP + 72), *(void**)(R_RSP + 80)); DEBUG_LOG; (void)cpu; }
void vFuuuiiiiiuiiiiilll(uintptr_t fcn) { __CPU;  vFuuuiiiiiuiiiiilll_t fn = (vFuuuiiiiiuiiiiilll_t)fcn; fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48), *(int32_t*)(R_RSP + 56), *(int32_t*)(R_RSP + 64), *(intptr_t*)(R_RSP + 72), *(intptr_t*)(R_RSP + 80), *(intptr_t*)(R_RSP + 88)); DEBUG_LOG; (void)cpu; }
void vFppuiiiiipuiiiiiiii(uintptr_t fcn) { __CPU;  vFppuiiiiipuiiiiiiii_t fn = (vFppuiiiiipuiiiiiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48), *(int32_t*)(R_RSP + 56), *(int32_t*)(R_RSP + 64), *(int32_t*)(R_RSP + 72), *(int32_t*)(R_RSP + 80), *(int32_t*)(R_RSP + 88), *(int32_t*)(R_RSP + 96)); DEBUG_LOG; (void)cpu; }
void vFpup(uintptr_t fcn) { __CPU; vFpup_t fn = (vFpup_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void HFp(uintptr_t fcn) { __CPU; HFp_t fn = (HFp_t)fcn; unsigned __int128 u128 = fn((void*)R_RDI); R_RAX=(u128&0xFFFFFFFFFFFFFFFFL); R_RDX=(u128>>64)&0xFFFFFFFFFFFFFFFFL; DEBUG_LOG; (void)cpu; }
void uWp(uintptr_t fcn) { __CPU; uWp_t fn = (uWp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpU(uintptr_t fcn) { __CPU; vFpU_t fn = (vFpU_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI); DEBUG_LOG; (void)cpu; }
void uFpi(uintptr_t fcn) { __CPU; uFpi_t fn = (uFpi_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void uFpu(uintptr_t fcn) { __CPU; uFpu_t fn = (uFpu_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void iFpupuu(uintptr_t fcn) { __CPU; iFpupuu_t fn = (iFpupuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpuiip(uintptr_t fcn) { __CPU; vFpuiip_t fn = (vFpuiip_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpuu(uintptr_t fcn) { __CPU; vFpuu_t fn = (vFpuu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpdd(uintptr_t fcn) { __CPU; vFpdd_t fn = (vFpdd_t)fcn; fn((void*)R_RDI, R_XMMD(0), R_XMMD(1)); DEBUG_LOG; (void)cpu; }
void vFpdddddd(uintptr_t fcn) { __CPU; vFpdddddd_t fn = (vFpdddddd_t)fcn; fn((void*)R_RDI, R_XMMD(0), R_XMMD(1), R_XMMD(2), R_XMMD(3), R_XMMD(4), R_XMMD(5)); DEBUG_LOG; (void)cpu; }
void dFp(uintptr_t fcn) { __CPU; dFp_t fn = (dFp_t)fcn; R_XMMD(0)=fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFdddddd(uintptr_t fcn) { __CPU; pFdddddd_t fn = (pFdddddd_t)fcn; R_RAX=(uintptr_t)fn(R_XMMD(0), R_XMMD(1), R_XMMD(2), R_XMMD(3), R_XMMD(4), R_XMMD(5)); DEBUG_LOG; (void)cpu; }
void vFpd(uintptr_t fcn) { __CPU; vFpd_t fn = (vFpd_t)fcn; fn((void*)R_RDI, R_XMMD(0)); DEBUG_LOG; (void)cpu; }
void pFffff(uintptr_t fcn) { __CPU; pFffff_t fn = (pFffff_t)fcn; R_RAX=(uintptr_t)fn(R_XMMS(0), R_XMMS(1), R_XMMS(2), R_XMMS(3)); DEBUG_LOG; (void)cpu; }
void vFpffff(uintptr_t fcn) { __CPU; vFpffff_t fn = (vFpffff_t)fcn; fn((void*)R_RDI, R_XMMS(0), R_XMMS(1), R_XMMS(2), R_XMMS(3)); DEBUG_LOG; (void)cpu; }
void vFpuuuu(uintptr_t fcn) { __CPU; vFpuuuu_t fn = (vFpuuuu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void pFpfffi(uintptr_t fcn) { __CPU; pFpfffi_t fn = (pFpfffi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, R_XMMS(0), R_XMMS(1), R_XMMS(2), (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpfffi(uintptr_t fcn) { __CPU; vFpfffi_t fn = (vFpfffi_t)fcn; fn((void*)R_RDI, R_XMMS(0), R_XMMS(1), R_XMMS(2), (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpuupp(uintptr_t fcn) { __CPU; vFpuupp_t fn = (vFpuupp_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpupu(uintptr_t fcn) { __CPU; vFpupu_t fn = (vFpupu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppiiu(uintptr_t fcn) { __CPU; vFppiiu_t fn = (vFppiiu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpppppp(uintptr_t fcn) { __CPU; vFpppppp_t fn = (vFpppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void uFppp(uintptr_t fcn) { __CPU; uFppp_t fn = (uFppp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void uFppLp(uintptr_t fcn) { __CPU; uFppLp_t fn = (uFppLp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void uFpppp(uintptr_t fcn) { __CPU; uFpppp_t fn = (uFpppp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void uFppLpp(uintptr_t fcn) { __CPU; uFppLpp_t fn = (uFppLpp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void LFpp(uintptr_t fcn) { __CPU; LFpp_t fn = (LFpp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void iFpLppp(uintptr_t fcn) { __CPU; iFpLppp_t fn = (iFpLppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpff(uintptr_t fcn) { __CPU; vFpff_t fn = (vFpff_t)fcn; fn((void*)R_RDI, R_XMMS(0), R_XMMS(1)); DEBUG_LOG; (void)cpu; }
void vFpppppppppp(uintptr_t fcn) { __CPU; vFpppppppppp_t fn = (vFpppppppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void iFppppppu(uintptr_t fcn) { __CPU; iFppppppu_t fn = (iFppppppu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFppppppp(uintptr_t fcn) { __CPU; vFppppppp_t fn = (vFppppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpppppu(uintptr_t fcn) { __CPU; vFpppppu_t fn = (vFpppppu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void pFppppppu(uintptr_t fcn) { __CPU; pFppppppu_t fn = (pFppppppu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFuuu(uintptr_t fcn) { __CPU; pFuuu_t fn = (pFuuu_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpiipppp(uintptr_t fcn) { __CPU; iFpiipppp_t fn = (iFpiipppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpiiff(uintptr_t fcn) { __CPU; vFpiiff_t fn = (vFpiiff_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, R_XMMS(0), R_XMMS(1)); DEBUG_LOG; (void)cpu; }
void pFip(uintptr_t fcn) { __CPU; pFip_t fn = (pFip_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpiC(uintptr_t fcn) { __CPU; vFpiC_t fn = (vFpiC_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (uint8_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpiip(uintptr_t fcn) { __CPU; vFpiip_t fn = (vFpiip_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpiipp(uintptr_t fcn) { __CPU; vFpiipp_t fn = (vFpiipp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpiipCpp(uintptr_t fcn) { __CPU; vFpiipCpp_t fn = (vFpiipCpp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (uint8_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpiiii(uintptr_t fcn) { __CPU; vFpiiii_t fn = (vFpiiii_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void WFp(uintptr_t fcn) { __CPU; WFp_t fn = (WFp_t)fcn; R_RAX=(unsigned short)fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFpW(uintptr_t fcn) { __CPU; vFpW_t fn = (vFpW_t)fcn; fn((void*)R_RDI, (uint16_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpppppppppppp(uintptr_t fcn) { __CPU; vFpppppppppppp_t fn = (vFpppppppppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFpf(uintptr_t fcn) { __CPU; vFpf_t fn = (vFpf_t)fcn; fn((void*)R_RDI, R_XMMS(0)); DEBUG_LOG; (void)cpu; }
void pFppipppppppppppp(uintptr_t fcn) { __CPU; pFppipppppppppppp_t fn = (pFppipppppppppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64), *(void**)(R_RSP + 72)); DEBUG_LOG; (void)cpu; }
void iFiup(uintptr_t fcn) { __CPU; iFiup_t fn = (iFiup_t)fcn; R_RAX=(int32_t)fn((int32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void pFppuip(uintptr_t fcn) { __CPU; pFppuip_t fn = (pFppuip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpupiu(uintptr_t fcn) { __CPU; vFpupiu_t fn = (vFpupiu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFppui(uintptr_t fcn) { __CPU; vFppui_t fn = (vFppui_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpiiu(uintptr_t fcn) { __CPU; vFpiiu_t fn = (vFpiiu_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppppii(uintptr_t fcn) { __CPU; vFppppii_t fn = (vFppppii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFppiiiiiiii(uintptr_t fcn) { __CPU; vFppiiiiiiii_t fn = (vFppiiiiiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFppiiiiii(uintptr_t fcn) { __CPU; vFppiiiiii_t fn = (vFppiiiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFppiiiiiiiii(uintptr_t fcn) { __CPU; vFppiiiiiiiii_t fn = (vFppiiiiiiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFppiiiiiii(uintptr_t fcn) { __CPU; vFppiiiiiii_t fn = (vFppiiiiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFpppiui(uintptr_t fcn) { __CPU; vFpppiui_t fn = (vFpppiui_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFppiiiip(uintptr_t fcn) { __CPU; vFppiiiip_t fn = (vFppiiiip_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFppiipii(uintptr_t fcn) { __CPU; vFppiipii_t fn = (vFppiipii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void uFpupi(uintptr_t fcn) { __CPU; uFpupi_t fn = (uFpupi_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void fFp(uintptr_t fcn) { __CPU; fFp_t fn = (fFp_t)fcn; R_XMMS(0)=fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFppippipipipipipip(uintptr_t fcn) { __CPU; pFppippipipipipipip_t fn = (pFppippipipipipipip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(void**)(R_RSP + 40), *(int32_t*)(R_RSP + 48), *(void**)(R_RSP + 56), *(int32_t*)(R_RSP + 64), *(void**)(R_RSP + 72), *(int32_t*)(R_RSP + 80), *(void**)(R_RSP + 88)); DEBUG_LOG; (void)cpu; }
void pFippu(uintptr_t fcn) { __CPU; pFippu_t fn = (pFippu_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void lFv(uintptr_t fcn) { __CPU; lFv_t fn = (lFv_t)fcn; R_RAX=(intptr_t)fn(); DEBUG_LOG; (void)cpu; }
void pFddd(uintptr_t fcn) { __CPU; pFddd_t fn = (pFddd_t)fcn; R_RAX=(uintptr_t)fn(R_XMMD(0), R_XMMD(1), R_XMMD(2)); DEBUG_LOG; (void)cpu; }
void pFppuuupp(uintptr_t fcn) { __CPU; pFppuuupp_t fn = (pFppuuupp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFupp(uintptr_t fcn) { __CPU; iFupp_t fn = (iFupp_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void uFpii(uintptr_t fcn) { __CPU; uFpii_t fn = (uFpii_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFppiu(uintptr_t fcn) { __CPU; pFppiu_t fn = (pFppiu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFppiup(uintptr_t fcn) { __CPU; pFppiup_t fn = (pFppiup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFppiiu(uintptr_t fcn) { __CPU; pFppiiu_t fn = (pFppiiu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFppiu(uintptr_t fcn) { __CPU; vFppiu_t fn = (vFppiu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpppippp(uintptr_t fcn) { __CPU; iFpppippp_t fn = (iFpppippp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFppiff(uintptr_t fcn) { __CPU; vFppiff_t fn = (vFppiff_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, R_XMMS(0), R_XMMS(1)); DEBUG_LOG; (void)cpu; }
void iFpippuuii(uintptr_t fcn) { __CPU; iFpippuuii_t fn = (iFpippuuii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFpifi(uintptr_t fcn) { __CPU; vFpifi_t fn = (vFpifi_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, R_XMMS(0), (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFppippi(uintptr_t fcn) { __CPU; vFppippi_t fn = (vFppippi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFppuuuu(uintptr_t fcn) { __CPU; vFppuuuu_t fn = (vFppuuuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void pFpuiippppppppppp(uintptr_t fcn) { __CPU; pFpuiippppppppppp_t fn = (pFpuiippppppppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64), *(void**)(R_RSP + 72)); DEBUG_LOG; (void)cpu; }
void vFpiuu(uintptr_t fcn) { __CPU; vFpiuu_t fn = (vFpiuu_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppppppppppp(uintptr_t fcn) { __CPU; vFppppppppppp_t fn = (vFppppppppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void pFppppppppppp(uintptr_t fcn) { __CPU; pFppppppppppp_t fn = (pFppppppppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void dFpi(uintptr_t fcn) { __CPU; dFpi_t fn = (dFpi_t)fcn; R_XMMD(0)=fn((void*)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void dFpu(uintptr_t fcn) { __CPU; dFpu_t fn = (dFpu_t)fcn; R_XMMD(0)=fn((void*)R_RDI, (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpdu(uintptr_t fcn) { __CPU; vFpdu_t fn = (vFpdu_t)fcn; fn((void*)R_RDI, R_XMMD(0), (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFppuuppuiiiii(uintptr_t fcn) { __CPU; vFppuuppuiiiii_t fn = (vFppuuppuiiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFppiipppiiii(uintptr_t fcn) { __CPU; vFppiipppiiii_t fn = (vFppiipppiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFppuuppiiiiuii(uintptr_t fcn) { __CPU; vFppuuppiiiiuii_t fn = (vFppuuppiiiiuii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48), *(int32_t*)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void vFppuppiiu(uintptr_t fcn) { __CPU; vFppuppiiu_t fn = (vFppuppiiu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFppuuppiiiiu(uintptr_t fcn) { __CPU; vFppuuppiiiiu_t fn = (vFppuuppiiiiu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFppuppiiii(uintptr_t fcn) { __CPU; vFppuppiiii_t fn = (vFppuppiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFppipppiii(uintptr_t fcn) { __CPU; vFppipppiii_t fn = (vFppipppiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFppuippiip(uintptr_t fcn) { __CPU; vFppuippiip_t fn = (vFppuippiip_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFppiippppii(uintptr_t fcn) { __CPU; vFppiippppii_t fn = (vFppiippppii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFppuppuiiii(uintptr_t fcn) { __CPU; vFppuppuiiii_t fn = (vFppuppuiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFppiipppiiiiiii(uintptr_t fcn) { __CPU; vFppiipppiiiiiii_t fn = (vFppiipppiiiiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48), *(int32_t*)(R_RSP + 56), *(int32_t*)(R_RSP + 64)); DEBUG_LOG; (void)cpu; }
void vFppiipppiiiii(uintptr_t fcn) { __CPU; vFppiipppiiiii_t fn = (vFppiipppiiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFppipppiip(uintptr_t fcn) { __CPU; vFppipppiip_t fn = (vFppipppiip_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFppuuppiiii(uintptr_t fcn) { __CPU; vFppuuppiiii_t fn = (vFppuuppiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFppuppiii(uintptr_t fcn) { __CPU; vFppuppiii_t fn = (vFppuppiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFppddu(uintptr_t fcn) { __CPU; pFppddu_t fn = (pFppddu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), R_XMMD(1), (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFppdd(uintptr_t fcn) { __CPU; pFppdd_t fn = (pFppdd_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), R_XMMD(1)); DEBUG_LOG; (void)cpu; }
void vFpddu(uintptr_t fcn) { __CPU; vFpddu_t fn = (vFpddu_t)fcn; fn((void*)R_RDI, R_XMMD(0), R_XMMD(1), (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFppdd(uintptr_t fcn) { __CPU; vFppdd_t fn = (vFppdd_t)fcn; fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), R_XMMD(1)); DEBUG_LOG; (void)cpu; }
void uFpupp(uintptr_t fcn) { __CPU; uFpupp_t fn = (uFpupp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void dFpp(uintptr_t fcn) { __CPU; dFpp_t fn = (dFpp_t)fcn; R_XMMD(0)=fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void dFppd(uintptr_t fcn) { __CPU; dFppd_t fn = (dFppd_t)fcn; R_XMMD(0)=fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0)); DEBUG_LOG; (void)cpu; }
void dFppu(uintptr_t fcn) { __CPU; dFppu_t fn = (dFppu_t)fcn; R_XMMD(0)=fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFppd(uintptr_t fcn) { __CPU; vFppd_t fn = (vFppd_t)fcn; fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0)); DEBUG_LOG; (void)cpu; }
void vFppdu(uintptr_t fcn) { __CPU; vFppdu_t fn = (vFppdu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpppL(uintptr_t fcn) { __CPU; pFpppL_t fn = (pFpppL_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppdddd(uintptr_t fcn) { __CPU; vFppdddd_t fn = (vFppdddd_t)fcn; fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), R_XMMD(1), R_XMMD(2), R_XMMD(3)); DEBUG_LOG; (void)cpu; }
void vFpddddp(uintptr_t fcn) { __CPU; vFpddddp_t fn = (vFpddddp_t)fcn; fn((void*)R_RDI, R_XMMD(0), R_XMMD(1), R_XMMD(2), R_XMMD(3), (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void vFppddddu(uintptr_t fcn) { __CPU; vFppddddu_t fn = (vFppddddu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), R_XMMD(1), R_XMMD(2), R_XMMD(3), (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFppddddudd(uintptr_t fcn) { __CPU; vFppddddudd_t fn = (vFppddddudd_t)fcn; fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), R_XMMD(1), R_XMMD(2), R_XMMD(3), (uint32_t)R_RDX, R_XMMD(4), R_XMMD(5)); DEBUG_LOG; (void)cpu; }
void vFpppdd(uintptr_t fcn) { __CPU; vFpppdd_t fn = (vFpppdd_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, R_XMMD(0), R_XMMD(1)); DEBUG_LOG; (void)cpu; }
void vFppddpiu(uintptr_t fcn) { __CPU; vFppddpiu_t fn = (vFppddpiu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), R_XMMD(1), (void*)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFppddp(uintptr_t fcn) { __CPU; vFppddp_t fn = (vFppddp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), R_XMMD(1), (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFdddppp(uintptr_t fcn) { __CPU; vFdddppp_t fn = (vFdddppp_t)fcn; fn(R_XMMD(0), R_XMMD(1), R_XMMD(2), (void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpdup(uintptr_t fcn) { __CPU; vFpdup_t fn = (vFpdup_t)fcn; fn((void*)R_RDI, R_XMMD(0), (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void pFudddp(uintptr_t fcn) { __CPU; pFudddp_t fn = (pFudddp_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, R_XMMD(0), R_XMMD(1), R_XMMD(2), (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void iFpppu(uintptr_t fcn) { __CPU; iFpppu_t fn = (iFpppu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppipi(uintptr_t fcn) { __CPU; vFppipi_t fn = (vFppipi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFppdp(uintptr_t fcn) { __CPU; vFppdp_t fn = (vFppdp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpplp(uintptr_t fcn) { __CPU; vFpplp_t fn = (vFpplp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (intptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpppppppppppppppppppppppp(uintptr_t fcn) { __CPU; vFpppppppppppppppppppppppp_t fn = (vFpppppppppppppppppppppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64), *(void**)(R_RSP + 72), *(void**)(R_RSP + 80), *(void**)(R_RSP + 88), *(void**)(R_RSP + 96), *(void**)(R_RSP + 104), *(void**)(R_RSP + 112), *(void**)(R_RSP + 120), *(void**)(R_RSP + 128), *(void**)(R_RSP + 136), *(void**)(R_RSP + 144)); DEBUG_LOG; (void)cpu; }
void pFpdu(uintptr_t fcn) { __CPU; pFpdu_t fn = (pFpdu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, R_XMMD(0), (uint32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpud(uintptr_t fcn) { __CPU; vFpud_t fn = (vFpud_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, R_XMMD(0)); DEBUG_LOG; (void)cpu; }
void uFpup(uintptr_t fcn) { __CPU; uFpup_t fn = (uFpup_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpppuiiii(uintptr_t fcn) { __CPU; vFpppuiiii_t fn = (vFpppuiiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFpppui(uintptr_t fcn) { __CPU; vFpppui_t fn = (vFpppui_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpLpp(uintptr_t fcn) { __CPU; vFpLpp_t fn = (vFpLpp_t)fcn; fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppuuuuuuuu(uintptr_t fcn) { __CPU; vFppuuuuuuuu_t fn = (vFppuuuuuuuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void pFuui(uintptr_t fcn) { __CPU; pFuui_t fn = (pFuui_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpuip(uintptr_t fcn) { __CPU; vFpuip_t fn = (vFpuip_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpppppLp(uintptr_t fcn) { __CPU; iFpppppLp_t fn = (iFpppppLp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uintptr_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpppipppppppppppppp(uintptr_t fcn) { __CPU; vFpppipppppppppppppp_t fn = (vFpppipppppppppppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64), *(void**)(R_RSP + 72), *(void**)(R_RSP + 80), *(void**)(R_RSP + 88), *(void**)(R_RSP + 96)); DEBUG_LOG; (void)cpu; }
void vFpppippppppppppp(uintptr_t fcn) { __CPU; vFpppippppppppppp_t fn = (vFpppippppppppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64), *(void**)(R_RSP + 72)); DEBUG_LOG; (void)cpu; }
void vFpppppi(uintptr_t fcn) { __CPU; vFpppppi_t fn = (vFpppppi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFppuppp(uintptr_t fcn) { __CPU; iFppuppp_t fn = (iFppuppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFppuii(uintptr_t fcn) { __CPU; vFppuii_t fn = (vFppuii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpiiipp(uintptr_t fcn) { __CPU; vFpiiipp_t fn = (vFpiiipp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppdidd(uintptr_t fcn) { __CPU; iFppdidd_t fn = (iFppdidd_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), (int32_t)R_RDX, R_XMMD(1), R_XMMD(2)); DEBUG_LOG; (void)cpu; }
void vFppdidd(uintptr_t fcn) { __CPU; vFppdidd_t fn = (vFppdidd_t)fcn; fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), (int32_t)R_RDX, R_XMMD(1), R_XMMD(2)); DEBUG_LOG; (void)cpu; }
void vFpuiipp(uintptr_t fcn) { __CPU; vFpuiipp_t fn = (vFpuiipp_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFppuuu(uintptr_t fcn) { __CPU; vFppuuu_t fn = (vFppuuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void pFippppppppppppppppp(uintptr_t fcn) { __CPU; pFippppppppppppppppp_t fn = (pFippppppppppppppppp_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64), *(void**)(R_RSP + 72), *(void**)(R_RSP + 80), *(void**)(R_RSP + 88), *(void**)(R_RSP + 96)); DEBUG_LOG; (void)cpu; }
void vFpppippi(uintptr_t fcn) { __CPU; vFpppippi_t fn = (vFpppippi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFppppppppppppp(uintptr_t fcn) { __CPU; pFppppppppppppp_t fn = (pFppppppppppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void iFpipppppppppppp(uintptr_t fcn) { __CPU; iFpipppppppppppp_t fn = (iFpipppppppppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64)); DEBUG_LOG; (void)cpu; }
void vFpppiff(uintptr_t fcn) { __CPU; vFpppiff_t fn = (vFpppiff_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, R_XMMS(0), R_XMMS(1)); DEBUG_LOG; (void)cpu; }
void vFpupppui(uintptr_t fcn) { __CPU; vFpupppui_t fn = (vFpupppui_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void uFpplp(uintptr_t fcn) { __CPU; uFpplp_t fn = (uFpplp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (intptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpppuuu(uintptr_t fcn) { __CPU; vFpppuuu_t fn = (vFpppuuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFppil(uintptr_t fcn) { __CPU; vFppil_t fn = (vFppil_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpipppp(uintptr_t fcn) { __CPU; vFpipppp_t fn = (vFpipppp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void pFpLp(uintptr_t fcn) { __CPU; pFpLp_t fn = (pFpLp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpiipppp(uintptr_t fcn) { __CPU; vFpiipppp_t fn = (vFpiipppp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpipu(uintptr_t fcn) { __CPU; vFpipu_t fn = (vFpipu_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpiu(uintptr_t fcn) { __CPU; iFpiu_t fn = (iFpiu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpiL(uintptr_t fcn) { __CPU; vFpiL_t fn = (vFpiL_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFppppppppppppppp(uintptr_t fcn) { __CPU; vFppppppppppppppp_t fn = (vFppppppppppppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64), *(void**)(R_RSP + 72)); DEBUG_LOG; (void)cpu; }
void vFpppuii(uintptr_t fcn) { __CPU; vFpppuii_t fn = (vFpppuii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFppiipp(uintptr_t fcn) { __CPU; iFppiipp_t fn = (iFppiipp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFpiiiu(uintptr_t fcn) { __CPU; vFpiiiu_t fn = (vFpiiiu_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpuiiiu(uintptr_t fcn) { __CPU; vFpuiiiu_t fn = (vFpuiiiu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFppdd(uintptr_t fcn) { __CPU; iFppdd_t fn = (iFppdd_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0), R_XMMD(1)); DEBUG_LOG; (void)cpu; }
void iFppUup(uintptr_t fcn) { __CPU; iFppUup_t fn = (iFppUup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint64_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFpLi(uintptr_t fcn) { __CPU; pFpLi_t fn = (pFpLi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFELp(uintptr_t fcn) { __CPU; iFELp_t fn = (iFELp_t)fcn; R_RAX=(int32_t)fn((uintptr_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void iFEpp(uintptr_t fcn) { __CPU; iFEpp_t fn = (iFEpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void iFEvpp(uintptr_t fcn) { __CPU; iFEpp_t fn = (iFEpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFEppuppp(uintptr_t fcn) { __CPU; iFEppuppp_t fn = (iFEppuppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void LFEppppppii(uintptr_t fcn) { __CPU; LFEppppppii_t fn = (LFEppppppii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFEi(uintptr_t fcn) { __CPU; pFEi_t fn = (pFEi_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI); DEBUG_LOG; (void)cpu; }
void pFEpippppppp(uintptr_t fcn) { __CPU; pFEpippppppp_t fn = (pFEpippppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void pFEpipppppppi(uintptr_t fcn) { __CPU; pFEpipppppppi_t fn = (pFEpipppppppi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void pFEppppppi(uintptr_t fcn) { __CPU; pFEppppppi_t fn = (pFEppppppi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFEppppppp(uintptr_t fcn) { __CPU; pFEppppppp_t fn = (pFEppppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFEpppppppi(uintptr_t fcn) { __CPU; pFEpppppppi_t fn = (pFEpppppppi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFEuV(uintptr_t fcn) { __CPU; pFEuV_t fn = (pFEuV_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFppipipipipipipip(uintptr_t fcn) { __CPU; pFppipipipipipipip_t fn = (pFppipipipipipipip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(void**)(R_RSP + 48), *(int32_t*)(R_RSP + 56), *(void**)(R_RSP + 64), *(int32_t*)(R_RSP + 72), *(void**)(R_RSP + 80)); DEBUG_LOG; (void)cpu; }
void uFEupp(uintptr_t fcn) { __CPU; uFEupp_t fn = (uFEupp_t)fcn; R_RAX=(uint32_t)fn((uint32_t)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFEpA(uintptr_t fcn) { __CPU; vFEpA_t fn = (vFEpA_t)fcn; fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void vFEpiA(uintptr_t fcn) { __CPU; vFEpiA_t fn = (vFEpiA_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFEpippp(uintptr_t fcn) { __CPU; vFEpippp_t fn = (vFEpippp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFEpppi(uintptr_t fcn) { __CPU; vFEpppi_t fn = (vFEpppi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFEpppp(uintptr_t fcn) { __CPU; vFEpppp_t fn = (vFEpppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFEppppp(uintptr_t fcn) { __CPU; vFEppppp_t fn = (vFEppppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFEpppppuu(uintptr_t fcn) { __CPU; vFEpppppuu_t fn = (vFEpppppuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFEppV(uintptr_t fcn) { __CPU; vFEppV_t fn = (vFEppV_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpiiiiiiiiiiiiiiiiii(uintptr_t fcn) { __CPU; vFpiiiiiiiiiiiiiiiiii_t fn = (vFpiiiiiiiiiiiiiiiiii_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48), *(int32_t*)(R_RSP + 56), *(int32_t*)(R_RSP + 64), *(int32_t*)(R_RSP + 72), *(int32_t*)(R_RSP + 80), *(int32_t*)(R_RSP + 88), *(int32_t*)(R_RSP + 96), *(int32_t*)(R_RSP + 104)); DEBUG_LOG; (void)cpu; }
void vFpippppppppppp(uintptr_t fcn) { __CPU; vFpippppppppppp_t fn = (vFpippppppppppp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void pFiV(uintptr_t fcn) { __CPU; pFiV_t fn = (pFiV_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpC(uintptr_t fcn) { __CPU; pFpC_t fn = (pFpC_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFLup(uintptr_t fcn) { __CPU; pFLup_t fn = (pFLup_t)fcn; R_RAX=(uintptr_t)fn((uintptr_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFLp(uintptr_t fcn) { __CPU; vFLp_t fn = (vFLp_t)fcn; fn((uintptr_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void vFLup(uintptr_t fcn) { __CPU; vFLup_t fn = (vFLup_t)fcn; fn((uintptr_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFL(uintptr_t fcn) { __CPU; iFL_t fn = (iFL_t)fcn; R_RAX=(int32_t)fn((uintptr_t)R_RDI); DEBUG_LOG; (void)cpu; }
void iFpipLpp(uintptr_t fcn) { __CPU; iFpipLpp_t fn = (iFpipLpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (uintptr_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppiiu(uintptr_t fcn) { __CPU; iFppiiu_t fn = (iFppiiu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpUup(uintptr_t fcn) { __CPU; iFpUup_t fn = (iFpUup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpUU(uintptr_t fcn) { __CPU; iFpUU_t fn = (iFpUU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpll(uintptr_t fcn) { __CPU; pFpll_t fn = (pFpll_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (intptr_t)R_RSI, (intptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void UFp(uintptr_t fcn) { __CPU; UFp_t fn = (UFp_t)fcn; R_RAX=fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFppiipuu(uintptr_t fcn) { __CPU; vFppiipuu_t fn = (vFppiipuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpppuu(uintptr_t fcn) { __CPU; vFpppuu_t fn = (vFpppuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFppupp(uintptr_t fcn) { __CPU; vFppupp_t fn = (vFppupp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void LFpL(uintptr_t fcn) { __CPU; LFpL_t fn = (LFpL_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpii(uintptr_t fcn) { __CPU; vFpii_t fn = (vFpii_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void CFppp(uintptr_t fcn) { __CPU; CFppp_t fn = (CFppp_t)fcn; R_RAX=(unsigned char)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpCpp(uintptr_t fcn) { __CPU; iFpCpp_t fn = (iFpCpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint8_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFplpp(uintptr_t fcn) { __CPU; iFplpp_t fn = (iFplpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpLpp(uintptr_t fcn) { __CPU; iFpLpp_t fn = (iFpLpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFplupp(uintptr_t fcn) { __CPU; iFplupp_t fn = (iFplupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (intptr_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFppd(uintptr_t fcn) { __CPU; iFppd_t fn = (iFppd_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, R_XMMD(0)); DEBUG_LOG; (void)cpu; }
void iFppiupp(uintptr_t fcn) { __CPU; iFppiupp_t fn = (iFppiupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppLippp(uintptr_t fcn) { __CPU; iFppLippp_t fn = (iFppLippp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFppLpiuppp(uintptr_t fcn) { __CPU; iFppLpiuppp_t fn = (iFppLpiuppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void iFppLppp(uintptr_t fcn) { __CPU; iFppLppp_t fn = (iFppLppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFpplupp(uintptr_t fcn) { __CPU; iFpplupp_t fn = (iFpplupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (intptr_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppLupp(uintptr_t fcn) { __CPU; iFppLupp_t fn = (iFppLupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppppppp(uintptr_t fcn) { __CPU; iFppppppp_t fn = (iFppppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpppupp(uintptr_t fcn) { __CPU; iFpppupp_t fn = (iFpppupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFppuippp(uintptr_t fcn) { __CPU; iFppuippp_t fn = (iFppuippp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFppupupp(uintptr_t fcn) { __CPU; iFppupupp_t fn = (iFppupupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpulpp(uintptr_t fcn) { __CPU; iFpulpp_t fn = (iFpulpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (intptr_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFpuppp(uintptr_t fcn) { __CPU; iFpuppp_t fn = (iFpuppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFpwpp(uintptr_t fcn) { __CPU; iFpwpp_t fn = (iFpwpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int16_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpWpp(uintptr_t fcn) { __CPU; iFpWpp_t fn = (iFpWpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint16_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void LFEpppp(uintptr_t fcn) { __CPU; LFEpppp_t fn = (LFEpppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void lFpLp(uintptr_t fcn) { __CPU; lFpLp_t fn = (lFpLp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void lFplpp(uintptr_t fcn) { __CPU; lFplpp_t fn = (lFplpp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (intptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void lFpLpp(uintptr_t fcn) { __CPU; lFpLpp_t fn = (lFpLpp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void lFpp(uintptr_t fcn) { __CPU; lFpp_t fn = (lFpp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void lFppLipp(uintptr_t fcn) { __CPU; lFppLipp_t fn = (lFppLipp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void LFppLL(uintptr_t fcn) { __CPU; LFppLL_t fn = (LFppLL_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void lFppLpp(uintptr_t fcn) { __CPU; lFppLpp_t fn = (lFppLpp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void lFppp(uintptr_t fcn) { __CPU; lFppp_t fn = (lFppp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void LFppp(uintptr_t fcn) { __CPU; LFppp_t fn = (LFppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void lFpppipiipp(uintptr_t fcn) { __CPU; lFpppipiipp_t fn = (lFpppipiipp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void lFpppippppp(uintptr_t fcn) { __CPU; lFpppippppp_t fn = (lFpppippppp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void lFpppLpp(uintptr_t fcn) { __CPU; lFpppLpp_t fn = (lFpppLpp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uintptr_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void lFpppp(uintptr_t fcn) { __CPU; lFpppp_t fn = (lFpppp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void lFppupp(uintptr_t fcn) { __CPU; lFppupp_t fn = (lFppupp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void LFuui(uintptr_t fcn) { __CPU; LFuui_t fn = (LFuui_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFEiippppppp(uintptr_t fcn) { __CPU; pFEiippppppp_t fn = (pFEiippppppp_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void pFEppApp(uintptr_t fcn) { __CPU; pFEppApp_t fn = (pFEppApp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFEpppp(uintptr_t fcn) { __CPU; pFEpppp_t fn = (pFEpppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFEppppV(uintptr_t fcn) { __CPU; pFEppppV_t fn = (pFEppppV_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFEpppuipV(uintptr_t fcn) { __CPU; pFEpppuipV_t fn = (pFEpppuipV_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFiiLp(uintptr_t fcn) { __CPU; pFiiLp_t fn = (pFiiLp_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (int32_t)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFipp(uintptr_t fcn) { __CPU; pFipp_t fn = (pFipp_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void pFiupppppp(uintptr_t fcn) { __CPU; pFiupppppp_t fn = (pFiupppppp_t)fcn; R_RAX=(uintptr_t)fn((int32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFLuppp(uintptr_t fcn) { __CPU; pFLuppp_t fn = (pFLuppp_t)fcn; R_RAX=(uintptr_t)fn((uintptr_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFpiu(uintptr_t fcn) { __CPU; pFpiu_t fn = (pFpiu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFplp(uintptr_t fcn) { __CPU; pFplp_t fn = (pFplp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (intptr_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpLpi(uintptr_t fcn) { __CPU; pFpLpi_t fn = (pFpLpi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpLpp(uintptr_t fcn) { __CPU; pFpLpp_t fn = (pFpLpp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpLup(uintptr_t fcn) { __CPU; pFpLup_t fn = (pFpLup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFppiupp(uintptr_t fcn) { __CPU; pFppiupp_t fn = (pFppiupp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void pFppLp(uintptr_t fcn) { __CPU; pFppLp_t fn = (pFppLp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpplppp(uintptr_t fcn) { __CPU; pFpplppp_t fn = (pFpplppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (intptr_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void pFpppppppuipp(uintptr_t fcn) { __CPU; pFpppppppuipp_t fn = (pFpppppppuipp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void pFpppppppuipppp(uintptr_t fcn) { __CPU; pFpppppppuipppp_t fn = (pFpppppppuipppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void pFpppuipp(uintptr_t fcn) { __CPU; pFpppuipp_t fn = (pFpppuipp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpppuipppp(uintptr_t fcn) { __CPU; pFpppuipppp_t fn = (pFpppuipppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void pFpppupp(uintptr_t fcn) { __CPU; pFpppupp_t fn = (pFpppupp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void pFppuippp(uintptr_t fcn) { __CPU; pFppuippp_t fn = (pFppuippp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFppupp(uintptr_t fcn) { __CPU; pFppupp_t fn = (pFppupp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFppuppp(uintptr_t fcn) { __CPU; pFppuppp_t fn = (pFppuppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void pFppWpp(uintptr_t fcn) { __CPU; pFppWpp_t fn = (pFppWpp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint16_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFpupp(uintptr_t fcn) { __CPU; pFpupp_t fn = (pFpupp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpuppp(uintptr_t fcn) { __CPU; pFpuppp_t fn = (pFpuppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFpupppp(uintptr_t fcn) { __CPU; pFpupppp_t fn = (pFpupppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void pFpupppppp(uintptr_t fcn) { __CPU; pFpupppppp_t fn = (pFpupppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFpW(uintptr_t fcn) { __CPU; pFpW_t fn = (pFpW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint16_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFpWp(uintptr_t fcn) { __CPU; pFpWp_t fn = (pFpWp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint16_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpWppWpp(uintptr_t fcn) { __CPU; pFpWppWpp_t fn = (pFpWppWpp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint16_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (uint16_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpWWW(uintptr_t fcn) { __CPU; pFpWWW_t fn = (pFpWWW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint16_t)R_RSI, (uint16_t)R_RDX, (uint16_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFup(uintptr_t fcn) { __CPU; pFup_t fn = (pFup_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void pFuuip(uintptr_t fcn) { __CPU; pFuuip_t fn = (pFuuip_t)fcn; R_RAX=(uintptr_t)fn((uint32_t)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void uFEipipppp(uintptr_t fcn) { __CPU; uFEipipppp_t fn = (uFEipipppp_t)fcn; R_RAX=(uint32_t)fn((int32_t)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void uFEipippppp(uintptr_t fcn) { __CPU; uFEipippppp_t fn = (uFEipippppp_t)fcn; R_RAX=(uint32_t)fn((int32_t)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void uFEppipppp(uintptr_t fcn) { __CPU; uFEppipppp_t fn = (uFEppipppp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void uFEpppp(uintptr_t fcn) { __CPU; uFEpppp_t fn = (uFEpppp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void uFEppppppippp(uintptr_t fcn) { __CPU; uFEppppppippp_t fn = (uFEppppppippp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void uFEppppppp(uintptr_t fcn) { __CPU; uFEppppppp_t fn = (uFEppppppp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void uFipipp(uintptr_t fcn) { __CPU; uFipipp_t fn = (uFipipp_t)fcn; R_RAX=(uint32_t)fn((int32_t)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void uFppipp(uintptr_t fcn) { __CPU; uFppipp_t fn = (uFppipp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void uFppippp(uintptr_t fcn) { __CPU; uFppippp_t fn = (uFppippp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void uFppLpLuppp(uintptr_t fcn) { __CPU; uFppLpLuppp_t fn = (uFppLpLuppp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX, (uintptr_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void uFpppppupp(uintptr_t fcn) { __CPU; uFpppppupp_t fn = (uFpppppupp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void uFppupp(uintptr_t fcn) { __CPU; uFppupp_t fn = (uFppupp_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFEiippppppp(uintptr_t fcn) { __CPU; vFEiippppppp_t fn = (vFEiippppppp_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFEiippppV(uintptr_t fcn) { __CPU; vFEiippppV_t fn = (vFEiippppV_t)fcn; fn((int32_t)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFEipAippp(uintptr_t fcn) { __CPU; vFEipAippp_t fn = (vFEipAippp_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFEippp(uintptr_t fcn) { __CPU; vFEippp_t fn = (vFEippp_t)fcn; fn((int32_t)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFEiupippp(uintptr_t fcn) { __CPU; vFEiupippp_t fn = (vFEiupippp_t)fcn; fn((int32_t)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFEpipppp(uintptr_t fcn) { __CPU; vFEpipppp_t fn = (vFEpipppp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFEpippppppp(uintptr_t fcn) { __CPU; vFEpippppppp_t fn = (vFEpippppppp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFEppiipppp(uintptr_t fcn) { __CPU; vFEppiipppp_t fn = (vFEppiipppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFEppip(uintptr_t fcn) { __CPU; vFEppip_t fn = (vFEppip_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFEppipA(uintptr_t fcn) { __CPU; vFEppipA_t fn = (vFEppipA_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFEppipppp(uintptr_t fcn) { __CPU; vFEppipppp_t fn = (vFEppipppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFEppipV(uintptr_t fcn) { __CPU; vFEppipV_t fn = (vFEppipV_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFEppLippp(uintptr_t fcn) { __CPU; vFEppLippp_t fn = (vFEppLippp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFEpppiippp(uintptr_t fcn) { __CPU; vFEpppiippp_t fn = (vFEpppiippp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFEpppiipppp(uintptr_t fcn) { __CPU; vFEpppiipppp_t fn = (vFEpppiipppp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFEpppppppiippp(uintptr_t fcn) { __CPU; vFEpppppppiippp_t fn = (vFEpppppppiippp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFEpppuipV(uintptr_t fcn) { __CPU; vFEpppuipV_t fn = (vFEpppuipV_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFEpuipV(uintptr_t fcn) { __CPU; vFEpuipV_t fn = (vFEpuipV_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFLuui(uintptr_t fcn) { __CPU; vFLuui_t fn = (vFLuui_t)fcn; fn((uintptr_t)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppCuupp(uintptr_t fcn) { __CPU; vFppCuupp_t fn = (vFppCuupp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint8_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFppl(uintptr_t fcn) { __CPU; vFppl_t fn = (vFppl_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (intptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFppL(uintptr_t fcn) { __CPU; vFppL_t fn = (vFppL_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void wFppp(uintptr_t fcn) { __CPU; wFppp_t fn = (wFppp_t)fcn; R_RAX=fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void WFppp(uintptr_t fcn) { __CPU; WFppp_t fn = (WFppp_t)fcn; R_RAX=(unsigned short)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void IFv(uintptr_t fcn) { __CPU; IFv_t fn = (IFv_t)fcn; R_RAX=(int64_t)fn(); DEBUG_LOG; (void)cpu; }
void vFpLi(uintptr_t fcn) { __CPU; vFpLi_t fn = (vFpLi_t)fcn; fn((void*)R_RDI, (uintptr_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpLL(uintptr_t fcn) { __CPU; vFpLL_t fn = (vFpLL_t)fcn; fn((void*)R_RDI, (uintptr_t)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFupL(uintptr_t fcn) { __CPU; iFupL_t fn = (iFupL_t)fcn; R_RAX=(int32_t)fn((uint32_t)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFppU(uintptr_t fcn) { __CPU; iFppU_t fn = (iFppU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void uFpCi(uintptr_t fcn) { __CPU; uFpCi_t fn = (uFpCi_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (uint8_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void uFpui(uintptr_t fcn) { __CPU; uFpui_t fn = (uFpui_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void uFpuu(uintptr_t fcn) { __CPU; uFpuu_t fn = (uFpuu_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void uFppi(uintptr_t fcn) { __CPU; uFppi_t fn = (uFppi_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void LFpLi(uintptr_t fcn) { __CPU; LFpLi_t fn = (LFpLi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpCC(uintptr_t fcn) { __CPU; pFpCC_t fn = (pFpCC_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint8_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpUp(uintptr_t fcn) { __CPU; pFpUp_t fn = (pFpUp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint64_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpuuu(uintptr_t fcn) { __CPU; vFpuuu_t fn = (vFpuuu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpLLL(uintptr_t fcn) { __CPU; vFpLLL_t fn = (vFpLLL_t)fcn; fn((void*)R_RDI, (uintptr_t)R_RSI, (uintptr_t)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppup(uintptr_t fcn) { __CPU; vFppup_t fn = (vFppup_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppLp(uintptr_t fcn) { __CPU; vFppLp_t fn = (vFppLp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpuiL(uintptr_t fcn) { __CPU; iFpuiL_t fn = (iFpuiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpuui(uintptr_t fcn) { __CPU; iFpuui_t fn = (iFpuui_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpupi(uintptr_t fcn) { __CPU; iFpupi_t fn = (iFpupi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpupL(uintptr_t fcn) { __CPU; iFpupL_t fn = (iFpupL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpupp(uintptr_t fcn) { __CPU; iFpupp_t fn = (iFpupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpLpi(uintptr_t fcn) { __CPU; iFpLpi_t fn = (iFpLpi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void UFpipp(uintptr_t fcn) { __CPU; UFpipp_t fn = (UFpipp_t)fcn; R_RAX=fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void lFEppp(uintptr_t fcn) { __CPU; lFEppp_t fn = (lFEppp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpCWp(uintptr_t fcn) { __CPU; pFpCWp_t fn = (pFpCWp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint16_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpCuW(uintptr_t fcn) { __CPU; pFpCuW_t fn = (pFpCuW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpCuu(uintptr_t fcn) { __CPU; pFpCuu_t fn = (pFpCuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpuWp(uintptr_t fcn) { __CPU; pFpuWp_t fn = (pFpuWp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint16_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpuuC(uintptr_t fcn) { __CPU; pFpuuC_t fn = (pFpuuC_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint8_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpuup(uintptr_t fcn) { __CPU; pFpuup_t fn = (pFpuup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpupi(uintptr_t fcn) { __CPU; pFpupi_t fn = (pFpupi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFppii(uintptr_t fcn) { __CPU; pFppii_t fn = (pFppii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFppWW(uintptr_t fcn) { __CPU; pFppWW_t fn = (pFppWW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint16_t)R_RDX, (uint16_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFppuW(uintptr_t fcn) { __CPU; pFppuW_t fn = (pFppuW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFppuu(uintptr_t fcn) { __CPU; pFppuu_t fn = (pFppuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpppu(uintptr_t fcn) { __CPU; pFpppu_t fn = (pFpppu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpppip(uintptr_t fcn) { __CPU; vFpppip_t fn = (vFpppip_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFppppi(uintptr_t fcn) { __CPU; vFppppi_t fn = (vFppppi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpiiiu(uintptr_t fcn) { __CPU; iFpiiiu_t fn = (iFpiiiu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpiiiL(uintptr_t fcn) { __CPU; iFpiiiL_t fn = (iFpiiiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uintptr_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpuuup(uintptr_t fcn) { __CPU; iFpuuup_t fn = (iFpuuup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFpupup(uintptr_t fcn) { __CPU; iFpupup_t fn = (iFpupup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFppuup(uintptr_t fcn) { __CPU; iFppuup_t fn = (iFppuup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFpCpup(uintptr_t fcn) { __CPU; pFpCpup_t fn = (pFpCpup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFpCppp(uintptr_t fcn) { __CPU; pFpCppp_t fn = (pFpCppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFpuWWW(uintptr_t fcn) { __CPU; pFpuWWW_t fn = (pFpuWWW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint16_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8); DEBUG_LOG; (void)cpu; }
void pFpuuWW(uintptr_t fcn) { __CPU; pFpuuWW_t fn = (pFpuuWW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8); DEBUG_LOG; (void)cpu; }
void pFpuuup(uintptr_t fcn) { __CPU; pFpuuup_t fn = (pFpuuup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFppLii(uintptr_t fcn) { __CPU; pFppLii_t fn = (pFppLii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uintptr_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void pFpppup(uintptr_t fcn) { __CPU; pFpppup_t fn = (pFpppup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void pFppppi(uintptr_t fcn) { __CPU; pFppppi_t fn = (pFppppi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFEpuipp(uintptr_t fcn) { __CPU; vFEpuipp_t fn = (vFEpuipp_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpiiuuu(uintptr_t fcn) { __CPU; vFpiiuuu_t fn = (vFpiiuuu_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFpipppi(uintptr_t fcn) { __CPU; vFpipppi_t fn = (vFpipppi_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFppiiuu(uintptr_t fcn) { __CPU; vFppiiuu_t fn = (vFppiiuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFppiipi(uintptr_t fcn) { __CPU; vFppiipi_t fn = (vFppiipi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFpppiii(uintptr_t fcn) { __CPU; vFpppiii_t fn = (vFpppiii_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void iFppipiL(uintptr_t fcn) { __CPU; iFppipiL_t fn = (iFppipiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (uintptr_t)R_R9); DEBUG_LOG; (void)cpu; }
void uFpippup(uintptr_t fcn) { __CPU; uFpippup_t fn = (uFpippup_t)fcn; R_RAX=(uint32_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void UFpippup(uintptr_t fcn) { __CPU; UFpippup_t fn = (UFpippup_t)fcn; R_RAX=fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void lFEpippp(uintptr_t fcn) { __CPU; lFEpippp_t fn = (lFEpippp_t)fcn; R_RAX=(intptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void LFpipipi(uintptr_t fcn) { __CPU; LFpipipi_t fn = (LFpipipi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void pFpCuuCC(uintptr_t fcn) { __CPU; pFpCuuCC_t fn = (pFpCuuCC_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9); DEBUG_LOG; (void)cpu; }
void pFpCuuWW(uintptr_t fcn) { __CPU; pFpCuuWW_t fn = (pFpCuuWW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9); DEBUG_LOG; (void)cpu; }
void pFpCuuup(uintptr_t fcn) { __CPU; pFpCuuup_t fn = (pFpCuuup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void pFpuuwwu(uintptr_t fcn) { __CPU; pFpuuwwu_t fn = (pFpuuwwu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int16_t)R_RCX, (int16_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void pFpuuuuu(uintptr_t fcn) { __CPU; pFpuuuuu_t fn = (pFpuuuuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void pFpppppu(uintptr_t fcn) { __CPU; pFpppppu_t fn = (pFpppppu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFpppiipi(uintptr_t fcn) { __CPU; vFpppiipi_t fn = (vFpppiipi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFppppipi(uintptr_t fcn) { __CPU; vFppppipi_t fn = (vFppppipi_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpiiiiii(uintptr_t fcn) { __CPU; iFpiiiiii_t fn = (iFpiiiiii_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpiuiipp(uintptr_t fcn) { __CPU; iFpiuiipp_t fn = (iFpiuiipp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpuiuupp(uintptr_t fcn) { __CPU; iFpuiuupp_t fn = (iFpuiuupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpLipipi(uintptr_t fcn) { __CPU; iFpLipipi_t fn = (iFpLipipi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFppiiuup(uintptr_t fcn) { __CPU; iFppiiuup_t fn = (iFppiiuup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFppiipiL(uintptr_t fcn) { __CPU; iFppiipiL_t fn = (iFppiipiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(uintptr_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFppuipiL(uintptr_t fcn) { __CPU; iFppuipiL_t fn = (iFppuipiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (int32_t)R_R9, *(uintptr_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpCuwwWW(uintptr_t fcn) { __CPU; pFpCuwwWW_t fn = (pFpCuwwWW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (int16_t)R_RCX, (int16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpCuWCCC(uintptr_t fcn) { __CPU; pFpCuWCCC_t fn = (pFpCuWCCC_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9, *(uint8_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpCuuwwp(uintptr_t fcn) { __CPU; pFpCuuwwp_t fn = (pFpCuuwwp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int16_t)R_R8, (int16_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpCuuuuu(uintptr_t fcn) { __CPU; pFpCuuuuu_t fn = (pFpCuuuuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpCpWWup(uintptr_t fcn) { __CPU; pFpCpWWup_t fn = (pFpCpWWup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (void*)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpuuuwwu(uintptr_t fcn) { __CPU; pFpuuuwwu_t fn = (pFpuuuwwu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int16_t)R_R8, (int16_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpuupwwC(uintptr_t fcn) { __CPU; pFpuupwwC_t fn = (pFpuupwwC_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int16_t)R_R8, (int16_t)R_R9, *(uint8_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpippiiuu(uintptr_t fcn) { __CPU; vFpippiiuu_t fn = (vFpippiiuu_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFpWWipppp(uintptr_t fcn) { __CPU; iFpWWipppp_t fn = (iFpWWipppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint16_t)R_RSI, (uint16_t)R_RDX, (int32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFpuuupupu(uintptr_t fcn) { __CPU; iFpuuupupu_t fn = (iFpuuupupu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFpupppWWu(uintptr_t fcn) { __CPU; iFpupppWWu_t fn = (iFpupppWWu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFppiiipip(uintptr_t fcn) { __CPU; iFppiiipip_t fn = (iFppiiipip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFpCCuuwwC(uintptr_t fcn) { __CPU; pFpCCuuwwC_t fn = (pFpCCuuwwC_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint8_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (int16_t)R_R9, *(int16_t*)(R_RSP + 8), *(uint8_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFpCuwwWWu(uintptr_t fcn) { __CPU; pFpCuwwWWu_t fn = (pFpCuwwWWu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (int16_t)R_RCX, (int16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFpCuuuCup(uintptr_t fcn) { __CPU; pFpCuuuCup_t fn = (pFpCuuuCup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint8_t)R_R9, *(uint32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFpWWiCpup(uintptr_t fcn) { __CPU; pFpWWiCpup_t fn = (pFpWWiCpup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint16_t)R_RSI, (uint16_t)R_RDX, (int32_t)R_RCX, (uint8_t)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFpuuWWCuu(uintptr_t fcn) { __CPU; pFpuuWWCuu_t fn = (pFpuuWWCuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint8_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFpuuuupup(uintptr_t fcn) { __CPU; pFpuuuupup_t fn = (pFpuuuupup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFpuuupwwp(uintptr_t fcn) { __CPU; pFpuuupwwp_t fn = (pFpuuupwwp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (int16_t)R_R9, *(int16_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void pFpdwwWWui(uintptr_t fcn) { __CPU; pFpdwwWWui_t fn = (pFpdwwWWui_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, R_XMMD(0), (int16_t)R_RSI, (int16_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void pFpppppupp(uintptr_t fcn) { __CPU; pFpppppupp_t fn = (pFpppppupp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFpipiuiipp(uintptr_t fcn) { __CPU; vFpipiuiipp_t fn = (vFpipiuiipp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFpipppiipi(uintptr_t fcn) { __CPU; vFpipppiipi_t fn = (vFpipppiipi_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(void**)(R_RSP + 16), *(int32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void pFpCuWCCuuu(uintptr_t fcn) { __CPU; pFpCuWCCuuu_t fn = (pFpCuWCCuuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void pFpuuwwWWww(uintptr_t fcn) { __CPU; pFpuuwwWWww_t fn = (pFpuuwwWWww_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (int16_t)R_RCX, (int16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(int16_t*)(R_RSP + 16), *(int16_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void pFpupuuuuup(uintptr_t fcn) { __CPU; pFpupuuuuup_t fn = (pFpupuuuuup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void pFpppWWWWWp(uintptr_t fcn) { __CPU; pFpppWWWWWp_t fn = (pFpppWWWWWp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint16_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void iFpCuWCCCCup(uintptr_t fcn) { __CPU; iFpCuWCCCCup_t fn = (iFpCuWCCCCup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9, *(uint8_t*)(R_RSP + 8), *(uint8_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void pFpCuWCCuuCW(uintptr_t fcn) { __CPU; pFpCuWCCuuCW_t fn = (pFpCuWCCuuCW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint8_t*)(R_RSP + 24), *(uint16_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void pFpuwwWWuCuu(uintptr_t fcn) { __CPU; pFpuwwWWuCuu_t fn = (pFpuwwWWuCuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (int16_t)R_RDX, (int16_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint8_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void pFpuuuwwwwWW(uintptr_t fcn) { __CPU; pFpuuuwwwwWW_t fn = (pFpuuuwwwwWW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int16_t)R_R8, (int16_t)R_R9, *(int16_t*)(R_RSP + 8), *(int16_t*)(R_RSP + 16), *(uint16_t*)(R_RSP + 24), *(uint16_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void pFpuuuWWWCCi(uintptr_t fcn) { __CPU; pFpuuuWWWCCi_t fn = (pFpuuuWWWCCi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint8_t*)(R_RSP + 16), *(uint8_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void vFpipppiiiipi(uintptr_t fcn) { __CPU; vFpipppiiiipi_t fn = (vFpipppiiiipi_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(int32_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void iFpppiiuuiiuu(uintptr_t fcn) { __CPU; iFpppiiuuiiuu_t fn = (iFpppiiuuiiuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFpipppiiiipii(uintptr_t fcn) { __CPU; vFpipppiiiipii_t fn = (vFpipppiiiipii_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(int32_t*)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFpippppiiiipi(uintptr_t fcn) { __CPU; vFpippppiiiipi_t fn = (vFpippppiiiipi_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(void**)(R_RSP + 40), *(int32_t*)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void iFpCCCWCWCCCWp(uintptr_t fcn) { __CPU; iFpCCCWCWCCCWp_t fn = (iFpCCCWCWCCCWp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint8_t)R_RDX, (uint8_t)R_RCX, (uint16_t)R_R8, (uint8_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint8_t*)(R_RSP + 16), *(uint8_t*)(R_RSP + 24), *(uint8_t*)(R_RSP + 32), *(uint16_t*)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void pFWWiCCCCiipup(uintptr_t fcn) { __CPU; pFWWiCCCCiipup_t fn = (pFWWiCCCCiipup_t)fcn; R_RAX=(uintptr_t)fn((uint16_t)R_RDI, (uint16_t)R_RSI, (int32_t)R_RDX, (uint8_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9, *(uint8_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void pFpCuuWWwwCCup(uintptr_t fcn) { __CPU; pFpCuuWWwwCCup_t fn = (pFpCuuWWwwCCup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(int16_t*)(R_RSP + 8), *(int16_t*)(R_RSP + 16), *(uint8_t*)(R_RSP + 24), *(uint8_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(void**)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void pFpuuuWWWWWWWW(uintptr_t fcn) { __CPU; pFpuuuWWWWWWWW_t fn = (pFpuuuWWWWWWWW_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint16_t*)(R_RSP + 16), *(uint16_t*)(R_RSP + 24), *(uint16_t*)(R_RSP + 32), *(uint16_t*)(R_RSP + 40), *(uint16_t*)(R_RSP + 48)); DEBUG_LOG; (void)cpu; }
void vFpipppiiiiiiuu(uintptr_t fcn) { __CPU; vFpipppiiiiiiuu_t fn = (vFpipppiiiiiiuu_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(int32_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40), *(uint32_t*)(R_RSP + 48), *(uint32_t*)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void pFpCuuwwWWWWuup(uintptr_t fcn) { __CPU; pFpCuuwwWWWWuup_t fn = (pFpCuuwwWWWWuup_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int16_t)R_R8, (int16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint16_t*)(R_RSP + 16), *(uint16_t*)(R_RSP + 24), *(uint16_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(uint32_t*)(R_RSP + 48), *(void**)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void pFpuupppwwwwWWC(uintptr_t fcn) { __CPU; pFpuupppwwwwWWC_t fn = (pFpuupppwwwwWWC_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(int16_t*)(R_RSP + 8), *(int16_t*)(R_RSP + 16), *(int16_t*)(R_RSP + 24), *(int16_t*)(R_RSP + 32), *(uint16_t*)(R_RSP + 40), *(uint16_t*)(R_RSP + 48), *(uint8_t*)(R_RSP + 56)); DEBUG_LOG; (void)cpu; }
void iFpppwwWWwwWWpuu(uintptr_t fcn) { __CPU; iFpppwwWWwwWWpuu_t fn = (iFpppwwWWwwWWpuu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (int16_t)R_RCX, (int16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(int16_t*)(R_RSP + 16), *(int16_t*)(R_RSP + 24), *(uint16_t*)(R_RSP + 32), *(uint16_t*)(R_RSP + 40), *(void**)(R_RSP + 48), *(uint32_t*)(R_RSP + 56), *(uint32_t*)(R_RSP + 64)); DEBUG_LOG; (void)cpu; }
void pFppppppppppppppp(uintptr_t fcn) { __CPU; pFppppppppppppppp_t fn = (pFppppppppppppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64), *(void**)(R_RSP + 72)); DEBUG_LOG; (void)cpu; }
void pFpuuWWWWWWwwCCCuu(uintptr_t fcn) { __CPU; pFpuuWWWWWWwwCCCuu_t fn = (pFpuuWWWWWWwwCCCuu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint16_t*)(R_RSP + 16), *(uint16_t*)(R_RSP + 24), *(int16_t*)(R_RSP + 32), *(int16_t*)(R_RSP + 40), *(uint8_t*)(R_RSP + 48), *(uint8_t*)(R_RSP + 56), *(uint8_t*)(R_RSP + 64), *(uint32_t*)(R_RSP + 72), *(uint32_t*)(R_RSP + 80)); DEBUG_LOG; (void)cpu; }
void pFpippppppppppppppppp(uintptr_t fcn) { __CPU; pFpippppppppppppppppp_t fn = (pFpippppppppppppppppp_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16), *(void**)(R_RSP + 24), *(void**)(R_RSP + 32), *(void**)(R_RSP + 40), *(void**)(R_RSP + 48), *(void**)(R_RSP + 56), *(void**)(R_RSP + 64), *(void**)(R_RSP + 72), *(void**)(R_RSP + 80), *(void**)(R_RSP + 88), *(void**)(R_RSP + 96), *(void**)(R_RSP + 104)); DEBUG_LOG; (void)cpu; }
void pFpppWWCCpCpCpCWpCpCpC(uintptr_t fcn) { __CPU; pFpppWWCCpCpCpCWpCpCpC_t fn = (pFpppWWCCpCpCpCWpCpCpC_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint8_t)R_R9, *(uint8_t*)(R_RSP + 8), *(void**)(R_RSP + 16), *(uint8_t*)(R_RSP + 24), *(void**)(R_RSP + 32), *(uint8_t*)(R_RSP + 40), *(void**)(R_RSP + 48), *(uint8_t*)(R_RSP + 56), *(uint16_t*)(R_RSP + 64), *(void**)(R_RSP + 72), *(uint8_t*)(R_RSP + 80), *(void**)(R_RSP + 88), *(uint8_t*)(R_RSP + 96), *(void**)(R_RSP + 104), *(uint8_t*)(R_RSP + 112)); DEBUG_LOG; (void)cpu; }
void pFLL(uintptr_t fcn) { __CPU; pFLL_t fn = (pFLL_t)fcn; R_RAX=(uintptr_t)fn((uintptr_t)R_RDI, (uintptr_t)R_RSI); DEBUG_LOG; (void)cpu; }
void iFpLL(uintptr_t fcn) { __CPU; iFpLL_t fn = (iFpLL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFppI(uintptr_t fcn) { __CPU; pFppI_t fn = (pFppI_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFIp(uintptr_t fcn) { __CPU; pFIp_t fn = (pFIp_t)fcn; R_RAX=(uintptr_t)fn((int64_t)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void pFpUUU(uintptr_t fcn) { __CPU; pFpUUU_t fn = (pFpUUU_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpUpU(uintptr_t fcn) { __CPU; iFpUpU_t fn = (iFpUpU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (void*)R_RDX, (uint64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpUU(uintptr_t fcn) { __CPU; pFpUU_t fn = (pFpUU_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFppUp(uintptr_t fcn) { __CPU; iFppUp_t fn = (iFppUp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (uint64_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFU(uintptr_t fcn) { __CPU; pFU_t fn = (pFU_t)fcn; R_RAX=(uintptr_t)fn((uint64_t)R_RDI); DEBUG_LOG; (void)cpu; }
void vFEpi(uintptr_t fcn) { __CPU; vFEpi_t fn = (vFEpi_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFUU(uintptr_t fcn) { __CPU; vFUU_t fn = (vFUU_t)fcn; fn((uint64_t)R_RDI, (uint64_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFUU(uintptr_t fcn) { __CPU; pFUU_t fn = (pFUU_t)fcn; R_RAX=(uintptr_t)fn((uint64_t)R_RDI, (uint64_t)R_RSI); DEBUG_LOG; (void)cpu; }
void pFppU(uintptr_t fcn) { __CPU; pFppU_t fn = (pFppU_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFpLL(uintptr_t fcn) { __CPU; pFpLL_t fn = (pFpLL_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFppiU(uintptr_t fcn) { __CPU;  iFppiU_t fn = (iFppiU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (uint64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpCu(uintptr_t fcn) { __CPU;  pFpCu_t fn = (pFpCu_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFppppiii(uintptr_t fcn) { __CPU;  pFppppiii_t fn = (pFppppiii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(int32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpLiL(uintptr_t fcn) { __CPU;  iFpLiL_t fn = (iFpLiL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uintptr_t)R_RSI, (int32_t)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpCip(uintptr_t fcn) { __CPU;  pFpCip_t fn = (pFpCip_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void pFpCL(uintptr_t fcn) { __CPU; pFpCL_t fn = (pFpCL_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (uintptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void pFEppiiuuLipii(uintptr_t fcn) { __CPU;  pFEppiiuuLipii_t fn = (pFEppiiuuLipii_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uintptr_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16), *(void**)(R_RSP + 24), *(int32_t*)(R_RSP + 32), *(int32_t*)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void pFpCi(uintptr_t fcn) { __CPU; pFpCi_t fn = (pFpCi_t)fcn; R_RAX=(uintptr_t)fn((void*)R_RDI, (uint8_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFppl(uintptr_t fcn) { __CPU;  iFppl_t fn = (iFppl_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (intptr_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFppilp(uintptr_t fcn) { __CPU;  iFppilp_t fn = (iFppilp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (int32_t)R_RDX, (intptr_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpppL(uintptr_t fcn) { __CPU;  vFpppL_t fn = (vFpppL_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (uintptr_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpppppppp(uintptr_t fcn) { __CPU;  iFpppppppp_t fn = (iFpppppppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
//vulkan
void iFpUUU(uintptr_t fcn) { __CPU; iFpUUU_t fn = (iFpUUU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpUui(uintptr_t fcn) { __CPU; vFpUui_t fn = (vFpUui_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpiUuupup(uintptr_t fcn) { __CPU; vFpiUuupup_t fn = (vFpiUuupup_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (uint64_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFpUUi(uintptr_t fcn) { __CPU; vFpUUi_t fn = (vFpUUi_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppU(uintptr_t fcn) { __CPU; vFppU_t fn = (vFppU_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpUiUiupi(uintptr_t fcn) { __CPU; vFpUiUiupi_t fn = (vFpUiUiupi_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (int32_t)R_RDX, (uint64_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFpupup(uintptr_t fcn) { __CPU; vFpupup_t fn = (vFpupup_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpUipup(uintptr_t fcn) { __CPU; vFpUipup_t fn = (vFpUipup_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (int32_t)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFpUUup(uintptr_t fcn) { __CPU; vFpUUup_t fn = (vFpUUup_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpUUiup(uintptr_t fcn) { __CPU; vFpUUiup_t fn = (vFpUUiup_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFpUiUiup(uintptr_t fcn) { __CPU; vFpUiUiup_t fn = (vFpUiUiup_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (int32_t)R_RDX, (uint64_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpUiUup(uintptr_t fcn) { __CPU; vFpUiUup_t fn = (vFpUiUup_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (int32_t)R_RDX, (uint64_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFpUuuUUUi(uintptr_t fcn) { __CPU; vFpUuuUUUi_t fn = (vFpUuuUUUi_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint64_t)R_R8, (uint64_t)R_R9, *(uint64_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFpUU(uintptr_t fcn) { __CPU; vFpUU_t fn = (vFpUU_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpuuuiu(uintptr_t fcn) { __CPU; vFpuuuiu_t fn = (vFpuuuiu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFpUUuu(uintptr_t fcn) { __CPU; vFpUUuu_t fn = (vFpUUuu_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpUu(uintptr_t fcn) { __CPU; vFpUu_t fn = (vFpUu_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpUUUu(uintptr_t fcn) { __CPU; vFpUUUu_t fn = (vFpUUUu_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpUiuup(uintptr_t fcn) { __CPU; vFpUiuup_t fn = (vFpUiuup_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (int32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFpUi(uintptr_t fcn) { __CPU; vFpUi_t fn = (vFpUi_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpUuu(uintptr_t fcn) { __CPU; vFpUuu_t fn = (vFpUuu_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpfff(uintptr_t fcn) { __CPU; vFpfff_t fn = (vFpfff_t)fcn; fn((void*)R_RDI, R_XMMS(0), R_XMMS(1), R_XMMS(2)); DEBUG_LOG; (void)cpu; }
void vFpuup(uintptr_t fcn) { __CPU; vFpuup_t fn = (vFpuup_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpUUUp(uintptr_t fcn) { __CPU; vFpUUUp_t fn = (vFpUUUp_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpupiiupupup(uintptr_t fcn) { __CPU; vFpupiiupupup_t fn = (vFpupiiupupup_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(void**)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(void**)(R_RSP + 40)); DEBUG_LOG; (void)cpu; }
void vFpiUu(uintptr_t fcn) { __CPU; vFpiUu_t fn = (vFpiUu_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (uint64_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpUup(uintptr_t fcn) { __CPU; vFpUup_t fn = (vFpUup_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpUp(uintptr_t fcn) { __CPU; iFpUp_t fn = (iFpUp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpUp(uintptr_t fcn) { __CPU; vFpUp_t fn = (vFpUp_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpU(uintptr_t fcn) { __CPU; iFpU_t fn = (iFpU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpUpp(uintptr_t fcn) { __CPU; vFpUpp_t fn = (vFpUpp_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpiiiiip(uintptr_t fcn) { __CPU; iFpiiiiip_t fn = (iFpiiiiip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpUpp(uintptr_t fcn) { __CPU; iFpUpp_t fn = (iFpUpp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (void*)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpUuuLpUi(uintptr_t fcn) { __CPU; iFpUuuLpUi_t fn = (iFpUuuLpUi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uintptr_t)R_R8, (void*)R_R9, *(uint64_t*)(R_RSP + 8), *(int32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFpUUUip(uintptr_t fcn) { __CPU; iFpUUUip_t fn = (iFpUUUip_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (int32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void iFpupU(uintptr_t fcn) { __CPU; iFpupU_t fn = (iFpupU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint64_t)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpUi(uintptr_t fcn) { __CPU; iFpUi_t fn = (iFpUi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (int32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpupiU(uintptr_t fcn) { __CPU; iFpupiU_t fn = (iFpupiU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (uint64_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpuuuuuu(uintptr_t fcn) { __CPU; vFpuuuuuu_t fn = (vFpuuuuuu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpuuup(uintptr_t fcn) { __CPU; vFpuuup_t fn = (vFpuuup_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpUUp(uintptr_t fcn) { __CPU; vFpUUp_t fn = (vFpUUp_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpUUUUuu(uintptr_t fcn) { __CPU; vFpUUUUuu_t fn = (vFpUUUUuu_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (uint64_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void UFpp(uintptr_t fcn) { __CPU; UFpp_t fn = (UFpp_t)fcn; R_RAX=fn((void*)R_RDI, (void*)R_RSI); DEBUG_LOG; (void)cpu; }
void vFpiUUp(uintptr_t fcn) { __CPU; vFpiUUp_t fn = (vFpiUUp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFpiUUU(uintptr_t fcn) { __CPU; iFpiUUU_t fn = (iFpiUUU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (uint64_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpUUu(uintptr_t fcn) { __CPU; vFpUUu_t fn = (vFpUUu_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFpuupppp(uintptr_t fcn) { __CPU; vFpuupppp_t fn = (vFpuupppp_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpuiiii(uintptr_t fcn) { __CPU; vFpuiiii_t fn = (vFpuiiii_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFpiiULipp(uintptr_t fcn) { __CPU; vFpiiULipp_t fn = (vFpiiULipp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (uint64_t)R_RCX, (uintptr_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFpuUp(uintptr_t fcn) { __CPU; iFpuUp_t fn = (iFpuUp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint64_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpubp(uintptr_t fcn) { __CPU; iFpubp_t fn = (iFpubp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDX); R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, aligned_xcb, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpUUUUp(uintptr_t fcn) { __CPU; iFpUUUUp_t fn = (iFpUUUUp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (uint64_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFpUuiu(uintptr_t fcn) { __CPU; vFpUuiu_t fn = (vFpUuiu_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFpuuppp(uintptr_t fcn) { __CPU; vFpuuppp_t fn = (vFpuuppp_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFpuuUUuu(uintptr_t fcn) { __CPU; vFpuuUUuu_t fn = (vFpuuUUuu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint64_t)R_RCX, (uint64_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpuUUu(uintptr_t fcn) { __CPU; vFpuUUu_t fn = (vFpuUUu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (uint32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpUuupp(uintptr_t fcn) { __CPU; iFpUuupp_t fn = (iFpUuupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFpiUuup(uintptr_t fcn) { __CPU; vFpiUuup_t fn = (vFpiUuup_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (uint64_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFpuW(uintptr_t fcn) { __CPU; vFpuW_t fn = (vFpuW_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (uint16_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpupuuu(uintptr_t fcn) { __CPU; vFpupuuu_t fn = (vFpupuuu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFpupuuup(uintptr_t fcn) { __CPU; vFpupuuup_t fn = (vFpupuuup_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpUf(uintptr_t fcn) { __CPU; vFpUf_t fn = (vFpUf_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, R_XMMS(0)); DEBUG_LOG; (void)cpu; }
void iFpUupp(uintptr_t fcn) { __CPU; iFpUupp_t fn = (iFpUupp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void vFpupppp(uintptr_t fcn) { __CPU; vFpupppp_t fn = (vFpupppp_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFpupiUu(uintptr_t fcn) { __CPU; vFpupiUu_t fn = (vFpupiUu_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (uint64_t)R_R8, (uint32_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFpUppp(uintptr_t fcn) { __CPU; vFpUppp_t fn = (vFpUppp_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFpupiLpL(uintptr_t fcn) { __CPU; iFpupiLpL_t fn = (iFpupiLpL_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (uintptr_t)R_R8, (void*)R_R9, *(uintptr_t*)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpppppU(uintptr_t fcn) { __CPU; vFpppppU_t fn = (vFpppppU_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint64_t)R_R9); DEBUG_LOG; (void)cpu; }
void vFpppppuuu(uintptr_t fcn) { __CPU; vFpppppuuu_t fn = (vFpppppuuu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void iFpUuuLp(uintptr_t fcn) { __CPU; iFpUuuLp_t fn = (iFpUuuLp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uintptr_t)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void UFpUui(uintptr_t fcn) { __CPU; UFpUui_t fn = (UFpUui_t)fcn; R_RAX=fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppUUiUUUU(uintptr_t fcn) { __CPU; vFppUUiUUUU_t fn = (vFppUUiUUUU_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (int32_t)R_R8, (uint64_t)R_R9, *(uint64_t*)(R_RSP + 8), *(uint64_t*)(R_RSP + 16), *(uint64_t*)(R_RSP + 24)); DEBUG_LOG; (void)cpu; }
void vFpUUUUUUUUUUUuuu(uintptr_t fcn) { __CPU; vFpUUUUUUUUUUUuuu_t fn = (vFpUUUUUUUUUUUuuu_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (uint64_t)R_R8, (uint64_t)R_R9, *(uint64_t*)(R_RSP + 8), *(uint64_t*)(R_RSP + 16), *(uint64_t*)(R_RSP + 24), *(uint64_t*)(R_RSP + 32), *(uint64_t*)(R_RSP + 40), *(uint64_t*)(R_RSP + 48), *(uint32_t*)(R_RSP + 56), *(uint32_t*)(R_RSP + 64), *(uint32_t*)(R_RSP + 72)); DEBUG_LOG; (void)cpu; }
void iFpUu(uintptr_t fcn) { __CPU; iFpUu_t fn = (iFpUu_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX); DEBUG_LOG; (void)cpu; }
void iFpULp(uintptr_t fcn) { __CPU; iFpULp_t fn = (iFpULp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uintptr_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFpUiUi(uintptr_t fcn) { __CPU; iFpUiUi_t fn = (iFpUiUi_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (int32_t)R_RDX, (uint64_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void vFppUu(uintptr_t fcn) { __CPU; vFppUu_t fn = (vFppUu_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint64_t)R_RDX, (uint32_t)R_RCX); DEBUG_LOG; (void)cpu; }
void vFppUuupp(uintptr_t fcn) { __CPU; vFppUuupp_t fn = (vFppUuupp_t)fcn; fn((void*)R_RDI, (void*)R_RSI, (uint64_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFpUUUi(uintptr_t fcn) { __CPU; vFpUUUi_t fn = (vFpUUUi_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint64_t)R_RCX, (int32_t)R_R8); DEBUG_LOG; (void)cpu; }
void iFpiU(uintptr_t fcn) { __CPU; iFpiU_t fn = (iFpiU_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (int32_t)R_RSI, (uint64_t)R_RDX); DEBUG_LOG; (void)cpu; }
void vFpUuuUup(uintptr_t fcn) { __CPU; vFpUuuUup_t fn = (vFpUuuUup_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint64_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFpuuuuup(uintptr_t fcn) { __CPU; iFpuuuuup_t fn = (iFpuuuuup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void vFEpiiiupupup(uintptr_t fcn) { __CPU; vFEpiiiupupup_t fn = (vFEpiiiupupup_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8), *(void**)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(void**)(R_RSP + 32)); DEBUG_LOG; (void)cpu; }
void iFEpUuppp(uintptr_t fcn) { __CPU; iFEpUuppp_t fn = (iFEpUuppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9); DEBUG_LOG; (void)cpu; }
void vFEpUp(uintptr_t fcn) { __CPU; vFEpUp_t fn = (vFEpUp_t)fcn; fn((void*)R_RDI, (uint64_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void iFEpUp(uintptr_t fcn) { __CPU; iFEpUp_t fn = (iFEpUp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (void*)R_RDX); DEBUG_LOG; (void)cpu; }
void vFEpiiiiipp(uintptr_t fcn) { __CPU; vFEpiiiiipp_t fn = (vFEpiiiiipp_t)fcn; fn((void*)R_RDI, (int32_t)R_RSI, (int32_t)R_RDX, (int32_t)R_RCX, (int32_t)R_R8, (int32_t)R_R9, *(void**)(R_RSP + 8), *(void**)(R_RSP + 16)); DEBUG_LOG; (void)cpu; }
void vFEpupup(uintptr_t fcn) { __CPU; vFEpupup_t fn = (vFEpupup_t)fcn; fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFEpUppp(uintptr_t fcn) { __CPU; iFEpUppp_t fn = (iFEpUppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFEpUup(uintptr_t fcn) { __CPU; iFEpUup_t fn = (iFEpUup_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); DEBUG_LOG; (void)cpu; }
void iFEpuppp(uintptr_t fcn) { __CPU; iFEpuppp_t fn = (iFEpuppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); DEBUG_LOG; (void)cpu; }
void iFEpuvvppp(uintptr_t fcn) { __CPU; iFEpuppp_t fn = (iFEpuppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint32_t)R_RSI, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
void iFEpUUuppp(uintptr_t fcn) { __CPU; iFEpUUuppp_t fn = (iFEpUUuppp_t)fcn; R_RAX=(int32_t)fn((void*)R_RDI, (uint64_t)R_RSI, (uint64_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (void*)R_R9, *(void**)(R_RSP + 8)); DEBUG_LOG; (void)cpu; }
//endvulkan
//xcbV2
void pFb(uintptr_t fcn) { __CPU; pFb_t fn = (pFb_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuu(uintptr_t fcn) { __CPU; pFbuu_t fn = (pFbuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbup(uintptr_t fcn) { __CPU; pFbup_t fn = (pFbup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (void*)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbupuuuuup(uintptr_t fcn) { __CPU; pFbupuuuuup_t fn = (pFbupuuuuup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (void*)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbWWiCpup(uintptr_t fcn) { __CPU; pFbWWiCpup_t fn = (pFbWWiCpup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint16_t)R_RSI, (uint16_t)R_RDX, (int32_t)R_RCX, (uint8_t)R_R8, (void*)R_R9, *(uint32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbdwwWWui(uintptr_t fcn) { __CPU; pFbdwwWWui_t fn = (pFbdwwWWui_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, R_XMMD(0), (int16_t)R_RSI, (int16_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint32_t)R_R9, *(int32_t*)(R_RSP + 8)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuupwwC(uintptr_t fcn) { __CPU; uFbuupwwC_t fn = (uFbuupwwC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX, (int16_t)R_R8, (int16_t)R_R9, *(uint8_t*)(R_RSP + 8)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void iFbupppWWu(uintptr_t fcn) { __CPU; iFbupppWWu_t fn = (iFbupppWWu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(int32_t)fn(aligned_xcb, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbp(uintptr_t fcn) { __CPU; pFbp_t fn = (pFbp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuuWWWCCi(uintptr_t fcn) { __CPU; pFbuuuWWWCCi_t fn = (pFbuuuWWWCCi_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint8_t*)(R_RSP + 16), *(uint8_t*)(R_RSP + 24), *(int32_t*)(R_RSP + 32)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbppu(uintptr_t fcn) { __CPU; pFbppu_t fn = (pFbppu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI, (void*)R_RDX, (uint32_t)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuC(uintptr_t fcn) { __CPU; uFbuC_t fn = (uFbuC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint8_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbpu(uintptr_t fcn) { __CPU; pFbpu_t fn = (pFbpu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI, (uint32_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbppppuuCC(uintptr_t fcn) { __CPU; pFbppppuuCC_t fn = (pFbppppuuCC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint8_t*)(R_RSP + 16), *(uint8_t*)(R_RSP + 24)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuu(uintptr_t fcn) { __CPU; uFbuu_t fn = (uFbuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuW(uintptr_t fcn) { __CPU; uFbuW_t fn = (uFbuW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint16_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuupwwp(uintptr_t fcn) { __CPU; pFbuuupwwp_t fn = (pFbuuupwwp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8, (int16_t)R_R9, *(int16_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbpi(uintptr_t fcn) { __CPU; pFbpi_t fn = (pFbpi_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI, (int32_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuuuwwu(uintptr_t fcn) { __CPU; uFbuuuwwu_t fn = (uFbuuuwwu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int16_t)R_R8, (int16_t)R_R9, *(uint32_t*)(R_RSP + 8)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFb(uintptr_t fcn) { __CPU; uFb_t fn = (uFb_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbiiCpWWup(uintptr_t fcn) { __CPU; pFbiiCpWWup_t fn = (pFbiiCpWWup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (int32_t)R_RSI, (int32_t)R_RDX, (uint8_t)R_RCX, (void*)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(void**)(R_RSP + 24)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbWWWCCCCCCCCWCCCCCC(uintptr_t fcn) { __CPU; uFbWWWCCCCCCCCWCCCCCC_t fn = (uFbWWWCCCCCCCCWCCCCCC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint16_t)R_RSI, (uint16_t)R_RDX, (uint16_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9, *(uint8_t*)(R_RSP + 8), *(uint8_t*)(R_RSP + 16), *(uint8_t*)(R_RSP + 24), *(uint8_t*)(R_RSP + 32), *(uint8_t*)(R_RSP + 40), *(uint8_t*)(R_RSP + 48), *(uint16_t*)(R_RSP + 56), *(uint8_t*)(R_RSP + 64), *(uint8_t*)(R_RSP + 72), *(uint8_t*)(R_RSP + 80), *(uint8_t*)(R_RSP + 88), *(uint8_t*)(R_RSP + 96), *(uint8_t*)(R_RSP + 104)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbWu(uintptr_t fcn) { __CPU; uFbWu_t fn = (uFbWu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint16_t)R_RSI, (uint32_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbWWWWWWp(uintptr_t fcn) { __CPU; uFbWWWWWWp_t fn = (uFbWWWWWWp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint16_t)R_RSI, (uint16_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbWW(uintptr_t fcn) { __CPU; uFbWW_t fn = (uFbWW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint16_t)R_RSI, (uint16_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuuC(uintptr_t fcn) { __CPU; uFbuuC_t fn = (uFbuuC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint8_t)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuC(uintptr_t fcn) { __CPU; pFbuuC_t fn = (pFbuuC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint8_t)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuWWCuu(uintptr_t fcn) { __CPU; pFbuuWWCuu_t fn = (pFbuuWWCuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint8_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbu(uintptr_t fcn) { __CPU; uFbu_t fn = (uFbu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuwwWWuCuu(uintptr_t fcn) { __CPU; pFbuwwWWuCuu_t fn = (pFbuwwWWuCuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (int16_t)R_RDX, (int16_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint8_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuWWWWWWwwCCCuu(uintptr_t fcn) { __CPU; pFbuuWWWWWWwwCCCuu_t fn = (pFbuuWWWWWWwwCCCuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint16_t*)(R_RSP + 16), *(uint16_t*)(R_RSP + 24), *(int16_t*)(R_RSP + 32), *(int16_t*)(R_RSP + 40), *(uint8_t*)(R_RSP + 48), *(uint8_t*)(R_RSP + 56), *(uint8_t*)(R_RSP + 64), *(uint32_t*)(R_RSP + 72), *(uint32_t*)(R_RSP + 80)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbpp(uintptr_t fcn) { __CPU; pFbpp_t fn = (pFbpp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI, (void*)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuWWW(uintptr_t fcn) { __CPU; pFbuWWW_t fn = (pFbuWWW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint16_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbC(uintptr_t fcn) { __CPU; pFbC_t fn = (pFbC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuup(uintptr_t fcn) { __CPU; pFbuup_t fn = (pFbuup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbCuuuCup(uintptr_t fcn) { __CPU; uFbCuuuCup_t fn = (uFbCuuuCup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint8_t)R_R9, *(uint32_t*)(R_RSP + 8), *(void**)(R_RSP + 16)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuup(uintptr_t fcn) { __CPU; uFbuup_t fn = (uFbuup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuwwWW(uintptr_t fcn) { __CPU; pFbCuwwWW_t fn = (pFbCuwwWW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (int16_t)R_RCX, (int16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbu(uintptr_t fcn) { __CPU; pFbu_t fn = (pFbu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuWp(uintptr_t fcn) { __CPU; pFbuWp_t fn = (pFbuWp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint16_t)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void iFb(uintptr_t fcn) { __CPU; iFb_t fn = (iFb_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(int32_t)fn(aligned_xcb); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuuuu(uintptr_t fcn) { __CPU; pFbuuuuu_t fn = (pFbuuuuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuuwwwwWW(uintptr_t fcn) { __CPU; pFbuuuwwwwWW_t fn = (pFbuuuwwwwWW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int16_t)R_R8, (int16_t)R_R9, *(int16_t*)(R_RSP + 8), *(int16_t*)(R_RSP + 16), *(uint16_t*)(R_RSP + 24), *(uint16_t*)(R_RSP + 32)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbCuuu(uintptr_t fcn) { __CPU; uFbCuuu_t fn = (uFbCuuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuuWWWWWWWW(uintptr_t fcn) { __CPU; pFbuuuWWWWWWWW_t fn = (pFbuuuWWWWWWWW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint16_t*)(R_RSP + 16), *(uint16_t*)(R_RSP + 24), *(uint16_t*)(R_RSP + 32), *(uint16_t*)(R_RSP + 40), *(uint16_t*)(R_RSP + 48)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuuup(uintptr_t fcn) { __CPU; uFbuuup_t fn = (uFbuuup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbCuuWW(uintptr_t fcn) { __CPU; uFbCuuWW_t fn = (uFbCuuWW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbCuuwwWWWWuup(uintptr_t fcn) { __CPU; uFbCuuwwWWWWuup_t fn = (uFbCuuwwWWWWuup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int16_t)R_R8, (int16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint16_t*)(R_RSP + 16), *(uint16_t*)(R_RSP + 24), *(uint16_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(uint32_t*)(R_RSP + 48), *(void**)(R_RSP + 56)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFbu(uintptr_t fcn) { __CPU; vFbu_t fn = (vFbu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); fn(aligned_xcb, (uint32_t)R_RSI); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFbU(uintptr_t fcn) { __CPU; vFbU_t fn = (vFbU_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); fn(aligned_xcb, (uint64_t)R_RSI); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbpup(uintptr_t fcn) { __CPU; pFbpup_t fn = (pFbpup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI, (uint32_t)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuwwWWu(uintptr_t fcn) { __CPU; pFbCuwwWWu_t fn = (pFbCuwwWWu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (int16_t)R_RCX, (int16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCC(uintptr_t fcn) { __CPU; pFbCC_t fn = (pFbCC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint8_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbCuuuuu(uintptr_t fcn) { __CPU; uFbCuuuuu_t fn = (uFbCuuuuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(uint32_t*)(R_RSP + 8)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuWCCuuCW(uintptr_t fcn) { __CPU; pFbCuWCCuuCW_t fn = (pFbCuWCCuuCW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint8_t*)(R_RSP + 24), *(uint16_t*)(R_RSP + 32)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuWCCC(uintptr_t fcn) { __CPU; pFbCuWCCC_t fn = (pFbCuWCCC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9, *(uint8_t*)(R_RSP + 8)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuuCC(uintptr_t fcn) { __CPU; pFbCuuCC_t fn = (pFbCuuCC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuWCCuuu(uintptr_t fcn) { __CPU; pFbCuWCCuuu_t fn = (pFbCuWCCuuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint8_t)R_R8, (uint8_t)R_R9, *(uint32_t*)(R_RSP + 8), *(uint32_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuuwwp(uintptr_t fcn) { __CPU; pFbCuuwwp_t fn = (pFbCuuwwp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (int16_t)R_R8, (int16_t)R_R9, *(void**)(R_RSP + 8)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbCWp(uintptr_t fcn) { __CPU; uFbCWp_t fn = (uFbCWp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint16_t)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuWp(uintptr_t fcn) { __CPU; uFbuWp_t fn = (uFbuWp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint16_t)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void iFbupp(uintptr_t fcn) { __CPU; iFbupp_t fn = (iFbupp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(int32_t)fn(aligned_xcb, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuup(uintptr_t fcn) { __CPU; pFbuuup_t fn = (pFbuuup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuuup(uintptr_t fcn) { __CPU; pFbCuuup_t fn = (pFbCuuup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFbp(uintptr_t fcn) { __CPU; vFbp_t fn = (vFbp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); fn(aligned_xcb, (void*)R_RSI); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFb(uintptr_t fcn) { __CPU; vFb_t fn = (vFb_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); fn(aligned_xcb); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuuWWwwCCup(uintptr_t fcn) { __CPU; pFbCuuWWwwCCup_t fn = (pFbCuuWWwwCCup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint16_t)R_R8, (uint16_t)R_R9, *(int16_t*)(R_RSP + 8), *(int16_t*)(R_RSP + 16), *(uint8_t*)(R_RSP + 24), *(uint8_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(void**)(R_RSP + 48)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuWW(uintptr_t fcn) { __CPU; pFbuuWW_t fn = (pFbuuWW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbCuup(uintptr_t fcn) { __CPU; uFbCuup_t fn = (uFbCuup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (void*)R_R8); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFbi(uintptr_t fcn) { __CPU; vFbi_t fn = (vFbi_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); fn(aligned_xcb, (int32_t)R_RSI); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbipp(uintptr_t fcn) { __CPU; uFbipp_t fn = (uFbipp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void UFbipp(uintptr_t fcn) { __CPU; UFbipp_t fn = (UFbipp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=fn(aligned_xcb, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbippup(uintptr_t fcn) { __CPU; uFbippup_t fn = (uFbippup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (void*)R_R9); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void UFbippup(uintptr_t fcn) { __CPU; UFbippup_t fn = (UFbippup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=fn(aligned_xcb, (int32_t)R_RSI, (void*)R_RDX, (void*)R_RCX, (uint32_t)R_R8, (void*)R_R9); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCpWWup(uintptr_t fcn) { __CPU; pFbCpWWup_t fn = (pFbCpWWup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (void*)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8, (uint32_t)R_R9, *(void**)(R_RSP + 8)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuu(uintptr_t fcn) { __CPU; pFbCuu_t fn = (pFbCuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbpppp(uintptr_t fcn) { __CPU; pFbpppp_t fn = (pFbpppp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI, (void*)R_RDX, (void*)R_RCX, (void*)R_R8); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCuW(uintptr_t fcn) { __CPU; pFbCuW_t fn = (pFbCuW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbUp(uintptr_t fcn) { __CPU; pFbUp_t fn = (pFbUp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint64_t)R_RSI, (void*)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuwwWWww(uintptr_t fcn) { __CPU; pFbuuwwWWww_t fn = (pFbuuwwWWww_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (int16_t)R_RCX, (int16_t)R_R8, (uint16_t)R_R9, *(uint16_t*)(R_RSP + 8), *(int16_t*)(R_RSP + 16), *(int16_t*)(R_RSP + 24)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFEp(uintptr_t fcn) { __CPU; vFEp_t fn = (vFEp_t)fcn; fn((void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCu(uintptr_t fcn) { __CPU;  pFbCu_t fn = (pFbCu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint32_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void iFbppip(uintptr_t fcn) { __CPU;  iFbppip_t fn = (iFbppip_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(int32_t)fn(aligned_xcb, (void*)R_RSI, (void*)R_RDX, (int32_t)R_RCX, (void*)R_R8); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void iFbpiU(uintptr_t fcn) { __CPU;  iFbpiU_t fn = (iFbpiU_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(int32_t)fn(aligned_xcb, (void*)R_RSI, (int32_t)R_RDX, (uint64_t)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuu(uintptr_t fcn) { __CPU; pFbuuu_t fn = (pFbuuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbpWp(uintptr_t fcn) { __CPU; pFbpWp_t fn = (pFbpWp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI, (uint16_t)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuwwu(uintptr_t fcn) { __CPU; pFbuuwwu_t fn = (pFbuuwwu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (int16_t)R_RCX, (int16_t)R_R8, (uint32_t)R_R9); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbCCuuwwC(uintptr_t fcn) { __CPU; pFbCCuuwwC_t fn = (pFbCCuuwwC_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint8_t)R_RSI, (uint8_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (int16_t)R_R9, *(int16_t*)(R_RSP + 8), *(uint8_t*)(R_RSP + 16)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuuu(uintptr_t fcn) { __CPU; uFbuuu_t fn = (uFbuuu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void iFbpp(uintptr_t fcn) { __CPU; iFbpp_t fn = (iFbpp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(int32_t)fn(aligned_xcb, (void*)R_RSI, (void*)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void CFbupp(uintptr_t fcn) { __CPU; CFbupp_t fn = (CFbupp_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(unsigned char)fn(aligned_xcb, (uint32_t)R_RSI, (void*)R_RDX, (void*)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbup(uintptr_t fcn) { __CPU; uFbup_t fn = (uFbup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (void*)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void vFpC(uintptr_t fcn) { __CPU; vFpC_t fn = (vFpC_t)fcn; fn((void*)R_RDI, (uint8_t)R_RSI); DEBUG_LOG; (void)cpu; }
void HFpp(uintptr_t fcn) { __CPU; HFpp_t fn = (HFpp_t)fcn; unsigned __int128 u128 = fn((void*)R_RDI, (void*)R_RSI); R_RAX=(u128&0xFFFFFFFFFFFFFFFFL); R_RDX=(u128>>64)&0xFFFFFFFFFFFFFFFFL; DEBUG_LOG; (void)cpu; }
void uFbuU(uintptr_t fcn) { __CPU; uFbuU_t fn = (uFbuU_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint64_t)R_RDX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbppU(uintptr_t fcn) { __CPU; pFbppU_t fn = (pFbppU_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI, (void*)R_RDX, (uint64_t)R_RCX); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbpCpppwwwwwwWW(uintptr_t fcn) { __CPU; pFbpCpppwwwwwwWW_t fn = (pFbpCpppwwwwwwWW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (void*)R_RSI, (uint8_t)R_RDX, (void*)R_RCX, (void*)R_R8, (void*)R_R9, *(int16_t*)(R_RSP + 8), *(int16_t*)(R_RSP + 16), *(int16_t*)(R_RSP + 24), *(int16_t*)(R_RSP + 32), *(int16_t*)(R_RSP + 40), *(int16_t*)(R_RSP + 48), *(uint16_t*)(R_RSP + 56), *(uint16_t*)(R_RSP + 64)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuuWW(uintptr_t fcn) { __CPU; uFbuuWW_t fn = (uFbuuWW_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint16_t)R_RCX, (uint16_t)R_R8); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void uFbuuiup(uintptr_t fcn) { __CPU; uFbuuiup_t fn = (uFbuuiup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uint32_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (int32_t)R_RCX, (uint32_t)R_R8, (void*)R_R9); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void fFbu(uintptr_t fcn) { __CPU; fFbu_t fn = (fFbu_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_XMMS(0)=fn(aligned_xcb, (uint32_t)R_RSI); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuUUU(uintptr_t fcn) { __CPU; pFbuuUUU_t fn = (pFbuuUUU_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint64_t)R_RCX, (uint64_t)R_R8, (uint64_t)R_R9); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
void pFbuuuuuwwuuuuUUUup(uintptr_t fcn) { __CPU; pFbuuuuuwwuuuuUUUup_t fn = (pFbuuuuuwwuuuuUUUup_t)fcn; void *aligned_xcb = align_xcb_connection((void*)R_RDI); R_RAX=(uintptr_t)fn(aligned_xcb, (uint32_t)R_RSI, (uint32_t)R_RDX, (uint32_t)R_RCX, (uint32_t)R_R8, (uint32_t)R_R9, *(int16_t*)(R_RSP + 8), *(int16_t*)(R_RSP + 16), *(uint32_t*)(R_RSP + 24), *(uint32_t*)(R_RSP + 32), *(uint32_t*)(R_RSP + 40), *(uint32_t*)(R_RSP + 48), *(uint64_t*)(R_RSP + 56), *(uint64_t*)(R_RSP + 64), *(uint64_t*)(R_RSP + 72), *(uint32_t*)(R_RSP + 80), *(void**)(R_RSP + 88)); unalign_xcb_connection(aligned_xcb, (void*)R_RDI); DEBUG_LOG; (void)cpu; }
//xcbV2end
#undef R_RAX
#undef R_RDI
#undef R_RSI
#undef R_RDX
#undef R_RCX
#undef R_R8
#undef R_R9
#undef R_RSP
#undef R_XMMD
#undef R_XMMS
