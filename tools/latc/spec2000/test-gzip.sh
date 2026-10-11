#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
    echo "usage: $0 LATC RUNNER GZIP INPUT EXPECTED WORKDIR" >&2
    exit 2
fi

latc=$1
runner=$2
gzip_guest=$3
input=$4
expected=$5
workdir=$6

mkdir -p "$workdir"
rm -f "$workdir/gzip.la64" "$workdir/actual.out" "$workdir/stats.json"
"$latc" compile "$gzip_guest" -o "$workdir/gzip.la64" --runner "$runner"
chmod +x "$workdir/gzip.la64"
(
    cd "$workdir"
    LATX_AOT=0 LATC_STATS_OUT=stats.json ./gzip.la64 "$input" 2 >actual.out
)
cmp "$workdir/actual.out" "$expected"
python3 - "$workdir/stats.json" <<'PY'
import json
import sys

data = json.load(open(sys.argv[1]))
assert data["cfg_tbs"] > 0, data
assert data["pretranslated"] == \
    data["cfg_tbs"] + data["continuation_tbs"] + \
    data["edge_target_tbs"], data
assert data["failed"] == 0, data
assert (data["same_extent"] + data["shorter_than_cfg"] +
        data["longer_than_cfg"]) == data["cfg_tbs"], data
assert "runtime_tb_gen_calls" in data, data
print("test-spec2000-gzip: PASS", data)
PY
sha256sum "$workdir/actual.out" "$expected"
