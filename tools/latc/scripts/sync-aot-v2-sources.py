#!/usr/bin/env python3
"""Synchronize generated LAT import files from the main LAT source tree."""

import argparse
import filecmp
import shutil
from pathlib import Path

from aot_v2_sources import load_generated_sources

def display(path: Path, repository_root: Path) -> str:
    try:
        return str(path.relative_to(repository_root))
    except ValueError:
        return str(path)

def main() -> int:
    default_latc_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(
        description="sync generated tools/latc/lat files from main LAT sources"
    )
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--repository-root", type=Path,
                        default=default_latc_root.parents[1])
    parser.add_argument("--latc-root", type=Path, default=default_latc_root)
    args = parser.parse_args()
    repository_root = args.repository_root.resolve()
    latc_root = args.latc_root.resolve()

    try:
        entries = load_generated_sources(repository_root, latc_root)
    except (OSError, ValueError) as error:
        print(f"AOT v2 source manifest invalid: {error}")
        return 1
    if not entries:
        print("AOT v2 source manifest has no generated files")
        return 1

    problems = []
    for entry in entries:
        source = entry.source
        generated = entry.target
        if not source.is_file():
            problems.append(("missing source", source, generated))
        elif not generated.is_file():
            problems.append(("missing generated copy", source, generated))
        elif not filecmp.cmp(source, generated, shallow=False):
            problems.append(("mismatch", source, generated))

    if args.check:
        if problems:
            print("AOT v2 source sync failed")
            for problem, source, generated in problems:
                print(
                    f"  {problem}: {display(generated, repository_root)} "
                    f"(generated from {display(source, repository_root)})"
                )
            return 1
        print(f"AOT v2 source sync OK generated={len(entries)}")
        return 0

    missing_sources = [item for item in problems if item[0] == "missing source"]
    if missing_sources:
        print("AOT v2 source sync failed")
        for _, source, generated in missing_sources:
            print(
                f"  missing source: {display(source, repository_root)} "
                f"(target {display(generated, repository_root)})"
            )
        return 1
    for entry in entries:
        source = entry.source
        generated = entry.target
        generated.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, generated)
    print(f"AOT v2 sources synchronized generated={len(entries)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
