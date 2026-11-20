#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
arch=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the $arch guest"
    exit 77
fi

case $arch in
x86_64)
    target=x86_64-linux-gnu
    extra_flags=-mno-red-zone
    ;;
i386)
    target=i386-linux-gnu
    extra_flags=
    ;;
*)
    echo "FAIL: unsupported arch $arch" >&2
    exit 1
    ;;
esac

if ! "$clang" --target="$target" -fuse-ld=lld -nostdlib -static \
        -Wl,--build-id=none -x assembler /dev/null -o "$workdir/check" \
        >/dev/null 2>&1; then
    echo "SKIP: clang with ld.lld is required to build the $arch guest"
    exit 77
fi

guest="$workdir/xlat-$arch"
"$clang" --target="$target" -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none -O2 -ffreestanding -fno-builtin -fno-pic \
    -fno-stack-protector $extra_flags "$source_file" -o "$guest"

set +e
LATX_AOT=0 LATX_MT=0 LATX_KZT=0 timeout -s KILL 30 \
    "$emulator" "$guest"
status=$?
set -e

case $status in
0) echo "PASS: $arch xlat segment/address-size override" ;;
124) echo "FAIL: $arch xlat test timed out" >&2; exit 1 ;;
*) echo "FAIL: $arch xlat test exited with status $status" >&2
   exit "$status" ;;
esac
