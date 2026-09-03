#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
tmp_root=${TMPDIR:-${HOME}/tmp}
mkdir -p "$tmp_root"
workdir=$(mktemp -d "$tmp_root/static-helper-live-temp.XXXXXX")
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
    -Wl,--build-id=none "$source_file" -o "$workdir/live-temp"

run_guest()
{
    mode=$1
    timeout 10s env LATX_AOT=0 LATX_KZT=0 LATX_SOFTFPU=0 \
        LATX_STATIC_HELPER="$mode" "$emulator" "$workdir/live-temp"
}

run_guest 0
run_guest 1

timeout 10s env LATX_AOT=0 LATX_KZT=0 LATX_SOFTFPU=0 \
    LATX_STATIC_HELPER=1 LATX_STATIC_HELPER_STATS=1 \
    "$emulator" "$workdir/live-temp" 2>"$workdir/stats.err"
grep -q 'translated-callsite mode=static generated-ir2=' "$workdir/stats.err"

echo "PASS: static helper prologue preserves live RIP-relative addresses"
