#!/usr/bin/env python3
"""Report auditable static coverage and code-generation savings for SHBR."""

import argparse
import json
import re
import sys
from pathlib import Path

import latx_hbr_audit


GENERATION_SITES = (
    ("tr-simd.c", "translate_addss", "SHBR_ON_32", 1, 1),
    ("tr-simd.c", "translate_addsd", "SHBR_ON_64", 1, 1),
    ("tr-simd.c", "translate_divss", "SHBR_ON_32", 1, 1),
    ("tr-simd.c", "translate_divsd", "SHBR_ON_64", 1, 1),
    ("tr-simd.c", "translate_maxss", "SHBR_ON_32", 1, 1),
    ("tr-simd.c", "translate_maxsd", "SHBR_ON_64", 1, 1),
    ("tr-simd.c", "translate_minss", "SHBR_ON_32", 1, 1),
    ("tr-simd.c", "translate_minsd", "SHBR_ON_64", 1, 1),
    ("tr-simd.c", "translate_mulss", "SHBR_ON_32", 1, 1),
    ("tr-simd.c", "translate_mulsd", "SHBR_ON_64", 1, 1),
    ("tr-simd.c", "translate_rcpss", "SHBR_ON_32", 1, 1),
    ("tr-simd.c", "translate_rsqrtss", "SHBR_ON_32", 1, 1),
    ("tr-simd.c", "translate_subss", "SHBR_ON_32", 1, 1),
    ("tr-simd.c", "translate_subsd", "SHBR_ON_64", 1, 1),
    ("tr-simd.c", "translate_sqrtss", "SHBR_ON_32", 1, 1),
    ("tr-simd.c", "translate_sqrtsd", "SHBR_ON_64", 1, 1),
    ("tr-simd.c", "translate_roundss", "SHBR_ON_32", 1, 1),
    ("tr-simd.c", "translate_roundsd", "SHBR_ON_64", 1, 1),
    ("tr-simd-mov.c", "translate_movss", "SHBR_ON_32", 1, 2),
    ("tr-simd-mov.c", "translate_movsd", "SHBR_ON_64", 1, 2),
    ("tr-simd-cvt.c", "translate_cvtsd2ss", "SHBR_ON_32", 1, 1),
    ("tr-simd-cvt.c", "translate_cvtss2sd", "SHBR_ON_64", 1, 1),
)


class MetricsError(Exception):
    pass


def function_body(source, function):
    match = re.search(
        r"\bbool\s+" + re.escape(function) + r"\s*\([^)]*\)\s*\{", source
    )
    if not match:
        raise MetricsError(f"missing translator function: {function}")
    start = match.end()
    depth = 1
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if not depth:
                return source[start:index]
    raise MetricsError(f"unterminated translator function: {function}")


def translator_gate_sites(repo_root):
    sites = set()
    translator = Path(repo_root) / "target/i386/latx/translator"
    for path in translator.glob("*.c"):
        source = path.read_text(encoding="utf-8")
        function_matches = list(re.finditer(
            r"\bbool\s+(translate_[A-Za-z0-9_]+)\s*\(", source
        ))
        for gate in ("SHBR_ON_32", "SHBR_ON_64"):
            for gate_match in re.finditer(r"\b" + gate + r"\s*\(", source):
                owners = [
                    match for match in function_matches
                    if match.start() < gate_match.start()
                ]
                if not owners:
                    raise MetricsError(
                        f"{path.name}:{gate_match.start()} has no function"
                    )
                sites.add((path.name, owners[-1].group(1), gate))
    return sites


def collect(repo_root):
    repo_root = Path(repo_root)
    audit = latx_hbr_audit.audit(repo_root)
    sources = {}
    sites = []
    for filename, function, gate, lasx_removed, lsx_removed in GENERATION_SITES:
        source = sources.setdefault(
            filename,
            (repo_root / "target/i386/latx/translator" / filename).read_text(
                encoding="utf-8"
            ),
        )
        if gate not in function_body(source, function):
            raise MetricsError(f"{function} no longer contains {gate}")
        sites.append({
            "source": filename,
            "function": function,
            "gate": gate,
            "lasx_removed": lasx_removed,
            "lsx_removed": lsx_removed,
        })
    declared_sites = {
        (site["source"], site["function"], site["gate"])
        for site in sites
    }
    actual_sites = translator_gate_sites(repo_root)
    if actual_sites != declared_sites:
        missing = sorted(actual_sites - declared_sites)
        extra = sorted(declared_sites - actual_sites)
        details = []
        if missing:
            details.append(f"untracked generation sites: {missing}")
        if extra:
            details.append(f"stale generation sites: {extra}")
        raise MetricsError("; ".join(details))
    counts = audit["counts"]
    precise = counts["opcode_specific"]
    relevant = counts["shbr_relevant"]
    by_gate = {
        gate: sum(site["gate"] == gate for site in sites)
        for gate in ("SHBR_ON_32", "SHBR_ON_64")
    }
    return {
        "coverage": {
            "precise": precise,
            "conservative": counts["conservative_unmodeled"],
            "relevant": relevant,
            "precise_percent": round(100 * precise / relevant, 2),
        },
        "generation_sites": sites,
        "generation_site_counts": by_gate,
        "minimum_removed_per_full_hit": {
            "lasx": sum(site["lasx_removed"] for site in sites),
            "lsx": sum(site["lsx_removed"] for site in sites),
        },
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = collect(args.repo_root)
    except (MetricsError, latx_hbr_audit.AuditError, OSError) as exc:
        print(f"SHBR metrics failed: {exc}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        coverage = result["coverage"]
        print("SHBR metrics")
        print(f"precise_coverage={coverage['precise']}/{coverage['relevant']} "
              f"({coverage['precise_percent']:.2f}%)")
        print(f"conservative_coverage={coverage['conservative']}")
        for gate, count in result["generation_site_counts"].items():
            print(f"generation_sites.{gate}={count}")
        for isa, count in result["minimum_removed_per_full_hit"].items():
            print(f"minimum_removed_per_full_hit.{isa}={count}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
