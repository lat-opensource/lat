#!/usr/bin/env python3
"""Check the opt-in GHBR hit/refusal counters."""

import re
import sys
from pathlib import Path


def require(pattern, source, message):
    if not re.search(pattern, source, re.S):
        raise AssertionError(message)


def main():
    root = Path(__file__).resolve().parents[1]
    hbr_source = (root / "target/i386/latx/optimization/hbr.c").read_text(
        encoding="utf-8"
    )
    ir2_source = (
        root / "target/i386/latx/optimization/ir2-optimization.c"
    ).read_text(encoding="utf-8")

    for source, label in ((hbr_source, "GHBR"), (ir2_source, "IR2")):
        require(
            r'getenv\("LATX_GHBR_STATS"\).*'
            r'value\s*&&\s*!strcmp\(value,\s*"1"\)',
            source,
            f"{label} counters must require LATX_GHBR_STATS=1",
        )
    for field in ("modeled_def_instructions", "candidates", "hits",
                  "external_edges"):
        require(
            re.escape(field),
            hbr_source,
            f"missing GHBR counter {field}",
        )
    require(
        r"\[GHBR\]\[TU\].*external_edges",
        hbr_source,
        "missing aggregate GHBR log line",
    )
    require(
        r"successor &&.*hbr_in_tu_successor",
        hbr_source,
        "external edge counter must exclude null successors",
    )
    require(
        r"count_ghbr_external_edges\(tb_list, tb_num_in_tu, stats\);\s*"
        r"while\(continue_flag\)",
        hbr_source,
        "external edge counter must run outside fixed-point iteration",
    )

    print("LATX GHBR stats: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"LATX GHBR stats: FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
