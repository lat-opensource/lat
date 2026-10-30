#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu

emulator=$(readlink -f "$1")
source_dir=$(dirname "$(readlink -f "$2")")
guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_cc=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
native_cc=${LATX_NATIVE_CC:-cc}
workdir=$(mktemp -d)
cleanup() {
    status=$?
    if [ "$status" = 0 ]; then
        rm -rf "$workdir"
    else
        echo "KZT RAS failure evidence: $workdir" >&2
    fi
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

if [ "$(uname -m)" != loongarch64 ] || [ ! -d "$guest_root" ]; then
    echo 'SKIP: KZT requires LoongArch and an x86_64 guest sysroot'
    exit 77
fi
if ! command -v "$native_cc" >/dev/null 2>&1 ||
        ! printf '#include <X11/Xlib.h>\n#include <X11/Xlibint.h>\n' | \
            "$native_cc" -E -x c - >/dev/null 2>&1; then
    echo 'SKIP: native compiler and Xlib development headers are required'
    exit 77
fi
if ! command -v nm >/dev/null ||
        ! nm -a "$emulator" 2>/dev/null |
            grep -F 'hras_test_marker_init' >/dev/null; then
    echo 'SKIP: HRAS marker checks require a CONFIG_LATX_DEBUG build'
    exit 77
fi

guest=${LATX_HRAS_TEST_KZT_GUEST:-}
guest_lib=${LATX_HRAS_TEST_KZT_LIBDIR:-}
if [ -z "$guest" ]; then
    if ! command -v "$guest_cc" >/dev/null 2>&1; then
        echo 'SKIP: need an x86 compiler or LATX_HRAS_TEST_KZT_GUEST'
        exit 77
    fi
    if ! printf '#include <X11/Xlib.h>\n#include <X11/Xlibint.h>\n' | \
            "$guest_cc" --sysroot="$guest_root" -E -x c - >/dev/null 2>&1; then
        echo 'SKIP: guest sysroot lacks Xlib development headers'
        exit 77
    fi
    printf '%s\n' 'int main(void) { return 0; }' >"$workdir/probe.c"
    if ! "$guest_cc" --sysroot="$guest_root" "$workdir/probe.c" \
            -o "$workdir/guest-probe" >"$workdir/guest-probe.log" 2>&1; then
        echo 'SKIP: x86 compiler cannot link against the guest sysroot'
        cat "$workdir/guest-probe.log"
        exit 77
    fi
    guest=$workdir/guest
    guest_lib=$workdir/guest-lib
    mkdir -p "$guest_lib"
    "$guest_cc" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wl,-soname,libX11.so.6 "$source_dir/x11-async-bridge-dummy.c" \
        -o "$guest_lib/libX11.so.6"
    "$guest_cc" --sysroot="$guest_root" -O2 -fno-optimize-sibling-calls \
        -nostartfiles -Wl,-e,_start "$source_dir/hardware-ras-kzt.c" \
        "$guest_lib/libX11.so.6" -o "$guest"
fi
if [ -z "$guest_lib" ] || [ ! -f "$guest_lib/libX11.so.6" ]; then
    echo 'FAIL: supply the guest dummy libX11 with LATX_HRAS_TEST_KZT_LIBDIR'
    exit 1
fi
printf '%s\n' 'int probe(void) { return 0; }' >"$workdir/native-probe.c"
if ! "$native_cc" -shared -fPIC "$workdir/native-probe.c" \
        -o "$workdir/compiler-probe.so" >"$workdir/native-probe.log" 2>&1; then
    echo 'SKIP: native compiler cannot link a shared probe'
    cat "$workdir/native-probe.log"
    exit 77
fi
"$native_cc" -shared -fPIC -O2 -Wl,-soname,libX11.so.6 \
    "$source_dir/x11-async-bridge-probe.c" -o "$workdir/native-probe.so"
for aot in ${LATX_HRAS_TEST_AOT_MODES:-1 2}; do
for hras in 0 1; do
    home="$workdir/home-$aot-$hras"
    echo "KZT case AOT=$aot HRAS=$hras HOME=$home"
    mkdir -p "$home"
    for run in 1 2 3 4 5; do
        echo "KZT run $run"
        generate=1
        if [ "$run" = 5 ]; then
            generate=0
            rm -f "$home/loaded"
        fi
        rm -f "$home/hras"
        timeout 45 env HOME="$home" LD_PRELOAD="$workdir/native-probe.so" \
            BOX64_LD_LIBRARY_PATH="$guest_lib" LATX_AOT="$aot" LATX_KZT=1 \
            LATX_HRAS="$hras" LATX_TU=1 LATX_JRRA=0 LATX_JRRA_STACK=0 \
            LATX_AOT_GENERATE="$generate" LATX_SOFTFPU=0 \
            LATX_TEST_AOT_LOAD_MARKER="$home/loaded" \
            LATX_TEST_HRAS_MARKER="$home/hras" \
            "$emulator" -L "$guest_root" "$guest" \
            >"$workdir/log" 2>&1 || {
                cat "$workdir/log" >&2
                exit 1
            }
        grep -q ASYNC_NATIVE_ENTER: "$workdir/log"
        grep -q ASYNC_NATIVE_BRIDGE_TARGET: "$workdir/log"
        grep -q 'PASS: native Xlib' "$workdir/log"
        if grep -q ASYNC_GUEST_DUMMY_CALLED: "$workdir/log"; then
            echo 'FAIL: the native wrapper was bypassed' >&2
            exit 1
        fi
    done
    if [ "$hras" = 1 ]; then
        if ! grep -qx call "$home/hras" ||
                ! grep -qx ret "$home/hras"; then
            echo 'FAIL: missing HRAS CALL/RET hints' >&2
            exit 1
        fi
    elif [ -e "$home/hras" ]; then
        echo 'FAIL: HRAS hints emitted while disabled' >&2
        exit 1
    fi
    grep -Fx "$guest" "$home/loaded" >/dev/null
    echo "PASS: KZT callback and load-only AOT with AOT=$aot HRAS=$hras"
done
done
