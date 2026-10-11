#!/bin/sh
set -eu

if [ "$#" -ne 8 ]; then
    echo "usage: $0 LATCD LATC RUNNER RUNTIME_DIR ROOTFS STATIC_GUEST DYNAMIC_GUEST WORKDIR" >&2
    exit 2
fi

latcd=$1
latc=$2
runner=$3
runtime_dir=$4
rootfs=$5
static_guest=$6
dynamic_guest=$7
work=$8
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
export LATC_FAKE_REAL=$latc
export LATX_AOT=0
rm -rf "$work"
mkdir -m 700 -p "$work"
daemon_pid=
guest_pid=

cleanup()
{
    if [ -n "$guest_pid" ] && kill -0 "$guest_pid" 2>/dev/null; then
        kill -TERM "$guest_pid" 2>/dev/null || true
        wait "$guest_pid" 2>/dev/null || true
    fi
    if [ -n "$daemon_pid" ] && kill -0 "$daemon_pid" 2>/dev/null; then
        kill -TERM "$daemon_pid" 2>/dev/null || true
        wait "$daemon_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT HUP INT TERM

start_daemon()
{
    phase=$1
    compiler=$2
    mkdir -m 700 "$phase"
    socket=$phase/latcd.sock
    cache=$phase/cache
    stats=$phase/stats.json
    "$latcd" --serve --socket "$socket" --cache-dir "$cache" \
      --latc "$compiler" --runner "$runner" --runtime-dir "$runtime_dir" \
      --x86-rootfs "$rootfs" --stats "$stats" \
      >"$phase/daemon.stdout" 2>"$phase/daemon.stderr" &
    daemon_pid=$!
    n=0
    while [ ! -S "$socket" ] && kill -0 "$daemon_pid" 2>/dev/null; do
        n=$((n + 1))
        [ "$n" -lt 500 ] || break
        sleep 0.01
    done
    [ -S "$socket" ]
}

stop_daemon()
{
    kill -TERM "$daemon_pid"
    wait "$daemon_pid"
    daemon_pid=
}

mkdir -m 700 "$work/no-daemon-cache"
LD_LIBRARY_PATH="$runtime_dir" \
LATX_AOT_V2_CACHE_DIR="$work/no-daemon-cache" \
LATX_AOT_V2_LATCD_SOCKET="$work/missing.sock" \
LATX_AOT_V2_REPORT=1 LATC_DISABLE_PRETRANSLATE=1 \
  timeout 60 "$runner" "$static_guest" >"$work/no-daemon.stdout" \
  2>"$work/no-daemon.stderr"
test "$(cat "$work/no-daemon.stdout")" = "Hello, LATC!"
grep -q 'compiler_submissions=0 compiler_submission_failures=1' \
  "$work/no-daemon.stderr"

start_daemon "$work/live-submit" "$script_dir/fake-latc-slow.sh"
LD_LIBRARY_PATH="$runtime_dir" LATX_AOT_V2_CACHE_DIR="$cache" \
LATX_AOT_V2_LATCD_SOCKET="$socket" LATC_DISABLE_PRETRANSLATE=1 \
  "$runner" -L "$rootfs" "$rootfs/usr/bin/sleep" 10 \
  >"$phase/guest.stdout" 2>"$phase/guest.stderr" &
guest_pid=$!
n=0
while :; do
    if [ -f "$stats" ] && python3 - "$stats" 2>/dev/null <<'PY'
import json
import sys
assert json.load(open(sys.argv[1]))["requests"] > 0
PY
    then
        break
    fi
    n=$((n + 1))
    [ "$n" -lt 200 ] || exit 1
    kill -0 "$guest_pid"
    sleep 0.01
done
kill -0 "$guest_pid"
kill -TERM "$guest_pid"
wait "$guest_pid" 2>/dev/null || true
guest_pid=
stop_daemon

start_daemon "$work/static-cold" "$script_dir/fake-latc-slow.sh"
start=$(date +%s%N)
LD_LIBRARY_PATH="$runtime_dir" LATX_AOT_V2_CACHE_DIR="$cache" \
LATX_AOT_V2_LATCD_SOCKET="$socket" LATX_AOT_V2_REPORT=1 \
LATC_DISABLE_PRETRANSLATE=1 timeout 60 "$runner" "$static_guest" \
  >"$phase/guest.stdout" 2>"$phase/guest.stderr"
end=$(date +%s%N)
elapsed_ms=$(((end - start) / 1000000))
test "$(cat "$phase/guest.stdout")" = "Hello, LATC!"
test "$elapsed_ms" -lt 900
grep -q 'compiler_submissions=1 compiler_submission_failures=0' \
  "$phase/guest.stderr"
grep -q 'module=missing' "$phase/guest.stderr"
"$latcd" --flush-source --socket "$socket" "$static_guest" \
  >"$phase/flush.client"
grep -q '^status=0$' "$phase/flush.client"
test "$(find "$cache" -maxdepth 1 -name '*.so' | wc -l)" -eq 1
test "$(find "$cache" -maxdepth 1 -name '*.current' | wc -l)" -eq 1
stop_daemon

static_source_sha=$(sha256sum "$static_guest" | cut -d ' ' -f 1)
static_tbset="$cache/.tbsets/$static_source_sha.tbset"
static_tbset_sha=$(sha256sum "$static_tbset" | cut -d ' ' -f 1)
python3 - "$cache/$static_source_sha.current" \
  "$static_source_sha-$static_tbset_sha.so" <<'PY'
import json
import sys
current = json.load(open(sys.argv[1]))
assert current["version"] == 2, current
assert current["module"] == sys.argv[2], current
PY
chmod 0644 "$cache/$static_source_sha.current"
printf '{broken current index\n' >"$cache/$static_source_sha.current"
chmod 0444 "$cache/$static_source_sha.current"
LD_LIBRARY_PATH="$runtime_dir" LATX_AOT_V2_CACHE_DIR="$cache" \
LATX_AOT_V2_LATCD_SOCKET="$work/missing.sock" \
LATC_STATS_OUT="$work/corrupt-current.stats.json" \
LATX_AOT_V2_REPORT=1 LATC_DISABLE_PRETRANSLATE=1 \
  timeout 60 "$runner" "$static_guest" \
  >"$work/corrupt-current.stdout" 2>"$work/corrupt-current.stderr"
test "$(cat "$work/corrupt-current.stdout")" = "Hello, LATC!"
grep -q 'module=rejected reason=AOT v2 module manifest is invalid' \
  "$work/corrupt-current.stderr"
grep -Eq 'aot_lookups=0 jit_fallbacks=[1-9][0-9]*' \
  "$work/corrupt-current.stderr"
grep -q 'compiler_submissions=0 compiler_submission_failures=1' \
  "$work/corrupt-current.stderr"
python3 - "$work/corrupt-current.stats.json" <<'PY'
import json
import sys

stats = json.load(open(sys.argv[1]))
assert stats["runtime_file_tb_gen_calls"] > 0, stats
assert stats["runtime_file_tb_gen_attempts"] > 0, stats
PY
set +e
LD_LIBRARY_PATH="$runtime_dir" LATX_AOT_V2_CACHE_DIR="$cache" \
LATX_AOT_V2_LATCD_SOCKET="$work/missing.sock" \
LATX_AOT_V2_REPORT=1 LATC_DISABLE_PRETRANSLATE=1 LATC_STRICT_AOT=1 \
  timeout 60 "$runner" "$static_guest" \
  >"$work/strict-no-stats.stdout" 2>"$work/strict-no-stats.stderr"
strict_no_stats_status=$?
set -e
test "$strict_no_stats_status" -eq 125
test ! -s "$work/strict-no-stats.stdout"
grep -q 'module=rejected reason=AOT v2 module manifest is invalid' \
  "$work/strict-no-stats.stderr"
grep -q 'strict AOT rejected runtime TB generation' \
  "$work/strict-no-stats.stderr"

start_daemon "$work/dynamic" "$latc"
start=$(date +%s%N)
LD_LIBRARY_PATH="$runtime_dir" LATC_TEST_ENV=works LATX_AOT=0 \
LATX_AOT_V2_CACHE_DIR="$cache" LATX_AOT_V2_LATCD_SOCKET="$socket" \
LATX_AOT_V2_REPORT=1 LATC_DISABLE_PRETRANSLATE=1 \
  timeout 60 "$runner" -L "$rootfs" "$dynamic_guest" alpha beta \
  >"$phase/cold.stdout" 2>"$phase/cold.stderr"
end=$(date +%s%N)
dynamic_ms=$(((end - start) / 1000000))
test "$(cat "$phase/cold.stdout")" = "Hello from glibc!"
grep -q 'compiler_submissions=3 compiler_submission_failures=0' \
  "$phase/cold.stderr"
test "$(grep -c 'TB set submitted' "$phase/cold.stderr")" -eq 3
test "$(grep -c 'discovered ELF.*module=missing' "$phase/cold.stderr")" -ge 3
"$latcd" --flush-all --socket "$socket" >"$phase/flush.client"
grep -q '^status=0$' "$phase/flush.client"
publish_end=$(date +%s%N)
dynamic_publish_ms=$(((publish_end - start) / 1000000))
test "$dynamic_publish_ms" -lt 5000
test "$(find "$cache" -maxdepth 1 -name '*.so' | wc -l)" -eq 3
test "$(find "$cache" -maxdepth 1 -name '*.current' | wc -l)" -eq 3
test -z "$(find "$cache/.tmp" -mindepth 1 -maxdepth 1 -print -quit)"
stop_daemon

tbsets_before=$(find "$cache/.tbsets" -type f -exec sha256sum {} + | \
  LC_ALL=C sort | sha256sum | cut -d ' ' -f 1)
LD_LIBRARY_PATH="$runtime_dir" LATC_TEST_ENV=works LATX_AOT=0 \
LATX_AOT_V2_CACHE_DIR="$cache" LATX_AOT_V2_LATCD_SOCKET="$socket" \
LATX_AOT_V2_STRICT=1 LATX_AOT_V2_REPORT=1 LATC_DISABLE_PRETRANSLATE=1 \
LATC_STRICT_AOT=1 LATC_STATS_OUT="$phase/warm.stats.json" \
  timeout 60 "$runner" -L "$rootfs" \
  "$dynamic_guest" alpha beta >"$phase/warm.stdout" \
  2>"$phase/warm.stderr"
test "$(cat "$phase/warm.stdout")" = "Hello from glibc!"
test "$(grep -c 'discovered ELF.*module=registered' "$phase/warm.stderr")" -ge 3
test "$(grep -Ec 'module=registered aot_lookups=[1-9][0-9]*' \
  "$phase/warm.stderr")" -ge 3
grep -Eq 'direct_targets=[1-9][0-9]* compat_tb_allocations=0 file_dispatch_misses=0' \
  "$phase/warm.stderr"
grep -q 'compiler_submissions=0 compiler_submission_failures=0' \
  "$phase/warm.stderr"
tbsets_after=$(find "$cache/.tbsets" -type f -exec sha256sum {} + | \
  LC_ALL=C sort | sha256sum | cut -d ' ' -f 1)
test "$tbsets_before" = "$tbsets_after"
python3 - "$phase/warm.stats.json" <<'PY'
import json
import sys

stats = json.load(open(sys.argv[1]))
assert stats["runtime_file_tb_gen_calls"] == 0, stats
assert stats["runtime_file_tb_gen_attempts"] == 0, stats
PY

printf 'test-latcd-runner: PASS static_cold_ms=%s dynamic_cold_ms=%s dynamic_publish_ms=%s\n' \
  "$elapsed_ms" "$dynamic_ms" "$dynamic_publish_ms"
