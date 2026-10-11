#!/bin/sh
set -eu

if [ "$#" -ne 4 ]; then
    echo "usage: $0 INSTALL_PREFIX X86_ROOTFS X86_GUEST WORKDIR" >&2
    exit 2
fi

prefix=$1
rootfs=$2
guest=$3
work=$4
latc=$prefix/bin/latc
latcd=$prefix/bin/latcd
runner=$prefix/bin/latx-x86_64
runtime_dir=$prefix/lib
runtime=$runtime_dir/liblat-aot-runtime.so.2

rm -rf "$work"
mkdir -m 700 -p "$work"
daemon_pid=
cleanup()
{
    if [ -n "$daemon_pid" ] && kill -0 "$daemon_pid" 2>/dev/null; then
        kill -TERM "$daemon_pid" 2>/dev/null || true
        wait "$daemon_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT HUP INT TERM

for path in "$latc" "$latcd" "$runner" "$runtime"; do
    test -f "$path"
done
for script in compile-aot-v2-module.sh compile-native-image.sh \
              compile-aot.sh link-aot-v2-module.sh; do
    test "$(stat -c %a "$prefix/libexec/latc/scripts/$script")" = 755
done
test "$(stat -c %a "$prefix/libexec/latc/aot-v2/include/lat-aot-v2.h")" = 644
test "$(stat -c %a "$prefix/libexec/latc/aot-v2/compiler/module.map")" = 644

identity=$($latc build-id)
test "${#identity}" -eq 64
test "$(LD_LIBRARY_PATH="$runtime_dir" "$runner" --latc-build-id)" = \
     "$identity"
test "$($latcd --build-id)" = "$identity"
python3 - "$runtime" "$identity" <<'PY'
import ctypes
import sys

runtime = ctypes.CDLL(sys.argv[1])
runtime.lat_aot_runtime_build_id.restype = ctypes.c_char_p
assert runtime.lat_aot_runtime_build_id().decode() == sys.argv[2]
assert runtime.lat_aot_runtime_abi_version() == 2
PY

readelf -d "$runner" | grep -q 'Shared library: \[liblat-aot-runtime.so.2\]'
readelf -d "$runtime" | grep -q 'Library soname: \[liblat-aot-runtime.so.2\]'
if readelf -d "$runner" "$latc" "$latcd" "$runtime" | \
   grep -Eq 'RPATH|RUNPATH'; then
    echo "installed AOT v2 products contain an RPATH" >&2
    exit 1
fi

wrong=0000000000000000000000000000000000000000000000000000000000000000
mkdir -m 700 "$work/mismatch"
printf '%s\n' '#!/bin/sh' \
  'if [ "${1:-}" = build-id ]; then' \
  "  echo $wrong" \
  '  exit 0' \
  'fi' \
  "exec $latc \"\$@\"" >"$work/mismatch/latc"
printf '%s\n' '#!/bin/sh' \
  'if [ "${1:-}" = --latc-build-id ]; then' \
  "  echo $wrong" \
  '  exit 0' \
  'fi' \
  "exec $runner \"\$@\"" >"$work/mismatch/runner"
chmod 755 "$work/mismatch/latc" "$work/mismatch/runner"
mkdir -m 700 "$work/mismatch/runtime"
cp "$runtime" "$work/mismatch/runtime/liblat-aot-runtime.so.2"
chmod 755 "$work/mismatch/runtime/liblat-aot-runtime.so.2"
python3 - "$work/mismatch/runtime/liblat-aot-runtime.so.2" \
             "$identity" "$wrong" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
data = path.read_bytes()
old = sys.argv[2].encode()
new = sys.argv[3].encode()
assert data.count(old) >= 1
path.write_bytes(data.replace(old, new))
PY

expect_mismatch()
{
    name=$1
    compiler=$2
    selected_runner=$3
    selected_runtime=$4
    pattern=$5
    phase=$work/mismatch-$name
    mkdir -m 700 "$phase"
    set +e
    timeout 5 "$latcd" --once --socket "$phase/latcd.sock" \
      --cache-dir "$phase/cache" --latc "$compiler" \
      --runner "$selected_runner" --runtime-dir "$selected_runtime" \
      >"$phase/stdout" 2>"$phase/stderr"
    rc=$?
    set -e
    test "$rc" -eq 1
    grep -q "$pattern" "$phase/stderr"
    test ! -e "$phase/latcd.sock"
    test ! -e "$phase/cache"
}

expect_mismatch latc "$work/mismatch/latc" "$runner" "$runtime_dir" \
  'latc build identity mismatch'
expect_mismatch runner "$latc" "$work/mismatch/runner" "$runtime_dir" \
  'runner build identity mismatch'
expect_mismatch runtime "$latc" "$runner" "$work/mismatch/runtime" \
  'AOT runtime build identity mismatch'

LD_LIBRARY_PATH="$runtime_dir" LATX_AOT=0 LATC_DISABLE_PRETRANSLATE=1 \
  timeout 120 "$runner" -L "$rootfs" "$guest" \
  >"$work/jit.stdout" 2>"$work/jit.stderr"

phase=$work/cold
mkdir -m 700 "$phase"
socket=$phase/latcd.sock
cache=$phase/cache
stats=$phase/stats.json
"$latcd" --serve --socket "$socket" --cache-dir "$cache" \
  --latc "$latc" --runner "$runner" --runtime-dir "$runtime_dir" \
  --x86-rootfs "$rootfs" --stats "$stats" \
  >"$phase/daemon.stdout" 2>"$phase/daemon.stderr" &
daemon_pid=$!
n=0
while [ ! -S "$socket" ] && kill -0 "$daemon_pid" 2>/dev/null; do
    n=$((n + 1))
    test "$n" -lt 500
    sleep 0.01
done
test -S "$socket"

LD_LIBRARY_PATH="$runtime_dir" LATX_AOT=0 \
LATX_AOT_V2_CACHE_DIR="$cache" LATX_AOT_V2_LATCD_SOCKET="$socket" \
LATX_AOT_V2_REPORT=1 LATC_DISABLE_PRETRANSLATE=1 \
  timeout 120 "$runner" -L "$rootfs" "$guest" \
  >"$phase/guest.stdout" 2>"$phase/guest.stderr"
cmp "$work/jit.stdout" "$phase/guest.stdout"

n=0
until python3 - "$stats" 2>/dev/null <<'PY'
import json
import sys

stats = json.load(open(sys.argv[1]))
assert stats["compiled"] > 0
assert stats["failed"] == 0
assert stats["active_jobs"] == 0
PY
do
    n=$((n + 1))
    test "$n" -lt 2400
    sleep 0.05
done
kill -TERM "$daemon_pid"
wait "$daemon_pid"
daemon_pid=
test "$(find "$cache" -maxdepth 1 -name '*.so' | wc -l)" -gt 0
test "$(find "$cache" -maxdepth 1 -name '*.current' | wc -l)" -gt 0

codegen=$(printf '%s' "$identity" | sha256sum | cut -d ' ' -f 1)
for module in "$cache"/*.so; do
    "$latc" inspect-module --json "$module"
done >"$phase/modules.jsonl"
python3 - "$phase/modules.jsonl" "$codegen" <<'PY'
import json
import sys

modules = [json.loads(line) for line in open(sys.argv[1])]
assert modules
assert all(module["codegen_id"] == sys.argv[2] for module in modules)
PY

LD_LIBRARY_PATH="$runtime_dir" LATX_AOT=0 \
LATX_AOT_V2_CACHE_DIR="$cache" LATX_AOT_V2_LATCD_SOCKET="$work/missing.sock" \
LATX_AOT_V2_STRICT=1 LATX_AOT_V2_REPORT=1 LATC_DISABLE_PRETRANSLATE=1 \
LATC_STRICT_AOT=1 LATC_STATS_OUT="$phase/warm.stats.json" \
  timeout 120 "$runner" -L "$rootfs" "$guest" \
  >"$phase/warm.stdout" 2>"$phase/warm.stderr"
cmp "$work/jit.stdout" "$phase/warm.stdout"
test "$(grep -c 'discovered ELF.*module=registered' \
  "$phase/warm.stderr")" -ge 3
test "$(grep -c 'compiler submitted' "$phase/warm.stderr" || true)" -eq 0
python3 - "$phase/warm.stats.json" <<'PY'
import json
import sys

stats = json.load(open(sys.argv[1]))
assert stats["runtime_file_tb_gen_calls"] == 0, stats
assert stats["runtime_file_tb_gen_attempts"] == 0, stats
PY

source_sha=$(sha256sum "$guest" | cut -d ' ' -f 1)
module_name=$(python3 - "$cache/$source_sha.current" <<'PY'
import json
import sys
print(json.load(open(sys.argv[1]))["module"])
PY
)
stale_module=$cache/$module_name
tbset=$cache/.tbsets/$source_sha.tbset
chmod 0644 "$stale_module"
python3 - "$stale_module" "$codegen" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
data = path.read_bytes()
old = bytes.fromhex(sys.argv[2])
assert data.count(old) >= 2
path.write_bytes(data.replace(old, bytes(32)))
PY
chmod 0444 "$stale_module"
"$latc" inspect-module --json "$stale_module" | \
  grep -q '"codegen_id":"0000000000000000000000000000000000000000000000000000000000000000"'

phase=$work/stale-codegen
mkdir -m 700 "$phase"
socket=$phase/latcd.sock
stats=$phase/stats.json
"$latcd" --serve --socket "$socket" --cache-dir "$cache" \
  --latc "$latc" --runner "$runner" --runtime-dir "$runtime_dir" \
  --x86-rootfs "$rootfs" --stats "$stats" \
  >"$phase/daemon.stdout" 2>"$phase/daemon.stderr" &
daemon_pid=$!
n=0
while [ ! -S "$socket" ] && kill -0 "$daemon_pid" 2>/dev/null; do
    n=$((n + 1))
    test "$n" -lt 500
    sleep 0.01
done
test -S "$socket"
"$latcd" --submit --socket "$socket" --tbset "$tbset" "$guest" \
  >"$phase/submit.stdout"
n=0
until python3 - "$stats" 2>/dev/null <<'PY'
import json
import sys

stats = json.load(open(sys.argv[1]))
assert stats["compiled"] == 1
assert stats["cache_hits"] == 0
assert stats["failed"] == 0
assert stats["active_jobs"] == 0
PY
do
    n=$((n + 1))
    test "$n" -lt 2400
    sleep 0.05
done
kill -TERM "$daemon_pid"
wait "$daemon_pid"
daemon_pid=
"$latc" inspect-module --json "$stale_module" | \
  grep -q "\"codegen_id\":\"$codegen\""

echo "test-aot-v2-install: PASS identity=$identity"
