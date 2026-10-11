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
case "$latc" in /*) ;; *) latc=$(pwd)/$latc ;; esac
case "$guest" in /*) ;; *) guest=$(pwd)/$guest ;; esac
case "$work" in /*) ;; *) work=$(pwd)/$work ;; esac
rm -rf "$work"
mkdir -m 700 -p "$work"
phase=$work
socket=$work/latcd.sock
cache=$work/cache
stats=$work/stats.json
server_pid=
cleanup()
{
    touch "$work/release-static"
    if [ -n "$server_pid" ]; then
        kill -TERM "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT HUP INT TERM
static_compiler=$work/static-aware-latc
printf '#!/bin/sh\nreal=%s\nwork=%s\n' "\"$latc\"" "\"$work\"" >"$static_compiler"
cat >>"$static_compiler" <<'EOF'
for argument in "$@"; do
    if [ "$argument" = --static-tbset ]; then
        test -z "${LATC_NATIVE_BASE:-}" || exit 91
        if [ ! -f "$work/allow-static" ]; then
            touch "$work/static-started"
            while [ ! -f "$work/release-static" ]; do sleep 0.01; done
            echo 'static capability rejection' >&2
            exit 92
        fi
    fi
done
exec "$real" "$@"
EOF
chmod 755 "$static_compiler"
"$latcd" --serve --socket "$socket" --cache-dir "$cache" \
  --latc "$static_compiler" --runner "$runner" --runtime-dir "$runtime_dir" \
  --stats "$stats" --workers 2 --negative-ms 500 \
  >"$work/server.stdout" 2>"$work/server.stderr" &
server_pid=$!
n=0
while [ ! -S "$socket" ]; do
    n=$((n + 1))
    test "$n" -lt 500
    kill -0 "$server_pid"
    sleep 0.01
done
python3 - "$guest" "$work/runtime.tbset" "$script_dir" <<'PYKEY'
from pathlib import Path
import struct
import sys
import threading
import time
sys.path.insert(0, sys.argv[3])
from tb_key_set import write_key_set
source = Path(sys.argv[1])
data = source.read_bytes()
entry, phoff = struct.unpack_from("<QQ", data, 24)
size, count = struct.unpack_from("<HH", data, 54)
bases = [struct.unpack_from("<Q", data, phoff + i * size + 16)[0]
         for i in range(count)
         if struct.unpack_from("<I", data, phoff + i * size)[0] == 1]
write_key_set(source, Path(sys.argv[2]), [(entry - min(bases), 3)])
PYKEY
"$latcd" --submit --socket "$socket" --tbset "$work/runtime.tbset" "$guest" >"$work/runtime.client"
"$latcd" --flush-source --socket "$socket" "$guest" \
  >"$phase/runtime-flush.client"
python3 - "$socket" "$cache" "$guest" "$script_dir" "$phase" <<'PY'
import array
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import sys
import threading
import time

socket_path, cache_path, source_path, script_dir, phase_path = sys.argv[1:]
sys.path.insert(0, script_dir)
from tb_key_set import read_key_set, write_key_set
cache, source, phase = map(Path, (cache_path, source_path, phase_path))
digest = hashlib.sha256(source.read_bytes()).hexdigest()
manifest_path = cache / f"{digest}.current"
manifest_bytes = manifest_path.read_bytes()
manifest = json.loads(manifest_bytes)
canonical_path = cache / ".tbsets" / f"{digest}.tbset"
canonical_bytes = canonical_path.read_bytes()
generation = {name: (cache / manifest[name]).read_bytes()
              for name in ("module", "native", "tbset")}
_, _, records = read_key_set(canonical_path)
incoming = phase / "static.tbset"
write_key_set(source, incoming, records)

def request(operation, tbset=None):
    with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as connection:
        connection.settimeout(15)
        connection.connect(socket_path)
        fds = [os.open(source, os.O_RDONLY)]
        if tbset is not None:
            fds.append(os.open(tbset, os.O_RDONLY))
        try:
            packet = struct.pack("=IHHIIQQ", 0x4c415444, 2, 32,
                                 operation, 0, 99, 0)
            connection.sendmsg([packet], [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                                           array.array("i", fds))])
            response = connection.recv(264)
            assert len(response) == 264, response
            return struct.unpack_from("=i", response, 8)[0], response[72:]
        finally:
            for fd in fds:
                os.close(fd)

# Covered keys still require a checked compile; failure retains every old file.
status, message = request(6, incoming)
assert status == 0 and b"cache hit" not in message, (status, message)
status, message = request(1, incoming)
assert status == 0 and b"cache hit" in message, (status, message)
for _ in range(500):
    if (phase / "static-started").exists():
        break
    time.sleep(0.01)
else:
    raise AssertionError("checked compiler never started; flush returned early")
# A duplicate accepted during compilation must share its failure outcome.
status, message = request(6, incoming)
assert status == 0 and b"deduplicated" in message, (status, message)
flushed = []
thread = threading.Thread(target=lambda: flushed.append(request(2)))
thread.start()
time.sleep(0.1)
assert not flushed, flushed
(phase / "release-static").touch()
thread.join(15)
assert not thread.is_alive(), "flush did not complete after static failure"
status, message = flushed[0]
assert status != 0, (status, message)
status, message = request(2)
assert status != 0, (status, message)
assert manifest_path.read_bytes() == manifest_bytes
assert canonical_path.read_bytes() == canonical_bytes
for name, contents in generation.items():
    assert (cache / manifest[name]).read_bytes() == contents
# Static failure must not negative-cache an ordinary runtime submission.
status, message = request(1, incoming)
assert status == 0 and b"cache hit" in message, (status, message)
status, message = request(6, incoming)
assert status == 7, (status, message)
print("static source isolation and old-generation preservation: PASS")
# A successful retry clears the failed barrier and preserves discovered keys
# across runtime and subsequent static generations.
(phase / "allow-static").touch()
for _ in range(200):
    status, message = request(6, incoming)
    if status == 0:
        break
    assert status == 7, (status, message)
    time.sleep(0.01)
else:
    raise AssertionError("static retry remained negative cached")
assert request(2)[0] == 0
entry = records[0][0]
write_key_set(source, incoming, [(entry + 5, 3)])
assert request(6, incoming)[0] == 0
assert request(2)[0] == 0
write_key_set(source, incoming, [(entry + 10, 3)])
assert request(1, incoming)[0] == 0
assert request(2)[0] == 0
manifest = json.loads(manifest_path.read_bytes())
assert (entry + 5, 3) in read_key_set(cache / manifest["tbset"])[2]
write_key_set(source, incoming, records)
assert request(6, incoming)[0] == 0
assert request(2)[0] == 0
manifest = json.loads(manifest_path.read_bytes())
assert {(entry, 3), (entry + 5, 3), (entry + 10, 3)} <= set(
    read_key_set(cache / manifest["tbset"])[2])
print("static/runtime publication retains successful key union: PASS")
PY

echo "test-latcd-static-origin: PASS"
