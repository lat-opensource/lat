#!/bin/sh
set -eu

emulator=$1
source_file=$2
arch=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if [ -n "${LATX_FD_SCOPE_GUEST_DIR:-}" ]; then
    guest=$LATX_FD_SCOPE_GUEST_DIR/fd-conversion-scope-$arch
else
    cc=${X86_64_CC:-x86_64-linux-gnu-gcc}
    if ! command -v "$cc" >/dev/null 2>&1; then
        echo "SKIP: x86 compiler or prebuilt fd-scope guests required"
        exit 77
    fi
    case $arch in
    x86_64) bits=-m64 ;;
    i386) bits=-m32 ;;
    *) exit 1 ;;
    esac
    guest=$workdir/fd-conversion-scope
    "$cc" "$bits" -O2 -Wall -Wextra -Werror -static -pthread \
        "$source_file" -o "$guest"
fi

for mode in shared failed split split-fork; do
    set +e
    timeout -s KILL 15 env LATX_AOT=0 LATX_KZT=0 \
        "$emulator" "$guest" "$mode"
    ret=$?
    set -e
    if [ "$ret" -eq 77 ]; then
        echo "SKIP: unshare(CLONE_FILES) unavailable"
        exit 77
    fi
    if [ "$ret" -ne 0 ]; then
        echo "FAIL: $arch fd conversion scope ($mode)" >&2
        exit 1
    fi
done
echo "PASS: $arch fd conversion follows actual ABI requirements"
