#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
tmp_root=${TMPDIR:-${HOME}/tmp}
mkdir -p "$tmp_root"
workdir=$(mktemp -d "$tmp_root/push-pop-sp-i386.XXXXXX")
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the i386 guest"
    exit 77
fi

"$clang" --target=i386-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$workdir/push-pop-sp-i386"

set +e
timeout 10s env LATX_AOT=0 LATX_KZT=0 \
    "$emulator" "$workdir/push-pop-sp-i386"
ret=$?
set -e

case $ret in
0) ;;
10) echo "FAIL: i386 rt_sigaction failed" >&2; exit "$ret" ;;
20) echo "FAIL: 32-bit PUSH/POP values or ESP mismatch" >&2; exit "$ret" ;;
21) echo "FAIL: 32-bit PUSH ESP semantics mismatch" >&2; exit "$ret" ;;
22)
    echo "FAIL: PUSHAL/POPAL, PUSHFL/POPFL, or ENTER/LEAVE mismatch" >&2
    exit "$ret"
    ;;
23) echo "FAIL: 32-bit helper boundary did not preserve ESP" >&2; exit "$ret" ;;
24) echo "FAIL: 32-bit fault handler was not called" >&2; exit "$ret" ;;
25) echo "FAIL: 32-bit ESP was not restored after the fault" >&2; exit "$ret" ;;
30) echo "FAIL: wrong signal reached the i386 handler" >&2; exit "$ret" ;;
31) echo "FAIL: fault ucontext contained the wrong ESP" >&2; exit "$ret" ;;
124) echo "FAIL: 32-bit PUSH/POP test timed out" >&2; exit "$ret" ;;
*) echo "FAIL: unexpected exit status $ret" >&2; exit "$ret" ;;
esac

echo "PASS: 32-bit PUSH/POP, stack instructions, and fault ESP restore"
