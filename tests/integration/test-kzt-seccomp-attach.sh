#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
emulator=$1
mode=$2
guest_source=$3
probe_source=$4
dummy_source=$5
guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_cc=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
native_cc=${LATX_NATIVE_CC:-cc}
x11_include=${LATX_X11_INCLUDE:-/usr/include}
artifacts=${LATX_SECCOMP_ATTACH_GUEST_ARTIFACT_DIR:-}
if [ "$(uname -m)" != loongarch64 ] || [ ! -d "$guest_root" ]; then
    echo 'SKIP: requires LoongArch and an x86-64 Guest runtime'
    exit 77
fi
if [ -z "$artifacts" ] && ! command -v "$guest_cc" >/dev/null 2>&1; then
    echo 'SKIP: requires a Guest compiler or verified Guest artifacts'
    exit 77
fi
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir "$work/guest-lib"
"$native_cc" -shared -fPIC -O2 -Wall -Wextra -Werror -I"$x11_include" \
    "$probe_source" -pthread -Wl,-soname,libX11.so.6 -o "$work/libX11.so.6"
if [ -n "$artifacts" ]; then
    for source in "$guest_source" "$dummy_source"; do
        name=$(basename "$source")
        actual=$(sha256sum "$source" | awk '{print $1}')
        expected=$(awk -v name="$name" '$2 == name {print $1}' "$artifacts/sources.sha256")
        [ "$actual" = "$expected" ] || { echo "FAIL: source mismatch $name"; exit 1; }
    done
    (cd "$artifacts" && sha256sum -c artifacts.sha256)
    cp "$artifacts/guest" "$work/guest"
    cp "$artifacts/libX11.so.6" "$work/guest-lib/libX11.so.6"
else
    "$guest_cc" --sysroot="$guest_root" -shared -fPIC -O2 -I"$x11_include" \
        "$dummy_source" -Wl,-soname,libX11.so.6 -o "$work/guest-lib/libX11.so.6"
    "$guest_cc" --sysroot="$guest_root" -O2 -Wall -Wextra -Werror \
        -I"$x11_include" "$guest_source" -L"$work/guest-lib" \
        -Wl,--no-as-needed -l:libX11.so.6 -ldl -pthread -o "$work/guest"
fi
LD_PRELOAD="$work/libX11.so.6" BOX64_LD_LIBRARY_PATH="$work/guest-lib" \
LATX_AOT=0 LATX_KZT=1 LATX_KZT_GUEST_TLS=1 LATX_KZT_LIBS=core,x11 \
LATX_KZT_LOG=1 "$emulator" -U LD_PRELOAD \
    -E "LD_LIBRARY_PATH=$work/guest-lib" -L "$guest_root" "$work/guest" "$mode"
