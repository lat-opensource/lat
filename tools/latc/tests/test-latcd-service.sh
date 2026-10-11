#!/bin/sh
set -eu

if [ "$#" -ne 7 ]; then
    echo "usage: $0 LATCD LATC RUNNER RUNTIME_DIR X86_GUEST WORKDIR COVERED_GUEST" >&2
    exit 2
fi

latcd=$1
latc=$2
runner=$3
runtime_dir=$4
guest=$5
work=$6
covered_guest=$7
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
export LATC_FAKE_REAL=$latc
rm -rf "$work"
mkdir -m 700 -p "$work"

start_service()
{
    name=$1
    compiler=$2
    shift 2
    phase=$work/$name
    mkdir -m 700 "$phase"
    socket=$phase/latcd.sock
    cache=$phase/cache
    stats=$phase/stats.json
    "$latcd" --serve --socket "$socket" --cache-dir "$cache" \
      --latc "$compiler" --runner "$runner" --runtime-dir "$runtime_dir" \
      --stats "$stats" --workers 1 "$@" >"$phase/server.stdout" \
      2>"$phase/server.stderr" &
    server_pid=$!
    n=0
    while [ ! -S "$socket" ] && kill -0 "$server_pid" 2>/dev/null; do
        n=$((n + 1))
        [ "$n" -lt 500 ] || break
        sleep 0.01
    done
    [ -S "$socket" ]
}

wait_stats()
{
    expression=$1
    n=0
    while :; do
        if [ -f "$stats" ] && \
           python3 - "$stats" "$expression" 2>/dev/null <<'PY'
import json
import sys
stats = json.load(open(sys.argv[1]))
assert eval(sys.argv[2], {"__builtins__": {}}, {"s": stats})
PY
        then
            return
        fi
        n=$((n + 1))
        [ "$n" -lt 1200 ] || {
            echo "timed out waiting for stats: $expression" >&2
            cat "$stats" 2>/dev/null || true
            return 1
        }
        sleep 0.05
    done
}

stop_service()
{
    kill -TERM "$server_pid"
    wait "$server_pid"
}

submit_source()
{
    submit_socket=$1
    submit_source_path=$2
    submit_output=$3
    shift 3
    submit_tbset=$(mktemp "$work/submit.XXXXXX.tbset")
    python3 "$script_dir/make-tbset.py" "$submit_source_path" "$submit_tbset"
    if "$latcd" --submit --socket "$submit_socket" --tbset "$submit_tbset" \
         "$@" "$submit_source_path" >"$submit_output"; then
        rm -f "$submit_tbset"
        return 0
    else
        submit_status=$?
        rm -f "$submit_tbset"
        return "$submit_status"
    fi
}

owner_cache=$work/cache-owner
owner_phase=$work/cache-owner-primary
mkdir -m 700 "$owner_phase"
owner_socket=$owner_phase/latcd.sock
owner_stats=$owner_phase/stats.json
"$latcd" --serve --socket "$owner_socket" --cache-dir "$owner_cache" \
  --latc "$latc" --runner "$runner" --runtime-dir "$runtime_dir" \
  --stats "$owner_stats" >"$owner_phase/server.stdout" \
  2>"$owner_phase/server.stderr" &
owner_pid=$!
n=0
while [ ! -S "$owner_socket" ] && kill -0 "$owner_pid" 2>/dev/null; do
    n=$((n + 1))
    [ "$n" -lt 500 ] || break
    sleep 0.01
done
[ -S "$owner_socket" ]
test "$(stat -c %a "$owner_cache/.latcd.lock")" = 600
grep -q "^pid=$owner_pid$" "$owner_cache/.latcd.lock"

second_phase=$work/cache-owner-second
mkdir -m 700 "$second_phase"
if "$latcd" --serve --socket "$second_phase/latcd.sock" \
     --cache-dir "$owner_cache" --latc "$latc" --runner "$runner" \
     --runtime-dir "$runtime_dir" >"$second_phase/server.stdout" \
     2>"$second_phase/server.stderr"; then
    echo "second latcd acquired an owned cache" >&2
    exit 1
fi
grep -q 'cache is already owned by another latcd' \
  "$second_phase/server.stderr"
test ! -e "$second_phase/latcd.sock"
test -z "$(find "$owner_cache" -maxdepth 1 -type f \
  \( -name '*.so' -o -name '*.current' \) -print -quit)"

kill -TERM "$owner_pid"
wait "$owner_pid"
normal_phase=$work/cache-owner-normal-recovery
mkdir -m 700 "$normal_phase"
normal_socket=$normal_phase/latcd.sock
"$latcd" --serve --socket "$normal_socket" --cache-dir "$owner_cache" \
  --latc "$latc" --runner "$runner" --runtime-dir "$runtime_dir" \
  >"$normal_phase/server.stdout" 2>"$normal_phase/server.stderr" &
normal_pid=$!
n=0
while [ ! -S "$normal_socket" ] && kill -0 "$normal_pid" 2>/dev/null; do
    n=$((n + 1))
    [ "$n" -lt 500 ] || break
    sleep 0.01
done
[ -S "$normal_socket" ]
kill -KILL "$normal_pid"
wait "$normal_pid" 2>/dev/null || true

kill_phase=$work/cache-owner-kill-recovery
mkdir -m 700 "$kill_phase"
kill_socket=$kill_phase/latcd.sock
kill_stats=$kill_phase/stats.json
"$latcd" --serve --socket "$kill_socket" --cache-dir "$owner_cache" \
  --latc "$latc" --runner "$runner" --runtime-dir "$runtime_dir" \
  --stats "$kill_stats" >"$kill_phase/server.stdout" \
  2>"$kill_phase/server.stderr" &
kill_pid=$!
n=0
while [ ! -S "$kill_socket" ] && kill -0 "$kill_pid" 2>/dev/null; do
    n=$((n + 1))
    [ "$n" -lt 500 ] || break
    sleep 0.01
done
[ -S "$kill_socket" ]
submit_source "$kill_socket" "$guest" "$kill_phase/client"
stats=$kill_stats
wait_stats 's["compiled"] == 1 and s["active_jobs"] == 0 and s["cache_owner_pid"] > 0'
find "$owner_cache" -maxdepth 1 -type f \
  \( -name '*.so' -o -name '*.current' \) -print0 | sort -z | \
  xargs -0 sha256sum >"$kill_phase/cache.before"
test "$(wc -l <"$kill_phase/cache.before")" -ge 2
data_second=$work/cache-owner-second-with-data
mkdir -m 700 "$data_second"
if "$latcd" --serve --socket "$data_second/latcd.sock" \
     --cache-dir "$owner_cache" --latc "$latc" --runner "$runner" \
     --runtime-dir "$runtime_dir" >"$data_second/server.stdout" \
     2>"$data_second/server.stderr"; then
    echo "second latcd acquired an owned populated cache" >&2
    exit 1
fi
grep -q 'cache is already owned by another latcd' \
  "$data_second/server.stderr"
find "$owner_cache" -maxdepth 1 -type f \
  \( -name '*.so' -o -name '*.current' \) -print0 | sort -z | \
  xargs -0 sha256sum >"$kill_phase/cache.after"
cmp "$kill_phase/cache.before" "$kill_phase/cache.after"
kill -TERM "$kill_pid"
wait "$kill_pid"

start_service concurrent "$latc"
wait_stats 's["requests"] == 0 and s["compiled"] == 0 and s["failed"] == 0 and s["active_jobs"] == 0 and s["queue_depth"] == 0'
cp "$guest" "$work/same-bytes.elf"
client_pids=
n=1
while [ "$n" -le 20 ]; do
    if [ $((n % 2)) -eq 0 ]; then source=$work/same-bytes.elf; else source=$guest; fi
    submit_source "$socket" "$source" "$phase/client.$n" 2>&1 &
    client_pids="$client_pids $!"
    n=$((n + 1))
done
for client_pid in $client_pids; do wait "$client_pid"; done
wait_stats 's["compiled"] == 1 and s["active_jobs"] == 0'
python3 - "$stats" <<'PY'
import json
import sys
s = json.load(open(sys.argv[1]))
assert s["requests"] == 20, s
assert s["queued"] == 1, s
assert s["compiled"] == 1 and s["failed"] == 0, s
assert s["deduplicated"] + s["cache_hits"] == 19, s
PY
"$latcd" --flush-all --socket "$socket" >"$phase/flush-all.client"
grep -q '^status=0$' "$phase/flush-all.client"
python3 - "$phase/flush-all.client" <<'PY'
import sys
values = dict(line.rstrip().split("=", 1)
              for line in open(sys.argv[1]) if "=" in line)
assert int(values["accepted_sequence"]) > 0, values
assert values["accepted_sequence"] == values["published_sequence"], values
PY
stop_service

start_service flush-only "$latc" --flush-only
submit_source "$socket" "$guest" "$phase/client"
sleep 0.2
python3 - "$stats" <<'PY'
import json
import sys
s = json.load(open(sys.argv[1]))
assert s["compiled"] == 0, s
assert s["queue_depth"] == 1 and s["active_jobs"] == 1, s
PY
"$latcd" --flush-all --socket "$socket" >"$phase/flush-all.client"
grep -q '^status=0$' "$phase/flush-all.client"
wait_stats 's["compiled"] == 1 and s["active_jobs"] == 0'
stop_service

start_service workers "$script_dir/fake-latc-slow.sh" --workers 2
cp "$guest" "$work/workers-a.elf"
cp "$guest" "$work/workers-b.elf"
printf a >>"$work/workers-a.elf"
printf b >>"$work/workers-b.elf"
submit_source "$socket" "$work/workers-a.elf" "$phase/a.client" &
worker_client_a=$!
submit_source "$socket" "$work/workers-b.elf" "$phase/b.client" &
worker_client_b=$!
wait "$worker_client_a"
wait "$worker_client_b"
wait_stats 's["active_jobs"] == 2 and s["queue_depth"] == 0 and s["workers"] == 2'
"$latcd" --flush-all --socket "$socket" >"$phase/flush-all.client"
grep -q '^status=0$' "$phase/flush-all.client"
python3 - "$phase/flush-all.client" <<'PY'
import sys
values = dict(line.rstrip().split("=", 1) for line in open(sys.argv[1]) if "=" in line)
assert int(values["published_sequence"]) >= int(values["accepted_sequence"]), values
PY
wait_stats 's["compiled"] == 2 and s["active_jobs"] == 0'
stop_service

start_service same-source-workers "$script_dir/fake-latc-slow.sh" --workers 2
python3 - "$socket" "$guest" "$script_dir" <<'PY'
import array
import hashlib
import os
from pathlib import Path
import socket
import struct
import sys
import tempfile

socket_path, source_path, script_dir = sys.argv[1:]
sys.path.insert(0, script_dir)
from tb_key_set import write_key_set
source_sha = hashlib.sha256(open(source_path, "rb").read()).hexdigest()
with open(source_path, "rb") as source:
    source.seek(24)
    entry = struct.unpack("<Q", source.read(8))[0]
    source.seek(32)
    phoff = struct.unpack("<Q", source.read(8))[0]
    source.seek(54)
    phentsize, phnum = struct.unpack("<HH", source.read(4))
    load_vaddrs = []
    for index in range(phnum):
        source.seek(phoff + index * phentsize)
        phdr = source.read(phentsize)
        if struct.unpack_from("<I", phdr)[0] == 1:  # PT_LOAD
            load_vaddrs.append(struct.unpack_from("<Q", phdr, 16)[0])
    assert load_vaddrs and entry >= min(load_vaddrs)
    entry_rva = entry - min(load_vaddrs)

for request_id, offset in ((101, 0), (102, 5)):
    if request_id == 102:
        import time
        time.sleep(0.2)
    tbset = tempfile.NamedTemporaryFile(delete=False)
    try:
        tbset.close()
        write_key_set(Path(source_path), Path(tbset.name),
                      [(entry_rva + offset, 3)])
        source_fd = os.open(source_path, os.O_RDONLY)
        tbset_fd = os.open(tbset.name, os.O_RDONLY)
        connection = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        connection.connect(socket_path)
        request = struct.pack("=IHHIIQQ", 0x4c415444, 2, 32,
                              1, 100, request_id, request_id)
        connection.sendmsg(
            [request],
            [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
              array.array("i", [source_fd, tbset_fd]))])
        response = connection.recv(264)
        assert len(response) == 264
        assert struct.unpack_from("=i", response, 8)[0] == 0
        connection.close()
        os.close(source_fd)
        os.close(tbset_fd)
    finally:
        try:
            os.unlink(tbset.name)
        except FileNotFoundError:
            pass
PY
"$latcd" --flush-source --socket "$socket" "$guest" \
  >"$phase/flush-source.client"
grep -q '^status=0$' "$phase/flush-source.client"
python3 - "$phase/flush-source.client" <<'PY'
import sys
values = dict(line.rstrip().split("=", 1) for line in open(sys.argv[1]) if "=" in line)
assert int(values["accepted_sequence"]) > 0, values
assert values["accepted_sequence"] == values["published_sequence"], values
PY
wait_stats 's["compiled"] == 2 and s["active_jobs"] == 0 and s["running_sources"] == 0'
python3 - "$cache" "$guest" "$script_dir" <<'PY'
import hashlib
import json
from pathlib import Path
import struct
import sys

cache = Path(sys.argv[1])
source = Path(sys.argv[2])
sys.path.insert(0, sys.argv[3])
from tb_key_set import read_key_set
source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
with source.open("rb") as elf:
    elf.seek(24)
    entry = struct.unpack("<Q", elf.read(8))[0]
    elf.seek(32)
    phoff = struct.unpack("<Q", elf.read(8))[0]
    elf.seek(54)
    phentsize, phnum = struct.unpack("<HH", elf.read(4))
    load_vaddrs = []
    for index in range(phnum):
        elf.seek(phoff + index * phentsize)
        phdr = elf.read(phentsize)
        if struct.unpack_from("<I", phdr)[0] == 1:
            load_vaddrs.append(struct.unpack_from("<Q", phdr, 16)[0])
entry_rva = entry - min(load_vaddrs)
tbset = cache / ".tbsets" / f"{source_sha}.tbset"
digest, sequence, records = read_key_set(tbset)
assert digest.hex() == source_sha, digest.hex()
assert records == [(entry_rva, 3), (entry_rva + 5, 3)], records
assert sequence == 2, sequence
assert not (cache / f"{source_sha}.so").exists()
current = json.loads((cache / f"{source_sha}.current").read_text())
assert current["version"] == 2, current
assert (cache / current["module"]).is_file(), current
assert (cache / current["native"]).is_file(), current
assert (cache / current["tbset"]).is_file(), current
assert len(list(cache.glob(f"{source_sha}-*.so"))) == 1
assert len(list(cache.glob(f"{source_sha}-*.native"))) == 1
assert len(list(cache.glob(f"{source_sha}-*.tbset"))) == 1
stats = json.loads((cache.parent / "stats.json").read_text())
assert stats["requests"] == 2, stats
assert stats["queued"] == 2 and stats["deduplicated"] == 1, stats
assert stats["compiled"] == 2 and stats["failed"] == 0, stats
PY
set +e
LD_LIBRARY_PATH="$runtime_dir" LATX_AOT_V2_CACHE_DIR="$cache" \
LATX_AOT_V2_LATCD_SOCKET="$socket" LATX_AOT_V2_STRICT=1 \
LATX_AOT_V2_REPORT=1 LATC_DISABLE_PRETRANSLATE=1 LATC_STRICT_AOT=1 \
LATC_STATS_OUT="$phase/single-module.stats.json" \
  "$runner" "$guest" >"$phase/single-module.stdout" \
  2>"$phase/single-module.stderr"
strict_status=$?
set -e
test "$strict_status" -eq 42
python3 - "$phase/single-module.stats.json" <<'PY'
import json
import sys
stats = json.load(open(sys.argv[1]))
assert stats["runtime_file_tb_gen_calls"] == 0, stats
assert stats["runtime_file_tb_gen_attempts"] == 0, stats
PY
python3 - "$socket" "$guest" "$script_dir" <<'PY'
import array
import os
from pathlib import Path
import socket
import struct
import sys
import tempfile

socket_path, source_path, script_dir = sys.argv[1:]
sys.path.insert(0, script_dir)
from tb_key_set import write_key_set
with open(source_path, "rb") as source:
    source.seek(24)
    entry = struct.unpack("<Q", source.read(8))[0]
    source.seek(32)
    phoff = struct.unpack("<Q", source.read(8))[0]
    source.seek(54)
    phentsize, phnum = struct.unpack("<HH", source.read(4))
    bases = []
    for index in range(phnum):
        source.seek(phoff + index * phentsize)
        phdr = source.read(phentsize)
        if struct.unpack_from("<I", phdr)[0] == 1:
            bases.append(struct.unpack_from("<Q", phdr, 16)[0])
entry_rva = entry - min(bases)
with tempfile.NamedTemporaryFile() as tbset:
    write_key_set(Path(source_path), Path(tbset.name), [(entry_rva + 10, 3)])
    source_fd = os.open(source_path, os.O_RDONLY)
    tbset_fd = os.open(tbset.name, os.O_RDONLY)
    connection = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    connection.connect(socket_path)
    request = struct.pack("=IHHIIQQ", 0x4c415444, 2, 32, 1, 100, 103, 103)
    connection.sendmsg([request], [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                                    array.array("i", [source_fd, tbset_fd]))])
    response = connection.recv(264)
    assert len(response) == 264
    assert struct.unpack_from("=i", response, 8)[0] == 0
    connection.close()
    os.close(source_fd)
    os.close(tbset_fd)
PY
"$latcd" --flush-source --socket "$socket" "$guest" \
  >"$phase/compact-flush.client"
grep -q '^status=0$' "$phase/compact-flush.client"
wait_stats 's["compiled"] == 3 and s["active_jobs"] == 0'
python3 - "$cache" "$guest" "$script_dir" <<'PY'
import hashlib
import json
from pathlib import Path
import struct
import sys

cache, source = Path(sys.argv[1]), Path(sys.argv[2])
sys.path.insert(0, sys.argv[3])
from tb_key_set import read_key_set
source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
with source.open("rb") as elf:
    elf.seek(24)
    entry = struct.unpack("<Q", elf.read(8))[0]
    elf.seek(32)
    phoff = struct.unpack("<Q", elf.read(8))[0]
    elf.seek(54)
    phentsize, phnum = struct.unpack("<HH", elf.read(4))
    bases = []
    for index in range(phnum):
        elf.seek(phoff + index * phentsize)
        phdr = elf.read(phentsize)
        if struct.unpack_from("<I", phdr)[0] == 1:
            bases.append(struct.unpack_from("<Q", phdr, 16)[0])
entry_rva = entry - min(bases)
current = json.loads((cache / f"{source_sha}.current").read_text())
_, sequence, records = read_key_set(cache / current["tbset"])
assert sequence == 3, sequence
assert records == [(entry_rva, 3), (entry_rva + 5, 3),
                   (entry_rva + 10, 3)], records
assert current["version"] == 2, current
assert (cache / current["native"]).is_file(), current
assert len(list(cache.glob(f"{source_sha}-*.so"))) == 1
assert len(list(cache.glob(f"{source_sha}-*.native"))) == 1
assert len(list(cache.glob(f"{source_sha}-*.tbset"))) == 1
PY
set +e
LD_LIBRARY_PATH="$runtime_dir" LATX_AOT_V2_CACHE_DIR="$cache" \
LATX_AOT_V2_LATCD_SOCKET="$socket" LATX_AOT_V2_STRICT=1 \
LATX_AOT_V2_REPORT=1 LATC_DISABLE_PRETRANSLATE=1 LATC_STRICT_AOT=1 \
LATC_STATS_OUT="$phase/incremented.stats.json" \
  "$runner" "$guest" >"$phase/incremented.stdout" \
  2>"$phase/incremented.stderr"
strict_status=$?
set -e
test "$strict_status" -eq 42
python3 - "$phase/incremented.stats.json" <<'PY'
import json
import sys
stats = json.load(open(sys.argv[1]))
assert stats["runtime_file_tb_gen_calls"] == 0, stats
assert stats["runtime_file_tb_gen_attempts"] == 0, stats
PY
stop_service

native_aware_counter=$work/native-aware-compiler.count
native_aware_compiler=$work/native-aware-latc
printf '%s\n' '#!/bin/sh' 'set -eu' \
  "if [ \"\${1:-}\" = build-id ]; then exec $latc \"\$@\"; fi" \
  "printf 'compile\\n' >>$native_aware_counter" \
  "exec $latc \"\$@\"" >"$native_aware_compiler"
chmod 755 "$native_aware_compiler"
native_aware_guest=$guest
guest=$covered_guest
test -f "$guest"
start_service native-aware "$native_aware_compiler" --flush-only
submit_source "$socket" "$guest" "$phase/initial.client"
"$latcd" --flush-source --socket "$socket" "$guest" \
  >"$phase/initial-flush.client"
test "$(wc -l <"$native_aware_counter")" -eq 1
python3 - "$cache" "$guest" "$phase/initial-generation.json" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

cache, source, output = map(Path, sys.argv[1:])
source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
manifest = json.loads((cache / f"{source_sha}.current").read_text())
output.write_text(json.dumps({name: manifest[name]
                              for name in ("module", "native")},
                             sort_keys=True))
PY
python3 - "$cache" "$guest" "$phase/covered.tbset" "$script_dir" <<'PY'
import hashlib
import json
from pathlib import Path
import struct
import sys

cache, source, output = map(Path, sys.argv[1:4])
sys.path.insert(0, sys.argv[4])
from tb_key_set import read_key_set, write_key_set
source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
manifest = json.loads((cache / f"{source_sha}.current").read_text())
_, _, records = read_key_set(cache / manifest["tbset"])
assert len(records) == 1
module = (cache / manifest["module"]).read_bytes()
section_offset = struct.unpack_from("<Q", module, 40)[0]
section_size, section_count, names_index = struct.unpack_from("<HHH", module, 58)
sections = [struct.unpack_from("<IIQQQQIIQQ", module,
                               section_offset + index * section_size)
            for index in range(section_count)]
names = sections[names_index]
name_data = module[names[4]:names[4] + names[5]]
tb_section = next(section for section in sections
                  if name_data[section[0]:].split(b"\0", 1)[0] ==
                     b".rodata.lat.tb")
tb_data = module[tb_section[4]:tb_section[4] + tb_section[5]]
module_keys = [(rva, flags) for rva, _, _, flags in
               struct.iter_unpack("<QQII", tb_data)]
covered = next(key for key in module_keys if key not in records)
write_key_set(source, output, [covered])
PY
"$latcd" --submit --socket "$socket" --tbset "$phase/covered.tbset" \
  "$guest" >"$phase/covered.client"
"$latcd" --flush-source --socket "$socket" "$guest" \
  >"$phase/covered-flush.client"
grep -q '^status=0$' "$phase/covered-flush.client"
test "$(wc -l <"$native_aware_counter")" -eq 1
python3 - "$cache" "$guest" "$phase/initial-generation.json" \
  "$script_dir" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

cache, source, initial = map(Path, sys.argv[1:4])
sys.path.insert(0, sys.argv[4])
from tb_key_set import read_key_set
source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
manifest = json.loads((cache / f"{source_sha}.current").read_text())
assert {name: manifest[name] for name in ("module", "native")} == \
       json.loads(initial.read_text())
_, _, records = read_key_set(cache / manifest["tbset"])
assert len(records) == 2, records
assert len(list(cache.glob(f"{source_sha}-*.so"))) == 1
assert len(list(cache.glob(f"{source_sha}-*.native"))) == 1
assert len(list(cache.glob(f"{source_sha}-*.tbset"))) == 1
PY
stop_service

guest=$native_aware_guest
atomic_marker=$work/atomic-before-manifest.fail
export LATCD_TEST_FAIL_BEFORE_MANIFEST=$atomic_marker
start_service atomic "$latc" --flush-only --negative-ms 10
submit_source "$socket" "$guest" "$phase/initial.client"
"$latcd" --flush-source --socket "$socket" "$guest" \
  >"$phase/initial-flush.client"
grep -q '^status=0$' "$phase/initial-flush.client"
python3 - "$cache" "$guest" "$phase/published.json" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

cache, source, output = map(Path, sys.argv[1:])
source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
manifest_path = cache / f"{source_sha}.current"
manifest = json.loads(manifest_path.read_text())
paths = [manifest_path] + [cache / manifest[name]
                           for name in ("module", "native", "tbset")]
state = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
         for path in paths}
output.write_text(json.dumps(state, sort_keys=True))
PY
python3 - "$cache" "$guest" "$phase/delta.tbset" "$script_dir" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

cache, source, output = map(Path, sys.argv[1:4])
sys.path.insert(0, sys.argv[4])
from tb_key_set import read_key_set, write_key_set
source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
manifest = json.loads((cache / f"{source_sha}.current").read_text())
_, _, records = read_key_set(cache / manifest["tbset"])
assert records
write_key_set(source, output, [(records[0][0] + 5, 3)])
PY
"$latcd" --submit --socket "$socket" --tbset "$phase/delta.tbset" \
  "$guest" >"$phase/delta.client"
touch "$atomic_marker"
set +e
"$latcd" --flush-source --socket "$socket" "$guest" \
  >"$phase/failed-flush.client"
atomic_status=$?
set -e
test "$atomic_status" -ne 0
python3 - "$cache" "$guest" "$phase/published.json" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

cache, source, snapshot = map(Path, sys.argv[1:])
source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
manifest_path = cache / f"{source_sha}.current"
manifest = json.loads(manifest_path.read_text())
paths = [manifest_path] + [cache / manifest[name]
                           for name in ("module", "native", "tbset")]
expected = json.loads(snapshot.read_text())
actual = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
          for path in paths}
assert actual == expected, (actual, expected)
assert len(list(cache.glob(f"{source_sha}-*.so"))) == 1
assert len(list(cache.glob(f"{source_sha}-*.native"))) == 1
assert len(list(cache.glob(f"{source_sha}-*.tbset"))) == 1
PY
sleep 0.05
"$latcd" --submit --socket "$socket" --tbset "$phase/delta.tbset" \
  "$guest" >"$phase/retry.client"
"$latcd" --flush-source --socket "$socket" "$guest" \
  >"$phase/retry-flush.client"
grep -q '^status=0$' "$phase/retry-flush.client"
python3 - "$cache" "$guest" "$script_dir" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

cache, source = map(Path, sys.argv[1:3])
sys.path.insert(0, sys.argv[3])
from tb_key_set import read_key_set
source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
manifest = json.loads((cache / f"{source_sha}.current").read_text())
_, _, records = read_key_set(cache / manifest["tbset"])
assert len(records) == 2 and {flags for _, flags in records} == {3}, records
assert len(list(cache.glob(f"{source_sha}-*.so"))) == 1
assert len(list(cache.glob(f"{source_sha}-*.native"))) == 1
assert len(list(cache.glob(f"{source_sha}-*.tbset"))) == 1
PY
stop_service
unset LATCD_TEST_FAIL_BEFORE_MANIFEST

start_service negative "$script_dir/fake-latc-fail.sh" --negative-ms 500
python3 - "$socket" <<'PY'
import socket
import struct
import sys
import time

connection = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
connection.connect(sys.argv[1])
time.sleep(1.2)
response = connection.recv(264)
assert len(response) == 264
assert struct.unpack_from("=i", response, 8)[0] == 1
connection.close()
PY
submit_source "$socket" "$guest" "$phase/first.client"
wait_stats 's["failed"] == 1 and s["active_jobs"] == 0'
if submit_source "$socket" "$guest" "$phase/second.client" 2>&1; then
    echo "negative cache accepted immediate retry" >&2
    exit 1
fi
grep -q '^status=7$' "$phase/second.client"
wait_stats 's["negative_hits"] == 1'
stop_service

start_service cpu-limit "$script_dir/fake-latc-cpu.sh" --cpu-seconds 1
submit_source "$socket" "$guest" "$phase/client"
wait_stats 's["failed"] == 1 and s["active_jobs"] == 0'
grep -q 'compiler terminated by signal' "$phase/server.stderr" || true
stop_service

cp "$guest" "$work/a.elf"
cp "$guest" "$work/b.elf"
cp "$guest" "$work/c.elf"
cp "$guest" "$work/d.elf"
printf a >>"$work/a.elf"
printf b >>"$work/b.elf"
printf c >>"$work/c.elf"
printf d >>"$work/d.elf"
start_service priority "$script_dir/fake-latc-slow.sh" --max-jobs 2
submit_source "$socket" "$work/a.elf" "$phase/a.client" --priority 0
wait_stats 's["active_jobs"] == 1 and s["queue_depth"] == 0'
submit_source "$socket" "$work/b.elf" "$phase/b.client" --priority 1
submit_source "$socket" "$work/c.elf" "$phase/c.client" --priority 200
wait_stats 's["queue_depth"] == 2'
if submit_source "$socket" "$work/d.elf" "$phase/d.client" --priority 1 \
     2>&1; then
    echo "full compiler queue accepted another request" >&2
    exit 1
fi
grep -q '^status=6$' "$phase/d.client"
wait_stats 's["compiled"] == 3 and s["active_jobs"] == 0'
b_sha=$(sha256sum "$work/b.elf" | cut -d ' ' -f 1)
c_sha=$(sha256sum "$work/c.elf" | cut -d ' ' -f 1)
python3 - "$cache" "$b_sha" "$c_sha" <<'PY'
import json
import os
from pathlib import Path
import sys
cache = Path(sys.argv[1])
b = json.loads((cache / f"{sys.argv[2]}.current").read_text())["module"]
c = json.loads((cache / f"{sys.argv[3]}.current").read_text())["module"]
assert os.stat(cache / c).st_mtime_ns < os.stat(cache / b).st_mtime_ns
PY
wait_stats 's["queue_full"] == 1 and s["compiled"] == 3'
stop_service

source_size=$(wc -c <"$work/b.elf")
start_service shutdown "$script_dir/fake-latc-cpu.sh" --cpu-seconds 60 \
  --max-jobs 10 --max-queue-bytes "$source_size"
submit_source "$socket" "$work/a.elf" "$phase/a.client"
wait_stats 's["active_jobs"] == 1 and s["queue_depth"] == 0'
submit_source "$socket" "$work/b.elf" "$phase/b.client"
wait_stats 's["queue_depth"] == 1 and s["queue_bytes"] > 0'
if submit_source "$socket" "$work/c.elf" "$phase/c.client" 2>&1; then
    echo "byte-limited compiler queue accepted another request" >&2
    exit 1
fi
grep -q '^status=6$' "$phase/c.client"
start=$(date +%s)
stop_service
elapsed=$(($(date +%s) - start))
[ "$elapsed" -lt 5 ]
grep -q '"queue_full":1' "$stats"
test -z "$(find "$cache/.tmp" -mindepth 1 -maxdepth 1 -print -quit)"

echo "test-latcd-service: PASS"
