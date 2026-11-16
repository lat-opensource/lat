#!/usr/bin/env python3
"""Signal/registration scope checks using production deferred protocol code."""
import json
import pathlib
import subprocess
import sys
import tempfile

controller, fixture = (str(pathlib.Path(p).resolve()) for p in sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='aot-report-') as tmp:
    for stage in ('before-connect', 'connected', 'forked-before-ready',
                  'ready-worker', 'ready-app', 'root-ready'):
        for sig in (0, 15, 9):
            case = pathlib.Path(tmp) / ('%s-%d' % (stage, sig))
            case.mkdir()
            report = case / 'report.json'
            result = subprocess.run([controller, '--report', str(report), '--',
                                     fixture, stage, str(sig), str(case)],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    timeout=12)
            data = json.loads(report.read_text())
            root_status = 128 + sig if stage == 'root-ready' and sig else 0
            assert result.returncode == data['app_status'] == root_status, (stage, sig, data)
            incomplete = stage in ('connected', 'forked-before-ready', 'ready-worker')
            assert data['collection_incomplete'] == incomplete, (stage, sig, data)
            assert data['report_scope'] == 'observed_collection_protocol', data
            assert data['coverage_status'] == ('incomplete' if incomplete else 'not_proven'), data
            assert (case / 'generated').exists() == (stage in ('ready-app', 'root-ready'))
            if stage != 'root-ready':
                observed = json.loads((case / 'observed.json').read_text())
                assert observed['si_code'] == (1 if sig == 0 else 2), observed
                assert observed['si_status'] == sig, observed
                assert not observed['generated_before_root_exit'], observed
                if stage in ('connected', 'forked-before-ready'):
                    assert len(data['registration_aborts']) == 1, data
                    assert data['registration_aborts'][0]['peer_pid'] == observed['app_pid'], data
            if stage == 'ready-worker':
                assert data['failures'] > 0 and len(data['workers']) == 1, data
                assert data['workers'][0]['status'] == (128 + sig if sig else 0), data
            if stage in ('ready-app', 'root-ready'):
                assert data['failures'] == 0 and len(data['workers']) == 1, data
                worker = data['workers'][0]
                assert worker['status'] == 0 and worker['released'] and worker['reaped'], data
                assert worker['start_ms'] >= data['app_exit_ms'], data
            print('PASS', stage, sig)
    # A signalled root without a snapshot is known incomplete, while the
    # compatibility field still reflects only the old protocol observations.
    for sig in (15, 9):
        report = pathlib.Path(tmp) / ('root-signal-%d.json' % sig)
        result = subprocess.run([controller, '--report', str(report), '--',
                                 sys.executable, '-c',
                                 'import os;os.kill(os.getpid(),%d)' % sig],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
        data = json.loads(report.read_text())
        assert result.returncode == data['app_status'] == 128 + sig, data
        assert not data['workers'] and not data['failures'] and not data['collection_incomplete'], data
        assert data['report_scope'] == 'observed_collection_protocol', data
        assert data['coverage_status'] == 'incomplete', data
        print('PASS root-signal-without-snapshot', sig)
