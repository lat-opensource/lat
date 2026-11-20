#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
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
result=$(mktemp -d "${TMPDIR:-/tmp}/latu-tu-hras-indirect.XXXXXX")
echo "Results: $result"
recovered='recovered indirect HRAS layout and parallel-safe signal PASS'

wait_loaded() {
    local marker=$1
    for ((attempt = 0; attempt < 100; attempt++)); do
        [[ -f "$marker" ]] && assert_loaded "$marker" && return
        sleep 0.1
    done
    assert_loaded "$marker"
}

wait_marker() {
    local marker=$1
    for ((attempt = 0; attempt < 100; attempt++)); do
        [[ -f "$marker" ]] && grep -qx indirect-call "$marker" && return
        sleep 0.1
    done
    grep -qx indirect-call "$marker"
}

gdb_has_tb=false
if command -v gdb >/dev/null &&
        gdb -nx -q -batch -ex "file $binary" \
            -ex 'ptype TranslationBlock' 2>/dev/null |
            grep -q 'type = struct TranslationBlock'; then
    gdb_has_tb=true
fi

if "$gdb_has_tb"; then
    gdb_home="$result/gdb-home"
    mkdir -p "$gdb_home"
    env HOME="$gdb_home" LATX_AOT=3 LATX_AOT_GENERATE=1 \
        LATX_HRAS=1 \
        gdb -nx -q -batch -x "$(dirname "$0")/tu-hras-indirect-gdb.py" \
        --args "$binary" "$guest" > "$result/gdb.log" 2>&1
    grep 'indirect HRAS layout and parallel-safe signal PASS' "$result/gdb.log"

    rm -f "$gdb_home/recover-loaded"
    env HOME="$gdb_home" LATX_AOT=3 LATX_AOT_GENERATE=0 \
        LATX_TEST_AOT_LOAD_MARKER="$gdb_home/recover-loaded" \
        gdb -nx -q -batch \
        -x "$(dirname "$0")/tu-hras-indirect-recover-gdb.py" \
        --args "$binary" "$guest" > "$gdb_home/recover.log" 2>&1
    wait_loaded "$gdb_home/recover-loaded"
    grep -Fx "$recovered" \
        "$gdb_home/recover.log"
else
    echo 'SKIP: indirect-call layout checks need GDB TB types'
fi

for mode in 1 2 3; do
    for hash in 0 1; do
        taskhome="$result/home-$mode-$hash"
        mkdir -p "$taskhome"
        export LATX_HRAS=1 LATX_AOT=$mode
        export LATX_FAST_JMPCACHE_HASH=$hash
        env HOME="$taskhome" LATX_TEST_HRAS_MARKER="$taskhome/hras" \
            "$binary" "$guest" --parallel
        cache=$(cache_path "$taskhome" "$mode")
        wait_cache "$cache"
        wait_marker "$taskhome/hras"
        rm -f "$taskhome/hras" "$taskhome/loaded"
        env HOME="$taskhome" LATX_AOT_GENERATE=0 \
            LATX_TEST_AOT_LOAD_MARKER="$taskhome/loaded" \
            LATX_TEST_HRAS_MARKER="$taskhome/hras" \
            "$binary" "$guest" --parallel
        wait_loaded "$taskhome/loaded"
        if "$gdb_has_tb" && [[ "$mode" != 3 && "$hash" == 0 ]]; then
            env HOME="$taskhome" LATX_AOT_GENERATE=0 \
                LATX_TEST_AOT_LOAD_MARKER="$taskhome/recover-loaded" \
                gdb -nx -q -batch \
                -x "$(dirname "$0")/tu-hras-indirect-recover-gdb.py" \
                --args "$binary" "$guest" --parallel \
                > "$taskhome/recover.log" 2>&1
            wait_loaded "$taskhome/recover-loaded"
            grep -Fx "$recovered" \
                "$taskhome/recover.log"
        fi
        if [[ "$mode" == 3 && "$hash" == 0 ]]; then
            rm -f "$taskhome/signal-loaded"
            env HOME="$taskhome" LATX_AOT_GENERATE=0 \
                LATX_TEST_AOT_LOAD_MARKER="$taskhome/signal-loaded" \
                "$binary" "$guest" --parallel-signals
            wait_loaded "$taskhome/signal-loaded"
        fi
    done
done

for control in hras-off tu-off; do
    control_home="$result/home-$control"
    mkdir -p "$control_home"
    if [[ "$control" == hras-off ]]; then
        control_env=(LATX_HRAS=0 LATX_TU=1)
    else
        control_env=(LATX_HRAS=1 LATX_TU=0)
    fi
    env HOME="$control_home" LATX_AOT=1 LATX_AOT_GENERATE=0 \
        "${control_env[@]}" LATX_TEST_HRAS_MARKER="$control_home/hras" \
        "$binary" "$guest" --parallel
    test ! -e "$control_home/hras"
done
sha256sum "$binary"
echo 'cross-page indirect HRAS CALL PASS'
