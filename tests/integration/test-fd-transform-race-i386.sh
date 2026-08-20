#!/bin/sh
set -eu

emulator=$1
source_file=$2
shim_source=$3
workdir=$(mktemp -d)
marker=$workdir/fd-transform-race-injected
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
    -Wl,--build-id=none "$source_file" -o "$workdir/fd-transform-race-i386"
"${CC:-cc}" -shared -fPIC -O2 -Wall -Wextra "$shim_source" \
    -ldl -pthread -o "$workdir/fd-transform-race-shim.so"

set +e
LATX_AOT=0 LATX_KZT=0 LATX_TEST_FD_TRANS_RACE=1 \
    LATX_TEST_FD_TRANS_RACE_MARKER="$marker" \
    LD_PRELOAD="$workdir/fd-transform-race-shim.so" \
    timeout -s KILL 15 "$emulator" "$workdir/fd-transform-race-i386"
ret=$?
set -e

if test "$ret" -eq 0 && test ! -f "$marker"; then
    echo "FAIL: fd-transform race injection did not run" >&2
    exit 1
fi

case $ret in
0) echo "PASS: i386 fd-transform table synchronization" ;;
77) echo "SKIP: RLIMIT_NOFILE cannot cross a table boundary"; exit 77 ;;
10) echo "FAIL: getrlimit(RLIMIT_NOFILE) failed" >&2 ;;
11) echo "FAIL: eventfd2 failed" >&2 ;;
13) echo "FAIL: reader clone failed" >&2 ;;
14) echo "FAIL: transform-table churner clone failed" >&2 ;;
15) echo "FAIL: concurrent transform-table growth failed" >&2 ;;
16) echo "FAIL: eventfd reader failed during transform-table growth" >&2 ;;
17) echo "FAIL: guest fork failed during fd-transform activity" >&2 ;;
18) echo "FAIL: waitpid failed for fd-transform fork child" >&2 ;;
19) echo "FAIL: fd-transform fork child did not complete table access" >&2 ;;
124) echo "FAIL: fd-transform race test timed out" >&2 ;;
*) echo "FAIL: unexpected fd-transform race status $ret" >&2 ;;
esac

test "$ret" -eq 0
