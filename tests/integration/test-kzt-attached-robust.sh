#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu
export LATX_KZT_GUEST_TLS=1

emulator=$1
guest_source=$2
probe_source=$3
dummy_source=$4
source_dir=$(dirname "$guest_source")

if [ "$(uname -m)" != loongarch64 ]; then
    echo "SKIP: KZT attached robust mutex test requires a LoongArch host"
    exit 77
fi

guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_compiler=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
native_compiler=${LATX_NATIVE_CC:-cc}
guest_artifacts=${LATX_KZT_ROBUST_GUEST_ARTIFACT_DIR:-}
workdir=$(mktemp -d)
guest_lib="$workdir/guest-lib"
guest_program="$workdir/kzt-attached-robust-guest"
host_probe="$workdir/libX11.so.6"

cleanup()
{
    rm -rf "$workdir"
}
trap cleanup EXIT HUP INT TERM
mkdir -p "$guest_lib"

"$native_compiler" -shared -fPIC -O2 -Wall -Wextra -Werror \
    -I"$source_dir" -Wl,-soname,libX11.so.6 \
    "$probe_source" -pthread -o "$host_probe"

if [ -n "$guest_artifacts" ]; then
    if [ ! -x "$guest_artifacts/kzt-attached-robust-guest" ] ||
       [ ! -f "$guest_artifacts/libX11.so.6" ] ||
       [ ! -f "$guest_artifacts/sources.sha256" ] ||
       [ ! -f "$guest_artifacts/artifacts.sha256" ]; then
        echo "FAIL: incomplete prebuilt attached robust artifacts" >&2
        exit 2
    fi
    for source_file in "$guest_source" "$dummy_source"; do
        source_name=$(basename "$source_file")
        expected_hash=$(sha256sum "$source_file" | awk '{print $1}')
        artifact_hash=$(awk -v name="$source_name" \
            '$2 == name {print $1}' \
            "$guest_artifacts/sources.sha256")

        if [ -z "$artifact_hash" ] ||
           [ "$artifact_hash" != "$expected_hash" ]; then
            echo "FAIL: prebuilt robust source mismatch: $source_name" >&2
            exit 2
        fi
    done
    if ! (cd "$guest_artifacts" && sha256sum -c artifacts.sha256); then
        echo "FAIL: prebuilt attached robust artifact mismatch" >&2
        exit 2
    fi
    cp "$guest_artifacts/kzt-attached-robust-guest" "$guest_program"
    cp "$guest_artifacts/libX11.so.6" "$guest_lib/libX11.so.6"
else
    if ! command -v "$guest_compiler" >/dev/null 2>&1; then
        echo "SKIP: x86_64 Guest compiler is unavailable"
        exit 77
    fi
    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror -I"$source_dir" \
        -Wl,-soname,libX11.so.6 "$dummy_source" \
        -o "$guest_lib/libX11.so.6"
    ln -s libX11.so.6 "$guest_lib/libX11.so"
    "$guest_compiler" --sysroot="$guest_root" -O2 \
        -Wall -Wextra -Werror -I"$source_dir" "$guest_source" \
        -L"$guest_lib" -lX11 -ldl -pthread -o "$guest_program"
fi

for aot_mode in 0 1; do
    output="$workdir/output-$aot_mode.log"
    set +e
    LD_PRELOAD="$host_probe" \
    BOX64_LD_LIBRARY_PATH="$guest_lib" \
    LATX_AOT=$aot_mode LATX_KZT=1 \
        "$emulator" -U LD_PRELOAD \
        -E "LD_LIBRARY_PATH=$guest_lib" -L "$guest_root" \
        "$guest_program" >"$output" 2>&1
    result=$?
    set -e
    if [ "$result" -ne 0 ]; then
        sed -n '1,160p' "$output" >&2
        exit "$result"
    fi
    grep -q \
        '^PASS: attached robust owner death woke blocked Guest waiter$' \
        "$output"
    cat "$output"
done
