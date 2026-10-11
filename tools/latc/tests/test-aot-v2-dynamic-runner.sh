#!/bin/sh
set -eu

if [ "$#" -ne 7 ]; then
    echo "usage: $0 LATC RUNNER RUNTIME_DIR ROOTFS X86_GUEST WORKDIR TIMEOUT" >&2
    exit 2
fi

latc=$1
runner=$2
runtime_dir=$3
rootfs=$4
guest=$5
work=$6
run_timeout=$7
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
rm -rf "$work"
mkdir -p "$work/empty-cache" "$work/hot-cache"

interp=$(readlink -f "$rootfs/lib64/ld-linux-x86-64.so.2")
libc=$(readlink -f "$rootfs/lib/x86_64-linux-gnu/libc.so.6")
test -f "$interp" -a -f "$libc"

compile_module()
{
    source=$1
    output=$2
    LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    LAT_LD_PREFIX="$rootfs" LATC_TEST_ENV=works \
      "$script_dir/../scripts/compile-aot-v2-module.sh" \
      "$latc" "$runner" "$source" "$runtime_dir" "$output" >/dev/null
    sha=$(sha256sum "$source" | awk '{print $1}')
    cp "$output" "$work/hot-cache/$sha.so"
    printf '{"version":2,"module":"%s.so"}\n' "$sha" \
      >"$work/hot-cache/$sha.current"
    chmod 444 "$work/hot-cache/$sha.current"
}

compile_module "$guest" "$work/main.so"
compile_module "$interp" "$work/interp.so"
compile_module "$libc" "$work/libc.so"

run_guest()
{
    cache=$1
    output=$2
    error=$3
    shift 3
    env LD_LIBRARY_PATH="$runtime_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
      LATX_AOT=0 LATX_AOT_V2_CACHE_DIR="$cache" \
      LATX_AOT_V2_REPORT=1 LATC_TEST_ENV=works \
      "$@" timeout -k 2s "$run_timeout" "$runner" -L "$rootfs" \
      "$guest" alpha beta >"$output" 2>"$error"
}

printf 'Hello from glibc!\n' >"$work/expected"
run_guest "$work/empty-cache" "$work/cold.out" "$work/cold.err"
cmp "$work/expected" "$work/cold.out"
test "$(grep -c 'discovered ELF.*module=missing' "$work/cold.err")" -ge 3
test "$(grep -Ec 'module=missing aot_lookups=0 jit_fallbacks=[1-9][0-9]*' \
  "$work/cold.err")" -ge 3

run_guest "$work/hot-cache" "$work/hot.out" "$work/hot.err"
cmp "$work/expected" "$work/hot.out"
test "$(grep -c 'discovered ELF.*module=registered' "$work/hot.err")" -ge 3
test "$(grep -Ec 'module=registered aot_lookups=[1-9][0-9]*' \
  "$work/hot.err")" -ge 3
grep -Eq 'direct_targets=[1-9][0-9]* compat_tb_allocations=0' "$work/hot.err"

run_guest "$work/hot-cache" "$work/no-aslr.out" "$work/no-aslr.err" \
  setarch "$(uname -m)" -R
cmp "$work/hot.out" "$work/no-aslr.out"
test "$(grep -c 'discovered ELF.*module=registered' \
  "$work/no-aslr.err")" -ge 3
test "$(grep -Ec 'module=registered aot_lookups=[1-9][0-9]*' \
  "$work/no-aslr.err")" -ge 3
grep -Eq 'direct_targets=[1-9][0-9]* compat_tb_allocations=0' \
  "$work/no-aslr.err"

echo "test-aot-v2-dynamic-runner: PASS"
