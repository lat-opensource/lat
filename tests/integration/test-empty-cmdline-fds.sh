#!/bin/sh
set -eu

emulator=$1
source_file=$2
abi=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

case $abi in
x86_64) guest=${LATX_EMPTY_CMDLINE_GUEST_X86_64:-} ;;
i386) guest=${LATX_EMPTY_CMDLINE_GUEST_I386:-} ;;
*) echo "unknown guest ABI: $abi" >&2; exit 1 ;;
esac
guest=${guest:-${LATX_EMPTY_CMDLINE_GUEST:-}}

if test -z "$guest"; then
    if command -v clang-19 >/dev/null 2>&1; then
        clang=clang-19
    elif command -v clang >/dev/null 2>&1; then
        clang=clang
    else
        echo "SKIP: clang or LATX_EMPTY_CMDLINE_GUEST is required"
        exit 77
    fi
    if ! printf 'int main(void) { return 0; }\n' |
         "$clang" --target="$abi-linux-gnu" -fuse-ld=lld -static -x c - \
         -o "$workdir/probe" 2>/dev/null; then
        echo "SKIP: $clang cannot link $abi static guests"
        exit 77
    fi
    "$clang" --target="$abi-linux-gnu" -fuse-ld=lld -static -no-pie \
        -O2 "$source_file" -o "$workdir/empty-cmdline-fds"
    guest=$workdir/empty-cmdline-fds
fi

env LATX_AOT=0 LATX_KZT=0 timeout -s KILL 10 \
    "$emulator" "$guest"
echo "PASS: $abi empty task cmdline does not leak descriptors"
