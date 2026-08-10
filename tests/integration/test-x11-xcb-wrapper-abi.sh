#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

emulator=$1
guest_source=$2
probe_source=$3
dummy_source=$4
source_dir=$(dirname "$guest_source")

if [ "$(uname -m)" != loongarch64 ]; then
    echo "SKIP: the wrapper ABI test requires a LoongArch host"
    exit 77
fi

guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_compiler=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
native_compiler=${LATX_NATIVE_CC:-cc}

if [ ! -d "$guest_root" ]; then
    echo "SKIP: x86_64 guest sysroot not found: $guest_root"
    exit 77
fi
if ! command -v "$guest_compiler" >/dev/null 2>&1; then
    echo "SKIP: x86_64 cross compiler not found: $guest_compiler"
    exit 77
fi
if ! command -v "$native_compiler" >/dev/null 2>&1; then
    echo "SKIP: native C compiler not found: $native_compiler"
    exit 77
fi

if ! printf '#include <xcb/xcb_image.h>\n' | \
     "$guest_compiler" --sysroot="$guest_root" -E -x c - \
     >/dev/null 2>&1; then
    echo "SKIP: x86_64 xcb-image development headers are unavailable"
    exit 77
fi
if ! printf '#include <xcb/xcb_image.h>\n' | \
     "$native_compiler" -E -x c - >/dev/null 2>&1; then
    echo "SKIP: native xcb-image development headers are unavailable"
    exit 77
fi

workdir=$(mktemp -d)
guest_library_dir="$workdir/guest-lib"
host_probe="$workdir/libxcb-image-probe.so"
guest_program="$workdir/x11-xcb-wrapper-abi-guest"
xvfb_pid=

cleanup()
{
    if [ -n "$xvfb_pid" ]; then
        kill "$xvfb_pid" >/dev/null 2>&1 || true
        wait "$xvfb_pid" >/dev/null 2>&1 || true
    fi
    rm -rf "$workdir"
}
trap cleanup EXIT HUP INT TERM

mkdir -p "$guest_library_dir"

"$native_compiler" -shared -fPIC -O2 -Wall -Wextra -Werror \
    -I"$source_dir" -Wl,-soname,libxcb-image.so.0 \
    "$probe_source" -o "$host_probe"

"$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
    -Wall -Wextra -Werror -Wno-unused-parameter -I"$source_dir" \
    -Wl,-soname,libxcb-image.so.0 "$dummy_source" \
    -o "$guest_library_dir/libxcb-image.so.0"
ln -s libxcb-image.so.0 "$guest_library_dir/libxcb-image.so"

"$guest_compiler" --sysroot="$guest_root" -O2 -Wall -Wextra -Werror \
    -I"$source_dir" "$guest_source" -L"$guest_library_dir" \
    -Wl,-rpath,'$ORIGIN/guest-lib' -lxcb-image -lX11 -lxcb \
    -o "$guest_program"

test_display=${DISPLAY:-}
if [ -z "$test_display" ]; then
    if ! command -v Xvfb >/dev/null 2>&1; then
        echo "SKIP: DISPLAY is unset and Xvfb is unavailable"
        exit 77
    fi
    display_file="$workdir/display"
    Xvfb -displayfd 1 -screen 0 640x480x24 -nolisten tcp \
        >"$display_file" 2>"$workdir/xvfb.log" &
    xvfb_pid=$!
    attempts=0
    while [ ! -s "$display_file" ] && kill -0 "$xvfb_pid" 2>/dev/null; do
        attempts=$((attempts + 1))
        if [ "$attempts" -ge 50 ]; then
            echo "FAIL: Xvfb did not publish a display number" >&2
            exit 1
        fi
        sleep 0.1
    done
    if [ ! -s "$display_file" ]; then
        echo "FAIL: Xvfb exited before publishing a display number" >&2
        sed -n '1,120p' "$workdir/xvfb.log" >&2
        exit 1
    fi
    test_display=:$(sed -n '1p' "$display_file")
fi

set +e
DISPLAY="$test_display" \
LD_PRELOAD="$host_probe" \
BOX64_LD_LIBRARY_PATH="$guest_library_dir" \
LATX_AOT=0 \
LATX_KZT=1 \
"$emulator" -U LD_PRELOAD -L "$guest_root" "$guest_program"
result=$?
set -e

if [ "$result" -ne 0 ]; then
    echo "FAIL: wrapper ABI guest exited with status $result" >&2
    exit "$result"
fi

echo "PASS: LoongArch host observed X11/XCB wrapper ABI sentinels"
