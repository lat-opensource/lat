#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail
binary=$(realpath "$1")
guest=$(realpath "$2")
test_dir=$(dirname "$(realpath "$0")")
source "$test_dir/tu-test-common.sh"
command -v gdb >/dev/null || exit 77
result=$(mktemp -d "${TMPDIR:-/tmp}/latu-tu-late-thunk.XXXXXX")
echo "Results: $result"
export LATX_AOT=3
export TU_THUNK_CALLER=0x$(nm "$guest" | awk '$3 == "late_call" {print $1}')
export TU_THUNK_TARGET=0x$(nm "$guest" | awk '$3 == "late_thunk" {print $1}')
export TU_THUNK_CONTINUATION=0x$(nm "$guest" | awk '$3 == "late_continuation" {print $1}')
root=$result
for topology in same-tu external; do
    result="$root/$topology"
    export HOME="$result/home" TU_THUNK_PHASE=generate
    export TU_THUNK_EXTERNAL=0
    [[ "$topology" != external ]] || export TU_THUNK_EXTERNAL=1
    mkdir -p "$HOME"
    gdb -nx -q -batch -x "$test_dir/tu-late-thunk-gdb.py" \
        --args "$binary" "$guest" > "$result/generate.log" 2>&1 || {
        cat "$result/generate.log"
        exit 1
    }
    cat "$result/generate.log"
    cache=$(cache_path "$HOME" 3)
    wait_cache "$cache"
    for attempt in 1 2; do
        env LATX_AOT_GENERATE=0 TU_THUNK_PHASE=reload \
            LATX_TEST_AOT_LOAD_MARKER="$result/loaded-$attempt" \
            gdb -nx -q -batch -x "$test_dir/tu-late-thunk-gdb.py" \
            --args "$binary" "$guest" > "$result/reload-$attempt.log" 2>&1 || {
            cat "$result/reload-$attempt.log"
            exit 1
        }
        cat "$result/reload-$attempt.log"
        assert_loaded "$result/loaded-$attempt"
    done
done
echo 'late-thunk generated cache execution PASS'
