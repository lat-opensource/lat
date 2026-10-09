#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build the IPC test seam from the current production source.

Including all of syscall.c also includes unrelated function-pointer tables.
ASan's global registration keeps those tables (and their missing dependencies)
alive despite --gc-sections. Select the IPC code and its real dependencies
instead, without disabling instrumentation or maintaining copied functions.
"""

import json
from pathlib import Path
import sys


def extract(source, filename, begin, end):
    if source.count(begin) != 1 or source.count(end) != 1:
        raise ValueError(f"syscall test boundary changed: {begin!r}, {end!r}")
    first = source.index(begin)
    last = source.index(end)
    if last <= first:
        raise ValueError(f"syscall test boundaries out of order: {begin!r}")
    line = source.count("\n", 0, first) + 1
    return f"#line {line} {json.dumps(str(filename))}\n" + source[first:last]


def main():
    filename = Path(sys.argv[1])
    source = filename.read_text(encoding="utf-8")
    sections = [
        ("#define ERRNO_TABLE_SIZE", "const char *target_strerror(int err)"),
        ("#define N_SHM_REGIONS", "static abi_ulong target_brk;"),
        ("#ifndef TARGET_FORCE_SHMLBA",
         "#ifdef TARGET_NR_ipc\n/* ??? This only works with linear mappings."),
        ("typedef struct GuestVmaName {", "void guest_vma_name_remap("),
        ("static abi_long guest_mdwe_mmap(CPUState *cpu, abi_ulong len, "
         "int prot)\n{",
         "\n/* Called with the linux-user mmap lock held. */\n"
         "static abi_long guest_mdwe_mprotect("),
    ]
    output = "/* Generated from syscall.c; do not edit. */\n"
    output += "\n".join(extract(source, filename, begin, end)
                        for begin, end in sections)
    Path(sys.argv[2]).write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
