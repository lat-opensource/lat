#!/usr/bin/env python3
"""Check GHBR's conditional CMPXCHG destination semantics."""

import sys
from pathlib import Path

from test_latx_ghbr_implicit import function_body, normalize, require


def main():
    root = Path(__file__).resolve().parents[1]
    source = (root / "target/i386/latx/optimization/hbr.c").read_text(
        encoding="utf-8"
    )
    definitions = normalize(function_body(source, "deal_hide_opnd_def"))

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

    print("GHBR conditional semantics: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"GHBR conditional semantics: FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
