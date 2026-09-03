#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
tmp_base=${TMPDIR:-"$HOME/tmp"}
mkdir -p "$tmp_base"
workdir=$(mktemp -d "$tmp_base/latx-jcc-conditions-test.XXXXXX")
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
    -Wl,--build-id=none "$source_file" -o "$workdir/jcc-conditions"
guest="$workdir/jcc-conditions"

# Existing CMP_JCC (0), SUB_JCC (21), AND_JCC (25), plus SHR_JE (34).
conditions_mask=0x402200001
positive='[1-9][0-9]*'
reject_re="reject\.non-adjacent=$positive"
reject_re="$reject_re|reject\.zero-shift-count=$positive"
env LATX_AOT=0 LATX_INSTPTN_MASK="$conditions_mask" LATX_INSTPTN_STATS=1 \
    "$emulator" "$guest" >"$workdir/enabled.log" 2>&1
for pattern in cmp-jcc sub-jcc and-jcc shr-je; do
    grep -Eq "$pattern match=[1-9][0-9]* .*eflags-fallback=[1-9][0-9]*" \
        "$workdir/enabled.log"
    grep -A1 "$pattern match=" "$workdir/enabled.log" | \
        grep -Eq "$reject_re"
done

for entry in \
    '0x1 cmp-jcc' \
    '0x200000 sub-jcc' \
    '0x2000000 and-jcc' \
    '0x400000000 shr-je'; do
    set -- $entry
    mask=$1
    pattern=$2
    env LATX_AOT=0 LATX_INSTPTN_MASK="$mask" LATX_INSTPTN_STATS=1 \
        "$emulator" "$guest" >"$workdir/$pattern-only.log" 2>&1
    grep -Eq "$pattern match=[1-9][0-9]* " "$workdir/$pattern-only.log"
done

env LATX_AOT=0 LATX_INSTPTN_MASK=0x2 LATX_INSTPTN_STATS=1 \
    "$emulator" "$guest" >"$workdir/disabled.log" 2>&1
for pattern in cmp-jcc sub-jcc and-jcc shr-je; do
    grep -Eq "$pattern match=0 " "$workdir/disabled.log"
done
grep -Eq 'reject\.disabled=[1-9][0-9]*' "$workdir/disabled.log"

mkdir -p "$workdir/aot-home"
env HOME="$workdir/aot-home" LATX_AOT=1 \
    LATX_INSTPTN_MASK="$conditions_mask" \
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
env HOME="$workdir/aot-home" LATX_AOT=1 \
    LATX_INSTPTN_MASK="$conditions_mask" \
    "$emulator" "$guest" >"$workdir/aot-load.log" 2>&1

for args in 'cmp' 'cmp sub' 'cmp sub and'; do
    set +e
    env LATX_AOT=0 LATX_INSTPTN_MASK="$conditions_mask" \
        "$emulator" "$guest" $args >"$workdir/fault-enabled.log" 2>&1
    fault_enabled=$?
    env LATX_AOT=0 LATX_INSTPTN_MASK=0x2 \
        "$emulator" "$guest" $args >"$workdir/fault-disabled.log" 2>&1
    fault_disabled=$?
    set -e
    test "$fault_enabled" -ne 0
    test "$fault_enabled" -eq "$fault_disabled"
done

echo "PASS: expanded Jcc widths, overflow, flags, memory, count=0," \
    "LOCK, rollback, fault, JIT/TU and AOT"
