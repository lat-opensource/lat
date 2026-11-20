#!/bin/sh
set -eu
emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
. "$(dirname "$(readlink -f "$0")")/no-lbt-test-lib.sh"
name=$(basename "$source_file" .S)
no_lbt_build_guest "$source_file" "$workdir/$name"
"$emulator" -latx-lbt 0 "$workdir/$name"
echo "PASS: no-LBT $name"
