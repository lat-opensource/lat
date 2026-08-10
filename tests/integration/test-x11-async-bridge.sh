#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

emulator=$1
guest_source=$2
probe_source=$3
dummy_source=$4
source_dir=$(dirname "$guest_source")

if [ "$(uname -m)" != loongarch64 ]; then
    echo "SKIP: the X11 async bridge test requires a LoongArch host"
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
if ! printf '#include <X11/Xlibint.h>\n' | \
     "$guest_compiler" --sysroot="$guest_root" -E -x c - \
     >/dev/null 2>&1; then
    echo "SKIP: x86_64 Xlib internal development headers are unavailable"
    exit 77
fi
if ! printf '#include <X11/Xlibint.h>\n' | \
     "$native_compiler" -E -x c - >/dev/null 2>&1; then
    echo "SKIP: native Xlib internal development headers are unavailable"
    exit 77
fi

workdir=$(mktemp -d)
guest_library_dir="$workdir/guest-lib"
host_probe="$workdir/libX11-async-probe.so.6"
guest_program="$workdir/x11-async-bridge-guest"

cleanup()
{
    rm -rf "$workdir"
}
trap cleanup EXIT HUP INT TERM
ulimit -c 0

mkdir -p "$guest_library_dir"

"$native_compiler" -shared -fPIC -O2 -Wall -Wextra -Werror \
    -I"$source_dir" -Wl,-soname,libX11.so.6 \
    "$probe_source" -o "$host_probe"

"$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
    -Wall -Wextra -Werror -I"$source_dir" \
    -Wl,-soname,libX11.so.6 "$dummy_source" \
    -o "$guest_library_dir/libX11.so.6"
ln -s libX11.so.6 "$guest_library_dir/libX11.so"

"$guest_compiler" --sysroot="$guest_root" -O2 -Wall -Wextra -Werror \
    -I"$source_dir" "$guest_source" -L"$guest_library_dir" \
    -Wl,-rpath,'$ORIGIN/guest-lib' -lX11 -pthread -o "$guest_program"

run_success_mode()
{
    mode=$1
    output="$workdir/$mode.log"

    set +e
    LD_PRELOAD="$host_probe" \
    BOX64_LD_LIBRARY_PATH="$guest_library_dir" \
    LATX_AOT=0 \
    LATX_KZT=1 \
    "$emulator" -U LD_PRELOAD -L "$guest_root" "$guest_program" "$mode" \
        >"$output" 2>&1
    result=$?
    set -e
    if [ "$result" -ne 0 ]; then
        echo "FAIL: async bridge mode $mode exited with status $result" >&2
        sed -n '1,200p' "$output" >&2
        exit "$result"
    fi
    grep -q 'ASYNC_NATIVE_ENTER:' "$output"
    grep -q 'ASYNC_NATIVE_BRIDGE_TARGET:' "$output"
    grep -q 'ASYNC_GUEST_CALLBACK:' "$output"
    if grep -q 'ASYNC_GUEST_DUMMY_CALLED:' "$output"; then
        echo "FAIL: mode $mode executed the guest dummy libX11" >&2
        sed -n '1,200p' "$output" >&2
        exit 1
    fi
}

run_success_mode dispatch
run_success_mode deterministic
run_success_mode concurrent

exhaust_output="$workdir/exhaust.log"
set +e
LD_PRELOAD="$host_probe" \
BOX64_LD_LIBRARY_PATH="$guest_library_dir" \
LATX_AOT=0 \
LATX_KZT=1 \
"$emulator" -U LD_PRELOAD -L "$guest_root" "$guest_program" exhaust \
    >"$exhaust_output" 2>&1
exhaust_result=$?
set -e
if [ "$exhaust_result" -eq 0 ]; then
    echo "FAIL: callback slot exhaustion did not stop execution" >&2
    sed -n '1,200p' "$exhaust_output" >&2
    exit 1
fi
if grep -q 'ASYNC_NATIVE_ENTER:' "$exhaust_output"; then
    echo "FAIL: callback slot exhaustion entered native Xlib" >&2
    sed -n '1,200p' "$exhaust_output" >&2
    exit 1
fi

echo "PASS: KZT X11 wrapper, native bridge target, and guest callbacks observed"
