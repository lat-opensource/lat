#!/bin/sh
set -eu
if [ "$#" -ne 6 ]; then
    echo "usage: $0 LATCD LATC RUNNER RUNTIME_DIR X86_GUEST WORKDIR" >&2
    exit 2
fi
latcd=$1
latc=$(realpath "$2")
runner=$3
runtime=$4
guest=$(realpath "$5")
work=$6
scripts=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
rm -rf "$work"
mkdir -m 700 -p "$work"
work=$(realpath "$work")
pid=
cleanup()
{
    touch "$work/release"
    if [ -n "$pid" ]; then
        kill -TERM "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT HUP INT TERM
printf '#!/bin/sh\nreal="%s"\nwork="%s"\n' "$latc" "$work" >"$work/compiler"
cat >>"$work/compiler" <<'EOF'
if [ "${1:-}" = build-id ]; then exec "$real" "$@"; fi
touch "$work/started"
while [ ! -f "$work/release" ]; do sleep 0.01; done
exit 92
EOF
chmod 755 "$work/compiler"
"$latcd" --serve --socket "$work/latcd.sock" --cache-dir "$work/cache" \
  --latc "$work/compiler" --runner "$runner" --runtime-dir "$runtime" \
  --stats "$work/stats.json" --workers 1 \
  >"$work/server.out" 2>"$work/server.err" &
pid=$!
python3 - "$work" "$guest" "$scripts" <<'PY'
import array
import os
from pathlib import Path
import socket
import struct
import sys
import time
work, guest = map(Path, sys.argv[1:3])
sys.path.insert(0, sys.argv[3])
from tb_key_set import write_key_set
data = guest.read_bytes()
entry, phoff = struct.unpack_from('<QQ', data, 24)
size, count = struct.unpack_from('<HH', data, 54)
bases = [struct.unpack_from('<Q', data, phoff + i * size + 16)[0]
         for i in range(count)
         if struct.unpack_from('<I', data, phoff + i * size)[0] == 1]
entry -= min(bases)

def wait_path(path):
    for _ in range(500):
        if path.exists(): return
        time.sleep(0.01)
    raise AssertionError(f'timed out waiting for {path}')

def request(operation, tbset=None):
    fds = [os.open(guest, os.O_RDONLY)] if operation != 3 else []
    if tbset is not None: fds.append(os.open(tbset, os.O_RDONLY))
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as sock:
            sock.settimeout(5)
            sock.connect(str(work / 'latcd.sock'))
            packet = struct.pack('=IHHIIQQ', 0x4c415444, 2, 32,
                                 operation, 100, 123, 0)
            rights = [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                       array.array('i', fds))] if fds else []
            sock.sendmsg([packet], rights)
            response = sock.recv(264)
            assert len(response) == 264
            return struct.unpack_from('=i', response, 8)[0]
    finally:
        for fd in fds: os.close(fd)

wait_path(work / 'latcd.sock')
incoming = work / 'input.tbset'
write_key_set(guest, incoming, [(entry, 3)])
assert request(1, incoming) == 0
wait_path(work / 'started')
write_key_set(guest, incoming, [(entry + 5, 3)])
assert request(1, incoming) == 0
(work / 'release').touch()
assert request(2) == 3
assert request(3) == 3
print('runtime dirty compile failure completes source/all flush: PASS')
PY
