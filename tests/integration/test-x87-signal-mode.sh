#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

guest_dir=${LATX_X87_SIGNAL_GUEST_DIR:-$workdir}
if [ -n "${LATX_X87_SIGNAL_GUEST_DIR:-}" ]; then
    : # Allow guests built from this source on an x86 build host.
elif command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

for case_id in 0 1 2 3 4; do
    guest="$guest_dir/x87-signal-mode-$case_id"
    if [ -z "${LATX_X87_SIGNAL_GUEST_DIR:-}" ]; then
        "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
            -Wl,--build-id=none -DCASE=$case_id "$source_file" -o "$guest"
    fi
    if [ ! -x "$guest" ]; then
        echo "FAIL: guest executable missing: $guest" >&2
        exit 1
    fi
    for softfpu in 0 1 2; do
        for tu in 0 1; do
            if LATX_AOT=0 LATX_MT=0 LATX_TU=$tu LATX_SOFTFPU=$softfpu \
                timeout -s KILL 10 "$emulator" "$guest" \
                > "$workdir/result"; then
                echo "PASS: x87 signal case=$case_id softfpu=$softfpu tu=$tu"
            else
                status=$?
                printf 'FAIL: case=%s softfpu=%s tu=%s exit=%s\n' \
                    "$case_id" "$softfpu" "$tu" "$status" >&2
                od -An -tx1 "$workdir/result" >&2
                exit "$status"
            fi
        done
    done
done
