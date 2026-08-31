#!/bin/sh
set -eu

emulator=$1
source_file=$2
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

guest="$workdir/vex128-ymmh-zero"
"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$guest"

LATX_AOT=0 LATX_TU=0 "$emulator" "$guest"

aot_home="$workdir/aot"
mkdir -p "$aot_home"
HOME="$aot_home" LATX_AOT=1 LATX_TU=1 "$emulator" "$guest"

aot_file=
for _ in $(seq 1 100); do
    aot_file=$(find "$aot_home/.cache/latx" -type f -name '*.aot2' \
        -size +0c -print -quit 2>/dev/null || true)
    [ -n "$aot_file" ] && break
    sleep 0.1
done
if [ -z "$aot_file" ]; then
    echo "FAIL: no non-empty AOT file generated" >&2
    exit 1
fi

for _ in $(seq 1 10); do
    HOME="$aot_home" LATX_AOT=1 LATX_TU=1 "$emulator" "$guest"
done
echo "PASS: VEX.128 YMM-high zeroing and mixed-width HBR JIT/cold-AOT/hot-AOT"
