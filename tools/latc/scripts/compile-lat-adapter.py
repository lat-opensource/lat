#!/usr/bin/env python3
"""Compile one latc adapter with the flags of a copied LAT source file."""

import argparse
import json
import shlex
import subprocess
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compdb", type=Path, required=True)
    parser.add_argument("--reference-suffix", required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    entries = json.loads(args.compdb.read_text())
    entry = next((item for item in entries
                  if item.get("file", "").endswith(args.reference_suffix)), None)
    if entry is None:
        raise SystemExit(f"no compile command for {args.reference_suffix}")
    words = shlex.split(entry["command"])
    old_source = entry["file"]
    old_output = entry["output"]
    words = [str(args.source) if word == old_source else
             str(args.output) if word == old_output else word
             for word in words]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    print(" ".join(shlex.quote(word) for word in words))
    return subprocess.run(words, cwd=entry["directory"], check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
