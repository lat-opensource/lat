#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
tmp_base=${TMPDIR:-"$HOME/tmp"}
mkdir -p "$tmp_base"
workdir=$(mktemp -d "$tmp_base/latx-instptn-test.XXXXXX")
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
    -Wl,--build-id=none "$source_file" -o "$workdir/instptn-options-fork"
guest="$workdir/instptn-options-fork"

expect_failure()
{
    name=$1
    pattern=$2
    shift 2
    if "$@" >"$workdir/$name.log" 2>&1; then
        echo "FAIL: $name unexpectedly succeeded" >&2
        return 1
    fi
    grep -q "$pattern" "$workdir/$name.log"
}

expect_failure invalid-mask "LATX_INSTPTN_MASK must be" \
    "$emulator" -latx-instptn-mask invalid "$guest"
expect_failure invalid-stats "LATX_INSTPTN_STATS must be" \
    env LATX_INSTPTN_STATS=2 "$emulator" "$guest"

# A valid command-line value must override an invalid environment value.
env LATX_INSTPTN_MASK=invalid LATX_AOT=0 "$emulator" \
    -latx-instptn-mask 0x3ffffff "$guest" \
    >"$workdir/precedence.log" 2>&1

env LATX_AOT=0 LATX_INSTPTN_STATS=1 "$emulator" "$guest" \
    >"$workdir/default.log" 2>&1
grep -Eq 'sub-jcc match=[1-9][0-9]*' "$workdir/default.log"
grep -q 'reject.unsupported-cc=' "$workdir/default.log"
grep -q 'reject.non-adjacent=' "$workdir/default.log"
grep -q 'reject.unsupported-operand=' "$workdir/default.log"
grep -q 'reject.zero-shift-count=' "$workdir/default.log"

# The child resets inherited history, so the pre-fork SUB record appears once.
test "$(grep -c 'sub-jcc match=' "$workdir/default.log")" -eq 1
test "$(sed -n 's/.*pid=\([0-9][0-9]*\) mask=.*/\1/p' \
    "$workdir/default.log" | sort -u | wc -l)" -eq 2

env LATX_AOT=0 LATX_INSTPTN_MASK=0x3dfffff LATX_INSTPTN_STATS=1 \
    "$emulator" "$guest" >"$workdir/disabled.log" 2>&1
grep -q 'sub-jcc match=0' "$workdir/disabled.log"
grep -q 'reject.disabled=' "$workdir/disabled.log"

env LATX_AOT=0 "$emulator" "$guest" >"$workdir/stats-off.log" 2>&1
if grep -q '\[LATX\]\[instptn\]' "$workdir/stats-off.log"; then
    echo "FAIL: statistics were printed while disabled" >&2
    exit 1
fi

echo "PASS: instruction-pattern parsing, statistics, rejection," \
    "and fork semantics"
