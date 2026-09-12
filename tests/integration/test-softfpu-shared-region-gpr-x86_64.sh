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
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" \
    -o "$workdir/softfpu-shared-region-gpr-x86_64"

LATX_AOT=0 LATX_MT=0 LATX_SOFTFPU=1 LATX_SOFTFPU_FAST=0 \
    "$emulator" "$workdir/softfpu-shared-region-gpr-x86_64"

for fast in 0 0x40; do
    LATX_AOT=0 LATX_MT=0 LATX_SOFTFPU=2 LATX_SOFTFPU_FAST=$fast \
        "$emulator" "$workdir/softfpu-shared-region-gpr-x86_64"
done

echo "PASS: shared softfpu regions restore high GPRs between helpers"
