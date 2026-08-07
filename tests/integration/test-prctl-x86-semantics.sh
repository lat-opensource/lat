#!/bin/sh
set -eu

emulator=$1
source_file=$2
native_helper_source=$3
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

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static -no-pie \
    -ffreestanding -fno-builtin -fno-stack-protector \
    -Wl,--build-id=none "$source_file" -o "$workdir/prctl-x86-semantics"
"${CC:-cc}" -O2 -Wall -Wextra "$native_helper_source" \
    -o "$workdir/prctl-native-env-helper"

run_case()
{
    mode=$1
    label=$2
    case_name=$3
    shift 3

    set +e
    env LATX_AOT=0 LATX_KZT=0 LATX_TU="$mode" timeout -s KILL 10 \
        "$emulator" "$workdir/prctl-x86-semantics" "$case_name" "$@"
    ret=$?
    set -e

    if [ "$ret" -eq 0 ]; then
        echo "PASS: $label $case_name prctl semantics"
        return
    fi
    if [ "$ret" -eq 124 ]; then
        echo "FAIL: $label $case_name prctl semantics timed out" >&2
    else
        echo "FAIL: $label $case_name prctl semantics returned $ret" >&2
    fi
    exit "$ret"
}

for mode in 0 1; do
    if [ "$mode" -eq 0 ]; then
        label=non-tu
    else
        label=tu
    fi
    run_case "$mode" "$label" p
    run_case "$mode" "$label" v
    run_case "$mode" "$label" m
    run_case "$mode" "$label" r
    run_case "$mode" "$label" s
    run_case "$mode" "$label" f
    run_case "$mode" "$label" i
    run_case "$mode" "$label" e "$workdir/prctl-native-env-helper"
done
