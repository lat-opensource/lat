#!/bin/sh
set -eu

emulator=$1
source_file=$2
abi=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

case $abi in
x86_64) guest=${LATX_PRCTL_EXE_FD_GUEST_X86_64:-} ;;
i386) guest=${LATX_PRCTL_EXE_FD_GUEST_I386:-} ;;
*) echo "unknown guest ABI: $abi" >&2; exit 1 ;;
esac
guest=${guest:-${LATX_PRCTL_EXE_FD_GUEST:-}}
if test -z "$guest"; then
    if command -v clang-19 >/dev/null 2>&1; then
        clang=clang-19
    elif command -v clang >/dev/null 2>&1; then
        clang=clang
    else
        echo "SKIP: clang or LATX_PRCTL_EXE_FD_GUEST is required"
        exit 77
    fi
    if ! printf 'void _start(void) {}\n' |
         "$clang" --target="$abi-linux-gnu" -fuse-ld=lld -nostdlib -static \
         -no-pie -x c - -o "$workdir/probe" 2>/dev/null; then
        echo "SKIP: $clang cannot link $abi static guests"
        exit 77
    fi
    guest=$workdir/prctl-exe-fd
    "$clang" --target="$abi-linux-gnu" -fuse-ld=lld -nostdlib -static \
        -no-pie -O2 -ffreestanding -fno-builtin -fno-stack-protector \
        -Wl,--build-id=none "$source_file" -o "$guest"
fi

if ! unshare -Ur true 2>/dev/null; then
    echo "SKIP: an unprivileged user namespace is required"
    exit 77
fi

"${CC:-cc}" -shared -fPIC -O2 -Wall -Wextra -Werror \
    "$(dirname "$0")/prctl-io-flusher-shim.c" \
    -o "$workdir/prctl-io-flusher-shim.so" -ldl

for mode in file map; do
    unshare -Ur env LATX_AOT=0 LATX_KZT=0 \
        LD_PRELOAD="$workdir/prctl-io-flusher-shim.so" \
        timeout -s KILL 10 "$emulator" "$guest" "$mode" "$guest"
    echo "PASS: $abi PR_SET_MM $mode preserves standard descriptors"
done
