#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
emulator=$1
guest_source=$2
plugin_source=$3
probe_source=$4
guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_cc=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
native_cc=${LATX_NATIVE_CC:-cc}
artifacts=${LATX_DLSYM_GUEST_ARTIFACT_DIR:-}

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
"$native_cc" -shared -fPIC -O2 -Wall -Wextra -Werror "$probe_source" \
    -Wl,-soname,libX11.so.6 -o "$work/libX11.so.6"
if [ -n "$artifacts" ]; then
    for source in "$guest_source" "$plugin_source"; do
        name=$(basename "$source")
        actual=$(sha256sum "$source" | awk '{print $1}')
        expected=$(awk -v name="$name" '$2 == name {print $1}' "$artifacts/sources.sha256")
        [ "$actual" = "$expected" ] || { echo "FAIL: source mismatch $name"; exit 1; }
    done
    (cd "$artifacts" && sha256sum -c artifacts.sha256)
    cp "$artifacts/guest" "$work/guest"
    cp "$artifacts/plugin.so" "$work/plugin.so"
    cp "$artifacts/libX11.so.6" "$work/guest-lib/libX11.so.6"
else
    "$guest_cc" --sysroot="$guest_root" -O2 -Wall -Wextra -Werror \
        "$guest_source" -ldl -pthread -o "$work/guest"
    "$guest_cc" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror "$plugin_source" -o "$work/plugin.so"
    "$guest_cc" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror "$plugin_source" \
        -Wl,-soname,libX11.so.6 -o "$work/guest-lib/libX11.so.6"
fi

for mode in off tls-off tls-on; do
    kzt=1
    tls=0
    [ "$mode" != off ] || kzt=0
    [ "$mode" != tls-on ] || tls=1
    for kind in raw registered; do
        library="$work/plugin.so"
        marker=17
        if [ "$kind" = registered ]; then
            library=libX11.so.6
            [ "$kzt" = 0 ] || marker=61
        fi
        echo "CASE: mode=$mode handle=$kind"
        LD_PRELOAD="$work/libX11.so.6" \
        BOX64_LD_LIBRARY_PATH="$work/guest-lib" \
        LATX_AOT=0 LATX_KZT="$kzt" LATX_KZT_GUEST_TLS="$tls" \
        LATX_KZT_LIBS=core,x11 LATX_KZT_LOG=1 \
            "$emulator" -U LD_PRELOAD \
            -E "LD_LIBRARY_PATH=$work/guest-lib" \
            -L "$guest_root" "$work/guest" "$library" "$marker"
    done
done
