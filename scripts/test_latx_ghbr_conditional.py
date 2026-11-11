#!/usr/bin/env python3
"""Check definite CMOV and conditional CMPXCHG high-half writes."""

import json
import sys
from pathlib import Path

from test_latx_ghbr_implicit import function_body, normalize, require


def main():
    root = Path(__file__).resolve().parents[1]
    source = (root / "target/i386/latx/optimization/hbr.c").read_text(
        encoding="utf-8"
    )
    definitions = normalize(function_body(source, "deal_hide_opnd_def"))
    explicit = normalize(function_body(source, "def_h32"))

    require(
        r"caseWRAP\(CMPXCHG\):if\(width==32\)\{"
        r"IR1_OPND\*dest=ir1_get_opnd\(ir1,0\);"
        r"set_may_def_reg\(tb,ir1,eax_index\);"
        r"if\(ir1_opnd_is_gpr\(dest\)\)\{"
        r"set_may_def_reg\(tb,ir1,ir1_opnd_base_reg_num\(dest\)\);"
        r"\}\}break;",
        definitions,
        "32-bit CMPXCHG must conditionally define both EAX and a GPR "
        "destination",
    )
    require(
        r"caseWRAP\(CMOVS\):(?:/\*.*?\*/)?"
        r"des_def_gpr\(tb,ir1\);returntrue;",
        explicit,
        "CMOV r32 must definitely define its high half on both outcomes",
    )
    inventory = json.loads((root / (
        "target/i386/latx/optimization/hbr-gpr-semantics.json"
    )).read_text(encoding="utf-8"))
    cmov = next(rule for rule in inventory["def_rules"]
                if rule["family"] == "CMOVcc")
    assert cmov["operand_bits"] == 32
    assert not cmov["use"] and not cmov["preserve"]
    assert all(rule["family"] != "CMOVcc"
               for rule in inventory["may_def_rules"])

    print("GHBR conditional semantics: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"GHBR conditional semantics: FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
