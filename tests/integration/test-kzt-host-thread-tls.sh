#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu
export LATX_KZT_GUEST_TLS=1

emulator=$1
guest_source=$2
plugin_source=$3
probe_source=$4
dummy_source=$5
ie_plugin_source=$6
cxx_plugin_source=$7
plugin_b_source=$8
tlsdesc_plugin_source=$9
source_dir=$(dirname "$guest_source")

if [ "$(uname -m)" != loongarch64 ]; then
    echo "SKIP: KZT Host-thread TLS test requires a LoongArch host"
    exit 77
fi

guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_compiler=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
guest_cxx_compiler=${LATX_X86_64_CXX:-x86_64-linux-gnu-g++}
native_compiler=${LATX_NATIVE_CC:-cc}
guest_artifacts=${LATX_KZT_TLS_GUEST_ARTIFACT_DIR:-}
guest_stress_iterations=32

if [ ! -d "$guest_root" ]; then
    echo "SKIP: x86_64 Guest sysroot not found: $guest_root"
    exit 77
fi
if [ -z "$guest_artifacts" ]; then
    if ! command -v "$guest_compiler" >/dev/null 2>&1 ||
       ! command -v "$guest_cxx_compiler" >/dev/null 2>&1; then
        echo "SKIP: x86_64 Guest compilers are unavailable"
        exit 77
    fi
fi
if ! command -v "$native_compiler" >/dev/null 2>&1; then
    echo "SKIP: native compiler not found: $native_compiler"
    exit 77
fi
if [ -z "$guest_artifacts" ] && \
   ! printf '#include <X11/Xlibint.h>\n' | \
     "$guest_compiler" --sysroot="$guest_root" -E -x c - \
     >/dev/null 2>&1; then
    echo "SKIP: x86_64 Xlib development headers are unavailable"
    exit 77
fi
if ! printf '#include <X11/Xlibint.h>\n' | \
     "$native_compiler" -E -x c - >/dev/null 2>&1; then
    echo "SKIP: native Xlib development headers are unavailable"
    exit 77
fi

workdir=$(mktemp -d)
guest_lib="$workdir/guest-lib"
host_probe="$workdir/libX11.so.6"
guest_program="$workdir/kzt-host-thread-tls-guest"
guest_plugin="$guest_lib/libkzt-host-thread-tls-plugin.so"
guest_ie_plugin="$guest_lib/libkzt-host-thread-tls-ie-plugin.so"
guest_cxx_plugin="$guest_lib/libkzt-host-thread-cxx-tls-plugin.so"
guest_plugin_b="$guest_lib/libkzt-host-thread-tls-plugin-b.so"
guest_tlsdesc_plugin="$guest_lib/libkzt-host-thread-tls-tlsdesc-plugin.so"
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
        echo "FAIL: prebuilt Guest source mismatch: $source_name" >&2
        exit 2
    fi
}

guest_libc_build_id()
{
    for libc_path in \
        "$guest_root/lib/x86_64-linux-gnu/libc.so.6" \
        "$guest_root/usr/lib/x86_64-linux-gnu/libc.so.6" \
        "$guest_root/usr/lib64/libc.so.6"; do
        if [ -f "$libc_path" ]; then
            readelf -n "$libc_path" 2>/dev/null | \
                awk '/Build ID:/ {print $3; exit}'
            return
        fi
    done
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
    artifact_guest_lib="$guest_artifacts/guest-lib"
    if [ ! -x "$guest_artifacts/kzt-host-thread-tls-guest" ] || \
       [ ! -f "$artifact_guest_lib/libX11.so.6" ] || \
       [ ! -f \
         "$artifact_guest_lib/libkzt-host-thread-tls-plugin.so" ] ||
       [ ! -f \
         "$artifact_guest_lib/libkzt-host-thread-tls-ie-plugin.so" ] ||
       [ ! -f \
         "$artifact_guest_lib/libkzt-host-thread-tls-plugin-b.so" ] ||
       [ ! -f \
         "$artifact_guest_lib/libkzt-host-thread-tls-tlsdesc-plugin.so" ] ||
       [ ! -f "$guest_artifacts/sources.sha256" ] ||
       [ ! -f "$guest_artifacts/artifacts.sha256" ] ||
       [ ! -f "$guest_artifacts/build-info.txt" ]; then
        echo "FAIL: incomplete prebuilt Guest artifacts: $guest_artifacts" >&2
        exit 2
    fi
    verify_prebuilt_source "$guest_source"
    verify_prebuilt_source "$source_dir/kzt-pthread-tsd-alias.h"
    verify_prebuilt_source "$plugin_source"
    verify_prebuilt_source "$dummy_source"
    verify_prebuilt_source "$ie_plugin_source"
    verify_prebuilt_source "$cxx_plugin_source"
    verify_prebuilt_source "$plugin_b_source"
    verify_prebuilt_source "$tlsdesc_plugin_source"
    if ! (cd "$guest_artifacts" && sha256sum -c artifacts.sha256); then
        echo "FAIL: prebuilt Guest artifact hash mismatch" >&2
        exit 2
    fi
    expected_libc_build_id=$(guest_libc_build_id)
    artifact_libc_build_id=$(awk -F= \
        '$1 == "guest-libc-build-id" {print $2}' \
        "$guest_artifacts/build-info.txt")
    if [ -z "$expected_libc_build_id" ] ||
       [ "$artifact_libc_build_id" != "$expected_libc_build_id" ]; then
        echo "FAIL: prebuilt Guest libc build-id mismatch" >&2
        exit 2
    fi
    if grep -q 'linked against glibc 2.28' \
        "$guest_artifacts/build-info.txt"; then
        guest_stress_iterations=2
    fi
    cp "$guest_artifacts/kzt-host-thread-tls-guest" "$guest_program"
    cp "$artifact_guest_lib/libX11.so.6" "$guest_lib/libX11.so.6"
    cp "$artifact_guest_lib/libkzt-host-thread-tls-plugin.so" \
        "$guest_plugin"
    cp "$artifact_guest_lib/libkzt-host-thread-tls-ie-plugin.so" \
        "$guest_ie_plugin"
    cp "$artifact_guest_lib/libkzt-host-thread-tls-plugin-b.so" \
        "$guest_plugin_b"
    cp "$artifact_guest_lib/libkzt-host-thread-tls-tlsdesc-plugin.so" \
        "$guest_tlsdesc_plugin"
    if [ ! -f \
         "$artifact_guest_lib/libkzt-host-thread-cxx-tls-plugin.so" ]; then
        echo "FAIL: incomplete prebuilt Guest C++ TLS artifact" >&2
        exit 2
    fi
    cp "$artifact_guest_lib/libkzt-host-thread-cxx-tls-plugin.so" \
        "$guest_cxx_plugin"
else
    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror -I"$source_dir" \
        -Wl,-soname,libX11.so.6 "$dummy_source" \
        -o "$guest_lib/libX11.so.6"

    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror "$plugin_source" -o "$guest_plugin"

    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror -ftls-model=initial-exec \
        "$ie_plugin_source" -o "$guest_ie_plugin"

    "$guest_cxx_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror "$cxx_plugin_source" \
        -o "$guest_cxx_plugin"

    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror "$plugin_b_source" \
        -o "$guest_plugin_b"

    "$guest_compiler" --sysroot="$guest_root" -shared -fPIC -O2 \
        -Wall -Wextra -Werror -mtls-dialect=gnu2 \
        "$tlsdesc_plugin_source" -o "$guest_tlsdesc_plugin"

    "$guest_compiler" --sysroot="$guest_root" -O2 \
        -Wall -Wextra -Werror -I"$source_dir" \
        "$guest_source" \
        -L"$guest_lib" \
        -lX11 -ldl -lresolv -pthread -o "$guest_program"
fi

if ! readelf -rW "$guest_plugin" | grep -q 'R_X86_64_DTPMOD64' ||
   ! readelf -rW "$guest_ie_plugin" | grep -q 'R_X86_64_TPOFF64' ||
   ! readelf -rW "$guest_plugin_b" | grep -q 'R_X86_64_DTPMOD64' ||
   ! readelf -rW "$guest_tlsdesc_plugin" | grep -q 'R_X86_64_TLSDESC' ||
   ! readelf -Ws "$guest_cxx_plugin" | grep -q '__cxa_thread_atexit'; then
    echo "FAIL: Guest TLS artifact relocation model mismatch" >&2
    exit 2
fi

for aot_mode in 0 1; do
    output="$workdir/output-aot-$aot_mode.log"
    set +e
    LD_PRELOAD="$host_probe" \
    BOX64_LD_LIBRARY_PATH="$guest_lib" \
    LATX_AOT=$aot_mode \
    LATX_KZT=1 \
        "$emulator" -U LD_PRELOAD \
        -E "LD_LIBRARY_PATH=$guest_lib" \
        -L "$guest_root" "$guest_program" \
        "$guest_plugin" "$guest_ie_plugin" "$guest_cxx_plugin" \
        "$guest_plugin_b" "$guest_tlsdesc_plugin" \
        "$guest_stress_iterations" \
        >"$output" 2>&1
    result=$?
    set -e

    if [ "$result" -ne 0 ]; then
        sed -n '1,240p' "$output" >&2
        echo "FAIL: LATX_AOT=$aot_mode exited $result" >&2
        exit "$result"
    fi
    grep -q '^HOST_THREAD_TLS_NATIVE_ENTER$' "$output"
    pass_prefix='PASS: native Host threads synchronized Guest libc semantics '
    grep -q "^${pass_prefix}without leaks$" "$output"
    cat "$output"
done
