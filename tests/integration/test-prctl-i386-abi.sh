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
    -Wl,--build-id=none "$source_file" -o "$workdir/prctl-i386-abi"

set +e
LATX_AOT=0 LATX_KZT=0 timeout -s KILL 10 \
    "$emulator" "$workdir/prctl-i386-abi"
ret=$?
set -e

case $ret in
0) echo "PASS: i386 prctl ABI behavior" ;;
20) echo "FAIL: i386 prctl pointer translation" >&2 ;;
21) echo "FAIL: i386 PR_GET_AUXV compat layout" >&2 ;;
22) echo "FAIL: i386 PR_SET_MM compat translation" >&2 ;;
23) echo "FAIL: i386 syscall user dispatch" >&2 ;;
24) echo "FAIL: i386 TSC control" >&2 ;;
25) echo "FAIL: i386 MDWE mmap2/mprotect enforcement" >&2 ;;
26) echo "FAIL: i386 current prctl controls" >&2 ;;
124) echo "FAIL: i386 prctl ABI test timed out" >&2 ;;
*) echo "FAIL: unexpected i386 guest exit status $ret" >&2 ;;
esac

test "$ret" -eq 0
