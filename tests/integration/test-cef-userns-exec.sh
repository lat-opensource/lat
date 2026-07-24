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
    -Wl,--build-id=none "$source_file" -o "$workdir/cef-userns-exec"

set +e
SBX_USER_NS=1 LATX_AOT=0 LATX_KZT=0 \
    "$emulator" "$workdir/cef-userns-exec"
ret=$?
set -e

if [ "$ret" -eq 0 ]; then
    echo "PASS: namespace sandbox exec remained single-threaded"
elif [ "$ret" -eq 27 ]; then
    echo "FAIL: initial unshare(CLONE_NEWUSER) failed after exec" >&2
else
    echo "FAIL: unexpected guest exit status $ret" >&2
fi

exit "$ret"
