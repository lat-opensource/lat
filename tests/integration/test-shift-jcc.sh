#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
tmp_base=${TMPDIR:-"$HOME/tmp"}
mkdir -p "$tmp_base"
workdir=$(mktemp -d "$tmp_base/latx-shift-jcc-test.XXXXXX")
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
    -Wl,--build-id=none "$source_file" -o "$workdir/shift-jcc"
guest="$workdir/shift-jcc"

# SAR_JCC (bit 29), legacy SHR_JCC (bit 24), and SHR_JE (bit 34).
shift_mask=0x421000000
env LATX_AOT=0 LATX_INSTPTN_MASK="$shift_mask" LATX_INSTPTN_STATS=1 \
    "$emulator" "$guest" >"$workdir/enabled.log" 2>&1
for pattern in sar-jcc shr-jcc shr-je; do
    grep -Eq "$pattern match=[1-9][0-9]* .*eflags-fallback=[1-9][0-9]*" \
        "$workdir/enabled.log"
done
grep -Eq 'reject\.zero-shift-count=[1-9][0-9]*' "$workdir/enabled.log"
grep -A1 'shr-je match=' "$workdir/enabled.log" | \
    grep -Eq 'reject\.zero-shift-count=[1-9][0-9]*'
grep -Eq 'reject\.unsupported-cc=[1-9][0-9]*' "$workdir/enabled.log"
grep -Eq 'reject\.unsupported-operand=[1-9][0-9]*' "$workdir/enabled.log"
grep -Eq 'reject\.non-adjacent=[1-9][0-9]*' "$workdir/enabled.log"

# Each new option must roll back independently without changing behavior.
env LATX_AOT=0 LATX_INSTPTN_MASK=0x1000000 LATX_INSTPTN_STATS=1 \
    "$emulator" "$guest" >"$workdir/legacy-shr-only.log" 2>&1
grep -Eq 'shr-jcc match=[1-9][0-9]* ' "$workdir/legacy-shr-only.log"
grep -Eq 'sar-jcc match=0 ' "$workdir/legacy-shr-only.log"
grep -Eq 'shr-je match=0 ' "$workdir/legacy-shr-only.log"
grep -Eq 'reject\.disabled=[1-9][0-9]*' \
    "$workdir/legacy-shr-only.log"

env LATX_AOT=0 LATX_INSTPTN_MASK=0x400000000 LATX_INSTPTN_STATS=1 \
    "$emulator" "$guest" >"$workdir/shr-je-only.log" 2>&1
grep -Eq 'shr-je match=[1-9][0-9]* ' "$workdir/shr-je-only.log"
grep -Eq 'shr-jcc match=0 ' "$workdir/shr-je-only.log"
grep -Eq 'reject\.disabled=[1-9][0-9]*' "$workdir/shr-je-only.log"

env LATX_AOT=0 LATX_INSTPTN_MASK=0x1 LATX_INSTPTN_STATS=1 \
    "$emulator" "$guest" >"$workdir/disabled.log" 2>&1
grep -Eq 'sar-jcc match=0 ' "$workdir/disabled.log"
grep -Eq 'shr-je match=0 ' "$workdir/disabled.log"

# Generate and load AOT code with all three shift patterns enabled.
mkdir -p "$workdir/aot-home"
env HOME="$workdir/aot-home" LATX_AOT=1 LATX_INSTPTN_MASK="$shift_mask" \
    "$emulator" "$guest" >"$workdir/aot-generate.log" 2>&1

attempt=0
while [ "$attempt" -lt 100 ]; do
    if find "$workdir/aot-home/.cache/latx" -type f \
        -name 'v2-*.aot2' -size +0c -print -quit 2>/dev/null | \
        grep -q .; then
        break
    fi
    attempt=$((attempt + 1))
    sleep 0.05
done
test "$attempt" -lt 100

env HOME="$workdir/aot-home" LATX_AOT=1 LATX_INSTPTN_MASK="$shift_mask" \
    "$emulator" "$guest" >"$workdir/aot-load.log" 2>&1

# A matching faulting memory destination must fault identically after rollback.
set +e
env LATX_AOT=0 LATX_INSTPTN_MASK="$shift_mask" \
    "$emulator" "$guest" fault >"$workdir/fault-enabled.log" 2>&1
fault_enabled=$?
env LATX_AOT=0 LATX_INSTPTN_MASK=0x1 \
    "$emulator" "$guest" fault >"$workdir/fault-disabled.log" 2>&1
fault_disabled=$?
set -e
test "$fault_enabled" -ne 0
test "$fault_enabled" -eq "$fault_disabled"

echo "PASS: SAR/SHR+Jcc count matrix, widths, flags, memory, rollback," \
    "fault, JIT/TU and AOT"
