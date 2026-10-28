#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Verify that configuration cannot advertise AVX when LASX is unavailable."""
import ctypes
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile


def make_guest(path, abi):
    # CPUID.0 bounds leaf 7; reject AVX/FMA/F16C and AVX2, then exit.
    code = bytes.fromhex(
        '31c00fa289c7b8010000000fa2f7c100100030751a83ff077211'
        'b80700000031c90fa2f7c320000000750431dbeb05bb01000000')
    base = 0x400000
    if abi == 'x86_64':
        code += bytes.fromhex('89dfb83c0000000f05')
        ident = b'\x7fELF\x02\x01\x01' + bytes(9)
        header = struct.pack('<16sHHIQQQIHHHHHH', ident, 2, 62, 1,
                             base + 120, 64, 0, 0, 64, 56, 1, 0, 0, 0)
        phdr = struct.pack('<IIQQQQQQ', 1, 5, 0, base, base,
                           120 + len(code), 120 + len(code), 0x1000)
    else:
        code += bytes.fromhex('b801000000cd80')
        ident = b'\x7fELF\x01\x01\x01' + bytes(9)
        header = struct.pack('<16sHHIIIIIHHHHHH', ident, 2, 3, 1,
                             base + 84, 52, 0, 0, 52, 32, 1, 0, 0, 0)
        phdr = struct.pack('<IIIIIIII', 1, 0, base, base,
                           84 + len(code), 84 + len(code), 5, 0x1000)
    path.write_bytes(header + phdr + code)
    path.chmod(0o755)


def main():
    translator, abi, compiler, shim_source = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix='latx-lasx-cpuid-') as temp:
        root = Path(temp)
        shim = root / 'mask.so'
        subprocess.run(shlex.split(compiler) + ['-shared', '-fPIC', '-O2',
                       shim_source, '-ldl', '-o', str(shim)], check=True)
        guest = root / 'guest'
        make_guest(guest, abi)
        config_dir = root / '.config'
        config_dir.mkdir()
        config = config_dir / ('latx-' + abi + '.conf')
        for source in ('default', 'environment', 'command-line', 'config'):
            env = dict(os.environ, HOME=temp, LATX_AOT='0')
            env.pop('LATX_AVX_CPUID', None)
            env['LD_PRELOAD'] = str(shim)
            config.write_text('LATX_AVX_CPUID=1\n' if source == 'config'
                              else '')
            command = [translator]
            if source == 'environment':
                env['LATX_AVX_CPUID'] = '1'
            if source == 'command-line':
                command += ['-latx-avx-cpuid', '1']
            result = subprocess.run(command + [str(guest)], env=env,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=30)
            if (result.returncode != 0 or
                    b'mask-lasx: HWCAP' not in result.stderr):
                raise RuntimeError('{}: exit={} stdout={!r} stderr={!r}'.format(
                    source, result.returncode, result.stdout, result.stderr))
            print('PASS: LASX hidden, ' + source)
        # On a LASX host, an explicit AVX request must still be honored.
        libc = ctypes.CDLL(None)
        libc.getauxval.argtypes = [ctypes.c_ulong]
        libc.getauxval.restype = ctypes.c_ulong
        if libc.getauxval(16) & (1 << 5):
            env.pop('LD_PRELOAD')
            env['LATX_AVX_CPUID'] = '1'
            result = subprocess.run([translator, str(guest)], env=env,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=30)
            if result.returncode != 1:
                raise RuntimeError('LASX control: expected AVX, exit={}'.format(
                    result.returncode))
            print('PASS: real LASX retains AVX advertisement')


if __name__ == '__main__':
    main()
