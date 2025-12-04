#ifndef _IR2_H_
#define _IR2_H_

#include "common.h"
#include "la-ir2.h"

#define IR2_ITEMP_MAX 32

#define IR2_OPND_EQ(opnd1, opnd2) \
(*(uint64_t *)&(opnd1) == *(uint64_t *)&(opnd2))

void ir2_opnd_build(IR2_OPND *, IR2_OPND_TYPE, int value);

IR2_OPND ir2_opnd_new(IR2_OPND_TYPE, int value);
IR2_OPND ir2_opnd_new_none(void);
IR2_OPND ra_alloc_label(void);
IR2_OPND ra_alloc_data(void);

uint32 ir2_opnd_val(const IR2_OPND *);
int32 ir2_opnd_imm(const IR2_OPND *);
int ir2_opnd_base_reg_num(const IR2_OPND *);
IR2_OPND_TYPE ir2_opnd_type(const IR2_OPND *);
void ir2_set_opnd_type(IR2_OPND *, const IR2_OPND_TYPE);

int32 ir2_opnd_label_id(const IR2_OPND *);

int ir2_opnd_cmp(const IR2_OPND *, const IR2_OPND *);

int ir2_opnd_is_ireg(const IR2_OPND *);
int ir2_opnd_is_freg(const IR2_OPND *);
int ir2_opnd_is_creg(const IR2_OPND *);
int ir2_opnd_is_none(const IR2_OPND *);
int ir2_opnd_is_itemp(const IR2_OPND *);
int ir2_opnd_is_ftemp(const IR2_OPND *);
int ir2_opnd_is_imm_reg(const IR2_OPND *);
int ir2_opnd_is_mem(const IR2_OPND *);
int ir2_opnd_is_imm(const IR2_OPND *);
int ir2_opnd_is_label(const IR2_OPND *);
int ir2_opnd_is_pseudo(const IR2_OPND *);
int ir2_opnd_is_data(const IR2_OPND *);


int ir2_opnd_to_string(const IR2_OPND *, char *, bool);

void ir2_opnd_convert_label_to_imm(IR2_OPND *, int imm);

typedef struct IR2_INST {
    int16 _opcode;
    int16 _id;
    int16 _prev;
    int16 _next;
    int op_count;
    IR2_OPND _opnd[4]; /*LA has 4 opnds*/
} IR2_INST;

void ir2_build(IR2_INST *, IR2_OPCODE, IR2_OPND, IR2_OPND, IR2_OPND);

void ir2_set_id(IR2_INST *, int);
int ir2_get_id(const IR2_INST *);
IR2_OPCODE ir2_opcode(const IR2_INST *);
void ir2_set_opcode(IR2_INST *ir2, IR2_OPCODE type);
int ir2_dump(const IR2_INST *, bool);
void ir2_dump_init(void);
int ir2_to_string(const IR2_INST *, char *);
IR2_INST *ir2_prev(const IR2_INST *);
IR2_INST *ir2_next(const IR2_INST *);
void la_disasm(uint32_t insn);
IR2_OPCODE la_decode(uint32_t insn);

void ir2_append(IR2_INST *);
void ir2_remove(int id);
IR2_INST * ir2_insert_before(IR2_INST *ir2, int id);
void ir2_insert_after(IR2_INST *ir2, int id);
IR2_INST *ir2_allocate(void);

uint32 ir2_assemble(const IR2_INST *);

bool ir2_opcode_is_branch(IR2_OPCODE);
bool ir2_opcode_is_branch_with_3opnds(IR2_OPCODE);
bool ir2_opcode_is_branch_with_2opnds(IR2_OPCODE);
bool ir2_opcode_is_f_branch(IR2_OPCODE opcode);
bool ir2_opcode_is_convert(IR2_OPCODE opcode);
bool ir2_opcode_is_fcmp(IR2_OPCODE opcode);
bool ir2_opcode_is_jirl(IR2_OPCODE opcode);

IR2_OPND create_ir2_opnd(IR2_OPND_TYPE type, uint32 val);
IR2_OPND create_immh_opnd(int val);
IR2_INST *la_append_ir2_opnd0(IR2_OPCODE type);
IR2_INST *la_append_ir2_opndi(IR2_OPCODE type, int imm);
IR2_INST *la_append_ir2_opnd1(IR2_OPCODE type, IR2_OPND op0);
IR2_INST *la_append_ir2_opnd1i(IR2_OPCODE type, IR2_OPND op0, int imm);
IR2_INST *la_append_ir2_opnd2(IR2_OPCODE type, IR2_OPND op0, IR2_OPND op1);
IR2_INST *la_append_ir2_opnd2i(IR2_OPCODE type, IR2_OPND op0,IR2_OPND op1, int imm);
IR2_INST *la_append_ir2_opnd2ii(IR2_OPCODE type, IR2_OPND op0, IR2_OPND op1, int imm0, int imm1);
IR2_INST *la_append_ir2_opnd3(IR2_OPCODE type, IR2_OPND op0,IR2_OPND op1, IR2_OPND op2);
IR2_INST *la_append_ir2_opnd3i(IR2_OPCODE type, IR2_OPND op0, IR2_OPND op1, IR2_OPND op2, int imm0);
IR2_INST *la_append_ir2_opnd4(IR2_OPCODE type, IR2_OPND op0, IR2_OPND op1, IR2_OPND op2, IR2_OPND op3);


IR2_INST *la_append_ir2_opnda(IR2_OPCODE opcode, ADDR addr);
bool la_ir2_opcode_is_addi(IR2_OPCODE opcode);
bool la_ir2_can_patch_imm12(IR2_OPCODE opcode);
bool la_ir2_opcode_is_x86_inst(IR2_OPCODE opcode);
bool la_ir2_opcode_is_label(IR2_OPCODE opcode);
bool la_ir2_opcode_is_bstrins(IR2_OPCODE opcode);
bool la_ir2_opcode_is_store(IR2_OPCODE opcode);
bool la_ir2_opcode_is_load(IR2_OPCODE opcode);

/* from ir2-optimization.c */
void tr_ir2_optimize(TranslationBlock *tb);

void ir2_opt_push_pop_fix(TranslationBlock *tb, CPUState *cpu, int i);

void la_append_ir2_jmp_far(ADDR jmp_offset);
#endif
