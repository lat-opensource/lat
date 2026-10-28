#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only

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

guest="$workdir/shld-shrd-32-x86_64"
"$clang" --target=x86_64-linux-gnu -O2 -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$guest"

set +e
timeout -s KILL 30 env LATX_AOT=0 LATX_KZT=0 "$emulator" "$guest"
status=$?
set -e

case $status in
0)
    echo "PASS: 32-bit SHLD/SHRD operands"
    ;;
124)
    echo "FAIL: 32-bit SHLD/SHRD test timed out" >&2
    exit 1
    ;;
*)
    echo "FAIL: 32-bit SHLD/SHRD test exited with status $status" >&2
    exit "$status"
    ;;
esac
