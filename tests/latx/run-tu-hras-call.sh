#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Require actual single-slot links only in the experimental cross-page mode.
set -euo pipefail
binary=$(realpath "$1")
guest=$(realpath "$2")
if ! command -v nm >/dev/null ||
        ! nm -a "$binary" 2>/dev/null |
            grep -F 'hras_test_marker_init' >/dev/null; then
    echo 'SKIP: HRAS marker checks require a marker-enabled build'
    exit 77
fi
source "$(dirname "$0")/tu-test-common.sh"
gdb_has_tb=false
if command -v gdb >/dev/null &&
        gdb -nx -q -batch -ex "file $binary" \
            -ex 'ptype TranslationBlock' 2>/dev/null |
            grep -q 'type = struct TranslationBlock'; then
    gdb_has_tb=true
fi
result=$(mktemp -d "${TMPDIR:-/tmp}/latu-tu-hras-call.XXXXXX")
echo "Results: $result"
for mode in 1 3; do
    taskhome="$result/home-$mode"
    mkdir -p "$taskhome"
    export LATX_HRAS=1 LATX_AOT=$mode
    env HOME="$taskhome" "$binary" "$guest"
    cache=$(cache_path "$taskhome" "$mode")
    wait_cache "$cache"
    env HOME="$taskhome" LATX_AOT_GENERATE=0 \
        LATX_TEST_AOT_LOAD_MARKER="$taskhome/loaded" \
        LATX_TEST_HRAS_MARKER="$taskhome/hras" "$binary" "$guest"
    assert_loaded "$taskhome/loaded"
    if [[ "$mode" == 3 ]]; then
        grep -qx call "$taskhome/hras"
        grep -qx ret "$taskhome/hras"
        grep -qx single-call "$taskhome/hras"
        if "$gdb_has_tb"; then
            env HOME="$taskhome" LATX_AOT_GENERATE=0 \
                gdb -nx -q -batch \
                -x "$(dirname "$0")/tu-single-unlink-gdb.py" \
                --args "$binary" "$guest" > "$taskhome/unlink.log" 2>&1
            grep 'signal relink PASS' "$taskhome/unlink.log"
        else
            echo 'SKIP: single-slot unlink needs GDB TB types'
        fi
    else
        grep -qx pair-call "$taskhome/hras"
        if grep -qx single-call "$taskhome/hras"; then
            echo "FAIL: single CALL in AOT=$mode" >&2
            exit 1
        fi
        if "$gdb_has_tb"; then
            env HOME="$taskhome" LATX_AOT_GENERATE=0 \
                gdb -nx -q -batch \
                -x "$(dirname "$0")/tu-pair-unlink-gdb.py" \
                --args "$binary" "$guest" > "$taskhome/unlink.log" 2>&1
            grep 'two-word HRAS signal resume' "$taskhome/unlink.log"
        else
            echo 'SKIP: pair unlink needs GDB TB types'
        fi
    fi
done
sha256sum "$binary"
echo 'cross-page CALL and single-slot policy PASS'
