#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
mask=${3:-0}
expected=${4:-0}
aot_mode=${5:-0}
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
test_home="$workdir/home"
mkdir -p "$test_home"

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$workdir/eflags-cross-boundary"

run_guest()
{
    set +e
    (
        cd "$workdir"
        HOME="$test_home" LATX_AOT="$aot_mode" LATX_KZT=0 \
            LATX_EFLAGS_CROSS="$mask" \
            "$emulator" ./eflags-cross-boundary
    )
    ret=$?
    set -e
}

run_guest

if [ "$aot_mode" -ne 0 ] && [ "$ret" -eq "$expected" ]; then
    cache_file=""
    count=0
    while [ "$count" -lt 100 ]; do
        cache_file=$(find "$test_home/.cache/latx" -type f -name '*.aot2' \
            -size +0c -print -quit 2>/dev/null || true)
        if [ -n "$cache_file" ]; then
            break
        fi
        count=$((count + 1))
        sleep 0.1
    done
    if [ -z "$cache_file" ]; then
        echo "FAIL: AOT cache was not generated" >&2
        exit 1
    fi

    # The first run profiles and generates the TU; the second executes it.
    run_guest
fi

if [ "$ret" -ne "$expected" ]; then
    echo "FAIL: mask=$mask aot=$aot_mode expected=$expected actual=$ret" >&2
    exit 1
fi

echo "PASS: mask=$mask aot=$aot_mode result=$ret"
