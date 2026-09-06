#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
emulator=$1
source_file=$2
guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_cc=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
if ! command -v "$guest_cc" >/dev/null 2>&1; then
    echo 'SKIP: requires a Guest C compiler'
    exit 77
fi
if [ ! -r "$guest_root/lib64/ld-linux-x86-64.so.2" ] ||
   [ ! -r "$guest_root/lib/x86_64-linux-gnu/libc.so.6" ]; then
    echo 'SKIP: requires a matching x86-64 glibc runtime'
    exit 77
fi
task_dir=$(mktemp -d)
trap 'rm -rf "$task_dir"' EXIT HUP INT TERM
selected="$task_dir/selected-runtime"
mkdir -p "$selected/lib64" "$selected/lib/x86_64-linux-gnu"
cp -L "$guest_root/lib64/ld-linux-x86-64.so.2" "$selected/lib64/"
cp -L "$guest_root/lib/x86_64-linux-gnu/libc.so.6" \
    "$selected/lib/x86_64-linux-gnu/"
"$guest_cc" --sysroot="$guest_root" -O2 -Wall -Wextra -Werror \
    "$source_file" -o "$task_dir/runtime-prefix-exec"
LATX_AOT=0 LATX_KZT=0 LAT_LD_PREFIX="$guest_root" \
    "$emulator" -L "$selected" "$task_dir/runtime-prefix-exec" \
    "$selected" parent
