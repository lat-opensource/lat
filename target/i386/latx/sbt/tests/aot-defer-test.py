#!/usr/bin/env python3
"""Native process-tree tests; no translated guest or benchmark dependency."""
import json
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time

controller, fixture = map(os.path.abspath, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='aot-defer-test-') as directory:
    directory = pathlib.Path(directory)
    # Reproduce a connected application process dying before it forks/registers
    # a generator. The pipe proves connect happened before SIGTERM; no sleeps.
    client = directory / 'registration-client.py'
    client.write_text(r'''import os, pathlib, signal, socket, struct, sys
mode, marker, fixture = sys.argv[1:]
def connect():
    s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    s.settimeout(3)
    s.connect(os.environ['LATX_QUICKTRANS_AOT_CONTROL'])
    return s
if mode == 'killed-before-ready':
    r, w = os.pipe()
    pid = os.fork()
    if pid == 0:
        os.close(r)
        s = connect()
        os.write(w, b'C')
        while True:
            signal.pause()
    os.close(w)
    assert os.read(r, 1) == b'C'
    os.close(r)
    os.kill(pid, signal.SIGTERM)
    waited, status = os.waitpid(pid, 0)
    assert waited == pid and os.WIFSIGNALED(status)
    assert os.WTERMSIG(status) == signal.SIGTERM
    pathlib.Path(marker + '.peer').write_text(str(pid))
elif mode == 'abort-reuse':
    for i in range(3):
        s = connect()
        s.shutdown(socket.SHUT_WR)
        assert s.recv(1) == b''  # Controller acknowledged the abort by closing.
        s.close()
    pathlib.Path(marker + '.peer').write_text(str(os.getpid()))
elif mode == 'malformed-ready':
    s = connect()
    s.send(b'bad')
    assert s.recv(1) == b''
    s.close()
    sys.exit(0)
elif mode == 'registered-disconnect':
    s = connect()
    s.send(struct.pack('=Ii', 0x51414431, os.getpid()))
    command, ancillary, flags, address = s.recvmsg(1, socket.CMSG_SPACE(4))
    assert command == b'W'
    for level, kind, data in ancillary:
        if level == socket.SOL_SOCKET and kind == socket.SCM_RIGHTS:
            os.close(struct.unpack('=i', data[:4])[0])
    s.close()
    sys.exit(0)
os.execv(fixture, [fixture, 'pipe', marker])
''')
    for mode, limit, aborts in [('killed-before-ready', 64, 1),
                                ('abort-reuse', 1, 3),
                                ('malformed-ready', 64, 0),
                                ('registered-disconnect', 64, 0)]:
        marker, report = directory / mode, directory / (mode + '.json')
        result = subprocess.run([controller, '--report', str(report),
                                 '--max-workers', str(limit), '--', sys.executable,
                                 str(client), mode, str(marker), fixture],
                                capture_output=True, timeout=8)
        data = json.loads(report.read_text())
        assert result.returncode == 0, (mode, result, data)
        if aborts:
            assert data['failures'] == 0, (mode, data, result.stderr)
            assert len(data['registration_aborts']) == aborts, (mode, data)
            peer = int(pathlib.Path(str(marker) + '.peer').read_text())
            assert all(a['peer_pid'] == peer and
                       a['reason'] == 'eof_before_ready'
                       for a in data['registration_aborts']), (mode, data)
            assert marker.read_text() == 'G\n', (mode, data)
            assert len(data['workers']) == 1, (mode, data)
            worker = data['workers'][0]
            assert worker['released'] and worker['reaped'] and worker['status'] == 0, data
            assert worker['start_ms'] >= data['app_exit_ms'], data
        else:
            assert data['failures'] > 0 and not data['registration_aborts'], (mode, data)
            assert not marker.exists(), (mode, data)
        assert data['collection_incomplete'], (mode, data)
        print('PASS', mode, json.dumps(data), flush=True)
    for mode, limit, expected in [('pipe', 64, 1), ('orphan', 64, 1),
                                  ('multiple', 64, 2), ('app-failure', 64, 2),
                                  ('worker-failure', 64, 2), ('multiple', 1, 1)]:
        name = mode + str(limit)
        marker, report = directory / name, directory / (name + '.json')
        result = subprocess.run([controller, '--report', str(report),
                                 '--max-workers', str(limit), '--', fixture,
                                 mode, str(marker)], capture_output=True, timeout=8)
        data = json.loads(report.read_text())
        assert result.returncode == (23 if mode == 'app-failure' else 0), (name, result, data)
        assert len(data['workers']) == expected, (name, data)
        assert data['failures'] > 0 if limit == 1 or mode == 'worker-failure' else data['failures'] == 0, (name, data)
        assert not data['registration_aborts'], (name, data)
        assert data['collection_incomplete'] == bool(data['failures']), (name, data)
        assert marker.read_text().splitlines() == ['G'] * expected, (name, data)
        workers = sorted(data['workers'], key=lambda w: w['start_ms'])
        for i, worker in enumerate(workers):
            assert worker['released'] and worker['reaped'], (name, data)
            assert worker['start_ms'] >= data['app_exit_ms'], (name, data)
            assert worker['end_ms'] >= worker['start_ms'], (name, data)
        if mode == 'orphan':
            assert data['app_exit_ms'] - data['root_exit_ms'] >= 200, data
        print('PASS', name, json.dumps(data), flush=True)
    for name, command, status in [
        ('empty', ['/bin/true'], 0),
        ('signal', [sys.executable, '-c',
                    'import os,signal;os.kill(os.getpid(),signal.SIGTERM)'], 143),
        ('exec-failure', [str(directory / 'missing-program')], 127),
    ]:
        report = directory / (name + '.json')
        result = subprocess.run([controller, '--report', str(report), '--'] + command,
                                capture_output=True, timeout=4)
        data = json.loads(report.read_text())
        assert result.returncode == status and data['app_status'] == status, (name, data)
        assert not data['workers'] and not data['failures'], (name, data)
        print('PASS', name, flush=True)
    report = directory / 'ignored-sigchld.json'
    result = subprocess.run([controller, '--report', str(report), '--', '/bin/true'],
                            preexec_fn=lambda: signal.signal(signal.SIGCHLD, signal.SIG_IGN),
                            capture_output=True, timeout=4)
    data = json.loads(report.read_text())
    assert result.returncode == 0 and data['app_status'] == 0 and not data['failures'], data
    print('PASS inherited-SIGCHLD-ignore', flush=True)
    # Kill the controller while the application remains alive. Workers must
    # cancel on socket EOF, without writing the generation marker.
    marker = directory / 'cancel'
    process = subprocess.Popen([controller, '--', fixture, 'hold', str(marker)],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               start_new_session=True)
    try:
        waiting = pathlib.Path(str(marker) + '.waiting')
        cancelled = pathlib.Path(str(marker) + '.cancelled')
        deadline = time.monotonic() + 3
        while not waiting.exists():
            assert process.poll() is None, 'controller exited before worker waited'
            assert time.monotonic() < deadline, 'worker never reached release wait'
            time.sleep(.005)
        assert waiting.read_text() == 'W\n'
        assert not marker.exists(), 'worker generated before controller exit'
        process.kill()
        process.communicate(timeout=4)
        assert cancelled.read_text() == 'C\n', 'worker cancellation not reaped'
        assert not marker.exists()
        print('PASS controller-death', flush=True)
    finally:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
