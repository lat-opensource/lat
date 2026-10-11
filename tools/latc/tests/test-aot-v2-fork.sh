#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
    echo "usage: $0 LATC RUNNER RUNTIME_DIR ROOTFS GUEST WORKDIR" >&2
    exit 2
fi

latc=$1
runner=$2
runtime_dir=$3
rootfs=$4
guest=$5
work=$6
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

rm -rf "$work"
mkdir -p "$work/cache"
source_sha=$(sha256sum "$guest" | awk '{print $1}')
python3 "$script_dir/make-symbol-tbset.py" "$guest" \
  '^fork_hot_value$' "$work/fork.tbset"

LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
LAT_LD_PREFIX="$rootfs" \
  "$script_dir/../scripts/compile-aot-v2-module.sh" \
  "$latc" "$runner" "$guest" "$runtime_dir" "$work/fork.so" \
  "$work/fork.tbset" >"$work/compile.stdout"
cp "$work/fork.so" "$work/cache/$source_sha.so"
printf '{"version":2,"module":"%s.so"}\n' "$source_sha" \
  >"$work/cache/$source_sha.current"
chmod 444 "$work/cache/$source_sha.current"

env HOME="$work/home" LD_LIBRARY_PATH="$runtime_dir" LATX_AOT=0 \
  LATX_AOT_V2_CACHE_DIR="$work/cache" LATX_AOT_V2_REPORT=1 \
  timeout -k 2s 30s "$runner" -L "$rootfs" "$guest" \
  >"$work/run.stdout" 2>"$work/run.stderr"

grep -q '^FORK_OK parent_aot_child_aot=1 ' "$work/run.stdout"
grep -q '^FORK_CHILD_AOT_OK$' "$work/run.stdout"
grep -q '^latx: AOT v2 fork child retained AOT$' "$work/run.stderr"
test "$(grep -c 'fork child switched to JIT' "$work/run.stderr" || true)" -eq 0
grep -Eq "module stats source=$source_sha .*module=registered aot_lookups=[1-9]" \
  "$work/run.stderr"
grep -Eq 'direct_targets=[1-9][0-9]* compat_tb_allocations=0' \
  "$work/run.stderr"
echo "test-aot-v2-fork: PASS"
