#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Isolate hash policies and reject an incompatible cache even after renaming.
set -euo pipefail
build=$(realpath "$1")
guest=$(realpath "$2")
case "$guest" in
    *32) binary="$build/latx-i386" ;;
    *64) binary="$build/latx-x86_64" ;;
    *) exit 2 ;;
esac
for tool in bwrap flock sha256sum; do
    command -v "$tool" >/dev/null || { echo "SKIP: need $tool"; exit 77; }
done
cache_mount="$HOME/.cache/latx"
if [[ ! -d "$cache_mount" ]] ||
        ! bwrap --ro-bind / / -- /bin/true >/dev/null 2>&1; then
    echo 'SKIP: need an existing cache directory and private bind mounts'
    exit 77
fi
source "$(dirname "$0")/tu-test-common.sh"
work=$(mktemp -d "${TMPDIR:-/tmp}/latx-hash-cache.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
digest=$(printf %s "$guest" | sha256sum | cut -d ' ' -f 1)
run_guest() {
    bwrap --ro-bind / / --bind "$work" "$work" \
        --bind "$cache" "$cache_mount" -- "$@"
}
for mode in 1 3; do
    for hash in 0 1; do
        export LATX_AOT=$mode LATX_FAST_JMPCACHE_HASH=$hash
        cache="$work/cache-$mode-$hash"
        mkdir "$cache"
        file="v3-01-$(printf %x "$((256 + hash))")-$digest.aot2"
        run_guest env LATX_AOT_GENERATE=1 "$binary" "$guest" 0
        wait_cache "$cache/$file"
        cp "$cache/$file" "$work/saved-$mode-$hash"
        marker="$work/loaded-$mode-$hash"
        run_guest env LATX_AOT_GENERATE=0 \
            LATX_TEST_AOT_LOAD_MARKER="$marker" "$binary" "$guest" 0
        assert_loaded "$marker"
    done
    for hash in 0 1; do
        export LATX_FAST_JMPCACHE_HASH=$hash
        cache="$work/swapped-$mode-$hash"
        mkdir "$cache"
        file="v3-01-$(printf %x "$((256 + hash))")-$digest.aot2"
        cp "$work/saved-$mode-$((1 - hash))" "$cache/$file"
        marker="$work/rejected-$mode-$hash"
        run_guest env LATX_AOT_GENERATE=0 \
            LATX_TEST_AOT_LOAD_MARKER="$marker" "$binary" "$guest" 0
        if [[ -e "$marker" ]] && grep -Fx "$guest" "$marker" >/dev/null; then
            echo "FAIL: loaded incompatible hash policy in AOT=$mode" >&2
            exit 1
        fi
    done
done
sha256sum "$binary" "$guest"
echo 'hash cache namespace and incompatible-policy rejection PASS'
