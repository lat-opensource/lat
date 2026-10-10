#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if [ -n "${LATX_STATX_FORK_GUEST:-}" ]; then
    guest=$LATX_STATX_FORK_GUEST
else
    cc=${X86_64_CC:-x86_64-linux-gnu-gcc}
    if ! command -v "$cc" >/dev/null 2>&1; then
        echo "SKIP: x86_64 Linux compiler or LATX_STATX_FORK_GUEST is required"
        exit 77
    fi
    guest=$workdir/statx-fork-race
    "$cc" -O2 -Wall -Wextra -Werror -static -pthread \
        "$source_file" -o "$guest"
fi

# Kill the whole test process group if the translator's mutexes deadlock.
set +e
LATX_AOT=0 LATX_KZT=0 timeout -s KILL 60 "$emulator" "$guest"
ret=$?
set -e
if [ "$ret" -ne 0 ]; then
    echo "FAIL: statx/fork regression returned $ret" >&2
    exit 1
fi
echo "PASS: statx guest copyout does not deadlock fork"
