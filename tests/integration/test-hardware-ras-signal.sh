#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail
binary=$(realpath "$1")
source_file=$(realpath "$2")
bits=$3
tests=$(dirname "$source_file")
if ! command -v gdb >/dev/null ||
        ! gdb -nx -q -batch -ex "file $binary" \
            -ex 'ptype TranslationBlock' 2>/dev/null |
            grep -q 'type = struct TranslationBlock'; then
    echo 'SKIP: HRAS signal regression requires GDB and debug types'
    exit 77
fi
result=$(mktemp -d "${TMPDIR:-/tmp}/latx-hras-signal.XXXXXX")
cleanup() {
    status=$?
    if [[ $status == 0 ]]; then
        rm -rf "$result"
    else
        echo "HRAS signal failure evidence: $result" >&2
    fi
}
trap cleanup EXIT
guest=${LATX_HRAS_TEST_SIGNAL_GUEST:-}
if [[ -z "$guest" ]]; then
    compiler=${LATX_HRAS_TEST_CC:-}
    if [[ -z "$compiler" ]]; then
        if [[ $(uname -m) == x86_64 ]]; then
            compiler=${CC:-cc}
        elif command -v x86_64-linux-gnu-gcc >/dev/null; then
            compiler=x86_64-linux-gnu-gcc
        else
            echo 'SKIP: need x86 compiler or LATX_HRAS_TEST_SIGNAL_GUEST'
            exit 77
        fi
    fi
    guest="$result/guest"
    "$compiler" -m"$bits" -nostdlib -static -no-pie "$source_file" -o "$guest"
fi
guest=$(realpath "$guest")
file "$guest" | grep -q "ELF $bits-bit.*\(x86-64\|Intel 80386\)"
sha256sum "$source_file" "$guest" "$binary"
export LATX_KZT=0 LATX_TU=1 LATX_HRAS=1 LATX_JRRA=0 LATX_JRRA_STACK=0
export LATX_SOFTFPU=0
for mode in 1 2; do
    taskhome="$result/home-$mode"
    mkdir -p "$taskhome"
    env HOME="$taskhome" LATX_AOT="$mode" LATX_AOT_GENERATE=1 \
        "$binary" "$guest"
    # AOT generation is detached. Wait for all processes using this HOME,
    # including a generator that has not created its lock file yet.
    python3 - "$taskhome" <<'PYWAIT'
import os
import pathlib
import sys
import time
home = ('HOME=' + sys.argv[1]).encode()
deadline = time.monotonic() + 60
while True:
    active = False
    for proc in pathlib.Path('/proc').iterdir():
        if not proc.name.isdigit():
            continue
        try:
            if proc.stat().st_uid == os.getuid() and home in (
                    proc / 'environ').read_bytes().split(b'\0'):
                active = True
                break
        except OSError:
            pass
    if not active:
        break
    if time.monotonic() >= deadline:
        raise SystemExit('FAIL: detached AOT generation did not finish')
    time.sleep(0.05)
PYWAIT
    env HOME="$taskhome" LATX_AOT="$mode" LATX_AOT_GENERATE=0 \
        LATX_TEST_AOT_LOAD_MARKER="$taskhome/loaded" \
        gdb -nx -q -batch -x "$tests/hardware-ras-signal-gdb.py" \
        --args "$binary" "$guest" > "$taskhome/gdb.log" 2>&1
    grep -Fx "$guest" "$taskhome/loaded"
    grep 'HRAS signal resume.*PASS' "$taskhome/gdb.log"
done
echo "PASS: $bits-bit HRAS signal resume and ordinary-pair control"
