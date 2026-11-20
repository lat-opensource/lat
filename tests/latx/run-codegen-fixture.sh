#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail
binary=$(realpath "$1")
guest=$(realpath "$2")
kind=$3
tests=$(dirname "$(realpath "$0")")
for tool in bwrap flock sha256sum; do
    command -v "$tool" >/dev/null || { echo "SKIP: need $tool"; exit 77; }
done
cache_mount="$HOME/.cache/latx"
if [[ ! -d "$cache_mount" ]]; then
    echo 'SKIP: need an existing cache directory for a private bind mount'
    exit 77
fi
work=$(mktemp -d "${TMPDIR:-/tmp}/latx-codegen.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
if ! bwrap --ro-bind / / -- /bin/true >/dev/null 2>&1; then
    echo 'SKIP: unprivileged private mounts are unavailable'
    exit 77
fi
export LATX_KZT=0 LATX_TU=1
export LATX_FAST_JMPCACHE_HASH=0 LATX_JRRA=0 LATX_JRRA_STACK=0
export LATX_SOFTFPU=0
if [[ "$kind" == indirect-cold ]]; then
    if ! nm "$binary" | grep -F 'kzt_get_alternate_pc' >/dev/null; then
        echo 'SKIP: cold KZT tail requires a KZT-enabled translator'
        exit 77
    fi
    export TU_COLD_BEGIN=0x$(nm "$guest" | awk '$3 == "invoke" {print $1}')
    export TU_COLD_END=0x$(nm "$guest" |
        awk '$3 == "invoke_end" {print $1}')
fi

run_guest() {
    bwrap --ro-bind / / --bind "$work" "$work" \
        --bind "$cache" "$cache_mount" -- "$@"
}
wait_cache() {
    local artifact
    for ((attempt = 0; attempt < 100; attempt++)); do
        artifact=$(find "$cache" -type f -name '*.aot2' -size +0c -print -quit)
        if [[ -n "$artifact" ]]; then
            flock -w 30 "$artifact.lock" true
            return
        fi
        sleep 0.1
    done
    echo "FAIL: no current AOT artifact for $guest" >&2
    exit 1
}
for mode in 0 1 3; do
    hashes=(0)
    if [[ "$kind" == indirect-cold && "$mode" == 3 ]]; then
        hashes+=(1)
    fi
    for hash in "${hashes[@]}"; do
        export LATX_FAST_JMPCACHE_HASH=$hash
        cache="$work/cache-aot-$mode-hash-$hash"
        mkdir "$cache"
        export LATX_AOT=$mode LATX_AOT_GENERATE=1
        run_guest "$binary" "$guest"
        [[ "$mode" != 0 ]] || continue
        wait_cache
        export LATX_AOT_GENERATE=0
        marker="$work/loaded-aot-$mode-hash-$hash"
        run_guest env LATX_TEST_AOT_LOAD_MARKER="$marker" \
            "$binary" "$guest"
        grep -Fx "$guest" "$marker" >/dev/null || {
            echo "FAIL: current guest artifact was not accepted" >&2
            exit 1
        }
        if [[ "$kind" == indirect-cold && "$mode" == 3 ]]; then
            for kzt in 0 1; do
                export LATX_KZT=$kzt
                log="$work/gdb-kzt-$kzt-hash-$hash.log"
                run_guest gdb -nx -q -batch \
                    -x "$tests/tu-indirect-cold-gdb.py" -ex 'quit 1' \
                    --args "$binary" "$guest" > "$log" 2>&1 || {
                    cat "$log"; exit 1;
                }
                restored='AOT restored cold tail executed after signal '
                restored+='unlink/relink PASS'
                grep -F "$restored" "$log"
                grep -F 'AOT moved KZT helper relocation executed PASS' \
                    "$log"
            done
        fi
        export LATX_KZT=0
    done
done
sha256sum "$binary" "$guest"
echo "$kind PASS"
