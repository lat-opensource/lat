#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
tmp_base=${TMPDIR:-"$HOME/tmp"}
mkdir -p "$tmp_base"
workdir=$(mktemp -d "$tmp_base/latx-xor-jcc-test.XXXXXX")
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$workdir/xor-jcc"
guest="$workdir/xor-jcc"
mask=0x10000000

env LATX_AOT=0 LATX_INSTPTN_MASK="$mask" LATX_INSTPTN_STATS=1 \
    "$emulator" "$guest" >"$workdir/enabled.log" 2>&1
grep -Eq 'xor-jcc match=[1-9][0-9]* .*eflags-fallback=[1-9][0-9]*' \
    "$workdir/enabled.log"
grep -Eq 'reject\.non-adjacent=[1-9][0-9]*' "$workdir/enabled.log"
grep -Eq 'reject\.unsupported-cc=[1-9][0-9]*' "$workdir/enabled.log"
grep -Eq 'reject\.fault-or-helper=[1-9][0-9]*' "$workdir/enabled.log"

env LATX_AOT=0 LATX_INSTPTN_MASK=0x1 LATX_INSTPTN_STATS=1 \
    "$emulator" "$guest" >"$workdir/disabled.log" 2>&1
grep -Eq 'xor-jcc match=0 ' "$workdir/disabled.log"
grep -Eq 'reject\.disabled=[1-9][0-9]*' "$workdir/disabled.log"

env LATX_AOT=0 LATX_INSTPTN_MASK=0x200 LATX_INSTPTN_STATS=1 \
    "$emulator" "$guest" >"$workdir/xor-div.log" 2>&1

for unlink in 0 1; do
    env LATX_UNLINK="$unlink" LATX_AOT=0 LATX_INSTPTN_MASK="$mask" \
        "$emulator" "$guest" >"$workdir/unlink-$unlink.log" 2>&1
done

mkdir -p "$workdir/aot-home"
env HOME="$workdir/aot-home" LATX_AOT=1 LATX_INSTPTN_MASK="$mask" \
    "$emulator" "$guest" >"$workdir/aot-generate.log" 2>&1
attempt=0
while [ "$attempt" -lt 100 ]; do
    if find "$workdir/aot-home/.cache/latx" -type f \
        -name 'v2-*.aot2' -size +0c -print -quit 2>/dev/null | grep -q .; then
        break
    fi
    attempt=$((attempt + 1))
    sleep 0.05
done
test "$attempt" -lt 100
env HOME="$workdir/aot-home" LATX_AOT=1 LATX_INSTPTN_MASK="$mask" \
    "$emulator" "$guest" >"$workdir/aot-load.log" 2>&1

set +e
env LATX_AOT=0 LATX_INSTPTN_MASK="$mask" "$emulator" "$guest" fault \
    >"$workdir/fault-enabled.log" 2>&1
fault_enabled=$?
env LATX_AOT=0 LATX_INSTPTN_MASK=0x1 "$emulator" "$guest" fault \
    >"$workdir/fault-disabled.log" 2>&1
fault_disabled=$?
set -e
test "$fault_enabled" -ne 0
test "$fault_enabled" -eq "$fault_disabled"

echo "PASS: XOR+Jcc widths, conditions, flags, memory, XOR+DIV," \
    "rollback, fault, LOCK, JIT/TU, unlink and AOT"
