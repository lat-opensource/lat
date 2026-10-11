#!/usr/bin/env python3
"""Write a TBSET containing ELF symbols selected by a regular expression."""

from pathlib import Path
import re
import struct
import subprocess
import sys

from tb_key_set import CODE64, PARALLEL, write_key_set


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} X86_ELF SYMBOL_REGEX OUTPUT", file=sys.stderr)
        return 2
    source = Path(sys.argv[1])
    pattern = re.compile(sys.argv[2])
    data = source.read_bytes()
    if len(data) < 64 or data[:4] != b"\x7fELF" or data[4:6] != b"\x02\x01":
        print(f"{source}: not a little-endian ELF64 file", file=sys.stderr)
        return 1
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phentsize, phnum = struct.unpack_from("<HH", data, 54)
    load_bases = [
        struct.unpack_from("<Q", data, phoff + index * phentsize + 16)[0]
        for index in range(phnum)
        if struct.unpack_from("<I", data, phoff + index * phentsize)[0] == 1
    ]
    if not load_bases:
        print(f"{source}: no PT_LOAD segment", file=sys.stderr)
        return 1
    image_base = min(load_bases)
    records = set()
    output = subprocess.check_output(["nm", "-n", str(source)], text=True)
    for line in output.splitlines():
        fields = line.split()
        if len(fields) >= 3 and pattern.fullmatch(fields[2]):
            records.add((int(fields[0], 16) - image_base, CODE64 | PARALLEL))
    if not records:
        print(f"{source}: no symbols matched {pattern.pattern!r}", file=sys.stderr)
        return 1
    write_key_set(source, Path(sys.argv[3]), records)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
