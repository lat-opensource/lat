#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

emulator=$1
guest_source=$2
source_dir=$(dirname "$guest_source")

if [ "$(uname -m)" != loongarch64 ]; then
    echo "SKIP: KZT pthread TSD alias test requires a LoongArch host"
    exit 77
fi

guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_compiler=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
guest_artifacts=${LATX_KZT_TSD_GUEST_ARTIFACT_DIR:-}
workdir=$(mktemp -d)
guest_program="$workdir/kzt-pthread-tsd-alias-guest"

cleanup()
{
    rm -rf "$workdir"
}
trap cleanup EXIT HUP INT TERM

if [ ! -d "$guest_root" ]; then
    echo "SKIP: x86_64 Guest sysroot not found: $guest_root"
    exit 77
fi

if [ -n "$guest_artifacts" ]; then
    if [ ! -x "$guest_artifacts/kzt-pthread-tsd-alias-guest" ] ||
       [ ! -f "$guest_artifacts/sources.sha256" ]; then
        echo "FAIL: incomplete prebuilt Guest TSD artifacts" >&2
        exit 2
    fi
    for source_file in \
        "$guest_source" \
        "$source_dir/kzt-pthread-tsd-alias.h"; do
        source_name=$(basename "$source_file")
        expected_source_hash=$(sha256sum "$source_file" | awk '{print $1}')
        artifact_source_hash=$(awk -v name="$source_name" \
            '$2 == name {print $1}' "$guest_artifacts/sources.sha256")
        if [ -z "$artifact_source_hash" ] ||
           [ "$artifact_source_hash" != "$expected_source_hash" ]; then
            echo "FAIL: prebuilt Guest TSD source mismatch: " \
                 "$source_name" >&2
            exit 2
        fi
    done
    cp "$guest_artifacts/kzt-pthread-tsd-alias-guest" "$guest_program"
else
    if ! command -v "$guest_compiler" >/dev/null 2>&1; then
        echo "SKIP: x86_64 cross compiler not found: $guest_compiler"
        exit 77
    fi
    "$guest_compiler" --sysroot="$guest_root" -O2 \
        -Wall -Wextra -Werror "$guest_source" -ldl -pthread \
        -o "$guest_program"
fi

for aot_mode in 0 1; do
    for kzt_mode in 0 1; do
        output="$workdir/output-$aot_mode-$kzt_mode.log"
        guest_args=
        if [ "$kzt_mode" -eq 0 ]; then
            guest_args=--single-thread
        fi
        set +e
        LATX_AOT=$aot_mode LATX_KZT=$kzt_mode \
            "$emulator" -L "$guest_root" "$guest_program" $guest_args \
            >"$output" 2>&1
        result=$?
        set -e
        if [ "$result" -ne 0 ]; then
            sed -n '1,160p' "$output" >&2
            echo "FAIL: LATX_AOT=$aot_mode LATX_KZT=$kzt_mode " \
                 "exited $result" >&2
            exit "$result"
        fi
        grep -q '^PASS: Guest pthread TSD remains Guest-libc authoritative$' \
            "$output"
    done
done

echo "PASS: KZT on/off preserve Guest pthread TSD alias coherence"
