#ifndef _IR1_BD_H_
#define _IR1_BD_H_

#include "bddisasm/bddisasm.h"
#include "latx-disasm.h"
#include "common.h"
#include "reg-map.h"
#include "latx-config.h"
#include "ir1.h"
#include "lsenv.h"

typedef enum _ND_PREFIX {
    /* group 1 */
    PREFIX_LOCK =                  ND_PREFIX_G0_LOCK,
    PREFIX_REPN =                  ND_PREFIX_G1_REPNE_REPNZ,
    PREFIX_REP =                   ND_PREFIX_G1_REPE_REPZ,
    /* group 2 */
    PREFIX_CS_SEG_OVERRIDE =       ND_PREFIX_G2_SEG_CS,
    PREFIX_SS_SEG_OVERRIDE =       ND_PREFIX_G2_SEG_SS,
    PREFIX_DS_SEG_OVERRIDE =       ND_PREFIX_G2_SEG_DS,
    PREFIX_ES_SEG_OVERRIDE =       ND_PREFIX_G2_SEG_ES,
    PREFIX_FS_SEG_OVERRIDE =       ND_PREFIX_G2_SEG_FS,
    PREFIX_GS_SEG_OVERRIDE =       ND_PREFIX_G2_SEG_GS,
    /* group 3 */
    PREFIX_OP_SIZE_OVERRIDE =      ND_PREFIX_G3_OPERAND_SIZE,
    /* group 4 */
    PREFIX_ADDR_SIZE_OVERRIDE =    ND_PREFIX_G4_ADDR_SIZE,

    PREFIX_REX                   = ND_PREFIX_REX_MIN,
} ND_PREFIX;

typedef ND_INS_CLASS IR1_OPCODE_BD;
typedef ND_INS_CATEGORY IR1_OPCODE_CAT_BD;
typedef ND_OPERAND_TYPE IR1_OPND_TYPE_BD;
typedef ND_OPERAND IR1_OPND_BD;
typedef ND_PREFIX IR1_PREFIX_BD;
typedef ND_INS_CATEGORY IR1_OPCODE_TYPE_BD;

// #define MAX_IR1_NUM_PER_TB 128

// typedef struct IR1_INST {
//     struct la_dt_insn *info;
//     /**
//      * @brief Current inst eflag information
//      *
//      * `_eflag_use` and `_eflag_def` define current inst eflag
//      * information.
//      *
//      * - `_eflag_use` define current inst used eflag
//      * - `_eflag_def` define current inst generate eflag, this maybe changed by
//      * `flag-rdtn` or `flag-pattern`
//      */
//     union {
//         uint16_t _eflag;
//         struct {
//             uint8_t _eflag_use;  /** < current inst used eflag */
//             uint8_t _eflag_def;  /** < current inst generate eflag */
//         };
//     };

// #ifdef CONFIG_LATX_HBR
// #define SHBR_XMM_ZERO    0x00000000
// #define SHBR_XMM_OTHER   0x00010000
// #define SHBR_XMM_ALL     0x0000ffff
// #define SHBR_XMM_MASK    0x0000ffff
// #define SHBR_NEED_SRC    0x10000000
// #define SHBR_NEED_DES    0x20000000
// #define SHBR_UPDATE_DES  0x40000000
// #define SHBR_NO_OPT_SRC  0x01000000
// #define SHBR_NO_OPT_DES  0x02000000
// #define GHBR_GPR_ALL     0x0000ffff
// /* #define GHBR_GPR_ALL     0x0 */
//     union {
//         uint32_t xmm_def;
//         uint32_t gpr_def;
//     };
//     union {
//         uint32_t xmm_use;
//         uint32_t gpr_use;
//     };
// #define SHBR_CAN_OPT32    0x02
// #define SHBR_CAN_OPT64    0x04
// #define GHBR_CAN_OPT      0x08
//     uint8_t hbr_flag;
// #endif
// #ifdef CONFIG_LATX_INSTS_PATTERN
//     struct {
//         int opc;
//         struct IR1_INST * next; /* index of IR1 list */
//     } instptn;
// #endif
//     uint8_t decode_engine;
//     uint64_t address;
// } IR1_INST;

extern IR1_OPND_BD al_ir1_opnd_bd;
extern IR1_OPND_BD ah_ir1_opnd_bd;
extern IR1_OPND_BD ax_ir1_opnd_bd;
extern IR1_OPND_BD eax_ir1_opnd_bd;
extern IR1_OPND_BD rax_ir1_opnd_bd;

extern IR1_OPND_BD bl_ir1_opnd_bd;
extern IR1_OPND_BD bh_ir1_opnd_bd;
extern IR1_OPND_BD bx_ir1_opnd_bd;
extern IR1_OPND_BD ebx_ir1_opnd_bd;
extern IR1_OPND_BD rbx_ir1_opnd_bd;

extern IR1_OPND_BD cl_ir1_opnd_bd;
extern IR1_OPND_BD ch_ir1_opnd_bd;
extern IR1_OPND_BD cx_ir1_opnd_bd;
extern IR1_OPND_BD ecx_ir1_opnd_bd;
extern IR1_OPND_BD rcx_ir1_opnd_bd;

extern IR1_OPND_BD dl_ir1_opnd_bd;
extern IR1_OPND_BD dh_ir1_opnd_bd;
extern IR1_OPND_BD dx_ir1_opnd_bd;
extern IR1_OPND_BD edx_ir1_opnd_bd;
extern IR1_OPND_BD rdx_ir1_opnd_bd;

extern IR1_OPND_BD esp_ir1_opnd_bd;
extern IR1_OPND_BD rsp_ir1_opnd_bd;

extern IR1_OPND_BD ebp_ir1_opnd_bd;
extern IR1_OPND_BD rbp_ir1_opnd_bd;

extern IR1_OPND_BD si_ir1_opnd_bd;
extern IR1_OPND_BD esi_ir1_opnd_bd;
extern IR1_OPND_BD rsi_ir1_opnd_bd;

extern IR1_OPND_BD di_ir1_opnd_bd;
extern IR1_OPND_BD edi_ir1_opnd_bd;
extern IR1_OPND_BD rdi_ir1_opnd_bd;

extern IR1_OPND_BD eax_mem8_ir1_opnd_bd;
extern IR1_OPND_BD ecx_mem8_ir1_opnd_bd;
extern IR1_OPND_BD edx_mem8_ir1_opnd_bd;
extern IR1_OPND_BD ebx_mem8_ir1_opnd_bd;
extern IR1_OPND_BD esp_mem8_ir1_opnd_bd;
extern IR1_OPND_BD ebp_mem8_ir1_opnd_bd;
extern IR1_OPND_BD esi_mem8_ir1_opnd_bd;
extern IR1_OPND_BD edi_mem8_ir1_opnd_bd;
extern IR1_OPND_BD di_mem8_ir1_opnd_bd;
extern IR1_OPND_BD si_mem8_ir1_opnd_bd;

extern IR1_OPND_BD eax_mem16_ir1_opnd_bd;
extern IR1_OPND_BD ecx_mem16_ir1_opnd_bd;
extern IR1_OPND_BD edx_mem16_ir1_opnd_bd;
extern IR1_OPND_BD ebx_mem16_ir1_opnd_bd;
extern IR1_OPND_BD esp_mem16_ir1_opnd_bd;
extern IR1_OPND_BD ebp_mem16_ir1_opnd_bd;
extern IR1_OPND_BD esi_mem16_ir1_opnd_bd;
extern IR1_OPND_BD edi_mem16_ir1_opnd_bd;

extern IR1_OPND_BD eax_mem32_ir1_opnd_bd;
extern IR1_OPND_BD ecx_mem32_ir1_opnd_bd;
extern IR1_OPND_BD edx_mem32_ir1_opnd_bd;
extern IR1_OPND_BD ebx_mem32_ir1_opnd_bd;
extern IR1_OPND_BD esp_mem32_ir1_opnd_bd;
extern IR1_OPND_BD esp_mem64_ir1_opnd_bd;
extern IR1_OPND_BD ebp_mem32_ir1_opnd_bd;
extern IR1_OPND_BD esi_mem32_ir1_opnd_bd;
extern IR1_OPND_BD edi_mem32_ir1_opnd_bd;
extern IR1_OPND_BD si_mem16_ir1_opnd_bd;
extern IR1_OPND_BD di_mem16_ir1_opnd_bd;
extern IR1_OPND_BD si_mem32_ir1_opnd_bd;
extern IR1_OPND_BD di_mem32_ir1_opnd_bd;
#ifdef TARGET_X86_64
extern IR1_OPND_BD rax_mem64_ir1_opnd_bd;
extern IR1_OPND_BD rcx_mem64_ir1_opnd_bd;
extern IR1_OPND_BD rdx_mem64_ir1_opnd_bd;
extern IR1_OPND_BD rbx_mem64_ir1_opnd_bd;
extern IR1_OPND_BD rsp_mem64_ir1_opnd_bd;
extern IR1_OPND_BD rbp_mem64_ir1_opnd_bd;
extern IR1_OPND_BD rsi_mem64_ir1_opnd_bd;
extern IR1_OPND_BD rdi_mem64_ir1_opnd_bd;
extern IR1_OPND_BD rsi_mem8_ir1_opnd_bd;
extern IR1_OPND_BD rdi_mem8_ir1_opnd_bd;
extern IR1_OPND_BD rsi_mem32_ir1_opnd_bd;
extern IR1_OPND_BD rdi_mem32_ir1_opnd_bd;
extern IR1_OPND_BD rsi_mem16_ir1_opnd_bd;
extern IR1_OPND_BD rdi_mem16_ir1_opnd_bd;
extern IR1_OPND_BD esi_mem64_ir1_opnd_bd;
extern IR1_OPND_BD edi_mem64_ir1_opnd_bd;
#endif
bool debug_with_dis(const uint8_t *addr);
ADDRX ir1_disasm_bd(IR1_INST *ir1, uint8_t *addr, ADDRX t_pc, int ir1_num, void *pir1_base);
inline __attribute__ ((always_inline))
void ir1_opnd_build_reg_bd(IR1_OPND_BD * opnd, int size, int reg, int type)
{
    opnd->Type = ND_OP_REG;
    lsassert(size % 8 == 0);
    opnd->Size = size / 8;
    opnd->Info.Register.Type = type;
    opnd->Info.Register.Reg = reg;
}
inline __attribute__ ((always_inline))
void ir1_opnd_build_mem_bd(IR1_OPND_BD *opnd, int size, int base_reg, int base_size, int64_t disp)
{
    opnd->Type = ND_OP_MEM;
    lsassert(size % 8 == 0);
    opnd->Size = size / 8;
    opnd->Info.Memory.HasBase = true;
    opnd->Info.Memory.Base = base_reg;
    opnd->Info.Memory.BaseSize = base_size / 8;
    opnd->Info.Memory.HasIndex = false;
    opnd->Info.Memory.HasSeg = false;
    opnd->Info.Memory.Scale = 0;
    opnd->Info.Memory.HasDisp = true;
    opnd->Info.Memory.DispSize = 4;
    opnd->Info.Memory.Disp = disp;
    opnd->Info.Memory.IsRipRel = 0;
}
inline __attribute__ ((always_inline))
IR1_OPND_TYPE_BD ir1_opnd_type_bd(IR1_OPND_BD *opnd) { return opnd->Type; }
inline __attribute__ ((always_inline))
int ir1_opnd_size_bd(const IR1_OPND_BD *opnd) 
{
    switch (opnd->Type)
    {
        case ND_OP_REG:
            return opnd->Info.Register.Size << 3;
        case ND_OP_MEM:
        case ND_OP_IMM:
        case ND_OP_OFFS:
        case ND_OP_ADDR_FAR:
            return opnd->Size << 3;
        default:
            lsassertm(0, "unsupport operand type\n");
            return -1;
    }
}
inline __attribute__ ((always_inline))
int ir1_addr_size_bd(IR1_INST *ir1)
{
    return 16 << ((INSTRUX *)(ir1->info))->AddrMode;
}
inline __attribute__ ((always_inline))
int latxs_ir1_has_prefix_opsize_bd(IR1_INST *ir1)
{
    return ((INSTRUX *)(ir1->info))->HasOpSize;
}
inline __attribute__ ((always_inline))
int ir1_opnd_base_reg_bd(IR1_OPND_BD *opnd)
{
    lsassert(opnd->Type == ND_OP_MEM && opnd->Info.Memory.HasBase);
    return opnd->Info.Memory.Base;
}
inline __attribute__ ((always_inline))
int ir1_opnd_scale_bd(IR1_OPND_BD *opnd)
{
    lsassert(opnd->Type == ND_OP_MEM && opnd->Info.Memory.Scale);
    return opnd->Info.Memory.Scale;
}
longx ir1_opnd_simm_bd(IR1_OPND_BD *opnd);
ulongx ir1_opnd_uimm_bd(IR1_OPND_BD *opnd);
ulongx ir1_opnd_s2uimm_bd(IR1_OPND_BD *opnd);

inline __attribute__ ((always_inline))
int ir1_opnd_is_imm_bd(IR1_OPND_BD *opnd) { return opnd->Type == ND_OP_IMM; }
inline __attribute__ ((always_inline))
int ir1_opnd_is_const_bd(IR1_OPND_BD *opnd) { return opnd->Type == ND_OP_CONST; }
inline __attribute__ ((always_inline))
ulongx ir1_opnd_const_bd(IR1_OPND_BD *opnd)
{
    lsassert(ir1_opnd_is_const_bd(opnd));
    return opnd->Info.Constant.Const;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_8h_bd(IR1_OPND_BD *opnd)
{
    return opnd->Type == ND_REG_GPR && opnd->Info.Register.IsHigh8;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_8l_bd(const IR1_OPND_BD *opnd)
{
    return opnd->Type == ND_REG_GPR && ir1_opnd_size_bd(opnd) == 8
        && !opnd->Info.Register.IsHigh8;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_16bit_bd(const IR1_OPND_BD *opnd)
{
    return ir1_opnd_size_bd(opnd) == 16;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_32bit_bd(const IR1_OPND_BD *opnd)
{
    return ir1_opnd_size_bd(opnd) == 32;
}
#ifdef TARGET_X86_64
inline __attribute__ ((always_inline))
int ir1_opnd_is_64bit_bd(const IR1_OPND_BD *opnd)
{
    return ir1_opnd_size_bd(opnd) == 64;
}
#endif
inline __attribute__ ((always_inline))
int ir1_opnd_is_pc_relative_bd(IR1_OPND_BD *opnd)
{
    return opnd->Info.Memory.IsRipRel;
}
inline __attribute__ ((always_inline))
bool ir1_opnd_is_uimm12_bd(IR1_OPND_BD *opnd)
{
    return ir1_opnd_is_imm_bd(opnd) && ir1_opnd_uimm_bd(opnd) < 4096;
}
inline __attribute__ ((always_inline))
bool ir1_opnd_is_simm12_bd(IR1_OPND_BD *opnd)
{
    return ir1_opnd_is_imm_bd(opnd) && ir1_opnd_simm_bd(opnd) >= -2048 &&
           ir1_opnd_simm_bd(opnd) < 2047;
}
inline __attribute__ ((always_inline))
bool ir1_opnd_is_s2uimm12_bd(IR1_OPND_BD *opnd)
{
    return ir1_opnd_is_imm_bd(opnd) && ir1_opnd_s2uimm_bd(opnd) < 4096;
}
int ir1_opnd_is_gpr_used_bd(IR1_OPND_BD *opnd, uint8 gpr_index);
inline __attribute__ ((always_inline))
int ir1_opnd_is_reg_bd(const IR1_OPND_BD *opnd)
{
    return opnd->Type == ND_OP_REG;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_gpr_bd(const IR1_OPND_BD *opnd)
{
    return opnd->Type == ND_OP_REG && opnd->Info.Register.Type == ND_REG_GPR;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_same_reg_bd(const IR1_OPND_BD *opnd0, const IR1_OPND_BD *opnd1)
{
    return ir1_opnd_is_gpr_bd(opnd0) && ir1_opnd_is_gpr_bd(opnd1) &&
        opnd0->Info.Register.Reg == opnd1->Info.Register.Reg;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_same_reg_without_width_bd(IR1_OPND_BD *opnd0, IR1_OPND_BD *opnd1)
{
    lsassert(ir1_opnd_is_gpr_bd(opnd0) && ir1_opnd_is_gpr_bd(opnd1));
    return ((opnd0->Info.Register.IsHigh8 && (opnd0->Info.Register.Reg > NDR_BL)) ?
        opnd0->Info.Register.Reg - NDR_AH : opnd0->Info.Register.Reg) ==
        ((opnd1->Info.Register.IsHigh8 && (opnd1->Info.Register.Reg > NDR_BL)) ?
        opnd1->Info.Register.Reg - NDR_AH : opnd1->Info.Register.Reg);
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_fpr_bd(const IR1_OPND_BD *opnd)
{
    return opnd->Type == ND_OP_REG && opnd->Info.Register.Type == ND_REG_FPU;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_seg_bd(const IR1_OPND_BD *opnd)
{
    return opnd->Type == ND_OP_REG && opnd->Info.Register.Type == ND_REG_SEG;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_mmx_bd(const IR1_OPND_BD *opnd)
{
    return opnd->Type == ND_OP_REG && opnd->Info.Register.Type == ND_REG_MMX;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_xmm_bd(const IR1_OPND_BD *opnd)
{
    return opnd->Type == ND_OP_REG && opnd->Info.Register.Type == ND_REG_SSE &&
        opnd->Info.Register.Size == ND_SIZE_128BIT;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_ymm_bd(const IR1_OPND_BD *opnd)
{
    return opnd->Type == ND_OP_REG && opnd->Info.Register.Type == ND_REG_SSE &&
        opnd->Info.Register.Size == ND_SIZE_256BIT;
}
inline __attribute__ ((always_inline))
int ir1_opnd_is_mem_bd(IR1_OPND_BD *opnd) { return opnd->Type == ND_OP_MEM; }
inline __attribute__ ((always_inline))
int ir1_opnd_has_base_bd(IR1_OPND_BD *opnd)
{
    lsassert(opnd->Type == ND_OP_MEM);
    return opnd->Info.Memory.HasBase;
}
inline __attribute__ ((always_inline))
int ir1_opnd_has_index_bd(IR1_OPND_BD *opnd)
{
    lsassert(opnd->Type == ND_OP_MEM);
    return opnd->Info.Memory.HasIndex;
}
inline __attribute__ ((always_inline))
int ir1_opnd_has_seg_bd(IR1_OPND_BD *opnd)
{
    lsassert(opnd->Type == ND_OP_MEM || opnd->Type == ND_OP_ADDR_FAR);
    if(opnd->Type == ND_OP_MEM) {
        return opnd->Info.Memory.HasSeg;
    } else if (opnd->Type == ND_OP_ADDR_FAR) {
        return 1;
    }
    lsassert(0);
    return -1;
}
inline __attribute__ ((always_inline))
int ir1_opnd_get_seg_index_bd(IR1_OPND_BD *opnd)
{
    lsassert((ir1_opnd_type_bd(opnd) == ND_OP_MEM && opnd->Info.Memory.HasSeg) || (ir1_opnd_type_bd(opnd) == ND_OP_ADDR_FAR));
    if(ir1_opnd_type_bd(opnd) == ND_OP_MEM) {
        return opnd->Info.Memory.Seg;
    } else if (ir1_opnd_type_bd(opnd) == ND_OP_ADDR_FAR) {
        return opnd->Info.Address.BaseSeg;
    }
    lsassert(0);
    return -1;
}
inline __attribute__ ((always_inline))
IR1_PREFIX_BD ir1_prefix_bd(IR1_INST *ir1) { return ((INSTRUX *)(ir1->info))->Rep; }
inline __attribute__ ((always_inline))
IR1_PREFIX_BD ir1_prefix_opnd_size_bd(IR1_INST *ir1) { return ((INSTRUX *)(ir1->info))->HasOpSize; }
#ifdef TARGET_X86_64
inline __attribute__ ((always_inline))
uint8_t ir1_rex_bd(IR1_INST *ir1)
{   
    if (((INSTRUX *)(ir1->info))->HasRex)
        return ((INSTRUX *)(ir1->info))->Rex.Rex;
    return 0;
}
inline __attribute__ ((always_inline))
uint8_t ir1_rex_w_bd(IR1_INST *ir1)
{
    if (((INSTRUX *)(ir1->info))->HasRex)
        return ((INSTRUX *)(ir1->info))->Rex.w;
    return 0;
}
#endif
inline __attribute__ ((always_inline))
int ir1_opnd_num_bd(IR1_INST *ir1) { return ((INSTRUX *)(ir1->info))->ExpOperandsCount; }
inline __attribute__ ((always_inline))
ADDRX ir1_addr_bd(IR1_INST *ir1) { return ir1->address; }
inline __attribute__ ((always_inline))
ADDRX ir1_addr_next_bd(IR1_INST *ir1) { return ir1->address + ((INSTRUX *)(ir1->info))->Length; }
ADDRX ir1_target_addr_bd(IR1_INST *ir1);

inline __attribute__ ((always_inline))
IR1_OPCODE_BD ir1_opcode_bd(IR1_INST *ir1) { return ((INSTRUX *)(ir1->info))->Instruction; }

inline __attribute__ ((always_inline))
int ir1_get_opnd_size_bd(IR1_INST *inst)
{
    switch (ir1_opcode_bd(inst)) {
    case ND_INS_CALLNR:
    case ND_INS_CALLFD:
    case ND_INS_CALLFI:
    case ND_INS_CALLNI:
    case ND_INS_RETN:
    case ND_INS_RETF:
    case ND_INS_ENTER:
    case ND_INS_LEAVE: {
#ifndef TARGET_X86_64
        if (latxs_ir1_has_prefix_opsize_bd(inst)) {
            return 2 << 3;
        } else {
            return 4 << 3;
        }
#else
        if (!CODEIS64) {
            if (ir1_prefix_opnd_size_bd(inst) != 0) {
                return 2 << 3;
            } else {
                return 4 << 3;
            }
        }
        return 8 << 3;
#endif
        break;
    }
    default:
        lsassertm(0, "Not alloc the function,"
                    "if you need, you can insert your process function in this part");
        break;
    }
    return 0;
}

inline __attribute__ ((always_inline))
int ir1_is_branch_bd(IR1_INST *ir1)
{
    switch (((INSTRUX *)(ir1->info))->Instruction) {
    case ND_INS_JO:
    case ND_INS_JNO:
    case ND_INS_JC:
    case ND_INS_JNC:
    case ND_INS_JZ:
    case ND_INS_JNZ:
    case ND_INS_JBE:
    case ND_INS_JNBE:
    case ND_INS_JS:
    case ND_INS_JNS:
    case ND_INS_JP:
    case ND_INS_JNP:
    case ND_INS_JL:
    case ND_INS_JNL:
    case ND_INS_JLE:
    case ND_INS_JNLE:

    case ND_INS_LOOPNZ:
    case ND_INS_LOOPZ:
    case ND_INS_LOOP:
    case ND_INS_JrCXZ:

        return 1;

    default:
        return 0;
    }
}
inline __attribute__ ((always_inline))
int ir1_is_jump_bd(IR1_INST *ir1) { 
    return (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_JMPABS || 
        (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_JMPE || 
        (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_JMPNI ||
        (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_JMPNR ||
        (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_JMPFI ||
        (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_JMPFD;
}
inline __attribute__ ((always_inline))
int ir1_is_call_bd(IR1_INST *ir1)
{
    return (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_CALLNI ||
        (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_CALLNR;
}
inline __attribute__ ((always_inline))
int ir1_is_return_bd(IR1_INST *ir1)
{
    return (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_RETN || (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_IRET ||
        (((INSTRUX *)(ir1->info))->Instruction) == ND_INS_RETF;
}
inline __attribute__ ((always_inline))
int ir1_is_syscall_bd(IR1_INST *ir1)
{
    switch (((INSTRUX *)(ir1->info))->Instruction) {
    case ND_INS_INT:
#ifdef TARGET_X86_64
    case ND_INS_SYSCALL:
    case ND_INS_SYSENTER:
    case ND_INS_SYSEXIT:
    case ND_INS_SYSRET:
#endif
        return true;
    default:
        return false;
    }
}
bool ir1_is_tb_ending_bd(IR1_INST *ir1);

inline __attribute__ ((always_inline))
bool ir1_is_cf_def_bd(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 0);
}
inline __attribute__ ((always_inline))
bool ir1_is_pf_def_bd(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 1);
}
inline __attribute__ ((always_inline))
bool ir1_is_af_def_bd(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 2);
}
inline __attribute__ ((always_inline))
bool ir1_is_zf_def_bd(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 3);
}
inline __attribute__ ((always_inline))
bool ir1_is_sf_def_bd(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 4);
}
inline __attribute__ ((always_inline))
bool ir1_is_of_def_bd(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 5);
}

inline __attribute__ ((always_inline))
uint8_t ir1_get_eflag_def_bd(IR1_INST *ir1) { return ir1->_eflag_def; }
inline __attribute__ ((always_inline))
void ir1_set_eflag_use_bd(IR1_INST *ir1, uint8_t use) { ir1->_eflag_use = use; }
inline __attribute__ ((always_inline))
void ir1_set_eflag_def_bd(IR1_INST *ir1, uint8_t def) { ir1->_eflag_def = def; }

void ir1_make_ins_JMP_bd(IR1_INST *ir1, ADDRX addr);
int ir1_opnd_index_reg_num_bd(IR1_OPND_BD *opnd);
int ir1_opnd_base_reg_num_bd(const IR1_OPND_BD *opnd);

int ir1_dump_bd(IR1_INST *ir1);
int ir1_opcode_dump_bd(IR1_INST *ir1);
#ifdef CONFIG_LATX_DEBUG
inline __attribute__ ((always_inline))
const char * ir1_name_bd(IR1_INST *inst){
    return ((INSTRUX *)(inst->info))->Mnemonic;
}
#endif

bool ir1_need_calculate_of_bd(IR1_INST *ir1);
bool ir1_need_calculate_cf_bd(IR1_INST *ir1);
bool ir1_need_calculate_pf_bd(IR1_INST *ir1);
bool ir1_need_calculate_af_bd(IR1_INST *ir1);
bool ir1_need_calculate_zf_bd(IR1_INST *ir1);
bool ir1_need_calculate_sf_bd(IR1_INST *ir1);
bool ir1_need_calculate_any_flag_bd(IR1_INST *ir1);

bool tr_opt_simm12_bd(IR1_INST *ir1);

bool ir1_translate_bd(IR1_INST *ir1);

inline __attribute__ ((always_inline))
uint8_t ir1_get_opnd_num_bd(IR1_INST *ir1)
{
    return ((INSTRUX *)(ir1->info))->ExpOperandsCount;
}
inline __attribute__ ((always_inline))
IR1_OPND_BD *ir1_get_opnd_bd(const IR1_INST *ir1, int i)
{
    lsassert(i < ((INSTRUX *)(ir1->info))->ExpOperandsCount);
    return &(((INSTRUX *)(ir1->info))->Operands[i]);
}

inline __attribute__ ((always_inline))
int ir1_opnd_is_offs_bd(IR1_OPND_BD *opnd) { return opnd->Type == ND_OP_OFFS; }
inline __attribute__ ((always_inline))
bool ir1_is_indirect_call_bd(IR1_INST *ir1)
{
    return !ir1_opnd_is_offs_bd(ir1_get_opnd_bd(ir1, 0));
}
inline __attribute__ ((always_inline))
bool ir1_is_indirect_jmp_bd(IR1_INST *ir1)
{
    IR1_OPND_BD * opnd = ir1_get_opnd_bd(ir1, 0);
    return (opnd->Type == ND_OP_MEM) || (opnd->Type == ND_OP_REG);
}
inline __attribute__ ((always_inline))
bool ir1_is_prefix_lock_bd(IR1_INST *ir1)
{
    return (((INSTRUX *)(ir1->info))->InstructionBytes[0] == ND_PREFIX_G0_LOCK) && (((INSTRUX *)(ir1->info))->PrefLength >= 1);
}

inline __attribute__ ((always_inline))
IR1_INST *tb_ir1_inst_bd(TranslationBlock *tb, const int i)
{
    return (IR1_INST *)(tb->s_data->ir1) + i;
}
inline __attribute__ ((always_inline))
IR1_INST *tb_ir1_inst_last_bd(TranslationBlock *tb)
{
    return (IR1_INST *)(tb->s_data->ir1) + tb->icount - 1;
}
inline __attribute__ ((always_inline))
int tb_ir1_num_bd(TranslationBlock *tb)
{
    return tb->icount;
}
inline __attribute__ ((always_inline))
int ir1_has_prefix_repe_bd(IR1_INST *ir1)
{
    return ((INSTRUX *)(ir1->info))->Rep == ND_PREFIX_G1_REPE_REPZ;
}
inline __attribute__ ((always_inline))
int ir1_has_prefix_repne_bd(IR1_INST *ir1)
{
    return ((INSTRUX *)(ir1->info))->Rep == ND_PREFIX_G1_REPNE_REPNZ;
}

char* ir1_get_op_str_bd(IR1_INST *ir1);

inline __attribute__ ((always_inline))
int ir1_opmode_bd(IR1_INST *ir1)
{
    const unsigned char opnd_size[3] = { 2, 4, 8 };
    return opnd_size[((INSTRUX *)(ir1->info))->OpMode];
}
inline __attribute__ ((always_inline))
void set_ir1_opnd_size_bd(IR1_OPND_BD *opnd, uint32_t size)
{
    switch (opnd->Type)
    {
        case ND_OP_REG:
            opnd->Info.Register.Size = size;
            break;
        case ND_OP_MEM:
            opnd->Size = size;
            break;
        case ND_OP_IMM:
            opnd->Size = size;
            break;
        case ND_OP_OFFS:
            opnd->Info.RelativeOffset.RawSize = size;
            break;
        case ND_OP_ADDR_FAR:
            opnd->Size = size;
            break;
        default:
            lsassertm(0, "unsupport operand type\n");
            break;
    }
    return;
}
inline __attribute__ ((always_inline))
void set_ir1_opnd_base_reg_bd(IR1_OPND_BD *opnd, uint32_t reg)
{
    lsassert(ir1_opnd_is_reg_bd(opnd));
    opnd->Info.Register.Reg = reg;
    return;
}
#endif