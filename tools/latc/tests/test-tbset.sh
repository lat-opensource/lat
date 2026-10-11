#!/bin/sh
set -eu

latc=$1
guest=$2
bundle=$3
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
python3 "$script_dir/make-tbset.py" "$guest" "$bundle.entry.tbset"
python3 - "$bundle.entry.tbset" "$guest" "$script_dir" <<'PY'
from pathlib import Path
import hashlib
import struct
import sys
sys.path.insert(0, sys.argv[3])
from tb_key_set import read_key_set
source = Path(sys.argv[2]).read_bytes()
entry, phoff = struct.unpack_from("<QQ", source, 24)
size, count = struct.unpack_from("<HH", source, 54)
bases = [struct.unpack_from("<Q", source, phoff + i * size + 16)[0]
         for i in range(count)
         if struct.unpack_from("<I", source, phoff + i * size)[0] == 1]
digest, sequence, records = read_key_set(Path(sys.argv[1]))
assert digest == hashlib.sha256(source).digest()
assert sequence == 0 and records == [(entry - min(bases), 3)], records
PY
tbset="$bundle.input.tbset"
python3 "$script_dir/tb_key_set.py" "$guest" "$tbset" 0x1000:0x3 0x1001:0x3

static="$bundle.static.tbset"
"$latc" emit-static-tbset "$guest" -o "$static" >"$bundle.static.out"
python3 - "$static" "$guest" "$script_dir" <<'PY'
from pathlib import Path
import hashlib
import sys

sys.path.insert(0, sys.argv[3])
from tb_key_set import read_key_set

digest, sequence, records = read_key_set(Path(sys.argv[1]))
assert digest == hashlib.sha256(Path(sys.argv[2]).read_bytes()).digest()
assert sequence == 0 and records
assert all(flags == 3 for _, flags in records)
assert len({rva for rva, _ in records}) == len(records)
PY
grep -q '^static_tb_keys=' "$bundle.static.out"

message=$("$latc" compile "$guest" -o "$bundle" --runner "$guest" \
    --tbset "$tbset" 2>&1)
case "$message" in
    *"TB set matched=0 added=2 ignored=0"*) ;;
    *) echo "unexpected TB set result: $message" >&2; exit 1 ;;
esac

outside="$bundle.outside.tbset"
python3 "$script_dir/tb_key_set.py" "$guest" "$outside" 0x5508004504:0x3
if "$latc" compile "$guest" -o "$bundle.outside" --runner "$guest" \
    --tbset "$outside" >/dev/null 2>&1; then
    echo "outside TB set address was accepted without opt-in" >&2
    exit 1
fi
message=$("$latc" compile "$guest" -o "$bundle.outside" --runner "$guest" \
    --tbset "$outside" --tbset-ignore-outside-exec 2>&1)
case "$message" in
    *"TB set matched=0 added=0 ignored=1"*) ;;
    *) echo "unexpected ignored TB set result: $message" >&2; exit 1 ;;
esac
"$latc" inspect --json "$bundle" | python3 -c \
    'import json,sys; data=json.load(sys.stdin); assert data["selected_tbs"] == 42, data'

parallel="$bundle.parallel.tbset"
python3 "$script_dir/tb_key_set.py" "$guest" "$parallel" 0x1000:0x3
message=$("$latc" compile "$guest" -o "$bundle.parallel" --runner "$guest" \
    --tbset "$parallel" 2>&1)
case "$message" in
    *"TB set matched=0 added=1 ignored=0"*) ;;
    *) echo "unexpected parallel TB set result: $message" >&2; exit 1 ;;
esac
"$latc" inspect --json "$bundle.parallel" | python3 -c \
    'import json,sys; data=json.load(sys.stdin); assert data["selected_tbs"] == 41, data'

wrong="$bundle.wrong-source.tbset"
python3 "$script_dir/tb_key_set.py" "$guest" "$wrong" 0x1000:0x3
python3 - "$wrong" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
d = bytearray(p.read_bytes())
d[16:48] = bytes(32)
p.write_bytes(d)
PY
if "$latc" compile "$guest" -o "$bundle.wrong-source" --runner "$guest" \
    --tbset "$wrong" >/dev/null 2>&1; then
    echo "TB set with wrong source digest was accepted" >&2
    exit 1
fi
headerless="$bundle.headerless.tbset"
printf '0x1000 0x1 1\n' >"$headerless"
if "$latc" compile "$guest" -o "$bundle.headerless" --runner "$guest" \
    --tbset "$headerless" >/dev/null 2>&1; then
    echo "headerless three-column input was accepted" >&2
    exit 1
fi
python3 "$script_dir/test-static-entry.py" "$latc" "$guest" \
  "$bundle.entry.tbset"

echo "test-tbset: PASS"
