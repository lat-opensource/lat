#!/usr/bin/env python3
"""Check GHBR's implicit MUL/IMUL/DIV/IDIV register semantics."""

import re
import sys
from pathlib import Path


def function_body(source, name):
    match = re.search(
        r"static\s+void\s+" + re.escape(name) + r"\s*\([^)]*\)\s*\{",
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
    source = (root / "target/i386/latx/optimization/hbr.c").read_text(
        encoding="utf-8"
    )
    definitions = function_body(source, "deal_hide_opnd_def")
    uses = function_body(source, "deal_hide_opnd_use")
    operand_uses = function_body(source, "use_h32")
    use_def = function_body(source, "get_gpr_use_def")
    definitions = normalize(definitions)
    uses = normalize(uses)
    operand_uses = normalize(operand_uses)
    use_def = normalize(use_def)

    require(
        r"caseWRAP\(MUL\):caseWRAP\(IMUL\):if"
        r"\(ir1_get_opnd_num\(ir1\)==1&&\(width==32\|\|width==64\)\)"
        r"\{set_def_reg\(tb,ir1,eax_index\);"
        r"set_def_reg\(tb,ir1,edx_index\);\}",
        definitions,
        "one-operand MUL/IMUL must define EAX and EDX",
    )
    require(
        r"caseWRAP\(DIV\):caseWRAP\(IDIV\):if"
        r"\(width==32\|\|width==64\)\{set_def_reg\(tb,ir1,eax_index\);"
        r"set_def_reg\(tb,ir1,edx_index\);\}",
        definitions,
        "DIV/IDIV must define EAX and EDX",
    )
    require(
        r"caseWRAP\(MUL\):caseWRAP\(IMUL\):if"
        r"\(ir1_opnd_num\(ir1\)==1&&"
        r"ir1_opnd_size\(ir1_get_opnd\(ir1,0\)\)==64\)"
        r"\{set_use_reg\(tb,ir1,eax_index\);\}",
        uses,
        "64-bit one-operand MUL/IMUL must read RAX only",
    )
    require(
        r"caseWRAP\(DIV\):caseWRAP\(IDIV\):if"
        r"\(ir1_opnd_size\(ir1_get_opnd\(ir1,0\)\)==64\)"
        r"\{set_use_reg\(tb,ir1,eax_index\);"
        r"set_use_reg\(tb,ir1,edx_index\);\}",
        uses,
        "64-bit DIV/IDIV must read RAX and RDX",
    )

    mul_block = re.search(
        r"caseWRAP\(MUL\):caseWRAP\(IMUL\):.*?break;",
        uses,
    )
    if not mul_block or "edx_index" in mul_block.group(0):
        raise AssertionError("one-operand MUL/IMUL must not read RDX")

    require(
        r"if\(has_explicit_def\)\{i=1;\}",
        operand_uses,
        "only an explicit destination may be skipped during operand-use scan",
    )
    require(
        r"boolhas_explicit_def=def_h32\(tb,ir1\);"
        r"use_h32\(tb,ir1,has_explicit_def\);",
        use_def,
        "operand-use scan must use def_h32's explicit-destination result",
    )

    print("GHBR implicit semantics: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"GHBR implicit semantics: FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
