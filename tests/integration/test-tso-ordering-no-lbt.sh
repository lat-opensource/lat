#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

. "$(dirname "$(readlink -f "$0")")/no-lbt-test-lib.sh"

guest="$workdir/latx-tso-ordering-no-lbt"
no_lbt_build_guest "$source_file" "$guest"

"$emulator" -latx-lbt 0 "$guest"

echo "PASS: no-LBT mode preserves x86 TSO ordering"
