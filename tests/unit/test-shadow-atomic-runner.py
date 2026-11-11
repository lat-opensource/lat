#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check runner failure classification without cross-compiling a guest."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

runner = Path(sys.argv[1]) / 'tests/integration/test-shadow-small-atomic.sh'
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)

    def tool(name, body):
        path = root / name
        path.write_text('#!/bin/sh\n' + body)
        path.chmod(0o755)
        return str(path)

    tool('uname', 'echo loongarch64\n')
    tool('getconf', 'echo 16384\n')
    tool('ld.lld', 'exit 0\n')
    # The native capability probe is independent of the cross-compiler probe.
    tool('cc', '''
while [ "$#" -gt 0 ]; do
    if [ "$1" = -o ]; then
        shift
        printf '#!/bin/sh\\nexit 0\\n' > "$1"
        chmod +x "$1"
        exit 0
    fi
    shift
done
exit 1
''')
    cross = tool('cross-cc', '''
for arg do
    case "$arg" in
        */cross-probe.c) exit "$PROBE_STATUS" ;;
    esac
done
echo fixture-build-error >&2
exit 1
''')
    env = os.environ.copy()
    env.pop('LATX_SHADOW_ATOMIC_FIXTURE', None)
    env.update(PATH=directory + os.pathsep + env['PATH'],
               LATX_X86_64_CC=cross)
    for probe_status, expected in [('1', 77), ('0', 1)]:
        env['PROBE_STATUS'] = probe_status
        result = subprocess.run(['sh', str(runner), '/unused/emulator',
                                 '/unused/fixture.c'], env=env,
                                capture_output=True, text=True, timeout=10)
        if result.returncode != expected:
            sys.exit(f'probe={probe_status}: expected {expected}, '
                     f'got {result.returncode}\n'
                     f'{result.stdout}{result.stderr}')
        if probe_status == '0' and 'fixture-build-error' not in result.stderr:
            sys.exit('The real fixture build was not reached')
print('PASS: missing cross-toolchain skips; fixture build errors fail')
