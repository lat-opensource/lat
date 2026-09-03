#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
tmp_root=${TMPDIR:-${HOME}/tmp}
mkdir -p "$tmp_root"
workdir=$(mktemp -d "$tmp_root/static-helper-stubs.XXXXXX")
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
    -Wl,--build-id=none "$source_file" -o "$workdir/static-helper-stubs"

run_guest()
{
    mode=$1
    output=$2

    timeout 10s env LATX_AOT=0 LATX_KZT=0 LATX_VPAES=0 LATX_SMC=6 \
        LATX_STATIC_HELPER="$mode" \
        "$emulator" "$workdir/static-helper-stubs" >"$output"
    test "$(wc -c <"$output")" -eq 288
}

run_guest 0 "$workdir/inline.out"
run_guest 1 "$workdir/static.out"
cmp "$workdir/inline.out" "$workdir/static.out"

timeout 10s env LATX_AOT=0 LATX_KZT=0 LATX_VPAES=0 LATX_SMC=6 \
    "$emulator" "$workdir/static-helper-stubs" >"$workdir/default.out"
cmp "$workdir/static.out" "$workdir/default.out"

if env LATX_STATIC_HELPER=invalid "$emulator" \
    "$workdir/static-helper-stubs" >/dev/null 2>&1; then
    echo "FAIL: invalid LATX_STATIC_HELPER was accepted" >&2
    exit 1
fi
if env LATX_STATIC_HELPER_STATS=2 "$emulator" \
    "$workdir/static-helper-stubs" >/dev/null 2>&1; then
    echo "FAIL: invalid LATX_STATIC_HELPER_STATS was accepted" >&2
    exit 1
fi

timeout 10s env LATX_AOT=0 LATX_KZT=0 LATX_VPAES=0 LATX_SMC=6 \
    LATX_STATIC_HELPER=1 LATX_STATIC_HELPER_STATS=1 \
    "$emulator" "$workdir/static-helper-stubs" >/dev/null \
    2>"$workdir/stats.err"
grep -q 'translated-callsite mode=static generated-ir2=' "$workdir/stats.err"
test "$(grep -c 'translated-callsite mode=static ' "$workdir/stats.err")" \
    -ge 8
test "$(grep -c 'translated-callsite mode=static-nofp ' \
    "$workdir/stats.err")" -ge 3

timeout 10s env LATX_AOT=0 LATX_KZT=0 LATX_VPAES=0 LATX_SMC=6 \
    LATX_STATIC_HELPER=0 LATX_STATIC_HELPER_STATS=1 \
    "$emulator" "$workdir/static-helper-stubs" >/dev/null \
    2>"$workdir/inline-stats.err"
grep -q 'translated-callsite mode=inline-disabled generated-ir2=' \
    "$workdir/inline-stats.err"

echo "PASS: generic static helper stubs preserve signal, SMC, GPR, EFLAGS," \
    "x87, XMM, and YMM state"
