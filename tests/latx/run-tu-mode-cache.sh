#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Isolate cache policies and reject a valid cache renamed for the other mode.
set -euo pipefail
build=$(realpath "$1")
guest=$(realpath "$2")
case "$guest" in
    *32) binary="$build/latx-i386" ;;
    *64) binary="$build/latx-x86_64" ;;
    *) exit 2 ;;
esac
source "$(dirname "$0")/tu-test-common.sh"
result=$(mktemp -d "${TMPDIR:-/tmp}/latu-tu-mode-cache.XXXXXX")
echo "Results: $result"
for mode in 1 3; do
    home="$result/source-$mode"
    mkdir -p "$home"
    HOME="$home" LATX_AOT=$mode "$binary" "$guest" 0
    cache=$(cache_path "$home" "$mode")
    wait_cache "$cache"
    cp "$cache" "$result/saved-$mode"
    env HOME="$home" LATX_AOT=$mode LATX_AOT_GENERATE=0 \
        LATX_TEST_AOT_LOAD_MARKER="$result/loaded-$mode" \
        "$binary" "$guest" 0
    assert_loaded "$result/loaded-$mode"
done
for mode in 1 3; do
    home="$result/swapped-$mode"
    mkdir -p "$home/.cache/latx"
    cache=$(cache_path "$home" "$mode")
    other=$((4 - mode))
    cp "$result/saved-$other" "$cache"
    env HOME="$home" LATX_AOT=$mode LATX_AOT_GENERATE=0 \
        LATX_TEST_AOT_LOAD_MARKER="$result/rejected-$mode" \
        "$binary" "$guest" 0
    if [[ -e "$result/rejected-$mode" ]] &&
            grep -Fx "$guest" "$result/rejected-$mode" >/dev/null; then
        echo "FAIL: loaded incompatible TU policy in AOT=$mode" >&2
        exit 1
    fi
done

sha256sum "$binary"
echo 'cache namespace and incompatible-policy rejection PASS'
