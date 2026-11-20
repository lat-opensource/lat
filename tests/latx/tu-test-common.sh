#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Helpers for isolated cross-TU runtime tests. Source after selecting guest.
export LATX_KZT=0 LATX_TU=1 LATX_JRRA=0 LATX_JRRA_STACK=0
export LATX_FAST_JMPCACHE_HASH=0 LATX_SOFTFPU=0 LATX_AOT_GENERATE=1

cache_path() {
    local cache_root=$1 digest
    digest=$(printf %s "$guest" | sha256sum | cut -d ' ' -f 1)
    printf '%s/.cache/latx/v3-01-%x-%s.aot2\n' "$cache_root" \
        "$((256 + !!${LATX_FAST_JMPCACHE_HASH:-0}))" "$digest"
}

wait_cache() {
    local cache=$1
    for ((attempt = 0; attempt < 100; attempt++)); do
        [[ -s "$cache" ]] && break
        sleep 0.1
    done
    test -s "$cache"
    flock -w 30 "$cache.lock" true
}

assert_loaded() {
    grep -Fx "$guest" "$1" >/dev/null || {
        echo "FAIL: guest AOT was not loaded: $guest" >&2
        return 1
    }
}
