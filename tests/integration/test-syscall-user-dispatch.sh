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
    -Wl,--build-id=none "$source_file" -o "$workdir/syscall-user-dispatch"

set +e
LATX_AOT=0 LATX_KZT=0 timeout -s KILL 10 \
    "$emulator" "$workdir/syscall-user-dispatch"
ret=$?
set -e

case $ret in
0)
    echo "PASS: syscall user dispatch delivered guest SIGSYS"
    ;;
10)
    echo "FAIL: blocked guest syscall was executed" >&2
    ;;
11)
    echo "FAIL: syscall user dispatch setup failed" >&2
    ;;
12)
    echo "FAIL: guest SIGSYS metadata was invalid" >&2
    ;;
13)
    echo "FAIL: unknown prctl was not rejected with EINVAL" >&2
    ;;
14)
    echo "FAIL: syscall user dispatch was inherited across fork" >&2
    ;;
124)
    echo "FAIL: syscall user dispatch test timed out" >&2
    ;;
159)
    echo "FAIL: guest syscall dispatch leaked into the host (SIGSYS)" >&2
    ;;
*)
    echo "FAIL: unexpected guest exit status $ret" >&2
    ;;
esac

exit "$ret"
