#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the i386 guest"
    exit 77
fi

if [ -z "${LATX_I386_SYSROOT-}" ]; then
    for candidate in /usr/gnemul/lat-i386 /usr/gnemul/latx-i386; do
        if [ -e "$candidate/lib/ld-linux.so.2" ]; then
            LATX_I386_SYSROOT=$candidate
            break
        fi
    done
fi
if [ -z "${LATX_I386_SYSROOT-}" ]; then
    echo "SKIP: i386 guest sysroot not found"
    exit 77
fi

runtime=$workdir/runtime
mkdir -p "$runtime/lib/i386-linux-gnu" "$runtime/usr/lib/i386-linux-gnu"

"$clang" --target=i386-linux-gnu -fuse-ld=lld -nostdlib -static -no-pie \
    -Wl,--build-id=none "$source_file" -o "$workdir/probe"

LD_LIBRARY_PATH="$runtime/lib/i386-linux-gnu:$runtime/usr/lib/i386-linux-gnu" \
STEAM_RUNTIME="$runtime" \
LATX_AOT=0 LATX_KZT=0 \
    timeout -s KILL 10 "$emulator" "$workdir/probe" >"$workdir/output"

value=$(cat "$workdir/output")
first=${value%%:*}
rest=${value#*:}

case "$first" in
"$runtime"/*)
    echo "FAIL: Steam Runtime library path precedes guest sysroot: $value" >&2
    exit 1
    ;;
esac
if [ "$first" = "$value" ]; then
    echo "FAIL: guest sysroot was not inserted: $value" >&2
    exit 1
fi
case ":$rest:" in
*":$runtime/lib/i386-linux-gnu:"*) ;;
*)
    echo "FAIL: Runtime lib/i386-linux-gnu was removed: $value" >&2
    exit 1
    ;;
esac
case ":$rest:" in
*":$runtime/usr/lib/i386-linux-gnu:"*) ;;
*)
    echo "FAIL: Runtime usr/lib/i386-linux-gnu was removed: $value" >&2
    exit 1
    ;;
esac

guest_library_dir=
for candidate in /opt/32/lib /usr/lib; do
    file_path=$LATX_I386_SYSROOT$candidate/libc.so.6
    if [ ! -e "$file_path" ]; then
        continue
    fi
    if command -v file >/dev/null 2>&1 &&
        ! file -b "$file_path" | grep -q 'ELF 32-bit'; then
        continue
    fi
    guest_library_dir=$candidate
    break
done
if [ -z "$guest_library_dir" ]; then
    echo "SKIP: i386 guest library directory not found"
    exit 77
fi

pvbase=$workdir/pressure-vessel
pvname=soldier_platform_test
pvfiles=$pvbase/$pvname/files
pv_runtime_dir=$pvfiles/lib/i386-linux-gnu
mkdir -p "$pv_runtime_dir"
: >"$pvfiles/.ref"
: >"$pv_runtime_dir/libplaceholder.so"

LD_LIBRARY_PATH="$pv_runtime_dir:$runtime/lib/i386-linux-gnu:$runtime/usr/lib/i386-linux-gnu:$guest_library_dir" \
SYSTEM_LD_LIBRARY_PATH="$pv_runtime_dir:$runtime/lib/i386-linux-gnu:$guest_library_dir" \
STEAM_RUNTIME="$runtime" \
PRESSURE_VESSEL_RUNTIME_BASE="$pvbase" \
PRESSURE_VESSEL_RUNTIME="$pvname" \
LATX_STEAM_RUNTIME_FILES="$pvfiles" \
LATX_AOT=0 LATX_KZT=0 \
    timeout -s KILL 10 "$emulator" "$workdir/probe" >"$workdir/output-pv"

value=$(cat "$workdir/output-pv")
first=${value%%:*}
rest=${value#*:}

if [ "$first" != "$guest_library_dir" ]; then
    echo "FAIL: guest sysroot does not precede the pressure-vessel Runtime: $value" >&2
    exit 1
fi
case ":$rest:" in
*":$pv_runtime_dir:"*) ;;
*)
    echo "FAIL: pressure-vessel Runtime library path was removed: $value" >&2
    exit 1
    ;;
esac
case ":$rest:" in
*":$runtime/lib/i386-linux-gnu:"*) ;;
*)
    echo "FAIL: legacy Runtime lib/i386-linux-gnu was removed: $value" >&2
    exit 1
    ;;
esac

echo "PASS: guest i386 sysroot precedes legacy and pressure-vessel Steam Runtime libraries"
