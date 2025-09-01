#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ir1-bd.h"
#include "latx-config.h"
#include "latx-options.h"
#include "latx-disassemble-trace.h"
#include "labddisasm.h"
#include "insts-pattern.h"

IR1_OPND_BD al_ir1_opnd_bd;
IR1_OPND_BD ah_ir1_opnd_bd;
IR1_OPND_BD ax_ir1_opnd_bd;
IR1_OPND_BD eax_ir1_opnd_bd;
IR1_OPND_BD rax_ir1_opnd_bd;

IR1_OPND_BD cl_ir1_opnd_bd;
IR1_OPND_BD ch_ir1_opnd_bd;
IR1_OPND_BD cx_ir1_opnd_bd;
IR1_OPND_BD ecx_ir1_opnd_bd;
IR1_OPND_BD rcx_ir1_opnd_bd;

IR1_OPND_BD dl_ir1_opnd_bd;
IR1_OPND_BD dh_ir1_opnd_bd;
IR1_OPND_BD dx_ir1_opnd_bd;
IR1_OPND_BD edx_ir1_opnd_bd;
IR1_OPND_BD rdx_ir1_opnd_bd;

IR1_OPND_BD bl_ir1_opnd_bd;
IR1_OPND_BD bh_ir1_opnd_bd;
IR1_OPND_BD bx_ir1_opnd_bd;
IR1_OPND_BD ebx_ir1_opnd_bd;
IR1_OPND_BD rbx_ir1_opnd_bd;

IR1_OPND_BD esp_ir1_opnd_bd;
IR1_OPND_BD rsp_ir1_opnd_bd;

IR1_OPND_BD ebp_ir1_opnd_bd;
IR1_OPND_BD rbp_ir1_opnd_bd;

IR1_OPND_BD si_ir1_opnd_bd;
IR1_OPND_BD esi_ir1_opnd_bd;
IR1_OPND_BD rsi_ir1_opnd_bd;

IR1_OPND_BD di_ir1_opnd_bd;
IR1_OPND_BD edi_ir1_opnd_bd;
IR1_OPND_BD rdi_ir1_opnd_bd;

IR1_OPND_BD eax_mem8_ir1_opnd_bd;
IR1_OPND_BD ecx_mem8_ir1_opnd_bd;
IR1_OPND_BD edx_mem8_ir1_opnd_bd;
IR1_OPND_BD ebx_mem8_ir1_opnd_bd;
IR1_OPND_BD esp_mem8_ir1_opnd_bd;
IR1_OPND_BD ebp_mem8_ir1_opnd_bd;
IR1_OPND_BD esi_mem8_ir1_opnd_bd;
IR1_OPND_BD edi_mem8_ir1_opnd_bd;
IR1_OPND_BD di_mem8_ir1_opnd_bd;
IR1_OPND_BD si_mem8_ir1_opnd_bd;

IR1_OPND_BD eax_mem16_ir1_opnd_bd;
IR1_OPND_BD ecx_mem16_ir1_opnd_bd;
IR1_OPND_BD edx_mem16_ir1_opnd_bd;
IR1_OPND_BD ebx_mem16_ir1_opnd_bd;
IR1_OPND_BD esp_mem16_ir1_opnd_bd;
IR1_OPND_BD ebp_mem16_ir1_opnd_bd;
IR1_OPND_BD esi_mem16_ir1_opnd_bd;
IR1_OPND_BD edi_mem16_ir1_opnd_bd;

IR1_OPND_BD eax_mem32_ir1_opnd_bd;
IR1_OPND_BD ecx_mem32_ir1_opnd_bd;
IR1_OPND_BD edx_mem32_ir1_opnd_bd;
IR1_OPND_BD ebx_mem32_ir1_opnd_bd;
IR1_OPND_BD esp_mem32_ir1_opnd_bd;
IR1_OPND_BD esp_mem64_ir1_opnd_bd;
IR1_OPND_BD ebp_mem32_ir1_opnd_bd;
IR1_OPND_BD esi_mem32_ir1_opnd_bd;
IR1_OPND_BD edi_mem32_ir1_opnd_bd;
IR1_OPND_BD si_mem16_ir1_opnd_bd;
IR1_OPND_BD di_mem16_ir1_opnd_bd;
IR1_OPND_BD si_mem32_ir1_opnd_bd;
IR1_OPND_BD di_mem32_ir1_opnd_bd;

#ifdef TARGET_X86_64
IR1_OPND_BD rax_mem64_ir1_opnd_bd;
IR1_OPND_BD rcx_mem64_ir1_opnd_bd;
IR1_OPND_BD rdx_mem64_ir1_opnd_bd;
IR1_OPND_BD rbx_mem64_ir1_opnd_bd;
IR1_OPND_BD rsp_mem64_ir1_opnd_bd;
IR1_OPND_BD rbp_mem64_ir1_opnd_bd;
IR1_OPND_BD rsi_mem64_ir1_opnd_bd;
IR1_OPND_BD rdi_mem64_ir1_opnd_bd;
IR1_OPND_BD rsi_mem8_ir1_opnd_bd;
IR1_OPND_BD rdi_mem8_ir1_opnd_bd;
IR1_OPND_BD rsi_mem32_ir1_opnd_bd;
IR1_OPND_BD rdi_mem32_ir1_opnd_bd;
IR1_OPND_BD rsi_mem16_ir1_opnd_bd;
IR1_OPND_BD rdi_mem16_ir1_opnd_bd;
IR1_OPND_BD esi_mem64_ir1_opnd_bd;
IR1_OPND_BD edi_mem64_ir1_opnd_bd;
extern int GPR_USEDEF_TO_SAVE;
extern int FPR_USEDEF_TO_SAVE;
extern int XMM_USEDEF_TO_SAVE;
#endif

static IR1_OPND_BD ir1_opnd_new_static_reg_bd(int size, ND_UINT32 reg, ND_UINT8 ishigh8)
{
    IR1_OPND_BD ir1_opnd;

    ir1_opnd.Type = ND_OP_REG;
    lsassert(size % 8 == 0);
    ir1_opnd.Info.Register.Size = size / 8;
    ir1_opnd.Info.Register.Reg = reg;

    switch(reg){
        case NDR_EAX ... NDR_EDI:
            ir1_opnd.Info.Register.Type = ND_REG_GPR;
            break;
    }
    ir1_opnd.Info.Register.IsHigh8 = ishigh8;

    return ir1_opnd;
}

static IR1_OPND_BD ir1_opnd_new_static_mem_bd(int opnd_size, int base_reg, int base_size, uint64_t imm)
{
    IR1_OPND_BD ir1_opnd;

    ir1_opnd.Type = ND_OP_MEM;
    lsassert(opnd_size % 8 == 0);
    ir1_opnd.Size = opnd_size / 8;
    ir1_opnd.Info.Memory.HasBase = true;
    ir1_opnd.Info.Memory.Base = base_reg;
    ir1_opnd.Info.Memory.BaseSize = base_size / 8;
    ir1_opnd.Info.Memory.HasIndex = false;
    ir1_opnd.Info.Memory.HasSeg = false;
    ir1_opnd.Info.Memory.Scale = 0;
    ir1_opnd.Info.Memory.HasDisp = true;
    ir1_opnd.Info.Memory.DispSize = 4;
    ir1_opnd.Info.Memory.Disp = imm;
    ir1_opnd.Info.Memory.IsRipRel = 0;

    return ir1_opnd;
}

static void __attribute__((__constructor__)) x86tomisp_ir1_init_bd(void)
{
    al_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(8, NDR_AL, 0);
    ah_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(8, NDR_AH, 1);
    ax_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(16, NDR_AX, 0);
    eax_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(32, NDR_EAX, 0);
    rax_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(64, NDR_RAX, 0);

    cl_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(8, NDR_CL, 0);
    ch_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(8, NDR_CH, 1);
    cx_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(16, NDR_CX, 0);
    ecx_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(32, NDR_ECX, 0);
    rcx_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(64, NDR_RCX, 0);

    dl_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(8, NDR_DL, 0);
    dh_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(8, NDR_DH, 1);
    dx_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(16, NDR_DX, 0);
    edx_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(32, NDR_EDX, 0);
    rdx_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(64, NDR_RDX, 0);

    bl_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(8, NDR_BL, 0);
    bh_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(8, NDR_BH, 1);
    bx_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(16, NDR_BX, 0);
    ebx_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(32, NDR_EBX, 0);
    rbx_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(64, NDR_RBX, 0);

    esp_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(32, NDR_ESP, 0);
    rsp_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(64, NDR_RSP, 0);

    ebp_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(32, NDR_EBP, 0);
    rbp_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(64, NDR_RBP, 0);

    si_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(16, NDR_SI, 0);
    esi_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(32, NDR_ESI, 0);
    rsi_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(64, NDR_RSI, 0);

    di_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(16, NDR_DI, 0);
    edi_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(32, NDR_EDI, 0);
    rdi_ir1_opnd_bd = ir1_opnd_new_static_reg_bd(64, NDR_RDI, 0);

    eax_mem8_ir1_opnd_bd = ir1_opnd_new_static_mem_bd(8, NDR_EAX, 32, 0);
    ecx_mem8_ir1_opnd_bd = ir1_opnd_new_static_mem_bd(8, NDR_ECX, 32, 0);
    edx_mem8_ir1_opnd_bd = ir1_opnd_new_static_mem_bd(8, NDR_EDX, 32, 0);
    ebx_mem8_ir1_opnd_bd = ir1_opnd_new_static_mem_bd(8, NDR_EBX, 32, 0);
    esp_mem8_ir1_opnd_bd = ir1_opnd_new_static_mem_bd(8, NDR_ESP, 32, 0);
    ebp_mem8_ir1_opnd_bd = ir1_opnd_new_static_mem_bd(8, NDR_EBP, 32, 0);
    esi_mem8_ir1_opnd_bd = ir1_opnd_new_static_mem_bd(8, NDR_ESI, 32, 0);
    edi_mem8_ir1_opnd_bd = ir1_opnd_new_static_mem_bd(8, NDR_EDI, 32, 0);
    di_mem8_ir1_opnd_bd = ir1_opnd_new_static_mem_bd(8, NDR_DI, 16, 0);
    si_mem8_ir1_opnd_bd = ir1_opnd_new_static_mem_bd(8, NDR_SI, 16, 0);
    eax_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_EAX, 32, 0);
    ecx_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_ECX, 32, 0);
    edx_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_EDX, 32, 0);
    ebx_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_EBX, 32, 0);
    esp_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_ESP, 32, 0);
    ebp_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_EBP, 32, 0);
    esi_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_ESI, 32, 0);
    edi_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_EDI, 32, 0);

    eax_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_EAX, 32, 0);
    ecx_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_ECX, 32, 0);
    edx_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_EDX, 32, 0);
    ebx_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_EBX, 32, 0);
    esp_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_ESP, 32, 0);
    esp_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_ESP, 32, 0);
    ebp_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_EBP, 32, 0);
    esi_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_ESI, 32, 0);
    edi_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_EDI, 32, 0);
    si_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_SI, 16, 0);
    di_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_DI, 16, 0);
    si_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_SI, 16, 0);
    di_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_DI, 16, 0);
#ifdef TARGET_X86_64
    rax_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_RAX, 64, 0);
    rcx_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_RCX, 64, 0);
    rdx_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_RDX, 64, 0);
    rbx_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_RBX, 64, 0);
    rsp_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_RSP, 64, 0);
    rbp_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_RBP, 64, 0);
    rsi_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_RSI, 64, 0);
    rdi_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_RDI, 64, 0);
    rsi_mem8_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(8, NDR_RSI, 64, 0);
    rdi_mem8_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(8, NDR_RDI, 64, 0);
    rsi_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_RSI, 64, 0);
    rdi_mem16_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(16, NDR_RDI, 64, 0);
    rsi_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_RSI, 64, 0);
    rdi_mem32_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(32, NDR_RDI, 64, 0);
    esi_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_ESI, 32, 0);
    edi_mem64_ir1_opnd_bd =
        ir1_opnd_new_static_mem_bd(64, NDR_EDI, 32, 0);
#endif
}

#ifdef CONFIG_LATX_DECODE_DEBUG
bool debug_with_dis(const uint8_t *addr)
{return false;
    INSTRUX info;
    NDSTATUS status;
#if TARGET_ABI_BITS == 64
        status = NdDecodeEx(&info, addr, 15, ND_CODE_64, ND_DATA_64);
#elif TARGET_ABI_BITS == 32
        status = NdDecodeEx(&info, addr, 15, ND_CODE_32, ND_DATA_32);
#else
#error "disasm mode err"
#endif
    if (!ND_SUCCESS(status))
    {
#ifdef CONFIG_LATX_DEBUG
        // fprintf(stderr, "%s: can't disasm code 0x%x at addr %p\n",
        //     __func__, *(uint32_t *)addr, addr);
#endif
        return false;
    }
#define WRAP(ins) (ND_INS_##ins)
    switch (info.Instruction)
    {
        case WRAP(INVALID):

        case WRAP(AAA):
        case WRAP(AAD):
        case WRAP(AAS):
        case WRAP(AAM):
        case WRAP(DAA):
        case WRAP(DAS):
        case WRAP(ADC):
        case WRAP(ADD):
        case WRAP(CMP):
        case WRAP(DEC):
        case WRAP(DIV):
        case WRAP(IDIV):
        case WRAP(INC):
        case WRAP(IMUL):
        case WRAP(MUL):
        case WRAP(MULX):
        case WRAP(NEG):
        case WRAP(SBB):
        case WRAP(SUB):
        case WRAP(XADD):

        case WRAP(ADCX):
        case WRAP(ADOX):

        case WRAP(PEXT):
        case WRAP(PDEP):
        case WRAP(BEXTR):
        case WRAP(BLSMSK):
        case WRAP(BZHI):

        case WRAP(BT):
        case WRAP(BTC):
        case WRAP(BTR):
        case WRAP(BTS):
        case WRAP(BLSR):
        case WRAP(BLSI):
        case WRAP(SALC):

        case WRAP(POPF):
        case WRAP(PUSHF):
        case WRAP(CLC):
        case WRAP(CLD):
        case WRAP(STC):
        case WRAP(STD):

        case WRAP(WAIT):
        case WRAP(F2XM1):
        case WRAP(FABS):
        case WRAP(FADDP):
        case WRAP(FADD):
        case WRAP(FBLD):
        case WRAP(FBSTP):
        case WRAP(FCHS):
        case WRAP(FCMOVNB):
        case WRAP(FCMOVB):
        case WRAP(FCMOVNU):
        case WRAP(FCMOVU):
        case WRAP(FCMOVE):
        case WRAP(FCMOVNE):
        case WRAP(FCMOVBE):
        case WRAP(FCMOVNBE):
        case WRAP(FCOM):
        case WRAP(FCOMI):
        case WRAP(FCOMIP):
        case WRAP(FCOMP):
        case WRAP(FCOMPP):
        case WRAP(FCOS):
        case WRAP(FDECSTP):
        case WRAP(FDIV):
        case WRAP(FDIVP):
        case WRAP(FDIVR):
        case WRAP(FDIVRP):
        case WRAP(FFREE):
        case WRAP(FFREEP):
        case WRAP(FIADD):
        case WRAP(FICOM):
        case WRAP(FICOMP):
        case WRAP(UD0):
        case WRAP(UD1):
        case WRAP(UD2):
        case WRAP(FIDIV):
        case WRAP(FIDIVR):
        case WRAP(FILD):
        case WRAP(FIMUL):
        case WRAP(FINCSTP):
        case WRAP(FIST):
        case WRAP(FISTP):
        case WRAP(FISTTP):
        case WRAP(FISUB):
        case WRAP(FISUBR):
        case WRAP(FLD1):
        case WRAP(FLD):
        case WRAP(FLDCW):
        case WRAP(FLDENV):
        case WRAP(FLDL2E):
        case WRAP(FLDL2T):
        case WRAP(FLDLG2):
        case WRAP(FLDLN2):
        case WRAP(FLDPI):
        case WRAP(FLDZ):
        case WRAP(FMUL):
        case WRAP(FMULP):
        case WRAP(FNCLEX):
        case WRAP(FNINIT):
        case WRAP(FNSTENV):
        case WRAP(FNOP):
        case WRAP(FNSAVE):
        case WRAP(FNSTCW):
        case WRAP(FNSTSW):
        case WRAP(FPATAN):
        case WRAP(FPREM1):
        case WRAP(FPREM):
        case WRAP(FPTAN):
        case WRAP(FRNDINT):
        case WRAP(FRSTOR):
        case WRAP(FSCALE):
        // case WRAP(FSETPM):
        case WRAP(FSIN):
        case WRAP(FSINCOS):
        case WRAP(FSQRT):
        case WRAP(FST):
        case WRAP(FSTP):
        case WRAP(FSUB):
        case WRAP(FSUBP):
        case WRAP(FSUBR):
        case WRAP(FSUBRP):
        case WRAP(FTST):
        case WRAP(FUCOM):
        case WRAP(FUCOMI):
        case WRAP(FUCOMIP):
        case WRAP(FUCOMP):
        case WRAP(FUCOMPP):
        case WRAP(FXAM):
        case WRAP(FXCH):
        case WRAP(FXRSTOR):
        case WRAP(FXSAVE):
        case WRAP(FXTRACT):
        case WRAP(FYL2X):
        case WRAP(FYL2XP1):

        case WRAP(NOP):
        case WRAP(ENDBR):
        case WRAP(INT3):
        case WRAP(IN):
        case WRAP(OUT):
        case WRAP(PREFETCH):
        case WRAP(PREFETCHNTA):
        case WRAP(PREFETCHT1):
        case WRAP(PREFETCHT2):
        case WRAP(LFENCE):
        case WRAP(MFENCE):
        case WRAP(SFENCE):
        case WRAP(CLFLUSH):
        case WRAP(CLFLUSHOPT):
        case WRAP(INS):
        case WRAP(CALLNR):
        case WRAP(CALLNI):
        case WRAP(RETF):
        case WRAP(IRET):
        case WRAP(RETN):
        case WRAP(JMPNR):
        case WRAP(JMPNI):
        case WRAP(ENTER):
        case WRAP(LEAVE):
        case WRAP(INT):
        case WRAP(SYSCALL):
        case WRAP(HLT):
        case WRAP(RDTSC):
        case WRAP(RDTSCP):
        case WRAP(CPUID):
        case WRAP(CWD):
        case WRAP(CDQ):
        case WRAP(CQO):
        case WRAP(SAHF):
        case WRAP(LAHF):
        case WRAP(LOOPNZ):
        case WRAP(LOOPZ):
        case WRAP(LOOP):
        case WRAP(CMC):
        case WRAP(CBW):
        case WRAP(CWDE):
        case WRAP(CDQE):
        case WRAP(PREFETCHT0):
        case WRAP(PREFETCHW):
        case WRAP(EMMS):
        case WRAP(XLATB):
        case WRAP(CRC32):

        case WRAP(POR):
        case WRAP(PXOR):
        case WRAP(PACKSSWB):
        case WRAP(PACKSSDW):
        case WRAP(PACKUSWB):
        case WRAP(PADDB):
        case WRAP(PADDW):
        case WRAP(PADDD):
        case WRAP(PADDSB):
        case WRAP(PADDSW):
        case WRAP(PADDUSB):
        case WRAP(PADDUSW):
        case WRAP(PAND):
        case WRAP(PANDN):
        case WRAP(PMADDWD):
        case WRAP(PMULHUW):
        case WRAP(PMULHW):
        case WRAP(PMULLW):
        case WRAP(PSUBB):
        case WRAP(PSUBW):
        case WRAP(PSUBD):
        case WRAP(PSUBSB):
        case WRAP(PSUBSW):
        case WRAP(PSUBUSB):
        case WRAP(PSUBUSW):
        case WRAP(PUNPCKHBW):
        case WRAP(PUNPCKHWD):
        case WRAP(PUNPCKHDQ):
        case WRAP(PUNPCKLBW):
        case WRAP(PUNPCKLWD):
        case WRAP(PUNPCKLDQ):
        case WRAP(ADDPS):
        case WRAP(ADDSD):
        case WRAP(ADDSS):
        case WRAP(ANDNPD):
        case WRAP(ANDNPS):
        case WRAP(ANDPS):
        case WRAP(DIVPD):
        case WRAP(DIVPS):
        case WRAP(DIVSD):
        case WRAP(DIVSS):
        case WRAP(MAXPD):
        case WRAP(MAXPS):
        case WRAP(MAXSD):
        case WRAP(MAXSS):
        case WRAP(MINPD):
        case WRAP(MINPS):
        case WRAP(MINSD):
        case WRAP(MINSS):
        case WRAP(MULPD):
        case WRAP(MULPS):
        case WRAP(MULSD):
        case WRAP(MULSS):
        case WRAP(ORPD):
        case WRAP(ORPS):
        case WRAP(PADDQ):
        case WRAP(PAVGB):
        case WRAP(PAVGW):
        case WRAP(PEXTRW):
        case WRAP(PINSRW):
        case WRAP(PMAXSW):
        case WRAP(PMAXUB):
        case WRAP(PMINSW):
        case WRAP(PMINUB):
        case WRAP(PMULUDQ):
        case WRAP(PSADBW):
        case WRAP(PSHUFD):
        case WRAP(PSHUFW):
        case WRAP(PSHUFHW):
        case WRAP(PSHUFLW):
        case WRAP(PSUBQ):
        case WRAP(PUNPCKHQDQ):
        case WRAP(RCPSS):
        case WRAP(RCPPS):
        case WRAP(RSQRTSS):
        case WRAP(RSQRTPS):
        case WRAP(SQRTPD):
        case WRAP(SQRTPS):
        case WRAP(ADDPD):
        case WRAP(ANDPD):
        case WRAP(UNPCKLPS):
        case WRAP(UNPCKLPD):
        case WRAP(UNPCKHPD):
        case WRAP(UNPCKHPS):
        case WRAP(SHUFPS):
        case WRAP(SHUFPD):
        case WRAP(PUNPCKLQDQ):
        case WRAP(XORPS):
        case WRAP(XORPD):
        case WRAP(SUBSS):
        case WRAP(SUBSD):
        case WRAP(SUBPS):
        case WRAP(SUBPD):
        case WRAP(SQRTSD):
        case WRAP(SQRTSS):
        case WRAP(PAUSE):
        case WRAP(HADDPD):
        case WRAP(HADDPS):
        case WRAP(HSUBPD):
        case WRAP(HSUBPS):
        case WRAP(PSIGNB):
        case WRAP(PSIGNW):
        case WRAP(PSIGND):
        case WRAP(PABSB):
        case WRAP(PABSW):
        case WRAP(PABSD):
        case WRAP(PALIGNR):
        case WRAP(PSHUFB):
        case WRAP(PMULHRSW):
        case WRAP(PMADDUBSW):
        case WRAP(PHSUBW):
        case WRAP(PHSUBD):
        case WRAP(PHSUBSW):
        case WRAP(PHADDW):
        case WRAP(PHADDD):
        case WRAP(PHADDSW):
        case WRAP(DPPS):
        case WRAP(DPPD):
        case WRAP(BLENDPS):
        case WRAP(BLENDPD):
        case WRAP(BLENDVPS):
        case WRAP(BLENDVPD):
        case WRAP(ROUNDPS):
        case WRAP(ROUNDSS):
        case WRAP(ROUNDPD):
        case WRAP(ROUNDSD):
        case WRAP(INSERTPS):
        case WRAP(EXTRACTPS):
        case WRAP(MPSADBW):
        case WRAP(PHMINPOSUW):
        case WRAP(PMULLD):
        case WRAP(PMULDQ):
        case WRAP(PBLENDVB):
        case WRAP(PBLENDW):
        case WRAP(PMINSB):
        case WRAP(PMINUW):
        case WRAP(PMINSD):
        case WRAP(PMINUD):
        case WRAP(PMAXSB):
        case WRAP(PMAXUW):
        case WRAP(PMAXSD):
        case WRAP(PMAXUD):
        case WRAP(PEXTRB):
        case WRAP(PEXTRD):
        case WRAP(PEXTRQ):
        case WRAP(PINSRB):
        case WRAP(PINSRD):
        case WRAP(PINSRQ):
        case WRAP(PMOVSXBW):
        case WRAP(PMOVZXBW):
        case WRAP(PMOVSXBD):
        case WRAP(PMOVZXBD):
        case WRAP(PMOVSXBQ):
        case WRAP(PMOVZXBQ):
        case WRAP(PMOVSXWD):
        case WRAP(PMOVZXWD):
        case WRAP(PMOVSXWQ):
        case WRAP(PMOVZXWQ):
        case WRAP(PMOVSXDQ):
        case WRAP(PMOVZXDQ):
        case WRAP(PTEST):
        case WRAP(PCMPEQQ):
        case WRAP(PACKUSDW):
        case WRAP(MOVNTDQA):
        case WRAP(ANDN):
        case WRAP(MOVBE):
        case WRAP(PCMPESTRI):
        case WRAP(PCMPESTRM):
        case WRAP(PCMPISTRI):
        case WRAP(PCMPISTRM):
        case WRAP(PCLMULQDQ):
        case WRAP(AESDEC):
        case WRAP(AESDECLAST):
        case WRAP(AESENC):
        case WRAP(AESENCLAST):
        case WRAP(AESIMC):
        case WRAP(AESKEYGENASSIST):

        case WRAP(MOVDQ2Q):
        case WRAP(MOVMSKPD):
        case WRAP(MOVMSKPS):
        case WRAP(MOVNTDQ):
        case WRAP(MOVNTI):
        case WRAP(MOVNTPD):
        case WRAP(MOVNTPS):
        case WRAP(MOVNTQ):
        case WRAP(MOVQ2DQ):
        case WRAP(PMOVMSKB):
        case WRAP(MASKMOVQ):
        case WRAP(MASKMOVDQU):
        case WRAP(MOVUPD):
        case WRAP(MOVDQA):
        case WRAP(MOVDQU):
        case WRAP(MOVUPS):
        case WRAP(MOVAPD):
        case WRAP(LDDQU):
        case WRAP(MOVAPS):
        case WRAP(MOVHLPS):
        case WRAP(MOVSHDUP):
        case WRAP(MOVSLDUP):
        case WRAP(MOVLHPS):
        case WRAP(MOVSD):
        case WRAP(MOVSS):
        case WRAP(MOVHPD):
        case WRAP(MOVHPS):
        case WRAP(MOVLPD):
        case WRAP(MOVLPS):
        case WRAP(MOVDDUP):

        case WRAP(PSLLW):
        case WRAP(PSLLD):
        case WRAP(PSLLQ):
        case WRAP(PSRLW):
        case WRAP(PSRLD):
        case WRAP(PSRLQ):
        case WRAP(PSRAW):
        case WRAP(PSRAD):
        case WRAP(PSLLDQ):
        case WRAP(PSRLDQ):
        case WRAP(ADDSUBPD):
        case WRAP(ADDSUBPS):

        case WRAP(SHA1NEXTE):
        case WRAP(SHA1MSG1):
        case WRAP(SHA1MSG2):
        case WRAP(SHA1RNDS4):
        case WRAP(SHA256RNDS2):
        case WRAP(SHA256MSG1):
        case WRAP(SHA256MSG2):
        
        case WRAP(CVTDQ2PD):
        case WRAP(CVTDQ2PS):
        case WRAP(CVTTPS2DQ):
        case WRAP(CVTTPD2DQ):
        case WRAP(CVTPD2DQ):
        case WRAP(CVTPS2DQ):
        case WRAP(CVTPD2PI):
        case WRAP(CVTTPD2PI):
        case WRAP(CVTPD2PS):
        case WRAP(CVTPI2PS):
        case WRAP(CVTPI2PD):
        case WRAP(CVTPS2PI):
        case WRAP(CVTTPS2PI):
        case WRAP(CVTSD2SS):
        case WRAP(CVTSI2SD):
        case WRAP(CVTSI2SS):
        case WRAP(CVTSS2SD):
        case WRAP(CVTPS2PD):
        case WRAP(CVTSD2SI):
        case WRAP(CVTSS2SI):
        case WRAP(CVTTSD2SI):
        case WRAP(CVTTSS2SI):

        case WRAP(PCMPEQB):
        case WRAP(PCMPEQW):
        case WRAP(PCMPEQD):
        case WRAP(PCMPGTB):
        case WRAP(PCMPGTW):
        case WRAP(PCMPGTD):
        case WRAP(PCMPGTQ):
        case WRAP(CMPPD):
        case WRAP(CMPPS):
        case WRAP(CMPSS):
        case WRAP(COMISD):
        case WRAP(COMISS):
        case WRAP(UCOMISD):
        case WRAP(UCOMISS):
        case WRAP(CMPSD):

        case WRAP(SETO):
        case WRAP(SETNO):
        case WRAP(SETC):
        case WRAP(SETNC):
        case WRAP(SETZ):
        case WRAP(SETNZ):
        case WRAP(SETBE):
        case WRAP(SETNBE):
        case WRAP(SETS):
        case WRAP(SETNS):
        case WRAP(SETP):
        case WRAP(SETNP):
        case WRAP(SETL):
        case WRAP(SETNL):
        case WRAP(SETLE):
        case WRAP(SETNLE):
        case WRAP(TZCNT):
        case WRAP(BSF):
        case WRAP(BSR):
        case WRAP(LZCNT):

        case WRAP(POP):
        case WRAP(PUSH):
        case WRAP(MOV):
        case WRAP(MOV_CR):
        case WRAP(MOV_DR):
        case WRAP(MOVZX):
        case WRAP(MOVSX):
        case WRAP(MOVSXD):
        case WRAP(MOVS):
        case WRAP(STOS):
        case WRAP(LODS):
        case WRAP(CMPS):
        case WRAP(SCAS):
        case WRAP(CMOVNBE):
        case WRAP(CMOVNC):
        case WRAP(CMOVC):
        case WRAP(CMOVBE):
        case WRAP(CMOVZ):
        case WRAP(CMOVNLE):
        case WRAP(CMOVNL):
        case WRAP(CMOVL):
        case WRAP(CMOVLE):
        case WRAP(CMOVNZ):
        case WRAP(CMOVO):
        case WRAP(CMOVNO):
        case WRAP(CMOVP):
        case WRAP(CMOVNP):
        case WRAP(CMOVS):
        case WRAP(CMOVNS):
        case WRAP(LEA):
        case WRAP(XCHG):
        case WRAP(CMPXCHG):
        case WRAP(CMPXCHG8B):
        case WRAP(CMPXCHG16B):
        case WRAP(MOVQ):
        case WRAP(MOVD):
        case WRAP(PUSHA):
        case WRAP(PUSHAD):
        case WRAP(POPA):
        case WRAP(POPAD):
        case WRAP(POPCNT):

        case WRAP(XOR):
        case WRAP(AND):
        case WRAP(TEST):
        case WRAP(OR):
        case WRAP(NOT):
        case WRAP(SHL):
        case WRAP(SHR):
        case WRAP(SAL):
        case WRAP(SAR):
        case WRAP(ROL):
        case WRAP(ROR):
        case WRAP(RORX):
        case WRAP(RCL):
        case WRAP(RCR):
        case WRAP(SHRD):
        case WRAP(SHLD):
        case WRAP(BSWAP):
        case WRAP(SHRX):
        case WRAP(SHLX):
        case WRAP(SARX):

        case WRAP(JBE):
        case WRAP(JC):
        case WRAP(JL):
        case WRAP(JLE):
        case WRAP(JNBE):
        case WRAP(JNC):
        case WRAP(JNL):
        case WRAP(JNLE):
        case WRAP(JNO):
        case WRAP(JNP):
        case WRAP(JNS):
        case WRAP(JNZ):
        case WRAP(JO):
        case WRAP(JP):
        case WRAP(JS):
        case WRAP(JZ):
        case WRAP(JrCXZ):

        case WRAP(RSSSP):
        // case WRAP():



        /* code */
        return true;
    
    default:
        return false;
    }
#undef WRAP
}
#endif

ADDRX ir1_disasm_bd(IR1_INST *ir1, uint8_t *addr, ADDRX t_pc, int ir1_num, void *pir1_base)
{
    INSTRUX *info;
    int count;
    if (!CODEIS64) {
        count = labddisasm_get_32(addr, 15, &info, ir1_num, pir1_base);
    } else {
        count = labddisasm_get_64(addr, 15, &info, ir1_num, pir1_base);
    }

    if (!info->HasSeg) {
        for (int i = 0; i < info->ExpOperandsCount; i++) {
            if(info->Operands[i].Type == ND_OP_MEM)
                info->Operands[i].Info.Memory.HasSeg = false;
        }
    }
    ir1->info = (struct la_dt_insn*)info;
#if defined(CONFIG_LATX_DECODE_DEBUG) || defined(CONFIG_LATX_DEBUG)
    ir1->decode_engine = OPT_DECODE_BY_BDDISASM;
#endif
    ir1->address = t_pc;

    /* Disasm error */
    if (count != 1) {
        fprintf(stderr, "ERROR : disasm, ADDR : 0x%" PRIx64 "\n", (uint64_t)t_pc);
        exit(-1);
    }

    ir1->_eflag = 0;

#ifdef CONFIG_LATX_INSTS_PATTERN
    ir1->instptn.opc  = INSTPTN_OPC_NONE;
    ir1->instptn.next = NULL;
#endif

    return (ADDRX)((uint64_t)t_pc + info->Length);
}

longx ir1_opnd_simm_bd(IR1_OPND_BD *opnd)
{
    if(ir1_opnd_is_imm_bd(opnd)) {
        switch (opnd->Size << 3) {
            case 8:
                return (longx)((int8_t)(opnd->Info.Immediate.Imm));
            case 16:
                return (longx)((int16_t)(opnd->Info.Immediate.Imm));
            case 32:
                return (longx)((int32_t)(opnd->Info.Immediate.Imm));
#ifdef TARGET_X86_64
            case 64:
                return (longx)((int64_t)(opnd->Info.Immediate.Imm));
#endif
            default:
                lsassert(0);
        }
    } else if (ir1_opnd_is_mem_bd(opnd)) {
        return (longx)(opnd->Info.Memory.Disp);
    } else {
        lsassertm(0, "REG opnd has no imm\n");
    }
    abort();
}

ulongx ir1_opnd_uimm_bd(IR1_OPND_BD *opnd)
{
    if (ir1_opnd_is_imm_bd(opnd)) {
        switch (opnd->Size << 3) {
            case 8:
                return (ulongx)((uint8_t)(opnd->Info.Immediate.Imm));
            case 16:
                return (ulongx)((uint16_t)(opnd->Info.Immediate.Imm));
            case 32:
                return (ulongx)((uint32_t)(opnd->Info.Immediate.Imm));
#ifdef TARGET_X86_64
            case 64:
                return (ulongx)((uint64_t)(opnd->Info.Immediate.Imm));
#endif
            default:
                lsassert(0);
        }
    } else if (ir1_opnd_is_mem_bd(opnd)) {
        return (ulongx)(opnd->Info.Memory.Disp);
    } else {
        lsassertm(0, "REG opnd has no imm\n");
    }
    abort();
}

ulongx ir1_opnd_s2uimm_bd(IR1_OPND_BD *opnd)
{
    lsassert(ir1_opnd_is_imm_bd(opnd));
    switch (ir1_opnd_size_bd(opnd)) {
    case 8:
        return (ulongx)((int8_t)(opnd->Info.Immediate.Imm));
    case 16:
        return (ulongx)((int16_t)(opnd->Info.Immediate.Imm));
    case 32:
        return (ulongx)((int32_t)(opnd->Info.Immediate.Imm));
    case 64:
        return (ulongx)((int64_t)(opnd->Info.Immediate.Imm));
    default:
        lsassert(0);
    }
    abort();
}

int ir1_opnd_is_gpr_used_bd(IR1_OPND_BD *opnd, uint8 gpr_index)
{
    if (ir1_opnd_is_gpr_bd(opnd)) {
        return ir1_opnd_base_reg_num_bd(opnd) == gpr_index;
    } else if (ir1_opnd_is_mem_bd(opnd)) {
        if (ir1_opnd_has_base_bd(opnd)) {
#ifdef TARGET_X86_64
            if (CODEIS64 && opnd->Info.Memory.IsRipRel) {
                return 0;
            }
#endif
            return ir1_opnd_base_reg_num_bd(opnd) == gpr_index;
        }
        if (ir1_opnd_has_index_bd(opnd)) {
            return ir1_opnd_index_reg_num_bd(opnd) == gpr_index;
        }
    }
    return 0;
}

static int ir1_opnd_get_reg_num_bd(ND_REG_TYPE reg_type, ND_REG_SIZE reg_size, ND_UINT32 Reg, ND_UINT8 IsHigh8)
{
    switch (reg_type)
    {
        case ND_REG_GPR:
        {
            switch (reg_size)
            {
                case ND_SIZE_8BIT:
                {
                    switch (Reg)
                    {
                        case NDR_AL:
                        case NDR_CL:
                        case NDR_DL:
                        case NDR_BL:
                            return Reg;
                        case NDR_AH:
                            return IsHigh8 ? eax_index : esi_index;
                        case NDR_CH:
                            return IsHigh8 ? ecx_index : ebp_index;
                        case NDR_DH:
                            return IsHigh8 ? edx_index : esi_index;
                        case NDR_BH:
                            return IsHigh8 ? ebx_index : edi_index;
                        case NDR_R8L:
                        case NDR_R9L:
                        case NDR_R10L:
                        case NDR_R11L:
                        case NDR_R12L:
                        case NDR_R13L:
                        case NDR_R14L:
                        case NDR_R15L:
                            return Reg;
                        default:
                            lsassertm(0, "unsupport 8bit reg\n");
                            return -1;
                    }
                }
                case ND_SIZE_16BIT:
                case ND_SIZE_32BIT:
                case ND_SIZE_64BIT:
                {
                    lsassert(Reg<=NDR_R15D);
                    return Reg;
                }
            default:
                fprintf(stderr, "reg_size = %d\n", reg_size);
                lsassertm(0, "unsupport gpr size\n");
                return -1;
            }
        }
        case ND_REG_SEG:
        {
            lsassert(Reg <= NDR_GS);
            return Reg;
        }
        case ND_REG_FPU:
        case ND_REG_MMX:
        {
            lsassert(Reg <= NDR_ST7);
            return Reg;
        }
        case ND_REG_SSE:
        {
            lsassert(ND_SIZE_128BIT == reg_size || ND_SIZE_256BIT == reg_size);
            lsassert(Reg <= NDR_XMM15);
            return Reg;
        }
        break;
    default:
        lsassertm(0, "unsupport reg type\n");
        break;
    }
    lsassert(0);
    return -1;
}

int ir1_opnd_base_reg_num_bd(const IR1_OPND_BD *opnd)                      
{
    // xtm : base may not a mem opnd
    if (opnd->Type == ND_OP_MEM && opnd->Info.Memory.HasBase == false)
        return -1;

    if (opnd->Type == ND_OP_MEM) {
        return ir1_opnd_get_reg_num_bd(ND_REG_GPR, opnd->Info.Memory.BaseSize,
                opnd->Info.Memory.Base, false);
    } else if (opnd->Type == ND_OP_REG) {
        return ir1_opnd_get_reg_num_bd(opnd->Info.Register.Type, opnd->Info.Register.Size, 
                opnd->Info.Register.Reg, opnd->Info.Register.IsHigh8);
    } else {
        return -1;
    }
}

ADDRX ir1_target_addr_bd(IR1_INST *ir1)
{
    lsassert(ir1_opnd_type_bd(&(((INSTRUX *)(ir1->info))->Operands[0])) ==
             ND_OP_OFFS);
    uint64_t offset = (((INSTRUX *)(ir1->info))->Operands[0].Info.RelativeOffset.Rel);
    uint64_t size = ((INSTRUX *)(ir1->info))->Operands[0].Size;
    uint64_t target = (ADDRX)(offset + ir1->address + ((INSTRUX *)(ir1->info))->Length);
    switch (size) {
        case 2:
            target = target & 0xffff;
            break;
        case 4:
            target = target & 0xffffffff;
            break;
        case 8:
            break;
        default:
            lsassert(0);
            break;
    }
    return target;
}

bool ir1_is_tb_ending_bd(IR1_INST *ir1)
{
#if defined(CONFIG_LATX_KZT)
    if(option_kzt && ir1_opcode_bd(ir1) == ND_INS_INT3) return true;
#endif
    if (((ir1_opcode_bd(ir1) == ND_INS_CALLNR) || (ir1_opcode_bd(ir1) == ND_INS_CALLFD)) &&
        (ir1_addr_next_bd(ir1) == ir1_target_addr_bd(ir1) || (ht_pc_thunk_lookup(ir1_target_addr_bd(ir1)) >= 0))) {
        return false;
    }
#ifdef CONFIG_LATX_SYSCALL_TUNNEL
    return ir1_is_branch_bd(ir1) || ir1_is_jump_bd(ir1) || ir1_is_call_bd(ir1) ||
           ir1_is_return_bd(ir1);
#else
    return ir1_is_branch_bd(ir1) || ir1_is_jump_bd(ir1) || ir1_is_call_bd(ir1) ||
           ir1_is_return_bd(ir1) || ir1_is_syscall_bd(ir1);
#endif
}

bool tr_opt_simm12_bd(IR1_INST *ir1)
{
#ifdef CONFIG_LATX_FLAG_REDUCTION
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(ir1, 1);
    return !ir1_need_calculate_any_flag_bd(ir1) &&
            ir1_opnd_is_simm12_bd(opnd1);
#else
    return false;
#endif
}

void ir1_make_ins_JMP_bd(IR1_INST *ir1, ADDRX addr)
{
    uint8_t code[5];
    code[0] = 0xEB; code[1] = 0xFE;
    // memcpy(&code[1], &off, 4);

    // ((INSTRUX *)(ir1->info))->InstructionBytes[0] = 0x69;
    // *(int32_t *)(((INSTRUX *)(ir1->info))->InstructionBytes + 1) = off;
    // ir1->address = (uint64_t)addr;
    // ((INSTRUX *)(ir1->info))->Length = 5;
    ir1_disasm_bd(ir1, code, addr, 1, NULL);
}

int ir1_opnd_index_reg_num_bd(IR1_OPND_BD *opnd)
{
    lsassert(opnd->Type == ND_OP_MEM && opnd->Info.Memory.HasIndex);
    return ir1_opnd_get_reg_num_bd(ND_REG_GPR, opnd->Info.Memory.IndexSize, opnd->Info.Memory.Index, false);
}


#ifdef CONFIG_LATX_DEBUG
static char op_str[256];
char* ir1_get_op_str_bd(IR1_INST *ir1)
{
    if((((INSTRUX *)(ir1->info))->Instruction) == ND_INS_INVALID) {
        op_str[0] = 'i';
        op_str[1] = 'l';
        op_str[2] = 'l';
        op_str[3] = 'e';
        op_str[4] = 'g';
        op_str[5] = 'a';
        op_str[6] = 'l';
        op_str[7] = 0;
        return op_str;
    }

    NDSTATUS status = NdToText(((INSTRUX *)(ir1->info)), 0, sizeof(op_str), op_str);
    if (ND_SUCCESS(status)) {
        return op_str;
    } else {
        sprintf(op_str, "反汇编失败，错误码: %u\n", status);
        return op_str;
    }
}

int ir1_dump_bd(IR1_INST *ir1)
{
    fprintf(stderr, "0x%" PRIx64 ": ", ir1->address);
    int i = 0;
    for (; i < ((INSTRUX *)(ir1->info))->Length; i++) {
        fprintf(stderr, "%02x", ((INSTRUX *)(ir1->info))->InstructionBytes[i]);
    }
    for (; i < 16; i++) {
        fprintf(stderr, "  ");
    }
    fprintf(stderr, "\tAS=%1d ", ir1_addr_size_bd(ir1));

    char *op_str = ir1_get_op_str_bd(ir1);
    fprintf(stderr, "%s\t\n", op_str);

    return 0;
}

int ir1_opcode_dump_bd(IR1_INST *ir1)
{
    fprintf(stderr, "0x%" PRIx64 ":\t%s\n", ir1->address,
            ((INSTRUX *)(ir1->info))->Mnemonic);
    return 0;
}

#endif