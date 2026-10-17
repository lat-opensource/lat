#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
emulator=$1
source_file=$2
dummy_source=$3
guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_cc=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
native_cc=${LATX_NATIVE_CC:-cc}
if [ "$(uname -m)" != loongarch64 ] ||
   ! command -v "$guest_cc" >/dev/null 2>&1; then
    echo "SKIP: requires LoongArch and a Guest C compiler"
    exit 77
fi
task_dir=$(mktemp -d)
trap 'rm -rf "$task_dir"' EXIT HUP INT TERM
mkdir -p "$task_dir/guest"
"$native_cc" -shared -fPIC -O2 -Wall -Wextra -Werror -DHOST_PROBE \
    "$source_file" -pthread -ldl -Wl,-soname,libX11.so.6 \
    -o "$task_dir/libX11.so.6"
"$guest_cc" --sysroot="$guest_root" -shared -fPIC \
    -I"$(dirname "$dummy_source")" "$dummy_source" \
    -Wl,-soname,libX11.so.6 -o "$task_dir/guest/libX11.so.6"
"$guest_cc" --sysroot="$guest_root" -O2 -Wall -Wextra -Werror \
    "$source_file" -L"$task_dir/guest" -l:libX11.so.6 \
    -Wl,--no-as-needed -ldl -Wl,--as-needed -pthread \
    -o "$task_dir/guest/boundary"
LD_PRELOAD="$task_dir/libX11.so.6" \
BOX64_LD_LIBRARY_PATH="$task_dir/guest" \
LATX_AOT=0 LATX_KZT=1 LATX_KZT_LIBS=core,x11 LATX_KZT_GUEST_TLS=1 \
    "$emulator" -U LD_PRELOAD -E "LD_LIBRARY_PATH=$task_dir/guest" \
    -L "$guest_root" "$task_dir/guest/boundary"
