#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
    echo "usage: $0 LATC RUNNER RUNTIME_DIR X86_PIE TEST_BINARY WORKDIR" >&2
    exit 2
fi

latc=$1
runner=$2
runtime_dir=$3
guest=$4
test_binary=$5
work=$6
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

rm -rf "$work"
mkdir -p "$work"
export LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

LC_ALL=C readelf -h "$guest" | grep -Eq 'Type:.*DYN'
! LC_ALL=C readelf -lW "$guest" | grep -q INTERP

"$script_dir/../scripts/compile-native-image.sh" \
  "$latc" "$runner" "$guest" "$work/first.latnative" >/dev/null
"$script_dir/../scripts/compile-native-image.sh" \
  "$latc" "$runner" "$guest" "$work/second.latnative" >/dev/null
cmp "$work/first.latnative" "$work/second.latnative"

"$latc" inspect-native --json "$work/first.latnative" >"$work/native.json"
python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); assert d["flags"] & 1; assert d["guest_entry"] == 0x1000; assert d["preferred_guest_base"] == 0; assert d["tbs"] >= 1; assert d["pc_maps"] >= 1' \
  "$work/native.json"

"$script_dir/../scripts/link-aot-v2-module.sh" \
  "$latc" "$work/first.latnative" "$runtime_dir" "$work/module.so"
"$test_binary" "$work/module.so" "$work/first.latnative" --exit42

echo "test-aot-v2-pie: PASS"
