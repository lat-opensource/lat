#!/usr/bin/env python3
# coding=utf-8

# @file convert.py
# @author yuerengan <y347812075@163.com>
# @brief Add python script to generate instruction related file

import re
import json

def convertName(name = str):
    convert_name = "LISA_" + name.replace(".", "_").upper()
    return convert_name

def convertToFormatTable(name = str, opcode = str, opnd = list):
    convert_name = convertName(name)
    opnd_str = ""
    for i in range(len(opnd)):
        tmp = opnd[i].upper()
        if i < len(opnd) - 1:
            opnd_str += tmp + ", "
        else:
            opnd_str += tmp

    sstr = "{{{}, {}, {{{}}}}},".format(convert_name, opcode, opnd_str)
    return sstr

def instName(name = str):
    return "\"" + name + "\",\n"

def isJmpInst(name = str):
    jmp_inst_list = ["beq", "bne", "blt", "bltu", "bge", \
            "bgeu", "beqz", "bnez", "b", "bl", \
            "bceqz", "bcnez"]
    if name in jmp_inst_list:
        return True
    else:
        return False

def isTopInst(name = str):
    top_inst_list = ["x86settm", "x86clrtm", "x86mttop", "x86mftop", \
                "x86dectop", "x86inctop"]
    if name in top_inst_list:
        return True
    else:
        return False

def isMowgr2fcsr(name = str):
    if name == "movgr2fcsr":
        return True
    else:
        return False

def isImmLoad(name = str):
    imm_load_list = ["lu12i.w", "lu32i.d", "lu52i.d"]
    if name in imm_load_list:
        return True
    else:
        return False

def isBstrInst(name = str):
    bstr_inst_list = ["bstrins.w", "bstrins.d", "bstrpick.w", "bstrpick.d"]
    if name in bstr_inst_list:
        return True
    else:
        return False

def isAmInst(name = str):
    am_inst_list = ["amcas.b", "amcas.h", "amcas.w", "amcas.d", \
            "amcas.db.b", "amcas.db.h", "amcas.db.w", "amcas.db.d", \
            "amswap.w", "amswap.d", "amswap.db.w", "amswap.db.d", \
            "amadd.w", "amadd.d", "amadd.db.w", "amadd.db.d", \
            "amand.w", "amand.d", "amand.db.w", "amand.db.d", \
            "amor.w", "amor.d", "amor.db.w", "amor.db.d", \
            "amxor.w", "amxor.d", "amxor.db.w", "amxor.db.d", \
            "ammax.w", "ammax.d", "ammax.db.w", "ammax.db.d", \
            "ammin.w", "ammin.d", "ammin.db.w", "ammin.db.d", \
            "ammax.wu", "ammax.du", "ammax.db.wu", "ammax.db.du", \
            "ammin.wu", "ammin.du", "ammin.db.wu", "ammin.db.du", \
            "amswap.b", "amswap.h", "amswap.db.b", "amswap.db.h", \
            "amadd.b", "amadd.h", "amadd.db.b", "amadd.db.h"]
    if name in am_inst_list:
        return True
    else:
        return False

def isCsrInst(name = str):
    csr_inst_list = ["csrxchg", "gcsrxchg"]
    if name in csr_inst_list:
        return True
    else:
        return False

def checkOpndConstraints(name = str, opnd = list):
    if isBstrInst(name):
        return "    lsassertm({} >= {}, BSTRINST);\n".format(opnd[2], opnd[3])
    if isCsrInst(name):
        return "    lsassertm((ir2_opnd_base_reg_num(&rj) != 0) &&\n" \
             + "             (ir2_opnd_base_reg_num(&rj) != 1) ,CSRINST);\n"
    if isAmInst(name):
        return "    if(ir2_opnd_base_reg_num(&rd) != 0)\n" \
             + "        lsassertm((ir2_opnd_base_reg_num(&rd) != ir2_opnd_base_reg_num(&rj)) &&\n" \
             + "                  (ir2_opnd_base_reg_num(&rd) != ir2_opnd_base_reg_num(&rk)), AMINST);\n"

    return ""

def generatingCheck(name = str, opnd = str):
    if re.search("imm", opnd) and not isImmLoad(name):
        if re.search("si", opnd):
            len = re.findall(r"\d+", opnd)[-1]
            return "    lsassert({} == sextract64({}, 0, {}));\n".format(opnd, opnd, len)
        if re.search("(sa)|(ra)", opnd):
            len = re.findall(r"\d+", opnd)[-1]
            return "    lsassert({} >> {} == 0);\n".format(opnd, len)

    if re.search("gpr", opnd):
        return "    lsassert(ir2_opnd_is_ireg(&{}));\n".format(opnd.split("_")[-1])
    if re.search("fpr", opnd):
        return "    lsassert(ir2_opnd_is_freg(&{}));\n".format(opnd.split("_")[-1])

    return ""

def generatingFunctionBody(name = str, opcode = str, opnd = list):
    convert_name = convertName(name)
    imm_num = 0
    if isMowgr2fcsr(name):
        returnstr  = "    if (option_softfpu && fcsrl._reg_num == 0) {\n" \
                   + "        la_bstrins_w(rj, zero_ir2_opnd, 6, 6);\n" \
                   + "    }\n" \
                   + "    IR2_INST *pir2 = ir2_allocate();\n" \
                   + "    pir2->_opcode = {};\n".format(convert_name)
    else:
        returnstr = "    IR2_INST *pir2 = ir2_allocate();\n" \
                  + "    pir2->_opcode = {};\n".format(convert_name)
    if len(opnd) == 0:
        returnstr += "    pir2->op_count = 1;\n" \
                   + "    IR2_OPND ir2_opnd_none = ir2_opnd_new_none();\n" \
                   + "    pir2->_opnd[0] = ir2_opnd_none;\n"
    else:
        returnstr += "    pir2->op_count = {};\n".format(len(opnd))
    # Generate each opnd
    for i in range(len(opnd)):
        if re.search("imm", opnd[i]) and not isJmpInst(name):
            if re.search("si14", opnd[i]):
                returnstr += "    {} = {} >> 2;\n".format(opnd[i], opnd[i])
            returnstr += "    IR2_OPND op{}".format(imm_num) \
                       + " = create_immh_opnd({});\n".format(opnd[i]) \
                       + "    pir2->_opnd[{}] = op{};\n".format(i, imm_num)
            imm_num += 1
        else:
            returnstr += "    pir2->_opnd[{}] = {};\n".format(i, opnd[i].split("_")[-1])
        # Genatrate check code
        returnstr += generatingCheck(name, opnd[i])

    returnstr += checkOpndConstraints(name, opnd)

    returnstr += "    return pir2;\n"
    return returnstr

def convertToConstructor(name = str, opcode = str, opnd = list, header_list = list):
    returnstr = " "
    convert_name = name.replace(".", "_")
    parameter = ''
    parameter_to = ''
    for one in opnd:
        if re.search("imm", one) and not isJmpInst(name):
            parameter += "int {}, ".format(one)
            parameter_to += "{}, ".format(one)
        else:
            parameter += "IR2_OPND {}, ".format(one.split("_")[-1])
            parameter_to += "{}, ".format(one.split("_")[-1])
    if parameter != "":
        parameter = parameter[0 : -2]
        parameter_to = parameter_to[0 : -2]
    else :
        parameter = "void"
    func_body = generatingFunctionBody(name, opcode, opnd)
    function  = "inline __attribute__ ((always_inline))\n"
    function += "IR2_INST *generate_{}({}) {{\n{}}}\n\n".format(convert_name, parameter, func_body)
    function += "IR2_INST *la_{}({}) {{\n".format(convert_name, parameter)
    if isTopInst(name):
        function += "    if (option_softfpu) {\n" + \
                    "        return NULL;\n" + \
                    "    }\n"
    function += "    IR2_INST *pir2 = generate_{}({});\n".format(convert_name, parameter_to) + \
                "    ir2_append(pir2);\n" + \
                "    return pir2;\n" + \
                "}"
    func_header  = "IR2_INST *generate_{}({});\n".format(convert_name, parameter)
    func_header += "IR2_INST *la_{}({});\n".format(convert_name, parameter)
    header_list.append(func_header)
    return function

def convertToConstructorForPseudo(name = str, header_list = list):
    function = ""
    if name == "label":
        function = convertToConstructor("label", "0x0", ["opd_rd"], header_list)
        return function
    elif name == "x86_inst":
        function = convertToConstructor("x86_inst", "0x0", ["opd_rd"], header_list)
        return function
    elif name == "ill":
        function = convertToConstructor("ill", "-1", [], header_list)
        return function
    elif name == "mov64":
        function  = "inline __attribute__ ((always_inline))\n"
        function += "IR2_INST *la_{}".format(name) \
                    + "(IR2_OPND dest, IR2_OPND src) {\n" \
                    + "    if (ir2_opnd_cmp(&dest, &src)) {\n" \
                    + "        return NULL;\n" \
                    + "    } else {\n" \
                    + "        return la_or(dest, src, zero_ir2_opnd);\n" \
                    + "    }\n" \
                    + "}"
        func_header = "IR2_INST *la_{}(IR2_OPND dest, IR2_OPND src);\n".format(name)
    elif name == "mov32_sx":
        function  = "inline __attribute__ ((always_inline))\n"
        function += "IR2_INST *la_{}".format(name) \
                    + "(IR2_OPND dest, IR2_OPND src) {\n" \
                    + "    return la_slli_w(dest, src, 0);\n" \
                    + "}"
        func_header = "IR2_INST *la_{}(IR2_OPND dest, IR2_OPND src);\n".format(name)
    elif name == "mov32_zx":
        function  = "inline __attribute__ ((always_inline))\n"
        function += "IR2_INST *la_{}".format(name) \
                    + "(IR2_OPND dest, IR2_OPND src) {\n" \
                    + "    return la_bstrpick_d(dest, src, 31, 0);\n" \
                    + "}"
        func_header = "IR2_INST *la_{}(IR2_OPND dest, IR2_OPND src);\n".format(name)
    elif name == "add":
        function  = "inline __attribute__ ((always_inline))\n"
        function += "IR2_INST *la_{}".format(name) \
                    + "(IR2_OPND op0, IR2_OPND op1, IR2_OPND op2) {\n" \
                    + "#ifndef TARGET_X86_64\n" \
                    + "    return la_add_w(op0, op1, op2);\n" \
                    + "#else\n" \
                    + "    if (CODEIS64) {\n" \
                    + "        return la_add_d(op0, op1, op2);\n" \
                    + "    } else {\n" \
                    + "        return la_add_w(op0, op1, op2);\n" \
                    + "    }\n" \
                    + "#endif\n" \
                    + "}"
        func_header = "IR2_INST *la_{}".format(name) \
                    + "(IR2_OPND op0, IR2_OPND op1, IR2_OPND op2);\n"
    elif name == "sub":
        function  = "inline __attribute__ ((always_inline))\n"
        function += "IR2_INST *la_{}".format(name) \
                    + "(IR2_OPND op0, IR2_OPND op1, IR2_OPND op2) {\n" \
                    + "#ifndef TARGET_X86_64\n" \
                    + "    return la_sub_w(op0, op1, op2);\n" \
                    + "#else\n" \
                    + "    if (CODEIS64) {\n" \
                    + "        return la_sub_d(op0, op1, op2);\n" \
                    + "    } else {\n" \
                    + "        return la_sub_w(op0, op1, op2);\n" \
                    + "    }\n" \
                    + "#endif\n" \
                    + "}"
        func_header = "IR2_INST *la_{}".format(name) \
                    + "(IR2_OPND op0, IR2_OPND op1, IR2_OPND op2);\n"
    elif name == "addi_addrx":
        function  = "inline __attribute__ ((always_inline))\n"
        function += "IR2_INST *la_{}".format(name) \
                    + "(IR2_OPND op0, IR2_OPND op1, int imm) {\n" \
                    + "#ifndef TARGET_X86_64\n" \
                    + "    return la_addi_w(op0, op1, imm);\n" \
                    + "#else\n" \
                    + "    if (CODEIS64) {\n" \
                    + "        return la_addi_d(op0, op1, imm);\n" \
                    + "    } else {\n" \
                    + "        return la_addi_w(op0, op1, imm);\n" \
                    + "    }\n" \
                    + "#endif\n" \
                    + "}"
        func_header = "IR2_INST *la_{}(IR2_OPND op0, IR2_OPND op1, int imm);\n".format(name)
    elif name == "load_addrx":
        function  = "inline __attribute__ ((always_inline))\n"
        function += "IR2_INST *la_{}".format(name) \
                    + "(IR2_OPND op0, IR2_OPND op1, int imm) {\n" \
                    + "#ifndef TARGET_X86_64\n" \
                    + "    return la_ld_wu(op0, op1, imm);\n" \
                    + "#else\n" \
                    + "    if (CODEIS64) {\n" \
                    + "        return la_ld_d(op0, op1, imm);\n" \
                    + "    } else {\n" \
                    + "        return la_ld_wu(op0, op1, imm);\n" \
                    + "    }\n" \
                    + "#endif\n" \
                    + "}"
        func_header = "IR2_INST *la_{}(IR2_OPND op0, IR2_OPND op1, int imm);\n".format(name)
    elif name == "store_addrx":
        function  = "inline __attribute__ ((always_inline))\n"
        function += "IR2_INST *la_{}".format(name) \
                    + "(IR2_OPND op0, IR2_OPND op1, int imm) {\n" \
                    + "#ifndef TARGET_X86_64\n" \
                    + "    return la_st_w(op0, op1, imm);\n" \
                    + "#else\n" \
                    + "    if (CODEIS64) {\n" \
                    + "        return la_st_d(op0, op1, imm);\n" \
                    + "    } else {\n" \
                    + "        return la_st_w(op0, op1, imm);\n" \
                    + "    }\n" \
                    + "#endif\n" \
                    + "}"
        func_header = "IR2_INST *la_{}(IR2_OPND op0, IR2_OPND op1, int imm);\n".format(name)
    elif name == "nop":
        function  = "inline __attribute__ ((always_inline))\n"
        function += "IR2_INST *generate_nop(void) {\n" \
                    + "    return generate_andi(zero_ir2_opnd, zero_ir2_opnd, 0);\n" \
                    + "}\n\n"
        function += "inline __attribute__ ((always_inline))\n"
        function += "IR2_INST *la_nop(void) {\n" \
                    + "    return la_andi(zero_ir2_opnd, zero_ir2_opnd, 0);\n" \
                    + "}"
        func_header  = "IR2_INST *generate_nop(void);\n"
        func_header += "IR2_INST *la_nop(void);\n"
    header_list.append(func_header)
    return function


def main():
    # pseudo insts which also not generate function like 'la_xxx()'
    pseudoinstruction_nofunction_list = [
        "accl", "align", "code", "far_jump", "data_li", "data_add", "inst_diff",
        "data_st", "data_st_rel_table", "profile", "pcaddi_relocate",
        "jump_epilogue", "pseudo_end"]
    pseudoinstruction_list = ["label", "x86_inst",
                              "mov64", "mov32_sx", "mov32_zx", "add", "sub",
                              "addi_addrx", "load_addrx", "store_addrx", "ill", "nop"
                              ] + pseudoinstruction_nofunction_list

    input_json = open("inst_template.json", "r",  encoding = 'utf-8')
    format_table = open("include/format-table.h", "w",  encoding = 'utf-8')
    ir2_name = open("include/ir2-name.h", "w",  encoding = 'utf-8')
    ir2_opcode = open("include/ir2-opcode.h", "w",  encoding = 'utf-8')
    la_append = open("ir2/la-append.c", "w",  encoding = 'utf-8')
    la_append_header = open("include/la-append.h", "w",  encoding = 'utf-8')
    inst_json = json.load(input_json)
    table_list = []
    ir2_name_list = []
    ir2_opcode_list = []
    func_list = []
    header_list = []

    annotation_str = "/* this file is produced by convert.py */"
    table_list.append(annotation_str + "\n")
    ir2_name_list.append(annotation_str + "\n")
    func_list.append(annotation_str + "\n")
    func_list.append('#include "la-append.h"' + "\n")
    func_list.append('#include "reg-alloc.h"' + "\n")
    func_list.append('#include "latx-options.h"' + "\n")
    func_list.append('#include "qemu/bitops.h"' + "\n")

    func_list.append("\n" + '#define AMINST ')
    func_list.append('"\\nError: atomic memory operations insns require rd != rj && rd != rk when rd isn\'t r0\\n"')
    func_list.append("\n" + '#define CSRINST ')
    func_list.append('"\\nError: g?csrxchg require rj != r0 && rj != r1\\n"')
    func_list.append("\n" + '#define BSTRINST ')
    func_list.append('"\\nError: bstr(ins|pick).[wd] require msbd >= lsbd\\n"' + "\n\n")

    header_list.append(annotation_str + "\n")
    header_list.append("#ifndef _LA_APPEND_H_" + "\n")
    header_list.append("#define _LA_APPEND_H_" + "\n")
    header_list.append('#include "common.h"' + "\n")
    header_list.append('#include "ir2.h"' + "\n")
    for one in inst_json.keys():
        table_list.append(convertToFormatTable(one, \
                inst_json[one]["opcode"], inst_json[one]["opnd"]) + "\n")
        if one == "label":
            ir2_name_list.append(instName("   -->"))
        elif one == "x86_inst":
            ir2_name_list.append(instName("-------"))
        else:
            ir2_name_list.append(instName(one))

        if one == "invalid":
            ir2_opcode_list.append(convertName(one) + " = 128,\n")
        else:
            ir2_opcode_list.append(convertName(one) + ",\n")

        if one != "invalid" and one != "ending" and one not in pseudoinstruction_nofunction_list:
            if one in pseudoinstruction_list:
                func_list.append(convertToConstructorForPseudo(one, header_list) + "\n\n")
            else:
                func_list.append(convertToConstructor \
                    (one, inst_json[one]["opcode"], \
                    inst_json[one]["opnd"], header_list) + "\n\n")

    header_list.append("#endif")
    la_append_header.writelines(header_list)
    format_table.writelines(table_list)
    ir2_name.writelines(ir2_name_list)
    ir2_opcode.writelines(ir2_opcode_list)
    la_append.writelines(func_list)
    print("Generate ir2 backend successfully!")
if __name__ == "__main__":
    main()
