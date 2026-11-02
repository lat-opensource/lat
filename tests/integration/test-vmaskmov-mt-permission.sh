#!/bin/sh
# SPDX-FileCopyrightText: 2026 LAT Project Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# Prove that LATX_MT guest-page permission checks cover the VMASKMOV/VPMASKMOV
# element memory accesses.  A 16K host page is required so that a PROT_NONE 4K
# guest page remains host-accessible through the surrounding writable page;
# without the translator check the access is silently allowed.
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if [ "$(getconf PAGESIZE)" != 16384 ]; then
    echo "SKIP: VMASKMOV permission test requires a 16K host page"
    exit 77
fi

if ! command -v ld.lld >/dev/null 2>&1; then
    echo "SKIP: ld.lld is required to build the x86_64 guest"
    exit 77
fi

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

guest=$workdir/vmaskmov-mt-permission
# Toolchain availability was checked above. Compilation errors must fail.
"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$guest"

fail=0

run_case()
{
    mode=$1
    name=$2
    expect=$3

    set +e
    LATX_AOT=0 LATX_KZT=0 LATX_MT="$mode" \
        timeout -s KILL 20 "$emulator" "$guest" "$name"
    ret=$?
    set -e

    case $ret in
    0)
        # Fault cases reach here only after the handler verified SEGV_ACCERR
        # and the checked address; no-fault cases verify their own data.
        ;;
    20)
        if [ "$expect" = fault ]; then
            echo "FAIL: LATX_MT=$mode $name bypassed guest permissions" >&2
        else
            echo "FAIL: LATX_MT=$mode $name unexpectedly faulted" >&2
        fi
        fail=1
        ;;
    10) echo "FAIL: LATX_MT=$mode $name could not install the signal handler" >&2; fail=1 ;;
    11) echo "FAIL: LATX_MT=$mode $name mmap failed" >&2; fail=1 ;;
    12) echo "FAIL: LATX_MT=$mode $name mprotect failed" >&2; fail=1 ;;
    13) echo "FAIL: LATX_MT=$mode $name bad case argument" >&2; fail=1 ;;
    14) echo "FAIL: LATX_MT=$mode $name wrong data for a permitted access" >&2; fail=1 ;;
    15) echo "FAIL: LATX_MT=$mode $name wrong signal" >&2; fail=1 ;;
    16) echo "FAIL: LATX_MT=$mode $name wrong si_code (expected SEGV_ACCERR)" >&2; fail=1 ;;
    17) echo "FAIL: LATX_MT=$mode $name wrong fault address" >&2; fail=1 ;;
    124|137) echo "FAIL: LATX_MT=$mode $name timed out" >&2; fail=1 ;;
    *) echo "FAIL: LATX_MT=$mode $name unexpected status $ret" >&2; fail=1 ;;
    esac
}

# VMASKMOVPS/PD and VPMASKMOVD/Q selected elements in the denied page, plus the
# all-selected fast path.  These must fault only with the guest-page check.
for mode in 1 2; do
    for name in a b c d e f g h k l m n o p q s u w; do
        run_case "$mode" "$name" fault
    done
    # Unselected denied bytes and an all-clear mask must not fault.
    for name in i j r t v; do
        run_case "$mode" "$name" nofault
    done
done

if [ "$fail" -ne 0 ]; then
    exit 1
fi

echo "PASS: VMASKMOV guest-page permissions are enforced with LATX_MT=1/2"
