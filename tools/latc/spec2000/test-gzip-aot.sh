#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
    echo "usage: $0 LATC RUNNER GZIP INPUT EXPECTED WORKDIR" >&2
    exit 2
fi

latc=$1
runner=$2
guest=$3
input=$4
expected=$5
workdir=$6

rm -rf "$workdir"
mkdir -p "$workdir/run-home"
"$latc" compile "$guest" -o "$workdir/profile-stage.la64" --runner "$runner"
LATX_AOT=0 LATC_PROFILE_OUT="$workdir/missing.profile" \
    LATC_STATS_OUT="$workdir/profile-stats.json" \
    "$workdir/profile-stage.la64" "$input" 2 >"$workdir/profile.out"
cmp "$workdir/profile.out" "$expected"

"$(dirname "$0")/../scripts/compile-aot.sh" \
    "$latc" "$runner" "$guest" "$workdir/gzip-aot.la64" \
    "$workdir/missing.profile"
HOME="$workdir/run-home" LATC_STRICT_AOT=1 \
    LATC_STATS_OUT="$workdir/strict-stats.json" \
    "$workdir/gzip-aot.la64" "$input" 2 >"$workdir/strict.out"
cmp "$workdir/strict.out" "$expected"

python3 - "$workdir/profile-stats.json" "$workdir/strict-stats.json" <<'PY'
import json
import sys

profile = json.load(open(sys.argv[1]))
strict = json.load(open(sys.argv[2]))
assert profile["runtime_tb_gen_calls"] > 0, profile
assert strict["pretranslation_disabled"], strict
assert strict["pretranslated"] == 0, strict
assert strict["runtime_tb_gen_calls"] == 0, strict
print("test-spec2000-gzip-aot: PASS", {"profile": profile, "strict": strict})
PY
sha256sum "$workdir/strict.out" "$expected"
