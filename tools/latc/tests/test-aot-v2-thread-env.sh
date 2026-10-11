#!/bin/sh
set -eu
runner=$1
runtime=$2
rootfs=$3
work=$4
mkdir -p "$work"
for threads in 1 4 16; do
    env -u LATC_EMIT_AOT -u LATX_AOT_V2_CACHE_DIR \
      -u LATX_AOT_V2_MODULE -u LATX_AOT_V2_LATCD_SOCKET \
      LD_LIBRARY_PATH="$runtime" LATX_AOT=0 LATC_DISABLE_PRETRANSLATE=1 \
      LATC_AOT_THREADS="$threads" \
      timeout -k 2s 30s "$runner" -L "$rootfs" \
      "$rootfs/usr/bin/python3" -S -c '
import threading
results = []
for i in range(12):
    thread = threading.Thread(target=lambda: results.append(sum(range(100))))
    thread.start()
    thread.join()
assert results == [4950] * 12, results
print("THREAD_ENV_OK")
' >"$work/$threads.out" 2>"$work/$threads.err"
    grep -qx THREAD_ENV_OK "$work/$threads.out"
done
echo 'test-aot-v2-thread-env: PASS'
