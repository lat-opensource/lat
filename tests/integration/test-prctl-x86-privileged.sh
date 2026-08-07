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
if ! unshare -Ur true 2>/dev/null; then
    echo "SKIP: an unprivileged user namespace is required"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static -no-pie \
    -ffreestanding -fno-builtin -fno-stack-protector \
    -Wl,--build-id=none "$source_file" -o "$workdir/prctl-x86-semantics"

for mode in 0 1; do
    if [ "$mode" -eq 0 ]; then
        label=non-tu
    else
        label=tu
    fi
    set +e
    unshare -Ur env LATX_AOT=0 LATX_KZT=0 LATX_TU="$mode" \
        timeout -s KILL 10 "$emulator" \
        "$workdir/prctl-x86-semantics" x
    ret=$?
    set -e
    if [ "$ret" -ne 0 ]; then
        echo "FAIL: $label user-namespace PR_SET_MM semantics returned $ret" >&2
        exit "$ret"
    fi
    echo "PASS: $label user-namespace PR_SET_MM semantics"
done
