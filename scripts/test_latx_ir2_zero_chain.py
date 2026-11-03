#!/usr/bin/env python3
"""Check the conservative IR2 zero-high-chain cleanup."""

import re
import sys
from pathlib import Path


def function_body(source, name):
    match = re.search(
        r"(?:static\s+)?(?:bool|void)\s+" + re.escape(name) +
        r"\s*\([^)]*\)\s*\{",
        source,
    )
    if not match:
        raise AssertionError(f"missing function: {name}")
    start = match.end()
    depth = 1
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index]
    raise AssertionError(f"unterminated function: {name}")


def normalize(source):
    return re.sub(r"\s+", "", source)


def require(pattern, source, message):
    if not re.search(pattern, source):
        raise AssertionError(message)


def main():
    root = Path(__file__).resolve().parents[1]
    source_path = (
        Path(sys.argv[1]) if len(sys.argv) == 2 else
        root / "target/i386/latx/optimization/ir2-optimization.c"
    )
    source = source_path.read_text(encoding="utf-8")
    optimize = normalize(function_body(source, "tr_ir2_optimize"))
    recognize = normalize(function_body(source, "ir2_is_self_zero_extend"))
    cleanup = normalize(function_body(source, "ir2_opt_zero_high_chain"))

    require(
        r"ir2_opt_zero_high_chain\(tb\)",
        optimize,
        "tr_ir2_optimize must run the zero-high-chain cleanup",
    )
    require(
        r"returnir2_opcode\(ir2\)==LISA_BSTRPICK_D&&"
        r"ir2->op_count>=4&&"
        r"ir2_opnd_cmp\(&ir2->_opnd\[0\],&ir2->_opnd\[1\]\)&&"
        r"ir2_opnd_imm\(&ir2->_opnd\[2\]\)==31&&"
        r"ir2_opnd_imm\(&ir2->_opnd\[3\]\)==0;",
        recognize,
        "missing self zero-extension recognition",
    )
    require(
        r"if\(ir2_is_self_zero_extend\(curr\)\)\{.*"
        r"if\(ir2_gpr_is_known_zero\(known_zero,&curr->_opnd\[0\]\)\)\{"
        r"ir2_remove\(ir2_get_id\(curr\)\);.*"
        r"\}else\{ir2_gpr_set_known_zero"
        r"\(&known_zero,&curr->_opnd\[0\],true\);\}"
        r"curr=next;continue;\}",
        cleanup,
        "self zero-extension must only be removed for a known-zero GPR",
    )
    require(
        r"if\(op==LISA_X86_INST\|\|op==LISA_NOP\)\{"
        r"curr=next;continue;\}",
        cleanup,
        "X86 markers and NOPs must preserve known-zero state",
    )
    require(
        r"default:known_zero=0;break;",
        cleanup,
        "unknown opcodes must clear known-zero state",
    )
    require(
        r"caseLISA_LD_BU:caseLISA_LD_HU:caseLISA_LD_WU:caseLISA_ANDI:"
        r"ir2_gpr_set_known_zero\(&known_zero,&curr->_opnd\[0\],true\);break;",
        cleanup,
        "zero-extending loads and ANDI must mark the destination",
    )
    require(
        r"caseLISA_ORI:caseLISA_XORI:caseLISA_MOV64:"
        r"ir2_gpr_set_known_zero\(&known_zero,&curr->_opnd\[0\],"
        r"ir2_gpr_is_known_zero\(known_zero,&curr->_opnd\[1\]\)\);break;",
        cleanup,
        "copy-like operations must transfer known-zero state",
    )
    require(
        r"caseLISA_OR:caseLISA_XOR:"
        r"ir2_gpr_set_known_zero\(&known_zero,&curr->_opnd\[0\],"
        r"ir2_gpr_is_known_zero\(known_zero,&curr->_opnd\[1\]\)&&"
        r"ir2_gpr_is_known_zero\(known_zero,&curr->_opnd\[2\]\)\);break;",
        cleanup,
        "OR and XOR require both inputs to have zero high halves",
    )
    require(
        r"caseLISA_AND:"
        r"ir2_gpr_set_known_zero\(&known_zero,&curr->_opnd\[0\],"
        r"ir2_gpr_is_known_zero\(known_zero,&curr->_opnd\[1\]\)\|\|"
        r"ir2_gpr_is_known_zero\(known_zero,&curr->_opnd\[2\]\)\);break;",
        cleanup,
        "AND requires at least one input to have a zero high half",
    )
    require(
        r"caseLISA_SRLI_D:"
        r"ir2_gpr_set_known_zero\(&known_zero,&curr->_opnd\[0\],"
        r"ir2_opnd_imm\(&curr->_opnd\[2\]\)>=32\|\|"
        r"ir2_gpr_is_known_zero\(known_zero,&curr->_opnd\[1\]\)\);break;",
        cleanup,
        "SRLI_D must model large shifts and known-zero inputs",
    )
    require(
        r"caseLISA_ADDI_D:if\(ir2_opnd_imm\(&curr->_opnd\[2\]\)!=0\)\{"
        r"known_zero=0;\}else\{ir2_gpr_set_known_zero"
        r"\(&known_zero,&curr->_opnd\[0\],"
        r"ir2_gpr_is_known_zero\(known_zero,&curr->_opnd\[1\]\)\);\}break;",
        cleanup,
        "ADDI_D must only transfer state for an exact register copy",
    )

    print("LATX IR2 zero-high chain: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"LATX IR2 zero-high chain: FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
