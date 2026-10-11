#!/bin/sh
set -eu
if [ "$#" -ne 6 ]; then
    echo "usage: $0 LATCD LATC RUNNER RUNTIME ROOTFS WORKDIR" >&2
    exit 2
fi
latcd=$1 latc=$2 runner=$3 runtime=$4 rootfs=$5 work=$6
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
mkdir -m 700 -p "$work"
daemon_pid=
cleanup()
{
    if [ -n "$daemon_pid" ]; then
        kill -TERM "$daemon_pid" 2>/dev/null || true
        wait "$daemon_pid" || true
    fi
}
trap cleanup EXIT HUP INT TERM
LD_LIBRARY_PATH="$runtime" "$latcd" --serve --socket "$work/s" \
  --cache-dir "$work/cache" --latc "$latc" --runner "$runner" \
  --runtime-dir "$runtime" --x86-rootfs "$rootfs" --workers 2 \
  --flush-only --stats "$work/stats.json" \
  >"$work/daemon.out" 2>"$work/daemon.err" &
daemon_pid=$!
n=0
while [ ! -S "$work/s" ]; do
    kill -0 "$daemon_pid"
    n=$((n + 1))
    [ "$n" -lt 500 ]
    sleep 0.01
done
for mode in close dup2 dup3 setfd; do
    LD_LIBRARY_PATH="$runtime" LAT_LD_PREFIX="$rootfs" \
      LATX_AOT=0 LATC_DISABLE_PRETRANSLATE=1 LATX_AOT_V2_CACHE_DIR= \
      LATX_AOT_V2_MODULE= LATX_AOT_V2_LATCD_SOCKET="$work/s" \
      LATX_AOT_V2_REPORT=1 timeout -k 2s 60s "$runner" -L "$rootfs" \
      "$rootfs/usr/bin/python3" "$script_dir/aot-source-fd.py" \
      "$runner" "$rootfs" "$mode" >"$work/$mode.out" 2>"$work/$mode.err"
    grep -q "^SOURCE_FD_OK $mode " "$work/$mode.out"
    if grep -Eq 'submission failed|compiler_submission_failures=[1-9]' \
        "$work/$mode.err"; then
        echo "AOT source descriptor submission failed: $mode" >&2
        exit 1
    fi
    grep -q 'TB set submitted' "$work/$mode.err"
done
kill -TERM "$daemon_pid"
wait "$daemon_pid"
daemon_pid=
python3 "$script_dir/test-aot-v2-close-latency.py" "$runner" "$runtime" \
  "$rootfs" "$work/close-latency"
echo 'test-aot-v2-source-fd: PASS'
