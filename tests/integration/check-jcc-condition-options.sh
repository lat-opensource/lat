#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu

emulator=$(readlink -f "$1")
guest_dir=$(readlink -f "$2")

matches()
{
    awk -v pattern="$1" '
        $2 == pattern { split($3, count, "="); value = count[2] }
        END { print value + 0 }
    ' "$2"
}

for entry in \
    'cmp cmp-js-jns cmp-jcc 0x80000000' \
    'sub sub-js-jns sub-jcc 0x100000000' \
    'and and-je and-jcc 0x200000000'; do
    set -- $entry
    kind=$1
    pattern=$2
    legacy=$3
    enabled=$4
    for mask in 0 0x3ffffff 0x80000000 0x100000000 0x200000000; do
        expected=0
        if [ "$mask" = "$enabled" ]; then
            expected=2
        fi
        log="$guest_dir/options-$kind-$mask.log"
        env LATX_AOT=0 LATX_INSTPTN_STATS=1 LATX_INSTPTN_MASK="$mask" \
            "$emulator" "$guest_dir/probe-$kind" >"$log" 2>&1
        actual=$(matches "$pattern" "$log")
        old=$(matches "$legacy" "$log")
        if [ "$actual" -ne "$expected" ] || [ "$old" -ne 0 ]; then
            echo "FAIL: $pattern mask=$mask expected=$expected" \
                "actual=$actual legacy=$old"
            cat "$log"
            exit 1
        fi
        echo "PASS: $pattern mask=$mask match=$actual legacy=$old"
    done
done
