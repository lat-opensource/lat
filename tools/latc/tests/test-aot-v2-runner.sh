#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
    echo "usage: $0 LATC RUNNER RUNTIME_DIR X86_GUEST NATIVE_IMAGE WORKDIR" >&2
    exit 2
fi

latc=$1
runner=$2
runtime_dir=$3
guest=$4
native_image=$5
work=$6
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
lifecycle_rounds=${AOT_V2_LIFECYCLE_ROUNDS:-100000}
rm -rf "$work"
mkdir -p "$work"

"$script_dir/../scripts/link-aot-v2-module.sh" \
  "$latc" "$native_image" "$runtime_dir" "$work/module.so"
"$latc" inspect-module --json "$work/module.so" >"$work/module.json"
"$latc" inspect-native --json "$native_image" >"$work/native.json"
python3 -c 'import json,sys; n=json.load(open(sys.argv[1])); m=json.load(open(sys.argv[2])); assert m["tbs"] == n["tbs"]; assert m["pc_maps"] == n["pc_maps"]' \
  "$work/native.json" "$work/module.json"
grep -q '"pc_maps":[1-9]' "$work/module.json"
grep -q '"precise_pc_map":true' "$work/module.json"
grep -q '"test_only":false' "$work/module.json"
"$latc" compile "$guest" -o "$work/runner" --runner "$runner" >/dev/null

LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
LATX_AOT_V2_MODULE="$work/module.so" \
LATX_AOT_V2_SOURCE="$guest" \
LATX_AOT_V2_STRICT=1 \
LATX_AOT_V2_REPORT=1 \
LATX_AOT_V2_TEST_LIFECYCLE_ROUNDS="$lifecycle_rounds" \
LATC_DISABLE_PRETRANSLATE=1 \
LATC_STRICT_AOT=1 \
LATC_STATS_OUT="$work/stats.json" \
  "$work/runner" >"$work/stdout" 2>"$work/stderr"

printf 'Hello, LATC!\n' >"$work/expected"
cmp "$work/expected" "$work/stdout"
grep -Eq 'AOT v2 registered module with [1-9][0-9]* TBs' "$work/stderr"
grep -q "AOT v2 lifecycle stress PASS rounds=$lifecycle_rounds " \
  "$work/stderr"
grep -q 'live=1 retired=0 free=1 allocated=2 registry_retired=0' \
  "$work/stderr"
grep -q '"runtime_tb_gen_attempts":0' "$work/stats.json"
grep -q '"runtime_tb_gen_calls":0' "$work/stats.json"
grep -q '"pretranslation_disabled":true' "$work/stats.json"
LC_ALL=C readelf -lW "$work/module.so" | \
  awk '/ LOAD / && $0 ~ /W/ && $0 ~ /E/ { bad=1 } END { exit bad }'
LC_ALL=C readelf -dW "$work/module.so" | \
  grep -q 'Shared library: \[liblat-aot-runtime.so.2\]'
LC_ALL=C readelf -SW "$work/module.so" | grep -q '.rodata.lat.map'

LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
LATX_AOT_V2_MODULE="$work/module.so" \
LATX_AOT_V2_SOURCE="$work/missing-source" \
LATX_AOT_V2_STRICT=1 \
LATC_DISABLE_PRETRANSLATE=1 \
LATC_STRICT_AOT=1 \
LATC_STATS_OUT="$work/bundle-identity-stats.json" \
  "$work/runner" >"$work/bundle-identity-stdout" \
  2>"$work/bundle-identity-stderr"
cmp "$work/expected" "$work/bundle-identity-stdout"
grep -q '"runtime_tb_gen_attempts":0' "$work/bundle-identity-stats.json"
grep -q '"runtime_tb_gen_calls":0' "$work/bundle-identity-stats.json"

LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
LATX_AOT_V2_MODULE="$work/module.so" \
LATX_AOT_V2_SOURCE="$guest" \
LATX_AOT_V2_REPORT=1 \
LATX_AOT_V2_TEST_HWCAP=0x410 \
LATC_DISABLE_PRETRANSLATE=1 \
LATC_STATS_OUT="$work/lsx-fallback-stats.json" \
  "$work/runner" >"$work/lsx-fallback-stdout" \
  2>"$work/lsx-fallback-stderr"
cmp "$work/expected" "$work/lsx-fallback-stdout"
grep -q 'AOT CPU features are unavailable' "$work/lsx-fallback-stderr"
grep -q 'host_features=0x3' "$work/lsx-fallback-stderr"
grep -Eq '"runtime_tb_gen_attempts":[1-9][0-9]*' \
  "$work/lsx-fallback-stats.json"
echo "test-aot-v2-runner: PASS"
