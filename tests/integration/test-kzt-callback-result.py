#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise real native Cairo and inject preparation failures outside product code."""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import shutil
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('emulator', type=Path)
    parser.add_argument('mode')
    parser.add_argument('guest_source', type=Path)
    parser.add_argument('native_source', type=Path)
    args = parser.parse_args()
    artifacts = os.environ.get('LATX_CAIRO_CALLBACK_ARTIFACT_DIR')
    if platform.machine() != 'loongarch64' or not shutil.which('gdb') or not artifacts:
        print('SKIP: requires LoongArch, GDB, and LATX_CAIRO_CALLBACK_ARTIFACT_DIR')
        return 77
    artifacts = Path(artifacts).resolve()
    expected = hashlib.sha256(args.guest_source.read_bytes()).hexdigest()
    sources = dict(line.split()[::-1] for line in (artifacts / 'sources.sha256').read_text().splitlines())
    if sources.get(args.guest_source.name) != expected:
        raise SystemExit('FAIL: Guest source hash mismatch')
    for line in (artifacts / 'artifacts.sha256').read_text().splitlines():
        digest, relative = line.split()
        if hashlib.sha256((artifacts / relative).read_bytes()).hexdigest() != digest:
            raise SystemExit('FAIL: artifact mismatch: ' + relative)
    root = Path(os.environ.get('LATX_X86_64_SYSROOT', str(artifacts / 'runtime')))
    freetype = args.mode.startswith('ft-')
    mode_name = args.mode[3:] if freetype else args.mode
    threaded = mode_name in ('threaded-success', 'attach-failure')
    failure = mode_name in ('attach-failure', 'refresh-failure')
    mode = 'prepare-fail' if failure else 'guest-error' if mode_name.endswith('error') else 'nested' if mode_name.endswith('nested') else 'success'
    tls = '0' if mode_name.startswith('off-') else '1'
    with tempfile.TemporaryDirectory(prefix='lat-cairo-callback-') as temporary:
        work = Path(temporary)
        probe = work / 'native-probe.so'
        if threaded:
            subprocess.run(shlex.split(os.environ.get('LATX_NATIVE_CC', os.environ.get('CC', 'cc'))) + ['-shared', '-fPIC', '-O2',
                            '-Wall', '-Wextra', '-Werror', str(args.native_source),
                            '-pthread', '-l:libcairo.so.2', '-o', str(probe)], check=True)
        commands = '''set pagination off
set confirm off
set $armed=0
handle SIGSEGV SIGBUS SIGPIPE SIGILL nostop noprint pass
break my_cairo_surface_write_to_png_stream
commands
silent
printf "WRAPPER_ENTER\\n"
'''
        if threaded:
            commands += 'set variable my_cairo.cairo_surface_write_to_png_stream = (unsigned int (*)(void*,void*,void*))latx_test_cairo_thread_write\n'
        commands += '''disable 1
continue
end
break cairo_io_thunk_0
commands
silent
printf "CALLBACK_ENTER\\n"
set $armed=1
disable 2
continue
end
'''
        if failure:
            target = 'latx_attach_current_host_thread' if threaded else 'kzt_guest_tls_refresh_if_needed'
            commands += f'''break {target} if $armed
commands
silent
printf "INJECT_{args.mode}\\n"
set $armed=0
disable 3
return -1
continue
end
'''
        commands += 'run\nprintf "GUEST_EXIT_CODE=%d\\n", $_exitcode\n'
        if freetype:
            commands = commands.replace('my_cairo_surface_write_to_png_stream', 'my_FT_Outline_Decompose').replace('cairo_io_thunk_0', 'ft_move_thunk_0')
        script = work / 'callback.gdb'
        script.write_text(commands)
        env = dict(os.environ, LATX_AOT='0', LATX_KZT='1', LATX_KZT_LIBS='core,cairo',
                   LATX_KZT_GUEST_TLS=tls, LATX_KZT_LOG='1',
                   BOX64_LD_LIBRARY_PATH=str(artifacts / 'guest-lib'))
        env.pop('LD_PRELOAD', None)
        if threaded:
            env['LD_PRELOAD'] = str(probe)
        command = ['gdb', '-q', '-batch', '-x', str(script), '--args',
                   str(args.emulator.resolve()), '-U', 'LD_PRELOAD', '-L', str(root),
                   '-E', 'LD_LIBRARY_PATH=' + str(artifacts / 'guest-lib'),
                   str(artifacts / ('freetype-guest' if freetype else 'guest')), mode]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=45)
        print(result.stdout)
        required = ['using KZT wrapper for libcairo.so.2', 'WRAPPER_ENTER',
                    'CALLBACK_ENTER', 'GUEST_EXIT_CODE=0',
                    'PASS: Cairo callback execution/result boundary']
        if freetype:
            required[0] = 'using KZT wrapper for libfreetype.so.6'
            required[-1] = 'PASS: FreeType callback execution/result boundary'
        if threaded:
            required.append('NATIVE_CAIRO_THREAD=')
        if failure:
            required.append('INJECT_' + args.mode)
        if result.returncode or any(marker not in result.stdout for marker in required):
            return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
