#!/bin/sh
set -eu

if [ "$#" -ne 8 ]; then
    echo "usage: $0 LATC RUNNER RUNTIME_DIR ROOTFS MAIN PLUGIN WORKDIR TIMEOUT" >&2
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
mkdir -p "$work/empty-cache" "$work/hot-cache" "$work/loader-cache" \
  "$work/libc-cache" "$work/mixed-cache" "$work/race-cache"

interp=$(readlink -f "$rootfs/lib64/ld-linux-x86-64.so.2")
libc=$(readlink -f "$rootfs/lib/x86_64-linux-gnu/libc.so.6")
test -f "$interp" -a -f "$libc"
export LATC_DLOPEN_PLUGIN="$plugin"

compile_module()
{
    source=$1
    output=$2
    profile=${3:-}
    if [ -n "$profile" ]; then
        set -- "$profile"
    else
        set --
    fi
    LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    LAT_LD_PREFIX="$rootfs" \
      "$script_dir/../scripts/compile-aot-v2-module.sh" \
      "$latc" "$runner" "$source" "$runtime_dir" "$output" "$@" >/dev/null
    sha=$(sha256sum "$source" | awk '{print $1}')
    cp "$output" "$work/hot-cache/$sha.so"
    printf '{"version":2,"module":"%s.so"}\n' "$sha" \
      >"$work/hot-cache/$sha.current"
    chmod 444 "$work/hot-cache/$sha.current"
}

python3 "$script_dir/make-symbol-tbset.py" "$plugin" \
  '^(latc_plugin_apply|latc_plugin_signal_site|latc_plugin_signal_resume)$' \
  "$work/plugin.tbset"
compile_module "$plugin" "$work/plugin.so" "$work/plugin.tbset"
plugin_sha=$(sha256sum "$plugin" | awk '{print $1}')
cp "$work/plugin.so" "$work/race-cache/$plugin_sha.so"
printf '{"version":2,"module":"%s.so"}\n' "$plugin_sha" \
  >"$work/race-cache/$plugin_sha.current"
chmod 444 "$work/race-cache/$plugin_sha.current"
compile_module "$interp" "$work/interp.so"
interp_sha=$(sha256sum "$interp" | awk '{print $1}')
cp "$work/interp.so" "$work/loader-cache/$interp_sha.so"
printf '{"version":2,"module":"%s.so"}\n' "$interp_sha" \
  >"$work/loader-cache/$interp_sha.current"
chmod 444 "$work/loader-cache/$interp_sha.current"
cp "$work/interp.so" "$work/mixed-cache/$interp_sha.so"
printf '{"version":2,"module":"%s.so"}\n' "$interp_sha" \
  >"$work/mixed-cache/$interp_sha.current"
chmod 444 "$work/mixed-cache/$interp_sha.current"
compile_module "$libc" "$work/libc.so"
libc_sha=$(sha256sum "$libc" | awk '{print $1}')
cp "$work/libc.so" "$work/libc-cache/$libc_sha.so"
printf '{"version":2,"module":"%s.so"}\n' "$libc_sha" \
  >"$work/libc-cache/$libc_sha.current"
chmod 444 "$work/libc-cache/$libc_sha.current"
cp "$work/libc.so" "$work/mixed-cache/$libc_sha.so"
printf '{"version":2,"module":"%s.so"}\n' "$libc_sha" \
  >"$work/mixed-cache/$libc_sha.current"
chmod 444 "$work/mixed-cache/$libc_sha.current"

run_guest()
{
    cache=$1
    output=$2
    error=$3
    rc_file=${output%.out}.rc
    shift 3
    if env LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        LATX_AOT=0 LATX_AOT_V2_CACHE_DIR="$cache" \
        LATX_AOT_V2_REPORT=1 \
        "$@" timeout -k 2s "$run_timeout" "$runner" -L "$rootfs" \
        "$guest" >"$output" 2>"$error"; then
        rc=0
    else
        rc=$?
    fi
    printf '%s\n' "$rc" \
      >"$rc_file"
    return "$rc"
}

printf 'dlopen loads=100 signals=100 concurrent_invalidation=0 result=40 moved=1\n' \
  >"$work/expected"
run_guest "$work/empty-cache" "$work/cold.out" "$work/cold.err"
cmp "$work/expected" "$work/cold.out"
test "$(grep -c 'discovered ELF.*module=missing' "$work/cold.err")" -ge 102

run_guest "$work/loader-cache" "$work/loader.out" "$work/loader.err"
cmp "$work/expected" "$work/loader.out"
grep -Eq "module stats source=$interp_sha .*module=registered aot_lookups=[1-9]" \
  "$work/loader.err"
grep -Eq "module stats source=$libc_sha .*module=missing aot_lookups=0" \
  "$work/loader.err"
grep -Eq "module stats source=$plugin_sha .*module=missing aot_lookups=0" \
  "$work/loader.err"

run_guest "$work/libc-cache" "$work/libc.out" "$work/libc.err"
cmp "$work/expected" "$work/libc.out"
grep -Eq "module stats source=$interp_sha .*module=missing aot_lookups=0" \
  "$work/libc.err"
grep -Eq "module stats source=$libc_sha .*module=registered aot_lookups=[1-9]" \
  "$work/libc.err"
grep -Eq "module stats source=$plugin_sha .*module=missing aot_lookups=0" \
  "$work/libc.err"

run_guest "$work/mixed-cache" "$work/mixed.out" "$work/mixed.err"
cmp "$work/expected" "$work/mixed.out"
grep -Eq "module stats source=$interp_sha .*module=registered aot_lookups=[1-9]" \
  "$work/mixed.err"
grep -Eq "module stats source=$libc_sha .*module=registered aot_lookups=[1-9]" \
  "$work/mixed.err"
grep -Eq "module stats source=$plugin_sha .*module=missing aot_lookups=0" \
  "$work/mixed.err"

run_guest "$work/hot-cache" "$work/hot.out" "$work/hot.err"
cmp "$work/expected" "$work/hot.out"
test "$(grep -c 'discovered ELF.*module=registered' "$work/hot.err")" -ge 102
test "$(grep -Ec 'module=(registered|inactive) aot_lookups=[1-9][0-9]*' \
  "$work/hot.err")" -ge 3
grep -Eq "module stats source=$plugin_sha .*module=inactive aot_lookups=[1-9]" \
  "$work/hot.err"
test "$(grep -Ec "module stats source=$plugin_sha .*module=inactive" \
  "$work/hot.err")" -eq 1
module_stats_count=$(sed -n \
  's/.*host_index_retired=[0-9][0-9]* module_stats=\([0-9][0-9]*\).*/\1/p' \
  "$work/hot.err")
test -n "$module_stats_count" -a "$module_stats_count" -le 8
test "$(grep -c 'AOT v2 deactivated range=' "$work/hot.err")" -ge 100
grep -Eq 'invalidation_unmap=100 .*signal_pc_lookups=100 ' \
  "$work/hot.err"
grep -Eq 'direct_targets=[1-9][0-9]* compat_tb_allocations=0' "$work/hot.err"
grep -Eq 'signal_pc_lookups=100 signal_pc_hits=100 signal_pc_misses=0' \
  "$work/hot.err"

run_guest "$work/hot-cache" "$work/no-aslr.out" "$work/no-aslr.err" \
  setarch "$(uname -m)" -R
cmp "$work/hot.out" "$work/no-aslr.out"
grep -Eq "module stats source=$plugin_sha .*module=inactive aot_lookups=[1-9]" \
  "$work/no-aslr.err"
test "$(grep -Ec "module stats source=$plugin_sha .*module=inactive" \
  "$work/no-aslr.err")" -eq 1
grep -Eq 'direct_targets=[1-9][0-9]* compat_tb_allocations=0' \
  "$work/no-aslr.err"
grep -Eq 'signal_pc_lookups=100 signal_pc_hits=100 signal_pc_misses=0' \
  "$work/no-aslr.err"

printf 'dlopen loads=1 signals=1 concurrent_invalidation=1 result=40 moved=0\n' \
  >"$work/race.expected"
run_guest "$work/race-cache" "$work/race.out" "$work/race.err" \
  env LATC_SIGNAL_INVALIDATION_RACE=handler \
      LATX_AOT_V2_TEST_SIGNAL_INVALIDATION_RACE=1
cmp "$work/race.expected" "$work/race.out"
grep -Eq 'signal_pc_lookups=1 signal_pc_hits=1 signal_pc_misses=0 ' \
  "$work/race.err"
grep -Eq 'signal_invalidation_overlaps=1' "$work/race.err"
grep -Eq 'invalidation_unmap=1 .*signal_pc_lookups=1' "$work/race.err"
grep -Eq "signal diagnostic source=$plugin_sha generation=1 guest_pc=0x[0-9a-f]+ " \
  "$work/race.err"

echo "test-aot-v2-dlopen-runner: PASS"
