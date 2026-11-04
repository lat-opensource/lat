#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
arch=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
if ! command -v nasm >/dev/null 2>&1 ||
        ! command -v ld.lld >/dev/null 2>&1; then
    echo 'SKIP: nasm and ld.lld are required'
    exit 77
fi
case "$arch" in
    i386) format=elf32; emulation=elf_i386 ;;
    x86_64) format=elf64; emulation=elf_x86_64 ;;
    *) echo "Unsupported architecture: $arch" >&2; exit 2 ;;
esac
failures=0
for case_id in 0 1; do
    nasm -f "$format" -DTEST_CASE="$case_id" "$source_file" \
        -o "$workdir/guest.o"
    ld.lld -m "$emulation" -static "$workdir/guest.o" -o "$workdir/guest"
    for mode in 1 2; do
        # Retain the original reproduction: only SOFTFPU is configured.
        if timeout -s KILL 30 env -i LATX_SOFTFPU="$mode" \
                "$emulator" "$workdir/guest"; then
            echo "PASS: $arch case=$case_id softfpu=$mode"
        else
            result=$?
            echo "FAIL: $arch case=$case_id softfpu=$mode exit=$result" >&2
            failures=$((failures + 1))
        fi
    done
done
test "$failures" -eq 0
