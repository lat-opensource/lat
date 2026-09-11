#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if [ "$(getconf PAGESIZE)" != 16384 ]; then
    echo "SKIP: the guest permission test requires a 16K host page"
    exit 77
fi

fixture=${LATX_16K_PERMISSION_FIXTURE:-}
if [ -n "$fixture" ]; then
    if [ ! -f "$fixture" ] || [ ! -x "$fixture" ]; then
        echo "FAIL: prebuilt i386 fixture is not executable: $fixture" >&2
        exit 1
    fi
else
    if command -v clang-19 >/dev/null 2>&1; then
        clang=clang-19
    elif command -v clang >/dev/null 2>&1; then
        clang=clang
    else
        echo "SKIP: clang is required; alternatively set LATX_16K_PERMISSION_FIXTURE"
        exit 77
    fi
    if ! command -v ld.lld >/dev/null 2>&1; then
        echo "SKIP: ld.lld is required; alternatively set LATX_16K_PERMISSION_FIXTURE"
        exit 77
    fi
    fixture=$workdir/latx-16k-page-permission
    # Toolchain availability was checked above. Compilation errors must fail.
    "$clang" --target=i386-linux-gnu -fuse-ld=lld -nostdlib -static -no-pie \
        -Wl,--build-id=none "$source_file" -o "$fixture"
fi

run_fault_case()
{
    mode=$1
    case_name=$2
    set +e
    LATX_AOT=0 LATX_KZT=0 LATX_MT="$mode" timeout -s KILL 10 \
        "$emulator" "$fixture" "$case_name"
    ret=$?
    set -e

    case $ret in
    0) echo "PASS: LATX_MT=$mode $case_name signal context or valid access" ;;
    10) echo "FAIL: guest mmap failed" >&2; exit 1 ;;
    11) echo "FAIL: guest mprotect failed" >&2; exit 1 ;;
    12) echo "FAIL: $case_name access bypassed guest permissions" >&2; exit 1 ;;
    14) echo "FAIL: wrong signal or signal setup failed" >&2; exit 1 ;;
    15) echo "FAIL: incorrect fault address" >&2; exit 1 ;;
    16) echo "FAIL: incorrect fault instruction address" >&2; exit 1 ;;
    17) echo "FAIL: incorrect fault stack pointer" >&2; exit 1 ;;
    124|137) echo "FAIL: $case_name access timed out" >&2; exit 1 ;;
    *) echo "FAIL: unexpected $case_name status $ret" >&2; exit 1 ;;
    esac
}

for mode in 1 2; do
    run_fault_case "$mode" r
    run_fault_case "$mode" w
    run_fault_case "$mode" s
    run_fault_case "$mode" c
    run_fault_case "$mode" v
    run_fault_case "$mode" p
done
