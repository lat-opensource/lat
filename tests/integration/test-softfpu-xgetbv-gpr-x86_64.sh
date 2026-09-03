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
    -o "$workdir/softfpu-xgetbv-gpr-x86_64"

for mode in 0 1 2; do
    LATX_AOT=0 LATX_MT=0 LATX_SOFTFPU=$mode \
        "$emulator" "$workdir/softfpu-xgetbv-gpr-x86_64"
done

echo "PASS: XGETBV preserves x86_64 high GPRs in all softfpu modes"
