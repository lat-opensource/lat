#!/bin/sh
set -eu
if [ "$#" -ne 5 ]; then
    echo "usage: $0 LATC RUNNER RUNTIME_DIR STATIC_GUEST WORKDIR" >&2
    exit 2
fi
latc=$1
runner=$2
runtime=$3
guest=$4
work=$5
scripts=$(CDPATH= cd -- "$(dirname "$0")/../scripts" && pwd)
mkdir -p "$work/cache"
LD_LIBRARY_PATH="$runtime" LATX_AOT=0 LATX_AOT_V2=0 \
    "$runner" "$guest" >"$work/jit.out" 2>"$work/jit.err"
LD_LIBRARY_PATH="$runtime" LATC_NATIVE_OUTPUT="$work/image.native" \
    sh "$scripts/compile-aot-v2-module.sh" \
    "$latc" "$runner" "$guest" "$runtime" "$work/module.so" \
    >"$work/export.log" 2>&1
sha=$(sha256sum "$guest" | cut -d ' ' -f 1)
cp "$work/module.so" "$work/cache/$sha.so"
printf '{"version":2,"module":"%s.so"}\n' "$sha" >"$work/cache/$sha.current"
chmod 444 "$work/cache/$sha.current"
LD_LIBRARY_PATH="$runtime" LATX_AOT=0 LATX_AOT_V2=1 \
    LATX_AOT_V2_AUTOSTART=0 LATX_AOT_V2_REPORT=1 \
    LATX_AOT_V2_CACHE_DIR="$work/cache" LATC_STRICT_FILE_AOT=1 \
    "$runner" "$guest" >"$work/aot.out" 2>"$work/aot.err"
cmp "$work/jit.out" "$work/aot.out"
grep -Eq 'module=registered aot_lookups=[1-9][0-9]* jit_fallbacks=0' "$work/aot.err"
python3 - "$work/image.native" <<'PY'
import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
offset, count = struct.unpack_from("<QQ", data, 88)
targets = set()
for i in range(count):
    kind, target = struct.unpack_from("<II", data, offset + i * 40 + 16)
    if kind == 1:
        targets.add(target)
# Appended symbols retain their stable native-format indices.
assert {63, 64, 65} <= targets, targets
PY
echo 'test-aot-v2-xsave: PASS XSAVE/XSAVEOPT/XRSTOR strict roundtrip'
