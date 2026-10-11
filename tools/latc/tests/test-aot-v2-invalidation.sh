#!/bin/sh
set -eu

if [ "$#" -ne 8 ]; then
    echo "usage: $0 LATC RUNNER RUNTIME_DIR ROOTFS GUEST PLUGIN WORKDIR TIMEOUT" >&2
    exit 2
fi

latc=$1
runner=$2
runtime_dir=$3
rootfs=$4
guest=$5
plugin=$6
work=$7
run_timeout=$8
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
rm -rf "$work"
mkdir -p "$work/cache"

interp=$(readlink -f "$rootfs/lib64/ld-linux-x86-64.so.2")
libc=$(readlink -f "$rootfs/lib/x86_64-linux-gnu/libc.so.6")
test -f "$interp" -a -f "$libc"

python3 "$script_dir/make-symbol-tbset.py" "$plugin" \
  '^(latc_invalidation_value|latc_invalidation_cross_page|latc_invalidation_entry)$' \
  "$work/plugin.tbset"

LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
LAT_LD_PREFIX="$rootfs" \
  "$script_dir/../scripts/compile-aot-v2-module.sh" \
  "$latc" "$runner" "$plugin" "$runtime_dir" "$work/plugin.so" \
  "$work/plugin.tbset" >/dev/null
plugin_sha=$(sha256sum "$plugin" | awk '{print $1}')
cp "$work/plugin.so" "$work/cache/$plugin_sha.so"
printf '{"version":2,"module":"%s.so"}\n' "$plugin_sha" \
  >"$work/cache/$plugin_sha.current"
chmod 444 "$work/cache/$plugin_sha.current"

plugin_one=$work/latc-m5-invalidation-one.so
plugin_two=$work/latc-m5-invalidation-two.so
cp "$plugin" "$plugin_one"
cp "$plugin" "$plugin_two"
trap 'rm -f "$plugin_one" "$plugin_two"' EXIT HUP INT TERM

run_mode()
{
    mode=$1
    env LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
      LATX_AOT=0 LATX_AOT_V2_CACHE_DIR="$work/cache" \
      LATX_AOT_V2_REPORT=1 \
      timeout -k 2s "$run_timeout" "$runner" -L "$rootfs" \
      "$guest" "$mode" "$plugin_one" "$plugin_two" \
      >"$work/$mode.out" 2>"$work/$mode.raw.err"
    python3 "$script_dir/normalize-invalidation-stats.py" \
      "$work/$mode.raw.err" >"$work/$mode.err"
    grep -q "$mode: PASS" "$work/$mode.out"
    grep -Eq 'direct_targets=[1-9][0-9]* compat_tb_allocations=0' \
      "$work/$mode.err"
}

run_mode map-fixed
grep -Eq 'invalidated_instances=1 invalidated_exec_ranges=1 .*invalidation_map_fixed=1 ' \
  "$work/map-fixed.err"

run_mode map-fixed-failed
grep -Eq 'invalidated_instances=0 invalidated_exec_ranges=0 .*invalidation_map_fixed=0 ' \
  "$work/map-fixed-failed.err"

run_mode munmap-partial
grep -Eq 'invalidated_instances=1 invalidated_exec_ranges=1 .*invalidation_unmap=1 ' \
  "$work/munmap-partial.err"

run_mode munmap-complete
grep -Eq 'invalidated_instances=1 invalidated_exec_ranges=1 .*invalidation_unmap=1 ' \
  "$work/munmap-complete.err"

run_mode mprotect
grep -Eq 'invalidated_instances=1 invalidated_exec_ranges=1 .*invalidation_protection=1 ' \
  "$work/mprotect.err"
grep -Eq 'revalidated_instances=0 revalidation_failures=[1-9][0-9]* ' \
  "$work/mprotect.err"

run_mode mprotect-unchanged
grep -Eq 'invalidated_instances=1 invalidated_exec_ranges=1 .*invalidation_protection=1 ' \
  "$work/mprotect-unchanged.err"
grep -Eq 'revalidated_instances=1 revalidation_failures=0 ' \
  "$work/mprotect-unchanged.err"

run_mode mprotect-failed
grep -Eq 'invalidated_instances=0 invalidated_exec_ranges=0 .*invalidation_protection=0 ' \
  "$work/mprotect-failed.err"

run_mode mremap-failed
grep -Eq 'invalidated_instances=0 invalidated_exec_ranges=0 .*invalidation_unmap=0 ' \
  "$work/mremap-failed.err"

run_mode smc-cross
grep -Eq 'invalidated_instances=1 invalidated_exec_ranges=1 .*invalidation_protection=1 ' \
  "$work/smc-cross.err"

run_mode smc-thread
grep -Eq 'invalidated_instances=1 invalidated_exec_ranges=1 .*invalidation_protection=1 ' \
  "$work/smc-thread.err"

run_mode concurrent
grep -Eq 'invalidated_instances=1 invalidated_exec_ranges=1 .*invalidation_protection=1 ' \
  "$work/concurrent.err"

run_mode unaffected
test "$(grep -c 'discovered ELF.*module=registered' \
  "$work/unaffected.err")" -ge 2
test "$(grep -c "module stats source=$plugin_sha .*module=registered" \
  "$work/unaffected.err")" -eq 1
test "$(grep -c "module stats source=$plugin_sha " \
  "$work/unaffected.err")" -eq 1
grep -Eq 'instance_live=1 instance_retired=0 instance_free=1 ' \
  "$work/unaffected.err"

run_mode reload
test "$(grep -c 'discovered ELF.*module=registered' \
  "$work/reload.err")" -ge 2
grep -Eq 'invalidated_instances=1 invalidated_exec_ranges=1 .*invalidation_unmap=1 ' \
  "$work/reload.err"

echo "test-aot-v2-invalidation: PASS"
