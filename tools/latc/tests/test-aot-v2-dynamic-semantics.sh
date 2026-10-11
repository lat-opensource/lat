#!/bin/sh
set -eu

if [ "$#" -ne 10 ]; then
    echo "usage: $0 LATC RUNNER RUNTIME ROOTFS MAIN STARTUP PLUGIN PRELOAD WORK TIMEOUT" >&2
    exit 2
fi

latc=$1
runner=$2
runtime_dir=$3
rootfs=$4
guest=$5
startup=$6
plugin=$7
preload=$8
work=$9
run_timeout=${10}
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
rm -rf "$work"
mkdir -p "$work/empty-cache" "$work/hot-cache"

interp=$(readlink -f "$rootfs/lib64/ld-linux-x86-64.so.2")
libc=$(readlink -f "$rootfs/lib/x86_64-linux-gnu/libc.so.6")
cp "$startup" "$work/liblatc-semantics-startup.so"
export LATC_SEMANTICS_PLUGIN="$plugin"


make_tbset()
{
    source=$1
    output=$2
    pattern=$3
    python3 "$script_dir/make-symbol-tbset.py" "$source" "$pattern" "$output"
}

compile_module()
{
    source=$1
    output=$2
    profile=${3:-}
    if [ -n "$profile" ]; then set -- "$profile"; else set --; fi
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

make_tbset "$startup" "$work/startup.tbset" '^semantic_startup_.*'
make_tbset "$plugin" "$work/plugin.tbset" '^semantic_.*'
make_tbset "$preload" "$work/preload.tbset" '^semantic_.*'
compile_module "$startup" "$work/startup.so" "$work/startup.tbset"
compile_module "$plugin" "$work/plugin.so" "$work/plugin.tbset"
compile_module "$preload" "$work/preload.so" "$work/preload.tbset"
compile_module "$guest" "$work/main.so"
compile_module "$interp" "$work/interp.so"
compile_module "$libc" "$work/libc.so"

run_guest()
{
    cache=$1
    output=$2
    error=$3
    loops=${4:-0}
    guest_library_path="$work:$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    env LD_LIBRARY_PATH="$guest_library_path" \
      LD_PRELOAD="$preload" \
      LATX_AOT=0 LATX_AOT_V2_CACHE_DIR="$cache" LATX_AOT_V2_REPORT=1 \
      LATC_M3_CROSS_MODULE_LOOPS="$loops" \
      timeout -k 2s "$run_timeout" "$runner" -L "$rootfs" \
      "$guest" >"$output" 2>"$error"
}

printf 'startup=11 tls=7,9 ifunc=23 versions=31,32 hook=10 preload=77\n' \
  >"$work/expected"
run_guest "$work/empty-cache" "$work/cold.out" "$work/cold.err"
cmp "$work/expected" "$work/cold.out"
test "$(grep -c 'discovered ELF.*module=missing' "$work/cold.err")" -ge 6

run_guest "$work/hot-cache" "$work/hot.out" "$work/hot.err"
cmp "$work/expected" "$work/hot.out"
test "$(grep -c 'discovered ELF.*module=registered' "$work/hot.err")" -ge 6
test "$(grep -Ec 'module=(registered|inactive) aot_lookups=[1-9][0-9]*' \
  "$work/hot.err")" -ge 6
for source in "$startup" "$plugin" "$preload"; do
    sha=$(sha256sum "$source" | awk '{print $1}')
    grep -Eq "module stats source=$sha .*module=(registered|inactive) aot_lookups=[1-9]" \
      "$work/hot.err"
done
grep -Eq 'direct_targets=[1-9][0-9]* compat_tb_allocations=0' "$work/hot.err"

run_guest "$work/hot-cache" "$work/fast.out" "$work/fast.err" 100000
cmp "$work/expected" "$work/fast.out"
direct_targets=$(sed -n \
  's/.*runtime stats direct_targets=\([0-9][0-9]*\).*/\1/p' \
  "$work/fast.err")
test -n "$direct_targets"
test "$direct_targets" -lt 10000
grep -q 'compat_tb_allocations=0' "$work/fast.err"

echo "test-aot-v2-dynamic-semantics: PASS"
