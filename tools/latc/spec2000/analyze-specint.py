#!/usr/bin/env python3
"""Analyze the twelve SPECint2000 executables with latc."""

import argparse
import json
import subprocess
import sys
from pathlib import Path


PROGRAMS = (
    ("164.gzip", "gzip_base.Of.gcc830.dyn"),
    ("175.vpr", "vpr_base.Of.gcc830.dyn"),
    ("176.gcc", "cc1_base.Of.gcc830.dyn"),
    ("181.mcf", "mcf_base.Of.gcc830.dyn"),
    ("186.crafty", "crafty_base.Of.gcc830.dyn"),
    ("197.parser", "parser_base.Of.gcc830.dyn"),
    ("252.eon", "eon_base.Of.gcc830.dyn"),
    ("253.perlbmk", "perlbmk_base.Of.gcc830.dyn"),
    ("254.gap", "gap_base.Of.gcc830.dyn"),
    ("255.vortex", "vortex_base.Of.gcc830.dyn"),
    ("256.bzip2", "bzip2_base.Of.gcc830.dyn"),
    ("300.twolf", "twolf_base.Of.gcc830.dyn"),
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--latc", required=True, type=Path)
    parser.add_argument("--bindir", required=True, type=Path)
    parser.add_argument("--output", type=Path,
                        help="write the full JSON report to this path")
    args = parser.parse_args()

    results = []
    failed = False
    for benchmark, filename in PROGRAMS:
        executable = args.bindir / filename
        if not executable.is_file():
            print(f"missing: {benchmark}: {executable}", file=sys.stderr)
            failed = True
            continue
        process = subprocess.run(
            [str(args.latc), "analyze", "--json", str(executable)],
            check=False, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if process.returncode:
            print(f"analyze failed: {benchmark}: {process.stderr.strip()}",
                  file=sys.stderr)
            failed = True
            continue
        try:
            result = json.loads(process.stdout)
        except json.JSONDecodeError as exc:
            print(f"invalid JSON: {benchmark}: {exc}", file=sys.stderr)
            failed = True
            continue
        result["benchmark"] = benchmark
        result["executable"] = filename
        results.append(result)
        if result["error_functions"] != 0:
            failed = True
        print(
            f"{benchmark:11} functions={result['functions']:6} "
            f"tbs={result['tbs']:7} edges={result['edges']:7} "
            f"open={result['open_functions']:5} "
            f"errors={result['error_functions']:3} "
            f"jump_tables={result['jump_tables']:4}"
        )

    report = {
        "suite": "SPECint2000",
        "expected_programs": len(PROGRAMS),
        "analyzed_programs": len(results),
        "passed": not failed and len(results) == len(PROGRAMS),
        "programs": results,
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in
                      ("suite", "expected_programs", "analyzed_programs",
                       "passed")}))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
