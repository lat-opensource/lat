#!/bin/sh
set -eu

latc=$1
bundle=$2
bad=$3

cp "$bundle" "$bad"
python3 - "$bad" <<'PY'
import os, struct, sys
path = sys.argv[1]
with open(path, "r+b") as f:
    fmt = "<8sII" + "Q" * 11 + "64s64s160s"
    size = struct.calcsize(fmt)
    f.seek(-size, os.SEEK_END)
    footer = struct.unpack(fmt, f.read(size))
    guest_offset, guest_size = footer[4], footer[5]
    assert guest_size > 0
    f.seek(guest_offset + guest_size // 2)
    b = f.read(1)
    f.seek(-1, os.SEEK_CUR)
    f.write(bytes([b[0] ^ 0x80]))
PY
if "$latc" inspect "$bad" >/dev/null 2>&1; then
    echo "corrupt bundle unexpectedly accepted" >&2
    exit 1
fi
echo "test-bundle-corruption: PASS"
