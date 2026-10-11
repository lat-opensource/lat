#!/usr/bin/env python3
"""Write a minimal TBSET containing an ELF entry point."""

from pathlib import Path
import struct
import sys

from tb_key_set import CODE64, PARALLEL, write_key_set


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} X86_ELF OUTPUT", file=sys.stderr)
        return 2
    source = Path(sys.argv[1])
    data = source.read_bytes()
    if len(data) < 64 or data[:4] != b"\x7fELF" or data[4:6] != b"\x02\x01":
        print(f"{source}: not a little-endian ELF64 file", file=sys.stderr)
        return 1
    entry, phoff = struct.unpack_from("<QQ", data, 24)
    phentsize, phnum = struct.unpack_from("<HH", data, 54)
    load_bases = [
        struct.unpack_from("<Q", data, phoff + index * phentsize + 16)[0]
        for index in range(phnum)
        if struct.unpack_from("<I", data, phoff + index * phentsize)[0] == 1
    ]
    if not load_bases or entry < min(load_bases):
        print(f"{source}: invalid ELF entry point", file=sys.stderr)
        return 1
    write_key_set(source, Path(sys.argv[2]),
                  [(entry - min(load_bases), CODE64 | PARALLEL)])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
