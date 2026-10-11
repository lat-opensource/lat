#!/bin/sh
set -eu

if [ "$#" -ne 4 ]; then
    echo "usage: $0 LATC RUNNER X86_GUEST WORKDIR" >&2
    exit 2
fi

latc=$1
runner=$2
guest=$3
workdir=$4

rm -rf "$workdir"
mkdir -p "$workdir"
"$(dirname "$0")/../scripts/compile-native-elf.sh" \
    "$latc" "$runner" "$guest" "$workdir/long-tb.la64"
set +e
"$workdir/long-tb.la64" >"$workdir/stdout" 2>"$workdir/stderr"
rc=$?
set -e
test "$rc" -eq 42
test ! -s "$workdir/stdout"
test ! -s "$workdir/stderr"
"$workdir/long-tb.la64" --latc-inspect >"$workdir/inspect"
grep -q '^execution_model=lat-native-pie-shell$' "$workdir/inspect"
echo "test-native-long-tb: PASS"
