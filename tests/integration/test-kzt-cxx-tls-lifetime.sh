#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu
export LATX_KZT_GUEST_TLS=1

emulator=$1
guest_source=$2
plugin_source=$3
destructor_source=$4
probe_source=$5
dummy_source=$6
source_dir=$(dirname "$guest_source")

if [ "$(uname -m)" != loongarch64 ]; then
    echo "SKIP: KZT C++ TLS lifetime test requires a LoongArch host"
    exit 77
fi

guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_compiler=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
guest_cxx_compiler=${LATX_X86_64_CXX:-x86_64-linux-gnu-g++}
native_compiler=${LATX_NATIVE_CC:-cc}
guest_artifacts=${LATX_KZT_CXX_TLS_GUEST_ARTIFACT_DIR:-}
workdir=$(mktemp -d)
guest_lib="$workdir/guest-lib"
guest_program="$workdir/kzt-cxx-tls-lifetime-guest"
guest_plugin="$guest_lib/libkzt-cxx-tls-lifetime-plugin.so"
guest_destructor="$guest_lib/libkzt-cxx-tls-lifetime-destructor.so"
host_probe="$workdir/libX11.so.6"
output="$workdir/output.log"

verify_prebuilt_source()
{
    source_file=$1
    source_name=$(basename "$source_file")
    expected_hash=$(sha256sum "$source_file" | awk '{print $1}')
    artifact_hash=$(awk -v name="$source_name" \
        '$2 == name {print $1}' "$guest_artifacts/sources.sha256")

    if [ -z "$artifact_hash" ] ||
       [ "$artifact_hash" != "$expected_hash" ]; then
        echo "FAIL: prebuilt C++ TLS source mismatch: $source_name" >&2
        exit 2
    fi
}

cleanup()
{
    rm -rf "$workdir"
}
trap cleanup EXIT HUP INT TERM

if [ ! -d "$guest_root" ]; then
    echo "SKIP: x86_64 Guest sysroot not found: $guest_root"
    exit 77
fi
if ! command -v "$native_compiler" >/dev/null 2>&1; then
    echo "SKIP: native compiler not found: $native_compiler"
    exit 77
fi
mkdir -p "$guest_lib"

"$native_compiler" -shared -fPIC -O2 -Wall -Wextra -Werror \
    -I"$source_dir" -Wl,-soname,libX11.so.6 \
    "$probe_source" -pthread -o "$host_probe"

if [ -n "$guest_artifacts" ]; then
    if [ ! -x "$guest_artifacts/kzt-cxx-tls-lifetime-guest" ] ||
       [ ! -f "$guest_artifacts/libX11.so.6" ] ||
       [ ! -f \
         "$guest_artifacts/libkzt-cxx-tls-lifetime-plugin.so" ] ||
       [ ! -f \
         "$guest_artifacts/libkzt-cxx-tls-lifetime-destructor.so" ] ||
       [ ! -f "$guest_artifacts/sources.sha256" ] ||
       [ ! -f "$guest_artifacts/artifacts.sha256" ]; then
        echo "FAIL: incomplete prebuilt C++ TLS lifetime artifacts" >&2
        exit 2
    fi
    verify_prebuilt_source "$guest_source"
    verify_prebuilt_source "$plugin_source"
    verify_prebuilt_source "$destructor_source"
    verify_prebuilt_source \
        "$source_dir/kzt-cxx-tls-lifetime-shared.h"
    verify_prebuilt_source "$dummy_source"
    if ! (cd "$guest_artifacts" && sha256sum -c artifacts.sha256); then
        echo "FAIL: prebuilt C++ TLS artifact hash mismatch" >&2
        exit 2
    fi
    cp "$guest_artifacts/kzt-cxx-tls-lifetime-guest" "$guest_program"
    cp "$guest_artifacts/libX11.so.6" "$guest_lib/libX11.so.6"
    cp "$guest_artifacts/libkzt-cxx-tls-lifetime-plugin.so" \
        "$guest_plugin"
    cp "$guest_artifacts/libkzt-cxx-tls-lifetime-destructor.so" \
        "$guest_destructor"
else
    if ! command -v "$guest_compiler" >/dev/null 2>&1 ||
       ! command -v "$guest_cxx_compiler" >/dev/null 2>&1; then
        echo "SKIP: x86_64 Guest compilers are unavailable"
        exit 77
    fi
    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror -I"$source_dir" \
        -Wl,-soname,libX11.so.6 "$dummy_source" \
        -o "$guest_lib/libX11.so.6"
    "$guest_cxx_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror -I"$source_dir" \
        -Wl,-soname,libkzt-cxx-tls-lifetime-destructor.so \
        "$destructor_source" -o "$guest_destructor"
    "$guest_cxx_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror -I"$source_dir" "$plugin_source" \
        -L"$guest_lib" -lkzt-cxx-tls-lifetime-destructor \
        -Wl,-rpath,'$ORIGIN' -o "$guest_plugin"
    "$guest_compiler" --sysroot="$guest_root" -O2 \
        -Wall -Wextra -Werror -I"$source_dir" "$guest_source" \
        -L"$guest_lib" -lX11 -ldl -pthread -o "$guest_program"
fi

for aot_mode in 0 1; do
    output="$workdir/output-aot-$aot_mode.log"
    set +e
    LD_PRELOAD="$host_probe" \
    BOX64_LD_LIBRARY_PATH="$guest_lib" \
    LATX_AOT=$aot_mode LATX_KZT=1 \
        "$emulator" -U LD_PRELOAD \
        -E "LD_LIBRARY_PATH=$guest_lib" -L "$guest_root" \
        "$guest_program" "$guest_plugin" >"$output" 2>&1
    result=$?
    set -e
    if [ "$result" -ne 0 ]; then
        sed -n '1,200p' "$output" >&2
        echo "FAIL: LATX_AOT=$aot_mode exited $result" >&2
        exit "$result"
    fi
    grep -q '^PASS: C++ thread_local DSO survived until Host-thread exit$' \
        "$output"
    cat "$output"
done
