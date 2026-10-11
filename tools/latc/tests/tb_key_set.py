#!/usr/bin/env python3
"""Read and write the binary LAT TB key-set test format."""

import hashlib
from pathlib import Path
import struct
import sys


MAGIC = b"LATTBKS\0"
VERSION = 1
CODE64 = 1
PARALLEL = 2
HEADER = struct.Struct("<8sII32sQQ")
RECORD = struct.Struct("<QII")


def write_key_set(source: Path, output: Path, records, sequence: int = 0):
    write_key_set_digest(hashlib.sha256(source.read_bytes()).digest(), output,
                         records, sequence)


def write_key_set_digest(digest: bytes, output: Path, records,
                         sequence: int = 0):
    keys = sorted(set(records))
    for rva, flags in keys:
        if rva < 0 or flags != CODE64 | PARALLEL:
            raise ValueError(f"invalid TB key rva={rva:#x} flags={flags:#x}")
    data = bytearray(HEADER.pack(MAGIC, VERSION, HEADER.size, digest,
                                 len(keys), sequence))
    for rva, flags in keys:
        data += RECORD.pack(rva, flags, 0)
    output.write_bytes(data)


def read_key_set(path: Path):
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise ValueError("short TB key-set header")
    magic, version, header_size, digest, count, sequence = HEADER.unpack_from(data)
    if (magic != MAGIC or version != VERSION or header_size != HEADER.size or
            len(data) != HEADER.size + count * RECORD.size):
        raise ValueError("invalid TB key-set header")
    records = []
    for offset in range(HEADER.size, len(data), RECORD.size):
        rva, flags, reserved = RECORD.unpack_from(data, offset)
        if reserved or flags != CODE64 | PARALLEL:
            raise ValueError("invalid TB key")
        records.append((rva, flags))
    return digest, sequence, records


def main() -> int:
    if len(sys.argv) < 4:
        print(f"usage: {sys.argv[0]} SOURCE OUTPUT RVA:FLAGS...", file=sys.stderr)
        return 2
    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    records = []
    for value in sys.argv[3:]:
        rva, flags = value.split(":", 1)
        records.append((int(rva, 0), int(flags, 0)))
    write_key_set(source, output, records)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
