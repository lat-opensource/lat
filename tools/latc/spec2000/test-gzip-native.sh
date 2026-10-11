#!/bin/sh
set -eu

if [ "$#" -ne 7 ]; then
    echo "usage: $0 LATC RUNNER GZIP INPUT SIZE EXPECTED WORKDIR" >&2
    exit 2
fi

latc=$1
runner=$2
guest=$3
input=$(cd "$(dirname "$4")" && pwd)/$(basename "$4")
size=$5
expected=$(cd "$(dirname "$6")" && pwd)/$(basename "$6")
workdir=$7

rm -rf "$workdir"
mkdir -p "$workdir"
"$(dirname "$0")/../scripts/compile-native-elf.sh" \
    "$latc" "$runner" "$guest" "$workdir/gzip.la64"
for run in 1 2; do
    "$workdir/gzip.la64" "$input" "$size" \
        >"$workdir/actual-$run.out" 2>"$workdir/actual-$run.err"
    test ! -s "$workdir/actual-$run.err"
    cmp "$workdir/actual-$run.out" "$expected"
done
"$workdir/gzip.la64" --latc-inspect >"$workdir/inspect"
grep -q '^execution_model=lat-native-pie-shell$' "$workdir/inspect"
readelf -h "$workdir/gzip.la64" | grep -q 'Machine:.*LoongArch'
! readelf -d "$workdir/gzip.la64" | grep -q 'latx-x86_64'
sha256sum "$workdir/actual-1.out" "$workdir/actual-2.out" "$expected"
echo "test-spec2000-gzip-native: PASS size=$size"
