#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail
binary=$(realpath "$1")
guest=$(realpath "$2")
command -v nm >/dev/null || { echo 'SKIP: nm required'; exit 77; }
tests=$(dirname "$(realpath "$0")")
result=$(mktemp -d "${TMPDIR:-/tmp}/latu-entry-signal.XXXXXX")
echo "Results: $result"
pc=$(nm "$guest" | awk '$3 == "entry_spin" {print "0x"$1}')
[[ -n "$pc" ]]
export LATX_KZT=0 LATX_JRRA=0 LATX_JRRA_STACK=0 LATX_TU=1 LATX_SOFTFPU=0
export LATX_HRAS_SINGLE=1
modes=0
help=$("$binary" -help)
if grep -q '^-latx-aot[[:space:]]' <<<"$help"; then modes='0 1 3'; fi
for aot in $modes; do
    for hras in 0 1; do
        for hash in 0 1; do
            taskhome="$result/home-$aot-$hras-$hash"
            mkdir -p "$taskhome"
            export HOME="$taskhome" LATX_AOT=$aot LATX_HRAS=$hras
            export LATX_FAST_JMPCACHE_HASH=$hash LATX_AOT_GENERATE=1
            timeout -k 2 15 "$binary" "$guest" prepare
            if [[ "$aot" != 0 ]]; then
                cache_glob="$taskhome/.cache/latx/*.aot2"
                for ((attempt = 0; attempt < 100; attempt++)); do
                    if compgen -G "$cache_glob" >/dev/null; then
                        break
                    fi
                    sleep 0.1
                done
                shopt -s nullglob
                caches=("$taskhome"/.cache/latx/*.aot2)
                ((${#caches[@]}))
                for cache in "${caches[@]}"; do
                    flock -w 30 "$cache.lock" true
                done
            fi
            run_stage() {
                stage=$1
                chain=$2
                marker="$taskhome/loaded-$stage-$chain"
                chain_args=()
                [[ "$chain" == nochain ]] && chain_args=(-d nochain)
                rm -f "$marker"
                ENTRY_SIGNAL_STAGE="$stage" ENTRY_SIGNAL_PC="$pc" \
                    LATX_AOT_GENERATE=0 \
                    LATX_TEST_AOT_LOAD_MARKER="$marker" \
                    timeout -k 2 15 gdb -nx -q -batch \
                    -x "$tests/entry-signal-gdb.py" --args \
                    "$binary" "${chain_args[@]}" "$guest" \
                    > "$result/$aot-$hras-$hash-$stage-$chain.log" 2>&1
                grep -q '^entry-signal PASS' \
                    "$result/$aot-$hras-$hash-$stage-$chain.log"
                if [[ "$aot" != 0 ]]; then
                    grep -Fx "$guest" "$marker" >/dev/null
                fi
            }
            run_stage prologue nochain
            if [[ "$hras" == 0 && "$hash" == 0 ]]; then
                run_stage precheck chain
                run_stage prologue chain
            fi
        done
    done
done
echo 'entry-signal matrix PASS'
