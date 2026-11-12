#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail
binary=$(realpath "$1")
guest=$(realpath "$2")
tests=$(dirname "$(realpath "$0")")
source "$tests/tu-test-common.sh"
result=$(mktemp -d "${TMPDIR:-/tmp}/latu-tu-search-buffer.XXXXXX")
home="$result/home"
mkdir -p "$home"
echo "Results: $result"

timeout 60 env HOME="$home" LATX_AOT=3 LATX_AOT_GENERATE=1 LATX_HRAS=0 \
    "$binary" "$guest"
cache=$(cache_path "$home" 3)
wait_cache "$cache"
test -s "$cache"

timeout 60 env HOME="$home" LATX_AOT=3 LATX_AOT_GENERATE=0 LATX_HRAS=0 \
    LATX_TEST_AOT_LOAD_MARKER="$result/loaded" "$binary" "$guest"
assert_loaded "$result/loaded"
sha256sum "$binary" "$guest" "$cache"
echo 'TU search metadata bounds and AOT recovery PASS'
