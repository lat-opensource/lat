#!/usr/bin/env python3
"""Real x86 guest: lock contention must preserve both branch contributions."""
import fcntl
import hashlib
import json
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import time

translator, controller = map(os.path.abspath, sys.argv[1:])
# aot-collect-union.S: argv[1] selects an indirect call to independent A/B blocks.
text = bytes.fromhex(
    '488b442410803862488d0516000000488d0d18000000480f44c1ffd0b83c'
    '00000031ff0f05b80b00000083c007c3b81700000083e803c3')

def make_guest(path):
    size = 4096 + len(text)
    ident = b'\x7fELF\x02\x01\x01' + bytes(9)
    header = struct.pack('<16sHHIQQQIHHHHHH', ident, 2, 62, 1,
                         0x401000, 64, 0, 0, 64, 56, 1, 0, 0, 0)
    phdr = struct.pack('<IIQQQQQQ', 1, 5, 0, 0x400000, 0x400000,
                       size, size, 4096)
    path.write_bytes((header + phdr).ljust(4096, b'\0') + text)
    path.chmod(0o700)

def coverage(path):
    # Current serialized 64-bit AOT layout: segment 96 bytes, TB 160 bytes.
    data = path.read_bytes()
    u32 = lambda off: struct.unpack_from('<I', data, off)[0]
    u64 = lambda off: struct.unpack_from('<Q', data, off)[0]
    result = set()
    for i in range(u32(28)):
        segment = u32(24) + i * 96
        for j in range(u32(segment + 80)):
            tb = u32(segment + 76) + j * 160
            result.add(u64(segment + 8) + u32(tb + 80))
    return result

with tempfile.TemporaryDirectory(prefix='aot-collect-union-') as tmp:
    root = pathlib.Path(tmp)
    guest = root / 'guest'
    make_guest(guest)
    key = 'v2-01-' + hashlib.sha256(str(guest).encode()).hexdigest() + '.aot2'
    base = {k: v for k, v in os.environ.items()
            if not k.startswith(('LATX_', 'LAT_'))}

    def run(home, branch, label, locked=False, failure=False):
        cache = home / '.cache/latx'
        cache.mkdir(parents=True, exist_ok=True)
        lock_path = cache / (key + '.lock')
        report = root / (label + '.json')
        if failure:
            lock_path.mkdir()
        env = dict(base, HOME=str(home), LATX_FAST_TRANSLATOR='1',
                   LATX_FAST_TRANSLATOR_STRICT='1', LATX_AOT='0',
                   LATX_LOAD_AOT='0', LATX_AOT_GENERATE='0',
                   LATX_QUICKTRANS_AOT_COLLECT='1', LATX_KZT='0')
        fd = None
        process = None
        try:
            if locked:
                fd = os.open(lock_path, os.O_CREAT | os.O_RDWR, 0o600)
                fcntl.lockf(fd, fcntl.LOCK_EX)
            process = subprocess.Popen(
                [controller, '--report', str(report), '--', translator,
                 str(guest), branch], env=env, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE)
            if locked:
                time.sleep(.5)
                waiting = process.poll() is None
                fcntl.lockf(fd, fcntl.LOCK_UN)
                os.close(fd)
                fd = None
            stdout, stderr = process.communicate(timeout=20)
            info = json.loads(report.read_text())
            assert process.returncode == 0 and info['app_status'] == 0, info
            assert bool(info['failures']) == failure, (info, stderr)
            if failure:
                assert info['collection_incomplete'], info
                assert not (cache / key).exists()
                return set()
            assert not info['collection_incomplete'], info
            if locked:
                assert waiting, 'collection returned before lock release'
            return coverage(cache / key)
        finally:
            if fd is not None:
                os.close(fd)
            if process is not None and process.poll() is None:
                process.kill()
                process.communicate()

    a = run(root / 'a', 'a', 'reference-a')
    b = run(root / 'b', 'b', 'reference-b')
    assert a - b and b - a, (a, b)
    home = root / 'union'
    assert run(home, 'a', 'seed') == a
    assert run(home, 'b', 'contended', locked=True) == a | b
    assert run(home, 'a', 'duplicate') == a | b
    run(root / 'failure', 'a', 'lock-error', failure=True)
    print('PASS: distinct contributions, lock wait, duplicate, failure reporting')
