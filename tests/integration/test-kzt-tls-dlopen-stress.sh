#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu
export LATX_KZT_GUEST_TLS=1

emulator=$1
guest_source=$2
plugin_a_source=$3
plugin_b_source=$4
ie_plugin_source=$5
probe_source=$6
dummy_source=$7
source_dir=$(dirname "$guest_source")

if [ "$(uname -m)" != loongarch64 ]; then
    echo "SKIP: KZT TLS dlopen stress requires a LoongArch host"
    exit 77
fi

guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_compiler=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
native_compiler=${LATX_NATIVE_CC:-cc}
guest_artifacts=${LATX_KZT_TLS_STRESS_ARTIFACT_DIR:-}
workdir=$(mktemp -d)
guest_lib="$workdir/guest-lib"
guest_program="$workdir/kzt-tls-dlopen-stress-guest"
plugin_a="$guest_lib/libkzt-tls-stress-a.so"
plugin_b="$guest_lib/libkzt-tls-stress-b.so"
ie_plugin="$guest_lib/libkzt-tls-stress-ie.so"
host_probe="$workdir/libX11.so.6"
wrapped_probe="$guest_lib/libxcb.so.1"

verify_prebuilt_source()
{
    source_file=$1
    source_name=$(basename "$source_file")
    expected_hash=$(sha256sum "$source_file" | awk '{print $1}')
    artifact_hash=$(awk -v name="$source_name" \
        '$2 == name {print $1}' "$guest_artifacts/sources.sha256")

    if [ -z "$artifact_hash" ] ||
       [ "$artifact_hash" != "$expected_hash" ]; then
        echo "FAIL: prebuilt TLS stress source mismatch: $source_name" >&2
        exit 2
    fi
}

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
    if [ ! -x "$guest_artifacts/kzt-tls-dlopen-stress-guest" ] ||
       [ ! -f "$guest_artifacts/libX11.so.6" ] ||
       [ ! -f "$guest_artifacts/libxcb.so.1" ] ||
       [ ! -f "$guest_artifacts/libkzt-tls-stress-a.so" ] ||
       [ ! -f "$guest_artifacts/libkzt-tls-stress-b.so" ] ||
       [ ! -f "$guest_artifacts/libkzt-tls-stress-ie.so" ] ||
       [ ! -f "$guest_artifacts/sources.sha256" ] ||
       [ ! -f "$guest_artifacts/artifacts.sha256" ]; then
        echo "FAIL: incomplete prebuilt TLS stress artifacts" >&2
        exit 2
    fi
    verify_prebuilt_source "$guest_source"
    verify_prebuilt_source "$plugin_a_source"
    verify_prebuilt_source "$plugin_b_source"
    verify_prebuilt_source "$ie_plugin_source"
    verify_prebuilt_source "$dummy_source"
    verify_prebuilt_source \
        "$source_dir/kzt-tls-dlopen-stress-shared.h"
    if ! (cd "$guest_artifacts" && sha256sum -c artifacts.sha256); then
        echo "FAIL: prebuilt TLS stress artifact hash mismatch" >&2
        exit 2
    fi
    cp "$guest_artifacts/kzt-tls-dlopen-stress-guest" "$guest_program"
    cp "$guest_artifacts/libX11.so.6" "$guest_lib/libX11.so.6"
    cp "$guest_artifacts/libxcb.so.1" "$wrapped_probe"
    cp "$guest_artifacts/libkzt-tls-stress-a.so" "$plugin_a"
    cp "$guest_artifacts/libkzt-tls-stress-b.so" "$plugin_b"
    cp "$guest_artifacts/libkzt-tls-stress-ie.so" "$ie_plugin"
else
    if ! command -v "$guest_compiler" >/dev/null 2>&1; then
        exit 77
    fi
    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror -I"$source_dir" \
        -Wl,-soname,libX11.so.6 "$dummy_source" \
        -o "$guest_lib/libX11.so.6"
    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror -I"$source_dir" \
        -Wl,-soname,libxcb.so.1 "$dummy_source" \
        -o "$wrapped_probe"
    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror "$plugin_a_source" -o "$plugin_a"
    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror "$plugin_b_source" -o "$plugin_b"
    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror -I"$source_dir" \
        -ftls-model=initial-exec \
        "$ie_plugin_source" -o "$ie_plugin"
    "$guest_compiler" --sysroot="$guest_root" -O2 \
        -Wall -Wextra -Werror -I"$source_dir" "$guest_source" \
        -Wl,--export-dynamic -L"$guest_lib" -lX11 -ldl -pthread \
        -o "$guest_program"
fi

for aot_mode in 0 1; do
    aot_home="$workdir/aot-home-$aot_mode"
    cache_passes=cold

    mkdir -p "$aot_home"
    if [ "$aot_mode" -eq 1 ]; then
        cache_passes='cold warm'
    fi
    for cache_pass in $cache_passes; do
        output="$workdir/output-$aot_mode-$cache_pass.log"
        set +e
        HOME="$aot_home" LD_PRELOAD="$host_probe" \
        BOX64_LD_LIBRARY_PATH="$guest_lib" \
        LATX_AOT=$aot_mode LATX_KZT=1 \
            "$emulator" -U LD_PRELOAD \
            -E "LD_LIBRARY_PATH=$guest_lib" -L "$guest_root" \
            "$guest_program" "$plugin_a" "$plugin_b" "$ie_plugin" \
            >"$output" 2>&1
        result=$?
        set -e
        if [ "$result" -ne 0 ]; then
            sed -n '1,200p' "$output" >&2
            exit "$result"
        fi
        grep -q '^PASS: attached Guest TLS survived 64 A/B dlopen cycles ' \
            "$output"
        if [ "$aot_mode" -eq 1 ] && [ "$cache_pass" = cold ] &&
           ! find "$aot_home/.cache/latx" -type f \
               -name '*.aot2' -size +0c -print -quit | grep -q .; then
            echo "FAIL: cold AOT run did not publish a cache" >&2
            exit 1
        fi
        cat "$output"
    done
done
