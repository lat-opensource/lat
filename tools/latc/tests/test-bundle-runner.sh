#!/bin/sh
set -eu

latc=$1
runner=$2
guest=$3
workdir=$4

mkdir -p "$workdir"
bundle="$workdir/guest.la64"
stats="$workdir/stats.json"

"$latc" compile "$guest" -o "$bundle" --runner "$runner"
LATX_AOT=0 LATC_STATS_OUT="$stats" "$bundle" smoke-arg
python3 - "$stats" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
assert data["cfg_tbs"] > 0, data
assert data["pretranslated"] == data["cfg_tbs"], data
assert data["failed"] == 0, data
assert (data["same_extent"] + data["shorter_than_cfg"] +
        data["longer_than_cfg"]) == data["cfg_tbs"], data
assert "runtime_tb_gen_calls" in data, data
assert data["runtime_tb_gen_calls"] == 0, data
print("test-bundle-runner: PASS", data)
PY
