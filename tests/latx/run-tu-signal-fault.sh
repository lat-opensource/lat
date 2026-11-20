#!/bin/bash
# Run on the LoongArch build host with a static x86 guest built from
# tu-signal-fault.c and tu-signal-fault.S for the matching ABI.
set -euo pipefail
binary=$(realpath "$1")
guest=$(realpath "$2")
source "$(dirname "$0")/tu-test-common.sh"
result=$(mktemp -d "${TMPDIR:-/tmp}/latu-tu-signal-fault.XXXXXX")
echo "Results: $result"

LATX_AOT=0 timeout 60 "$binary" "$guest"
for aot in 1 2 3; do
    taskhome="$result/home-$aot"
    mkdir -p "$taskhome"
    env HOME="$taskhome" LATX_AOT=$aot timeout 60 "$binary" "$guest"
    cache=$(cache_path "$taskhome" "$aot")
    wait_cache "$cache"
    for attempt in 1 2 3; do
        env HOME="$taskhome" LATX_AOT=$aot \
            LATX_TEST_AOT_LOAD_MARKER="$result/loaded-$aot-$attempt" \
            timeout 60 "$binary" "$guest"
        assert_loaded "$result/loaded-$aot-$attempt"
        wait_cache "$cache"
    done
done
sha256sum "$binary"
echo PASS
