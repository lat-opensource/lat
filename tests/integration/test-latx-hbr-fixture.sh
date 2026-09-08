#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only

set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

guest_compiler=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}

if ! command -v "$guest_compiler" >/dev/null 2>&1; then
    echo "SKIP: x86_64 guest compiler not found: $guest_compiler"
    exit 77
fi
if [ ! -d "$guest_root" ]; then
    echo "SKIP: x86_64 guest sysroot not found: $guest_root"
    exit 77
fi

guest="$workdir/$(basename "${source_file%.c}")"
"$guest_compiler" --sysroot="$guest_root" -O2 -Wall -Wextra -Werror \
    -mavx2 -mxsave "$source_file" -o "$guest"

set +e
LATX_AOT=0 LATX_KZT=0 timeout -s KILL 90 "$emulator" -L "$guest_root" \
    "$guest"
status=$?
set -e

case $status in
0) echo "PASS: $(basename "$source_file" .c)" ;;
77) echo "SKIP: guest skipped $(basename "$source_file" .c)"; exit 77 ;;
124) echo "FAIL: $(basename "$source_file" .c) timed out" >&2; exit 1 ;;
*) echo "FAIL: $(basename "$source_file" .c) exited with status $status" >&2
   exit "$status" ;;
esac
