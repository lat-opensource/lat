#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail
binary=$(realpath "$1")
guest=$(realpath "$2")
tests=$(dirname "$(realpath "$0")")
source "$tests/tu-test-common.sh"
result=$(mktemp -d "${TMPDIR:-/tmp}/latu-tu-members.XXXXXX")
echo "Results: $result"
# Match this checkout's capacity; fail if an old translator is supplied.
export TU_MEMBER_LIMIT
TU_MEMBER_LIMIT=$(
    awk '/^#define MAX_TB_IN_TS / {
        gsub(/[()]/, "", $3)
        print $3
    }' "$tests/../../target/i386/latx/include/tu.h"
)
export LATX_AOT=3
export TU_MEMBER_REQUIRE_COLD=0
taskhome="$result/home"
mkdir -p "$taskhome"
for phase in generate load; do
    generate=1; [[ "$phase" == load ]] && generate=0
    env HOME="$taskhome" LATX_AOT_GENERATE="$generate" TU_MEMBER_PHASE="$phase" \
        gdb -nx -q -batch -x "$tests/tu-member-limit-gdb.py" \
        --args "$binary" "$guest" >"$result/$phase.log" 2>&1
    grep '^TU_MEMBER_RESULT ' "$result/$phase.log"
    if [[ "$phase" == generate ]]; then
        cache=$(cache_path "$taskhome" 3)
        wait_cache "$cache"
        sha256sum "$cache" >"$result/cache.sha256"
    else
        sha256sum -c "$result/cache.sha256"
    fi
done
sha256sum "$binary"
echo 'TU member limit PASS'
