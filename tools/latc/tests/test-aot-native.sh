#!/bin/sh
set -eu

latc=$1
runner=$2
guest=$3
workdir=$4

rm -rf "$workdir"
mkdir -p "$workdir/home"
"$(dirname "$0")/../scripts/compile-aot.sh" \
    "$latc" "$runner" "$guest" "$workdir/guest-aot.la64"
HOME="$workdir/home" LATC_STRICT_AOT=1 \
    LATC_STATS_OUT="$workdir/stats.json" "$workdir/guest-aot.la64"
python3 - "$workdir/stats.json" <<'PY'
import json
import sys

data = json.load(open(sys.argv[1]))
assert data["pretranslation_disabled"], data
assert data["pretranslated"] == 0, data
assert data["runtime_tb_gen_calls"] == 0, data
print("test-aot-native: PASS", data)
PY
