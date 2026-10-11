#!/bin/sh
set -eu

static_tbset=0
if [ "${1:-}" = "--static-tbset" ]; then
    static_tbset=1
    shift
fi
if [ "$#" -lt 5 ] || [ "$#" -gt 6 ]; then
    echo "usage: $0 [--static-tbset] LATC STATIC_RUNNER X86_GUEST RUNTIME_DIRECTORY OUTPUT [TBSET]" >&2
    exit 2
fi

latc=$1
runner=$2
guest=$3
runtime_dir=$4
output=$5
tbset=${6:-}
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
case "$output" in /*) ;; *) output="$(pwd)/$output";; esac
work=$(mktemp -d "${TMPDIR:-/tmp}/latc-aot-v2-compile.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

if [ -n "$tbset" ]; then
    set -- --module
    if [ "$static_tbset" -eq 1 ]; then
        set -- "$@" --static-tbset
    fi
    "$script_dir/compile-native-image.sh" "$@" \
        "$latc" "$runner" "$guest" \
        "$work/module.latnative" "$tbset" >/dev/null
else
    "$script_dir/compile-native-image.sh" --module \
        "$latc" "$runner" "$guest" \
        "$work/module.latnative" >/dev/null
fi
native_image=$work/module.latnative
if [ -n "${LATC_NATIVE_BASE:-}" ]; then
    "$latc" merge-native "$LATC_NATIVE_BASE" "$native_image" \
        -o "$work/merged.latnative"
    native_image=$work/merged.latnative
fi
if [ -n "$tbset" ]; then
    "$script_dir/link-aot-v2-module.sh" "$latc" "$native_image" \
        "$runtime_dir" "$output" "$guest" "$tbset"
else
    "$script_dir/link-aot-v2-module.sh" "$latc" "$native_image" \
        "$runtime_dir" "$output" "$guest"
fi
if [ -n "${LATC_NATIVE_OUTPUT:-}" ]; then
    cp "$native_image" "$LATC_NATIVE_OUTPUT"
fi
if [ "${LATC_SKIP_FINAL_INSPECT:-0}" -ne 1 ]; then
    "$latc" inspect-module --json "$output"
fi
