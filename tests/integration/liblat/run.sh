#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
runtime=$1
mode=$2
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_cc=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
artifacts=${LIBLAT_GUEST_ARTIFACT_DIR:-}
native_cc=${LATX_NATIVE_CC:-cc}
if [ "$(uname -m)" != loongarch64 ] || [ ! -d "$guest_root" ]; then
    echo "SKIP: requires LoongArch and an x86-64 Guest runtime"
    exit 77
fi
if ! command -v "$native_cc" >/dev/null 2>&1; then
    echo "SKIP: native compiler unavailable"
    exit 77
fi
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
probe=$work/host-test
"$native_cc" -O2 -Wall -Wextra -Werror -pthread \
    -I"$source_dir/../../../include" "$source_dir/host.c" -ldl -o "$probe"
if [ -n "$artifacts" ]; then
    for source in bootstrap.c guest.c; do
        expected=$(sha256sum "$source_dir/$source" | cut -d ' ' -f 1)
        test "$(cat "$artifacts/$source.sha256")" = "$expected" || {
            echo "FAIL: Guest artifacts do not match $source" >&2
            exit 1
        }
    done
    for file in bootstrap bootstrap-fail libguest.so; do
        test -f "$artifacts/$file" || exit 1
        cp "$artifacts/$file" "$work/$file"
    done
else
    if ! command -v "$guest_cc" >/dev/null 2>&1; then
        echo "SKIP: x86-64 compiler unavailable; set LIBLAT_GUEST_ARTIFACT_DIR"
        exit 77
    fi
    "$guest_cc" --sysroot="$guest_root" -no-pie -O2 \
        "$source_dir/bootstrap.c" -Wl,--no-as-needed -ldl -pthread \
        -o "$work/bootstrap"
    "$guest_cc" --sysroot="$guest_root" -no-pie -O2 -DBOOTSTRAP_STATUS=7 \
        "$source_dir/bootstrap.c" -Wl,--no-as-needed -ldl -pthread \
        -o "$work/bootstrap-fail"
    "$guest_cc" --sysroot="$guest_root" -shared -fPIC -O2 \
        "$source_dir/guest.c" -pthread -o "$work/libguest.so"
fi
bootstrap=$work/bootstrap
if [ "$mode" = bootstrap-status ]; then
    bootstrap=$work/bootstrap-fail
fi
export LAT_LD_PREFIX="$guest_root"
unset LATX_KZT_GUEST_TLS
"$probe" "$runtime" "$bootstrap" "$work/libguest.so" "$mode"
