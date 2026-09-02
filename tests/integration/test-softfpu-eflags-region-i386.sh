#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the i386 guest"
    exit 77
fi

"$clang" --target=i386-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" \
    -o "$workdir/softfpu-eflags-region-i386"

LATX_AOT=0 LATX_MT=0 LATX_SOFTFPU=2 LATX_SOFTFPU_FAST=0xc00000 \
    "$emulator" "$workdir/softfpu-eflags-region-i386"

echo "PASS: softfpu helper regions preserve x87 comparison flags"
