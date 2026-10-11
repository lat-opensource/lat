#!/bin/sh
set -eu

if [ "$#" -lt 4 ] || [ "$#" -gt 6 ]; then
    echo "usage: $0 LATC NATIVE_IMAGE RUNTIME_DIRECTORY OUTPUT [X86_GUEST [TBSET]]" >&2
    exit 2
fi

latc=$1
native_image=$2
runtime_dir=$3
output=$4
guest=${5:-}
tbset=${6:-}
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
include_dir=$(dirname "$script_dir")/aot-v2/include
case "$runtime_dir" in /*) ;; *) runtime_dir="$(pwd)/$runtime_dir";; esac
case "$output" in /*) ;; *) output="$(pwd)/$output";; esac
work=$(mktemp -d "${TMPDIR:-/tmp}/latc-aot-v2-link.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

if [ -n "$tbset" ]; then
    "$latc" emit-aot-v2 "$native_image" "$work" "$guest" "$tbset" >/dev/null
elif [ -n "$guest" ]; then
    "$latc" emit-aot-v2 "$native_image" "$work" "$guest" >/dev/null
else
    "$latc" emit-aot-v2 "$native_image" "$work" >/dev/null
fi
(
    cd "$work"
    umask 022
    "${CC:-cc}" -I"$include_dir" -O2 -fPIC -c -o module-meta.o module.c
    "${CC:-cc}" -I"$include_dir" -fPIC -c -o module-text.o module.S
    # Linker relaxation changes local-dispatch windows and PC-map offsets.
    "${CC:-cc}" -shared -nostdlib -Wl,--no-relax \
      -Wl,-z,defs -Wl,-z,now -Wl,-z,relro -Wl,-z,separate-code \
      -Wl,--unique=.text.lat.tu -Wl,--unique=.rodata.lat.tb \
      -Wl,--unique=.rodata.lat.guest \
      -Wl,--unique=.rodata.lat.map \
      -Wl,--unique=.rodata.lat.local \
      -Wl,--unique=.data.rel.ro.lat.module \
      -Wl,--version-script="$script_dir/../aot-v2/compiler/module.map" \
      -Wl,--build-id=sha1 -L"$runtime_dir" -Wl,--no-as-needed \
      -l:liblat-aot-runtime.so.2 -o "$output" module-meta.o module-text.o
)
chmod go-w "$output"
