#!/usr/bin/env python3
"""Keep an executable ELF entry even without a function symbol."""
from pathlib import Path
import struct
import subprocess
import sys
from tb_key_set import read_key_set

latc, source, output = sys.argv[1:]
data = bytearray(Path(source).read_bytes())
entry = struct.unpack_from('<Q', data, 24)[0]
phoff, shoff = struct.unpack_from('<QQ', data, 32)
phentsize, phnum, shentsize, shnum = struct.unpack_from('<HHHH', data, 54)
base = min(struct.unpack_from('<Q', data, phoff + i * phentsize + 16)[0]
           for i in range(phnum)
           if struct.unpack_from('<I', data, phoff + i * phentsize)[0] == 1)
changed = 0
for i in range(shnum):
    sh = shoff + i * shentsize
    if struct.unpack_from('<I', data, sh + 4)[0] not in (2, 11):
        continue
    offset, size = struct.unpack_from('<QQ', data, sh + 24)
    stride = struct.unpack_from('<Q', data, sh + 56)[0]
    for sym in range(offset, offset + size, stride):
        value = struct.unpack_from('<Q', data, sym + 8)[0]
        if data[sym + 4] & 15 == 2 and value == entry:
            data[sym + 4] &= 0xf0
            changed += 1
assert changed, 'fixture needs a function symbol at its ELF entry'
guest = Path(output + '.elf')
guest.write_bytes(data)
subprocess.run([latc, 'emit-static-tbset', str(guest), '-o', output], check=True)
assert (entry - base, 3) in read_key_set(Path(output))[2], 'missing symbol-less ELF entry'

# An entry in the read-only ELF header must not become executable code.
struct.pack_into('<Q', data, 24, base)
guest.write_bytes(data)
subprocess.run([latc, 'emit-static-tbset', str(guest), '-o', output], check=True)
assert (0, 3) not in read_key_set(Path(output))[2], 'non-executable entry accepted'
print('test-static-entry: PASS')
