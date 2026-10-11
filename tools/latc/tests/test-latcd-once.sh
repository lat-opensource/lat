#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
    echo "usage: $0 LATCD LATC RUNNER RUNTIME_DIR X86_GUEST WORKDIR" >&2
    exit 2
fi

latcd=$1
latc=$2
runner=$3
runtime_dir=$4
guest=$5
work=$6
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
export LATC_FAKE_REAL=$latc
rm -rf "$work"
mkdir -m 700 -p "$work"
guest_tbset=$work/guest.tbset
python3 "$script_dir/make-tbset.py" "$guest" "$guest_tbset"

submit_once()
{
    submit_socket=$1
    submit_source=$2
    submit_tbset=$3
    submit_output=$4
    "$latcd" --submit --socket "$submit_socket" --tbset "$submit_tbset" \
      "$submit_source" >"$submit_output"
}

start_once()
{
    socket=$1
    cache=$2
    compiler=$3
    shift 3
    "$latcd" --once --socket "$socket" --cache-dir "$cache" \
      --latc "$compiler" --runner "$runner" --runtime-dir "$runtime_dir" \
      "$@" \
      >"$socket.stdout" 2>"$socket.stderr" &
    server_pid=$!
    n=0
    while [ ! -S "$socket" ] && kill -0 "$server_pid" 2>/dev/null; do
        n=$((n + 1))
        [ "$n" -lt 500 ] || break
        sleep 0.01
    done
    [ -S "$socket" ]
}

failure_cache=$work/failure-cache
start_once "$work/failure.sock" "$failure_cache" "$script_dir/fake-latc-fail.sh"
if submit_once "$work/failure.sock" "$guest" "$guest_tbset" \
     "$work/failure.client" 2>&1; then
    echo "latcd accepted compiler failure" >&2
    exit 1
fi
if wait "$server_pid"; then
    echo "latcd server accepted compiler failure" >&2
    exit 1
fi
grep -q '^status=3$' "$work/failure.client"
test -z "$(find "$failure_cache" -maxdepth 1 -type f -name '*.so' -print -quit)"
test -z "$(find "$failure_cache/.tmp" -mindepth 1 -maxdepth 1 -print -quit)"

bad_cache=$work/bad-cache
start_once "$work/bad.sock" "$bad_cache" "$script_dir/fake-latc-fail.sh"
if submit_once "$work/bad.sock" /etc/hosts "$guest_tbset" \
     "$work/bad.client" 2>&1; then
    echo "latcd accepted non-ELF source" >&2
    exit 1
fi
if wait "$server_pid"; then
    echo "latcd server accepted non-ELF source" >&2
    exit 1
fi
grep -q '^status=2$' "$work/bad.client"
test -z "$(find "$bad_cache/.tmp" -mindepth 1 -maxdepth 1 -print -quit)"

writable_cache=$work/writable-cache
start_once "$work/writable.sock" "$writable_cache" "$script_dir/fake-latc-fail.sh"
python3 - "$work/writable.sock" "$guest" "$guest_tbset" <<'PY'
import array
import os
import socket
import struct
import sys

connection = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
connection.connect(sys.argv[1])
source = os.open(sys.argv[2], os.O_RDWR)
tbset = os.open(sys.argv[3], os.O_RDONLY)
request = struct.pack("=IHHIIQQ", 0x4c415444, 2, 32, 1, 100, 42, 42)
connection.sendmsg([request], [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                               array.array("i", [source, tbset]))])
response = connection.recv(264)
assert len(response) == 264
assert struct.unpack_from("=i", response, 8)[0] == 2
os.close(source)
os.close(tbset)
connection.close()
PY
if wait "$server_pid"; then
    echo "latcd server accepted writable source FD" >&2
    exit 1
fi
test -z "$(find "$writable_cache/.tmp" -mindepth 1 -maxdepth 1 -print -quit)"

invalid_cache=$work/invalid-cache
start_once "$work/invalid.sock" "$invalid_cache" \
  "$script_dir/fake-latc-invalid-module.sh"
if submit_once "$work/invalid.sock" "$guest" "$guest_tbset" \
     "$work/invalid.client" 2>&1; then
    echo "latcd accepted invalid compiler output" >&2
    exit 1
fi
if wait "$server_pid"; then
    echo "latcd server accepted invalid compiler output" >&2
    exit 1
fi
grep -q '^status=4$' "$work/invalid.client"
test -z "$(find "$invalid_cache" -maxdepth 1 -type f -name '*.so' -print -quit)"
test -z "$(find "$invalid_cache/.tmp" -mindepth 1 -maxdepth 1 -print -quit)"

cache=$work/cache
mkdir -m 700 "$cache"
source_sha=$(sha256sum "$guest" | cut -d ' ' -f 1)
tbset_sha=$(sha256sum "$guest_tbset" | cut -d ' ' -f 1)
module_name=$source_sha-$tbset_sha.so
cp /bin/true "$cache/$module_name"
bad_module_sha=$(sha256sum "$cache/$module_name" | cut -d ' ' -f 1)
start_once "$work/publish.sock" "$cache" "$latc"
timeout 60 "$latcd" --submit --socket "$work/publish.sock" \
  --tbset "$guest_tbset" "$guest" >"$work/publish.client"
wait "$server_pid"

module=$cache/$module_name
test -f "$module"
test "$(stat -c %a "$module")" = 444
test "$(sha256sum "$module" | cut -d ' ' -f 1)" != "$bad_module_sha"
"$latc" inspect-module --json "$module" >"$work/module.json"
python3 - "$work/module.json" "$source_sha" <<'PY'
import json
import sys

module = json.load(open(sys.argv[1]))
assert module["source_sha256"] == sys.argv[2]
assert module["tbs"] > 0
PY
test -z "$(find "$cache/.tmp" -mindepth 1 -maxdepth 1 -print -quit)"

LD_LIBRARY_PATH="$runtime_dir" \
LATX_AOT_V2_CACHE_DIR="$cache" \
LATX_AOT_V2_STRICT=1 \
LATX_AOT_V2_REPORT=1 \
LATC_DISABLE_PRETRANSLATE=1 \
LATC_STRICT_AOT=1 \
LATC_STATS_OUT="$work/guest.stats.json" \
  timeout 60 "$runner" "$guest" >"$work/guest.stdout" \
  2>"$work/guest.stderr"
grep -q 'module=registered' "$work/guest.stderr"
python3 - "$work/guest.stats.json" <<'PY'
import json
import sys

stats = json.load(open(sys.argv[1]))
assert stats["runtime_file_tb_gen_attempts"] == 0, stats
PY

start_once "$work/hit.sock" "$cache" "$script_dir/fake-latc-fail.sh"
submit_once "$work/hit.sock" "$guest" "$guest_tbset" "$work/hit.client"
wait "$server_pid"
grep -q 'cache hit:' "$work/hit.client"
test -z "$(find "$cache/.tmp" -mindepth 1 -maxdepth 1 -print -quit)"

chmod 0644 "$module"
start_once "$work/writable-module.sock" "$cache" "$script_dir/fake-latc-fail.sh"
if submit_once "$work/writable-module.sock" "$guest" "$guest_tbset" \
     "$work/writable-module.client" 2>&1; then
    echo "latcd accepted externally writable cached module" >&2
    exit 1
fi
if wait "$server_pid"; then
    echo "latcd treated externally writable cached module as a hit" >&2
    exit 1
fi
grep -q '^status=3$' "$work/writable-module.client"
chmod 0444 "$module"

eviction_cache=$work/eviction-cache
mkdir -m 700 "$eviction_cache"
cp "$module" "$eviction_cache/$module_name"
chmod 0444 "$eviction_cache/$module_name"
touch -t 200001010000 "$eviction_cache/$module_name"
cp "$guest" "$work/eviction-input.elf"
printf x >>"$work/eviction-input.elf"
eviction_sha=$(sha256sum "$work/eviction-input.elf" | cut -d ' ' -f 1)
eviction_tbset=$work/eviction.tbset
python3 "$script_dir/make-tbset.py" "$work/eviction-input.elf" "$eviction_tbset"
eviction_tbset_sha=$(sha256sum "$eviction_tbset" | cut -d ' ' -f 1)
eviction_module=$eviction_sha-$eviction_tbset_sha.so
cache_limit=$(( $(stat -c %s "$module") + 4096 ))
start_once "$work/eviction.sock" "$eviction_cache" "$latc" \
  --max-cache-bytes "$cache_limit"
timeout 60 "$latcd" --submit --socket "$work/eviction.sock" \
  --tbset "$eviction_tbset" "$work/eviction-input.elf" >"$work/eviction.client"
wait "$server_pid"
test -f "$eviction_cache/$eviction_module"
test ! -e "$eviction_cache/$module_name"
test ! -e "$eviction_cache/$source_sha.current"

versioned_cache=$work/versioned-eviction-cache
mkdir -m 700 "$versioned_cache"
old_tbset=$(printf old-tbset | sha256sum | cut -d ' ' -f 1)
old_name=$source_sha-$old_tbset.so
cp "$module" "$versioned_cache/$old_name"
chmod 0444 "$versioned_cache/$old_name"
printf '{"module":"%s","source_sha256":"%s"}\n' \
  "$old_name" "$source_sha" >"$versioned_cache/$source_sha.current"
chmod 0444 "$versioned_cache/$source_sha.current"
touch -t 200001010000 "$versioned_cache/$old_name"
start_once "$work/versioned-eviction.sock" "$versioned_cache" "$latc" \
  --max-cache-bytes "$cache_limit"
timeout 60 "$latcd" --submit --socket "$work/versioned-eviction.sock" \
  --tbset "$eviction_tbset" "$work/eviction-input.elf" \
  >"$work/versioned-eviction.client"
wait "$server_pid"
test -f "$versioned_cache/$eviction_module"
test ! -e "$versioned_cache/$old_name"
test ! -e "$versioned_cache/$source_sha.current"

echo "test-latcd-once: PASS source=$source_sha"
