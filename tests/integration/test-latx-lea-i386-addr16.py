#!/usr/bin/env python3

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def build_guest(path):
    image_base = 0x08048000
    code_offset = 0x100
    code = bytes.fromhex(
        "b8 ef be ad de"  # mov eax, 0xdeadbeef
        "bb 34 12 ad de"  # mov ebx, 0xdead1234
        "be 20 00 00 00"  # mov esi, 0x20
        "67 8d 00"        # addr16 lea eax, [bx + si]
        "31 db"           # xor ebx, ebx
        "3d 54 12 00 00"  # cmp eax, 0x1254
        "0f 95 c3"        # setne bl
        "b8 01 00 00 00"  # mov eax, 1 (exit)
        "cd 80"           # int 0x80
    )
    file_size = code_offset + len(code)
    ident = b"\x7fELF\x01\x01\x01" + b"\0" * 9
    elf_header = struct.pack(
        "<16sHHIIIIIHHHHHH",
        ident, 2, 3, 1, image_base + code_offset, 52, 0, 0,
        52, 32, 1, 0, 0, 0,
    )
    program_header = struct.pack(
        "<IIIIIIII",
        1, 0, image_base, image_base, file_size, file_size, 5, 0x1000,
    )
    path.write_bytes(
        elf_header + program_header +
        b"\0" * (code_offset - len(elf_header) - len(program_header)) + code
    )
    path.chmod(0o755)


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} LATX-I386", file=sys.stderr)
        return 2

    emulator = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as workdir:
        guest = Path(workdir) / "lea-i386-addr16"
        build_guest(guest)
        env = os.environ.copy()
        env.update(LATX_AOT="0", LATX_KZT="0")
        result = subprocess.run([emulator, guest], env=env, check=False)

    if result.returncode != 0:
        print(
            "FAIL: i386 addr16 LEA did not zero-extend the 16-bit "
            f"effective address (exit {result.returncode})",
            file=sys.stderr,
        )
        return 1

    print("PASS: i386 16-bit address-size LEA")
    return 0


if __name__ == "__main__":
    sys.exit(main())
