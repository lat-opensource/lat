#!/bin/bash
# LoongArch runtime test; build the static guest from tu-lifecycle*.c/.S
# with the matching x86 ABI before invoking this script.
set -euo pipefail
binary=$(realpath "$1")
guest=$(realpath "$2")
source "$(dirname "$0")/tu-test-common.sh"
result=$(mktemp -d "${TMPDIR:-/tmp}/latu-tu-lifecycle.XXXXXX")
export HOME="$result/home"
mkdir -p "$HOME"
echo "Results: $result"

# Exercise normal execution, code changes and concurrent execution.
for mode in 0 3 4; do
    LATX_AOT=0 timeout 60 "$binary" "$guest" "$mode"
done
export LATX_AOT=3
timeout 60 "$binary" "$guest" 0
cache=$(cache_path "$HOME" 3)
wait_cache "$cache"
for mode in 0 3 4; do
    LATX_TEST_AOT_LOAD_MARKER="$result/loaded-$mode" \
        timeout 60 "$binary" "$guest" "$mode" \
        > "$result/mode-$mode.log" 2>&1
    tail -1 "$result/mode-$mode.log"
    assert_loaded "$result/loaded-$mode"
    wait_cache "$cache"
done
timeout 60 "$binary" "$guest" 0
if [[ "$guest" == *64 ]]; then
    for aot in 0 1 2 3; do
        LATX_AOT=$aot timeout 60 "$binary" "$guest" 6 \
            > "$result/vsyscall-$aot.log" 2>&1
        tail -1 "$result/vsyscall-$aot.log"
    done
fi
sha256sum "$binary"
echo PASS
