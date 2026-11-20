#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
xadd_source=$(readlink -f "$2")
ordering_source=$(readlink -f "$3")
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

. "$(dirname "$(readlink -f "$0")")/no-lbt-test-lib.sh"

for source in "$xadd_source" "$ordering_source"; do
    output="$workdir/$(basename "$source" .S)"
    no_lbt_build_guest "$source" "$output"
    "$emulator" -latx-lbt 0 "$output"
done

echo "PASS: no-LBT locked operations preserve atomicity and x86 ordering"
