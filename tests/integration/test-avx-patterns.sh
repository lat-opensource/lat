#!/bin/sh
set -eu

emulator=$1
shift
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guests"
    exit 77
fi

for source_file in "$@"; do
    name=$(basename "$source_file" .S)
    "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
        -Wl,--build-id=none "$source_file" -o "$workdir/$name"

    LATX_AOT=0 LATX_TU=0 "$emulator" "$workdir/$name"

    aot_home="$workdir/aot-$name"
    mkdir -p "$aot_home"
    HOME="$aot_home" LATX_AOT=1 LATX_TU=1 \
        "$emulator" "$workdir/$name"

    aot_file=
    for _ in $(seq 1 100); do
        aot_file=$(find "$aot_home/.cache/latx" -type f -name '*.aot2' \
            -size +0c -print -quit 2>/dev/null || true)
        [ -n "$aot_file" ] && break
        sleep 0.1
    done
    if [ -z "$aot_file" ]; then
        echo "FAIL: no non-empty AOT file generated for $name" >&2
        exit 1
    fi

    HOME="$aot_home" LATX_AOT=1 LATX_TU=1 \
        "$emulator" "$workdir/$name"
done

echo "PASS: LATX AVX integration JIT/cold-AOT/hot-AOT"
