#!/bin/sh
set -eu
emulator=$1
source_file=$2
arch=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
if [ -n "${LATX_PROC_FD_GUEST_DIR:-}" ]; then
    guest=$LATX_PROC_FD_GUEST_DIR/proc-fd-thread-race-$arch
else
    cc=${X86_64_CC:-x86_64-linux-gnu-gcc}
    if ! command -v "$cc" >/dev/null 2>&1; then
        echo "SKIP: x86 compiler or prebuilt proc-fd guests required"
        exit 77
    fi
    case $arch in
    x86_64) bits=-m64 ;;
    i386) bits=-m32 ;;
    *) exit 1 ;;
    esac
    guest=$workdir/guest
    "$cc" "$bits" -O2 -Wall -Wextra -Werror -static -pthread \
        "$source_file" -o "$guest"
fi
for nr in 64 legacy; do
    for mode in tid race task-race other fork-self; do
        if ! timeout -s KILL 30 env LATX_AOT=0 LATX_KZT=0 \
            "$emulator" "$guest" "$mode" "$nr" "$workdir"; then
            echo "FAIL: $arch $mode/$nr proc-fd enumeration" >&2
            exit 1
        fi
        echo "PASS: $arch $mode/$nr proc-fd enumeration"
    done
done
