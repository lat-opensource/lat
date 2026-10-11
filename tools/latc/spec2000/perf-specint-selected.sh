#!/bin/bash
set -euo pipefail

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "usage: $0 SPEC_ROOT WRAPPER_ROOT [OUTPUT]" >&2
    exit 2
fi

root=$1
wrappers=$2
out=${3:-latc-perf-selected.csv}
events=cycles,instructions,branches,branch-misses,L1-icache-loads,L1-icache-load-misses,iTLB-load-misses

run_one()
{
    local benchmark=$1 mode=$2 round=$3
    local run="$root/benchspec/CINT2000/$benchmark/run/00000008"
    local stat
    stat=$(mktemp "${TMPDIR:-/tmp}/latc-perf.XXXXXX")
    trap 'rm -f "$stat"' RETURN
    case $benchmark in
    186.crafty)
        (cd "$run" && perf stat -x, -e "$events" -o "$stat" -- \
            "$wrappers/$mode/crafty_base.Of.gcc830.dyn" <crafty.in >/dev/null)
        ;;
    197.parser)
        (cd "$run" && perf stat -x, -e "$events" -o "$stat" -- \
            "$wrappers/$mode/parser_base.Of.gcc830.dyn" \
            2.1.dict -batch <train.in >/dev/null)
        ;;
    253.perlbmk)
        (cd "$run" && perf stat -x, -e "$events" -o "$stat" --append -- \
            "$wrappers/$mode/perlbmk_base.Of.gcc830.dyn" \
            -I./lib diffmail.pl 2 350 15 24 23 150 >/dev/null)
        (cd "$run" && perf stat -x, -e "$events" -o "$stat" --append -- \
            "$wrappers/$mode/perlbmk_base.Of.gcc830.dyn" \
            -I./lib perfect.pl b 3 >/dev/null)
        (cd "$run" && perf stat -x, -e "$events" -o "$stat" --append -- \
            "$wrappers/$mode/perlbmk_base.Of.gcc830.dyn" \
            -I. -I./lib scrabbl.pl <scrabbl.in >/dev/null)
        ;;
    254.gap)
        (cd "$run" && perf stat -x, -e "$events" -o "$stat" -- \
            "$wrappers/$mode/gap_base.Of.gcc830.dyn" \
            -l ./ -q -m 128M <train.in >/dev/null)
        ;;
    255.vortex)
        (cd "$run" && perf stat -x, -e "$events" -o "$stat" -- \
            "$wrappers/$mode/vortex_base.Of.gcc830.dyn" lendian.raw >/dev/null)
        ;;
    256.bzip2)
        (cd "$run" && perf stat -x, -e "$events" -o "$stat" -- \
            "$wrappers/$mode/bzip2_base.Of.gcc830.dyn" \
            input.compressed 8 >/dev/null)
        ;;
    esac
    awk -F, -v b="$benchmark" -v m="$mode" -v r="$round" '
        $3 ~ /^cycles/ {cycles += $1}
        $3 ~ /^instructions/ {instructions += $1}
        $3 ~ /^branches/ {branches += $1}
        $3 ~ /^branch-misses/ {misses += $1}
        $3 ~ /^L1-icache-loads/ {l1i += $1}
        $3 ~ /^L1-icache-load-misses/ {l1im += $1}
        $3 ~ /^iTLB-load-misses/ {itlb += $1}
        END {printf "%s,%s,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n",
                    b,m,r,cycles,instructions,branches,misses,l1i,l1im,itlb}
    ' "$stat" >>"$out"
}

echo benchmark,mode,round,cycles,instructions,branches,branch_misses,l1i_loads,l1i_misses,itlb_misses >"$out"
for round in 1 2; do
    for benchmark in 186.crafty 197.parser 253.perlbmk 254.gap 255.vortex 256.bzip2; do
        for mode in latc lat_aot; do
            run_one "$benchmark" "$mode" "$round"
            tail -1 "$out"
        done
    done
done
