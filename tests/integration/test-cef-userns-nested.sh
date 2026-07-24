#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if ! command -v unshare >/dev/null 2>&1 || ! unshare -Ur true 2>/dev/null; then
    echo "SKIP: unprivileged user namespaces are unavailable"
    exit 77
fi

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$workdir/cef-userns-nested"

set +e
SBX_USER_NS=1 LATX_AOT=0 LATX_KZT=0 \
    "$emulator" "$workdir/cef-userns-nested"
ret=$?
set -e

case $ret in
0)
    echo "PASS: nested guest user namespace setup remained single-threaded"
    ;;
10)
    echo "FAIL: clone(CLONE_NEWUSER) failed" >&2
    ;;
11)
    echo "FAIL: wait4 failed" >&2
    ;;
12)
    echo "FAIL: the namespace child was killed by a signal" >&2
    ;;
13)
    echo "FAIL: pipe creation failed" >&2
    ;;
14)
    echo "FAIL: writing the child uid_map failed" >&2
    ;;
15)
    echo "FAIL: releasing the namespace child failed" >&2
    ;;
20)
    echo "FAIL: child unshare(CLONE_NEWUSER) failed" >&2
    ;;
21)
    echo "FAIL: child namespace synchronization failed" >&2
    ;;
22)
    echo "FAIL: child user namespace mapping failed" >&2
    ;;
23)
    echo "FAIL: guest /proc/self/task did not report one thread" >&2
    ;;
24)
    echo "FAIL: combined user, PID and network namespace clone failed" >&2
    ;;
25)
    echo "FAIL: combined namespace child did not become PID 1" >&2
    ;;
26)
    echo "FAIL: guest helper thread creation failed" >&2
    ;;
28)
    echo "FAIL: guest /proc/self/task did not report two threads" >&2
    ;;
*)
    echo "FAIL: unexpected guest exit status $ret" >&2
    ;;
esac

exit "$ret"
