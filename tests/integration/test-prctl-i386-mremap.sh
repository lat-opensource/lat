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
    -O2 -fomit-frame-pointer -ffreestanding -fno-builtin \
    -fno-stack-protector -Wl,--build-id=none "$source_file" \
    -o "$workdir/prctl-i386-mremap"

host_mode=
if [ "$(getconf PAGESIZE)" -gt 4096 ]; then
    host_mode=h
fi

set +e
LATX_AOT=0 LATX_KZT=0 timeout -s KILL 15 \
    "$emulator" "$workdir/prctl-i386-mremap" "$host_mode"
ret=$?
set -e

case $ret in
0) echo "PASS: i386 shared and shadow mremap semantics" ;;
30) echo "FAIL: i386 shared mremap alias semantics" >&2 ;;
31) echo "FAIL: i386 partial shared mremap fail-closed semantics" >&2 ;;
32) echo "FAIL: i386 shadow mremap fail-closed semantics" >&2 ;;
124) echo "FAIL: i386 mremap test timed out" >&2 ;;
*) echo "FAIL: unexpected i386 mremap exit status $ret" >&2 ;;
esac

test "$ret" -eq 0
