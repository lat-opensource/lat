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

"$clang" --target=i386-linux-gnu -fuse-ld=lld -nostdlib -static -no-pie \
    -O2 -ffreestanding -fno-builtin -fno-pie -fno-stack-protector \
    -Wl,--build-id=none \
    "$source_file" -o "$workdir/futex-private-truncate-i386"

set +e
LATX_AOT=0 LATX_KZT=0 timeout -s KILL 15 \
    "$emulator" "$workdir/futex-private-truncate-i386"
ret=$?
set -e

case $ret in
0) echo "PASS: i386 private futexes fault on truncated file mappings" ;;
1) echo "FAIL: private futex returned a non-EFAULT after truncation" >&2 ;;
124) echo "FAIL: i386 private futex truncation test timed out" >&2 ;;
*) echo "FAIL: unexpected i386 futex test exit status $ret" >&2 ;;
esac

test "$ret" -eq 0
