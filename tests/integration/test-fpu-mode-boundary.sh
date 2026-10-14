#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
arch=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

guest_dir=${LATX_FPU_BOUNDARY_GUEST_DIR:-$workdir}
if [ -z "${LATX_FPU_BOUNDARY_GUEST_DIR:-}" ]; then
    if command -v clang-19 >/dev/null 2>&1; then
        clang=clang-19
    elif command -v clang >/dev/null 2>&1; then
        clang=clang
    else
        echo "SKIP: clang is required to build the $arch guest"
        exit 77
    fi
    if ! "$clang" --target="$arch-linux-gnu" -fuse-ld=lld -nostdlib -static \
            -Wl,--build-id=none -x assembler /dev/null \
            -o "$workdir/linker-check" >/dev/null 2>&1; then
        echo "SKIP: clang with ld.lld is required to build the $arch guest"
        exit 77
    fi
fi

for case_id in 1 2; do
    guest="$guest_dir/fpu-mode-boundary-$arch-$case_id"
    if [ -z "${LATX_FPU_BOUNDARY_GUEST_DIR:-}" ]; then
        "$clang" --target="$arch-linux-gnu" -fuse-ld=lld -nostdlib -static \
            -Wl,--build-id=none -DTEST_CASE=$case_id "$source_file" -o "$guest"
    fi
    if [ ! -x "$guest" ]; then
        echo "FAIL: guest executable missing: $guest" >&2
        exit 1
    fi
done

run_case()
{
    case_id=$1
    softfpu=$2
    fast=$3
    LATX_AOT=0 LATX_MT=0 LATX_KZT=0 LATX_SOFTFPU=$softfpu \
        LATX_SOFTFPU_FAST=$fast timeout -s KILL 10 "$emulator" \
        "$guest_dir/fpu-mode-boundary-$arch-$case_id"
    echo "PASS: $arch case=$case_id softfpu=$softfpu fast=$fast"
}

run_case 1 0 0
run_case 2 1 0
run_case 2 1 -1
run_case 2 2 0
run_case 2 2 32
run_case 2 2 -1
