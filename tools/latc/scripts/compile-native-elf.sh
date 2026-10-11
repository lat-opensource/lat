#!/bin/sh
set -eu

if [ "$#" -lt 4 ] || [ "$#" -gt 5 ]; then
    echo "usage: $0 LATC STATIC_RUNNER X86_GUEST LOONGARCH_ELF [TBSET]" >&2
    exit 2
fi

latc=$1
runner=$2
guest=$3
output=$4
tbset=${5:-}
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
offsets=$(dirname "$runner")/latc-x86-env-offsets.h
if [ ! -f "$offsets" ]; then
    echo "latc: missing runner ABI offsets: $offsets" >&2
    exit 1
fi
work=$(mktemp -d "${TMPDIR:-/tmp}/latc-native-elf.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

if [ -n "$tbset" ]; then
    "$script_dir/compile-native-image.sh" "$latc" "$runner" "$guest" \
        "$work/program.latnative" "$tbset" >/dev/null
else
    "$script_dir/compile-native-image.sh" "$latc" "$runner" "$guest" \
        "$work/program.latnative" >/dev/null
fi
"$script_dir/link-native-shell.sh" "$work/program.latnative" "$output" \
    "$offsets"
