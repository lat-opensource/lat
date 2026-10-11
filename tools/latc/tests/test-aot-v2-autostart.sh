#!/bin/sh
set -eu

if [ "$#" -ne 3 ]; then
    echo "usage: $0 INSTALL_PREFIX STATIC_GUEST WORKDIR" >&2
    exit 2
fi
prefix=$(realpath "$1")
guest=$(realpath "$2")
work=$3
mkdir -m 700 -p "$work"
work=$(realpath "$work")
runner=$prefix/bin/latx-x86_64
daemon=$prefix/bin/latcd
identity=$($prefix/bin/latc build-id)
pid=
peer=
cleanup()
{
    if [ -n "$pid" ]; then kill -TERM "$pid" 2>/dev/null || true; fi
    if [ -n "$peer" ]; then kill -TERM "$peer" 2>/dev/null || true; fi
}
trap cleanup EXIT HUP INT TERM

run_guest()
{
    env -i HOME="$work/home" XDG_CACHE_HOME="$work/cache" \
      XDG_STATE_HOME="$work/state" XDG_RUNTIME_DIR="$work/run" \
      PATH=/usr/bin:/bin TMPDIR="$work" LD_LIBRARY_PATH="$prefix/lib" \
      "$@" "$runner" "$guest"
}
mkdir -m 700 "$work/home" "$work/run"
run_guest LATX_AOT=0 > "$work/jit.stdout" 2> "$work/jit.stderr"
test "$(cat "$work/jit.stdout")" = 'Hello, LATC!'
test ! -d "$work/cache"
run_guest > "$work/default.stdout" 2> "$work/default.stderr"
cmp "$work/jit.stdout" "$work/default.stdout"
test ! -d "$work/cache"
test "$(find "$work/run" -type s | wc -l)" -eq 0

# An explicitly disabled v2 ignores stale path configuration.
run_guest LATX_AOT=0 LATX_AOT_V2=0 LATX_AOT_V2_CACHE_DIR="$work/stale-cache" \
  > "$work/off.stdout" 2> "$work/off.stderr"
cmp "$work/jit.stdout" "$work/off.stdout"
test ! -d "$work/stale-cache"
if run_guest LATX_AOT=1 LATX_AOT_V2=1 > "$work/conflict.stdout" 2> "$work/conflict.stderr"; then
    echo 'mutually exclusive modes were accepted' >&2
    exit 1
fi
test ! -s "$work/conflict.stdout"
grep -q 'cannot both be enabled' "$work/conflict.stderr"
if run_guest LATX_AOT=1 LATX_AOT_V2=1 LATX_SOFTFPU=1 \
  > "$work/conflict-softfpu.stdout" 2> "$work/conflict-softfpu.stderr"; then
    exit 1
fi
test ! -s "$work/conflict-softfpu.stdout"
grep -q 'cannot both be enabled' "$work/conflict-softfpu.stderr"
test ! -d "$work/cache"

# Concurrent short-lived guests must all submit successfully to one daemon.
children=
for n in 1 2 3 4 5 6 7 8; do
    run_guest LATX_AOT_V2=1 LATX_AOT_V2_REPORT=1 \
      > "$work/cold-$n.stdout" 2> "$work/cold-$n.stderr" &
    children="$children $!"
done
for child in $children; do wait "$child"; done
for n in 1 2 3 4 5 6 7 8; do
    cmp "$work/jit.stdout" "$work/cold-$n.stdout"
    grep -q 'compiler_submissions=1 compiler_submission_failures=0' "$work/cold-$n.stderr"
done
socket=$(find "$work/run" -type s -name '*.sock')
test -n "$socket"
pid=$(pgrep -f "^$daemon --serve --daemonize --socket $socket ")
test "$(printf '%s\n' "$pid" | wc -l)" -eq 1
"$daemon" --flush-all --socket "$socket" > "$work/flush.log"
kill -TERM "$pid"
for n in $(seq 1 200); do
    test -S "$socket" || break
    sleep 0.025
done
test ! -S "$socket"
pid=
run_guest LATX_AOT_V2=1 LATC_STRICT_FILE_AOT=1 LATX_AOT_V2_REPORT=1 \
  > "$work/warm.stdout" 2> "$work/warm.stderr"
cmp "$work/jit.stdout" "$work/warm.stdout"
grep -q 'module=registered' "$work/warm.stderr"
grep -q 'compiler_submissions=0 compiler_submission_failures=0' "$work/warm.stderr"
test ! -S "$socket"

# An unsafe cache is never taken over; ordinary execution still works.
mkdir -m 700 "$work/unsafe-real"
ln -s "$work/unsafe-real" "$work/unsafe-cache"
run_guest LATX_AOT_V2=1 LATX_AOT_V2_CACHE_DIR="$work/unsafe-cache" \
  > "$work/unsafe.stdout" 2> "$work/unsafe.stderr"
cmp "$work/jit.stdout" "$work/unsafe.stdout"
test "$(find "$work/run" -type s | wc -l)" -eq 0

# Non-XDG sessions must retain working automatic paths.
mkdir -m 700 "$work/fallback-home"
long_state=$work/fallback-home/long-state-directory-to-cover-unix-socket-paths-over-one-hundred-and-eight-bytes
env -i HOME="$work/fallback-home" XDG_STATE_HOME="$long_state" PATH=/usr/bin:/bin TMPDIR="$work" \
  LD_LIBRARY_PATH="$prefix/lib" LATX_AOT_V2=1 LATX_AOT_V2_REPORT=1 \
  "$runner" "$guest" > "$work/fallback.stdout" 2> "$work/fallback.stderr"
cmp "$work/jit.stdout" "$work/fallback.stdout"
grep -q 'compiler_submissions=1 compiler_submission_failures=0' "$work/fallback.stderr"
fallback_socket=$(find "$work/fallback-home" -type s -name '*.sock')
pid=$(pgrep -f "^$daemon --serve --daemonize --socket $fallback_socket ")
"$daemon" --flush-all --socket "$fallback_socket" > "$work/fallback-flush.log"
kill -TERM "$pid"
for n in $(seq 1 200); do
    test -S "$fallback_socket" || break
    sleep 0.025
done
test ! -S "$fallback_socket"
pid=

mkdir -m 500 "$work/readonly-cache"
run_guest LATX_AOT_V2=1 LATX_AOT_V2_CACHE_DIR="$work/readonly-cache" \
  > "$work/readonly.stdout" 2> "$work/readonly.stderr"
cmp "$work/jit.stdout" "$work/readonly.stdout"
test "$(find "$work/readonly-cache" -type f | wc -l)" -eq 0

# An explicit wrong-identity peer must not receive any compilation requests.
mkdir -m 700 "$work/wrong-cache"
python3 - "$work/run/wrong.sock" "$work/wrong-peer.log" <<'PY' &
import os, socket, struct, sys
s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
s.bind(sys.argv[1])
s.listen(1)
s.settimeout(10)
connection, _ = s.accept()
request = connection.recv(4096)
# ABI response size and field layout are asserted by the protocol unit tests.
magic, version, size, operation = struct.unpack_from('<IHHI', request)
assert operation == 5, operation
request_id = struct.unpack_from('<Q', request, 16)[0]
response = struct.pack('<IHHIIQQQ32s192s', 0x4c415452, 2, 264, 0, 0,
                       request_id, 0, 0, bytes(32), b'wrong-build')
connection.send(response)
connection.close()
s.settimeout(0.5)
try:
    s.accept()
except TimeoutError:
    open(sys.argv[2], 'w').write('no compilation request\n')
else:
    raise AssertionError('wrong peer received a second request')
s.close()
os.unlink(sys.argv[1])
PY
peer=$!
for n in $(seq 1 200); do
    test ! -S "$work/run/wrong.sock" || break
    sleep 0.025
done
run_guest LATX_AOT_V2=1 LATX_AOT_V2_CACHE_DIR="$work/wrong-cache" \
  LATX_AOT_V2_LATCD_SOCKET="$work/run/wrong.sock" \
  > "$work/wrong.stdout" 2> "$work/wrong.stderr"
wait "$peer"
peer=
cmp "$work/jit.stdout" "$work/wrong.stdout"
grep -q 'no compilation request' "$work/wrong-peer.log"

# Missing daemon with autostart disabled still executes via JIT.
run_guest LATX_AOT_V2=1 LATX_AOT_V2_CACHE_DIR="$work/disabled-cache" \
  LATX_AOT_V2_AUTOSTART=0 > "$work/disabled.stdout" 2> "$work/disabled.stderr"
cmp "$work/jit.stdout" "$work/disabled.stdout"
test "$(find "$work/run" -type s | wc -l)" -eq 0

# Reuse the same socket/cache to observe idle exit.
cache=$work/cache/lat-aot-v2/$identity/live
"$daemon" --serve --socket "$socket" --cache-dir "$cache" \
  --latc "$prefix/bin/latc" --runner "$runner" --runtime-dir "$prefix/lib" \
  --idle-seconds 1 > "$work/idle.stdout" 2> "$work/idle.stderr" &
pid=$!
wait "$pid"
pid=
test ! -S "$socket"
run_guest LATX_AOT_V2=1 LATX_AOT_V2_CACHE_DIR="$work/restart-cache" \
  LATX_AOT_V2_REPORT=1 > "$work/restart.stdout" 2> "$work/restart.stderr"
cmp "$work/jit.stdout" "$work/restart.stdout"
grep -q 'compiler_submissions=1 compiler_submission_failures=0' "$work/restart.stderr"
socket=$(find "$work/run" -type s -name '*.sock')
pid=$(pgrep -f "^$daemon --serve --daemonize --socket $socket ")
"$daemon" --flush-all --socket "$socket" > "$work/restart-flush.log"
echo 'test-aot-v2-autostart: PASS'
