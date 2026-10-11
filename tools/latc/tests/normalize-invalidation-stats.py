#!/usr/bin/env python3
"""Exclude explicitly marked loader MAP_FIXED events from operation counts."""

from pathlib import Path
import re
import sys


def normalize(text):
    loading = False
    count = 0
    output = []
    statistics = 0
    for line in text.splitlines(keepends=True):
        if line.strip() == "LATC_INVALIDATION_LOAD_BEGIN":
            if loading:
                raise ValueError("nested loader marker")
            loading = True
        elif line.strip() == "LATC_INVALIDATION_LOAD_END":
            if not loading:
                raise ValueError("unmatched loader marker")
            loading = False
        elif loading and line.startswith("latx: AOT v2 deactivated "):
            if not line.rstrip().endswith("reason=map-fixed"):
                raise ValueError("unexpected loader invalidation reason")
            count += 1
        elif line.startswith("latx: AOT v2 runtime stats "):
            if loading:
                raise ValueError("unfinished loader marker")
            statistics += 1
            for key in ("invalidated_instances", "invalidated_exec_ranges",
                        "invalidation_map_fixed"):
                pattern = rf"\b{key}=(\d+)\b"
                match = re.search(pattern, line)
                if not match or int(match[1]) < count:
                    raise ValueError(f"invalid loader baseline for {key}")
                line = re.sub(pattern, f"{key}={int(match[1]) - count}", line)
        output.append(line)
    if loading or statistics != 1:
        raise ValueError("incomplete invalidation statistics")
    return "".join(output)


if __name__ == "__main__":
    print(normalize(Path(sys.argv[1]).read_text()), end="")
