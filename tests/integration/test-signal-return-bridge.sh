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
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$workdir/signal-return-bridge"

set +e
timeout 10s env LATX_AOT=0 LATX_KZT=0 \
    "$emulator" "$workdir/signal-return-bridge"
ret=$?
set -e
case $ret in
0) echo "PASS: nested signal returns, ordinary calls and custom restorers" ;;
10) echo "FAIL: rt_sigaction setup" >&2; exit "$ret" ;;
11) echo "FAIL: wrong signal" >&2; exit "$ret" ;;
12) echo "FAIL: nested return/order or sigreturn returned" >&2; exit "$ret" ;;
13) echo "FAIL: handler/call/restorer counts" >&2; exit "$ret" ;;
124) echo "FAIL: signal return timed out" >&2; exit "$ret" ;;
*) echo "FAIL: unexpected status $ret" >&2; exit "$ret" ;;
esac
