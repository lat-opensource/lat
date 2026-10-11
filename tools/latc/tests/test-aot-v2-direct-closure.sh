#!/bin/sh
set -eu
latc=$1
runner=$2
runtime_dir=$3
guest=$4
work=$5
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
mkdir -p "$work"
"$script_dir/../scripts/compile-native-image.sh" \
  "$latc" "$runner" "$guest" "$work/image.native" >"$work/compile.log" 2>&1
"$script_dir/../scripts/link-aot-v2-module.sh" \
  "$latc" "$work/image.native" "$runtime_dir" "$work/module.so"
"$latc" compile "$guest" -o "$work/runner" --runner "$runner" >/dev/null
status=0
env LD_LIBRARY_PATH="$runtime_dir" LATX_AOT=0 \
  LATX_AOT_V2_MODULE="$work/module.so" LATX_AOT_V2_SOURCE="$guest" \
  LATX_AOT_V2_STRICT=1 LATC_STRICT_AOT=1 LATC_DISABLE_PRETRANSLATE=1 \
  LATC_STATS_OUT="$work/stats.json" \
  timeout -k 2s 10s "$work/runner" >"$work/stdout" 2>"$work/stderr" || status=$?
test "$status" -eq 42
python3 - "$work/stats.json" <<'PY'
import json
import sys
stats = json.load(open(sys.argv[1]))
assert stats["runtime_tb_gen_attempts"] == 0, stats
assert stats["runtime_tb_gen_calls"] == 0, stats
PY
printf 'test-aot-v2-direct-closure: PASS\n'
