#include "ir2-relocate.h"
#include "latx-options.h"
#include "la-append.h"
#include "reg-alloc.h"
#include "aot.h"

IR2_INST *la_code_align(int align, uint32_t filling)
{
    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_ALIGN);
    pir2->op_count = 2;
    pir2->_opnd[0] = create_ir2_opnd(IR2_OPND_PSEUDO, align);
    pir2->_opnd[1] = create_ir2_opnd(IR2_OPND_PSEUDO, filling);
    ir2_append(pir2);
    return pir2;
}

IR2_INST *la_data_li(IR2_OPND data_temp, uint64_t data)
{
    lsassert(ir2_opnd_is_data(&data_temp));
    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_DATA_LI);
    pir2->op_count = 3;
    pir2->_opnd[0] = data_temp;
    pir2->_opnd[1] = create_ir2_opnd(IR2_OPND_PSEUDO, (uint32_t)(data >> 32));
    pir2->_opnd[2] = create_ir2_opnd(IR2_OPND_PSEUDO, (uint32_t)(data));
    ir2_append(pir2);
    return pir2;
}

IR2_INST *la_data_add(IR2_OPND dest, IR2_OPND src1, IR2_OPND src2)
{
    lsassert(ir2_opnd_is_data(&dest));
    lsassert(ir2_opnd_is_data(&src1) || ir2_opnd_is_pseudo(&src1) ||
             ir2_opnd_is_label(&src1));
    lsassert(ir2_opnd_is_data(&src2) || ir2_opnd_is_pseudo(&src2) ||
             ir2_opnd_is_label(&src2));
    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_DATA_ADD);
    pir2->op_count = 3;
    pir2->_opnd[0] = dest;
    pir2->_opnd[1] = src1;
    pir2->_opnd[2] = src2;
    ir2_append(pir2);
    return pir2;
}

IR2_INST *la_far_jump(IR2_OPND jmp_target, IR2_OPND counter_base)
{
    lsassert(ir2_opnd_is_data(&jmp_target));
    lsassert(ir2_opnd_is_data(&counter_base));
    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_FAR_JUMP);
    pir2->op_count = 2;
    pir2->_opnd[0] = jmp_target;
    pir2->_opnd[1] = counter_base;
    ir2_append(pir2);
    return pir2;
}

IR2_INST *la_pcaddi_relocate(IR2_OPND rd, IR2_OPND jmp_target, IR2_OPND counter_base)
{
    lsassert(ir2_opnd_is_data(&jmp_target));
    lsassert(ir2_opnd_is_data(&counter_base));
    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_PCADDI_RELOCATE);
    pir2->op_count = 3;
    pir2->_opnd[0] = rd;
    pir2->_opnd[1] = jmp_target;
    pir2->_opnd[2] = counter_base;
    ir2_append(pir2);
    return pir2;
}

IR2_INST *la_inst_diff(IR2_OPND dest, IR2_OPND label1, IR2_OPND label2)
{
    lsassert(ir2_opnd_is_data(&dest));
    lsassert(ir2_opnd_is_data(&label1) || ir2_opnd_is_label(&label1));
    lsassert(ir2_opnd_is_data(&label2) || ir2_opnd_is_label(&label2));
    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_INST_DIFF);
    pir2->op_count = 3;
    pir2->_opnd[0] = dest;
    pir2->_opnd[1] = label1;
    pir2->_opnd[2] = label2;
    ir2_append(pir2);
    return pir2;
}

IR2_INST *la_data_st(IR2_OPND mem, IR2_OPND data_temp)
{
    lsassert(ir2_opnd_is_data(&mem));
    lsassert(ir2_opnd_is_data(&data_temp) || ir2_opnd_is_label(&data_temp));
    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_DATA_ST);
    pir2->op_count = 2;
    pir2->_opnd[0] = mem;
    pir2->_opnd[1] = data_temp;
    ir2_append(pir2);
    return pir2;
}

/*for aot, so far*/
IR2_INST *la_data_st_rel_table(uint32_t table_index,
    uint32_t offset, IR2_OPND data_temp)
{
    lsassert(ir2_opnd_is_data(&data_temp) || ir2_opnd_is_label(&data_temp));
    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_DATA_ST_REL_TABLE);
    pir2->op_count = 3;
    pir2->_opnd[0] = create_ir2_opnd(IR2_OPND_PSEUDO, (uint32_t)(table_index));
    pir2->_opnd[1] = create_ir2_opnd(IR2_OPND_PSEUDO, (uint32_t)(offset));
    pir2->_opnd[2] = data_temp;
    ir2_append(pir2);
    return pir2;
}

IR2_INST *generate_code(uint32_t code)
{
    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_CODE);
    pir2->op_count = 1;
    pir2->_opnd[0] = create_ir2_opnd(IR2_OPND_PSEUDO, code);
    return pir2;
}

IR2_INST *la_profile_begin(void)
{
    IR2_OPND label = ra_alloc_label();
    la_label(label);

    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_PROFILE);
    pir2->op_count = 1;
    pir2->_opnd[0] = create_ir2_opnd(IR2_OPND_PSEUDO, PROFILE_BEGIN);
    pir2->_opnd[1] = label;
    ir2_append(pir2);
    return pir2;
}

IR2_INST *la_profile_end(void)
{
    IR2_OPND label = ra_alloc_label();
    la_label(label);

    IR2_INST *pir2 = ir2_allocate();
    ir2_set_opcode(pir2, LISA_PROFILE);
    pir2->op_count = 1;
    pir2->_opnd[0] = create_ir2_opnd(IR2_OPND_PSEUDO, PROFILE_END);
    pir2->_opnd[1] = label;
    ir2_append(pir2);
    return pir2;
}

IR2_INST *ir2_relocate(TRANSLATION_DATA *lat_ctx, IR2_INST *current, int *counter,
                  int *label, uint64_t *data)
{
    lsassert(current != NULL && counter != NULL);
    int cur_id = ir2_get_id(current);
    IR2_OPCODE opcode = ir2_opcode(current);
    uint32_t opnd0, opnd1, opnd2;
    IR2_INST *pir2 = NULL;
    ptrdiff_t insn_offset;

    switch (opcode) {
    case LISA_ALIGN:
        lsassert(ir2_opnd_is_pseudo(&current->_opnd[0]));
        lsassert(ir2_opnd_is_pseudo(&current->_opnd[1]));
        /* align size */
        opnd0 = ir2_opnd_val(&current->_opnd[0]);
        /* filling data(code) */
        opnd1 = ir2_opnd_val(&current->_opnd[1]);
        for (; *counter % opnd0; ++*counter) {
            pir2 = generate_code(opnd1);
            ir2_insert_before(pir2, cur_id);
        }
        ir2_remove(cur_id);
        break;
    
    case LISA_FAR_JUMP:
        /* cannot be label, if you want to use label, to la_b(label) */
        lsassert(ir2_opnd_is_data(&current->_opnd[0])); /* jump abs pc */
        lsassert(ir2_opnd_is_data(&current->_opnd[1])); /* begin pc */
        /* if is data, we need to sub the current pc */
        insn_offset   = data[ir2_opnd_val(&current->_opnd[0])];
        insn_offset  -= (*counter << 2) + data[ir2_opnd_val(&current->_opnd[1])];
        insn_offset >>= 2;
#ifdef CONFIG_LATX_LARGE_CC
        if (insn_offset == sextract64(insn_offset, 0, 26)) {
            IR2_OPND ir2_opnd_addr;
            ir2_opnd_build(&ir2_opnd_addr, IR2_OPND_IMM, insn_offset);
            ir2_insert_before(generate_b(ir2_opnd_addr), cur_id);
            ir2_insert_before(generate_nop(), cur_id);
        } else {
            ptrdiff_t upper, lower;
            lower = (int16_t)insn_offset;
            upper = (insn_offset - lower) >> 16;
            IR2_OPND tmp = ra_alloc_itemp();
            ir2_insert_before(generate_pcaddu18i(tmp, upper), cur_id);
            ir2_insert_before(generate_jirl(zero_ir2_opnd, tmp, lower), cur_id);
            ra_free_temp(tmp);
        }
        (*counter) += 2;
#else
        /* B has +- 128M range */
        lsassert(insn_offset == sextract64(insn_offset, 0, 26));
        IR2_OPND ir2_opnd_addr;
        ir2_opnd_build(&ir2_opnd_addr, IR2_OPND_IMM, insn_offset);
        ir2_insert_before(generate_b(ir2_opnd_addr), cur_id);
        (*counter)++;
#endif
        ir2_remove(cur_id);

        break;

    case LISA_PCADDI_RELOCATE:
        lsassert(ir2_opnd_is_data(&current->_opnd[1])); /* jump abs pc */
        lsassert(ir2_opnd_is_data(&current->_opnd[2])); /* begin pc */

        insn_offset   = data[ir2_opnd_val(&current->_opnd[1])];
        insn_offset  -= (*counter << 2) +
                        data[ir2_opnd_val(&current->_opnd[2])];
        insn_offset >>= 2;
        ir2_insert_before(generate_pcaddi(current->_opnd[0],
                            insn_offset), cur_id);
        (*counter)++;
        ir2_remove(cur_id);

        break;

    case LISA_DATA_LI:
        lsassert(ir2_opnd_is_data(&current->_opnd[0]));
        lsassert(ir2_opnd_is_pseudo(&current->_opnd[1]));
        lsassert(ir2_opnd_is_pseudo(&current->_opnd[2]));
        opnd0 = ir2_opnd_val(&current->_opnd[1]); /* high */
        opnd1 = ir2_opnd_val(&current->_opnd[2]); /* low */
        /* insert into data array */
        data[ir2_opnd_val(&current->_opnd[0])] =
            ((uint64_t)opnd0 << 32) | opnd1;
        ir2_remove(cur_id);

        break;

    case LISA_DATA_ADD:
        lsassert(ir2_opnd_is_data(&current->_opnd[0]));
        opnd0 = ir2_opnd_val(&current->_opnd[0]);
        /* first */
        opnd1 = ir2_opnd_val(&current->_opnd[1]);
        if (ir2_opnd_is_data(&current->_opnd[1])) {
            data[opnd0] = data[opnd1];
        } else if (ir2_opnd_is_label(&current->_opnd[1])) {
            data[opnd0] = label[opnd1];
        } else if (ir2_opnd_is_pseudo(&current->_opnd[1])) {
            data[opnd0] = opnd1;
        } else
            lsassert(0);
        /* second */
        opnd1 = ir2_opnd_val(&current->_opnd[2]);
        if (ir2_opnd_is_data(&current->_opnd[2])) {
            data[opnd0] += data[opnd1];
        } else if (ir2_opnd_is_label(&current->_opnd[2])) {
            data[opnd0] += label[opnd1];
        } else if (ir2_opnd_is_pseudo(&current->_opnd[2])) {
            data[opnd0] += opnd1;
        } else
            lsassert(0);
        ir2_remove(cur_id);

        break;
    case LISA_INST_DIFF:
        lsassert(ir2_opnd_is_data(&current->_opnd[0]));
        lsassert(ir2_opnd_is_data(&current->_opnd[1]) ||
                 ir2_opnd_is_label(&current->_opnd[1]));
        lsassert(ir2_opnd_is_data(&current->_opnd[2]) ||
                 ir2_opnd_is_label(&current->_opnd[2]));
        /* index of data */
        opnd0 = ir2_opnd_val(&current->_opnd[0]);
        /* index of label/data */
        opnd1 = ir2_opnd_val(&current->_opnd[1]);
        opnd2 = ir2_opnd_val(&current->_opnd[2]);
        if (ir2_opnd_is_data(&current->_opnd[1]))
            data[opnd0] = data[opnd1];
        else if (ir2_opnd_is_label(&current->_opnd[1]))
            data[opnd0] = label[opnd1];
        else
            lsassert(0);

        if (ir2_opnd_is_data(&current->_opnd[2]))
            data[opnd0] -= data[opnd2];
        else if (ir2_opnd_is_label(&current->_opnd[2]))
            data[opnd0] -= label[opnd2];
        else
            lsassert(0);
        /* count number, not offset */
        data[opnd0] >>= 2;

        ir2_remove(cur_id);
        break;

    case LISA_DATA_ST:
        lsassert(ir2_opnd_is_data(&current->_opnd[0]));
        lsassert(ir2_opnd_is_data(&current->_opnd[1]) ||
                 ir2_opnd_is_label(&current->_opnd[1]));
        /* index of mem address */
        opnd0 = ir2_opnd_val(&current->_opnd[0]);
        /* index of store data */
        opnd1 = ir2_opnd_val(&current->_opnd[1]);
        if (ir2_opnd_is_data(&current->_opnd[1]))
            *(uint64_t *)data[opnd0] = data[opnd1];
        else if (ir2_opnd_is_label(&current->_opnd[1]))
            *(uint64_t *)data[opnd0] = label[opnd1];
        else
            lsassert(0);

        ir2_remove(cur_id);
        break;

#ifdef CONFIG_LATX_AOT
    case LISA_DATA_ST_REL_TABLE:
        lsassert(ir2_opnd_is_pseudo(&current->_opnd[0]));
        lsassert(ir2_opnd_is_pseudo(&current->_opnd[1]));
        lsassert(ir2_opnd_is_data(&current->_opnd[2]) ||
                 ir2_opnd_is_label(&current->_opnd[2]));
        /* index of rel_table */
        opnd0 = ir2_opnd_val(&current->_opnd[0]);
        /* offset of rel_table */
        opnd1 = ir2_opnd_val(&current->_opnd[1]);
        /* index of store data */
        opnd2 = ir2_opnd_val(&current->_opnd[2]);
        if (ir2_opnd_is_data(&current->_opnd[2])) {
            *(uint32_t *)((uintptr_t)&rel_table[opnd0] + opnd1) = data[opnd2];
        } else if (ir2_opnd_is_label(&current->_opnd[2])) {
            *(uint32_t *)((uintptr_t)&rel_table[opnd0] + opnd1) = label[opnd2];
        } else {
            lsassert(0);
        }

        ir2_remove(cur_id);
        break;
#endif

    case LISA_X86_INST:
        break;
    default:
        lsassertm(0, "%d", opcode);
        break;
    }

    /* current addr may be changed by realloc */
    return lat_ctx->ir2_inst_array + cur_id;
}
