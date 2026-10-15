#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Exercise callback installation on actual generated and linked LoongArch TBs.

GDB installs a harmless callback at the first TB execution boundary. The guest
executes its entry JMP five times. No product-only test switch is required.
"""
import argparse
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile


def commands(case):
    setup = '''break cpu_tb_exec
run GUEST
'''
    breakpoint = 1
    if case == 'interior':
        setup = '''break tb_gen_code
run GUEST
set $cache = (char *)tcg_ctx->code_gen_buffer
set tcg_ctx->code_gen_ptr = $cache + (200 << 20)
set tcg_ctx->code_gen_highwater = $cache + tcg_ctx->code_gen_buffer_size - 4096
disable 1
break cpu_tb_exec
continue
'''
        breakpoint = 2
    link_target = '''python
# The fixture places its target 16 bytes after the entry JMP.
n = len(gdb.lookup_global_symbol("tb_gen_code").type.fields())
gdb.execute("call mmap_lock()")
gdb.execute("set $target = tb_gen_code($cp, $entry + 16, 0, $original->flags, $cp->tcg_cflags" + (", 0, 0" if n == 7 else "") + ")")
gdb.execute("call mmap_unlock()")
end
'''
    install = '''set $hook = kzt_install_guest_pc_callback($st, $cp, $entry, (void (*)(CPUX86State *))qemu_log_flush)
if $hook == 0
printf "INSTALL_FAILED\\n"
quit 1
end
if $hook->cflags != $cp->tcg_cflags
printf "PRIVATE_FLAG_LEAKED\\n"
quit 1
end
'''
    script = '''set pagination off
set confirm off
set environment LATX_AOT 0
set environment LATX_KZT 0
handle SIGUSR1 nostop noprint pass
''' + setup + '''set $cp = cpu
set $original = itb
set ((CPUX86State *)cpu->env_ptr)->regs[12] = 0
set $entry = itb->pc
set $cache_start = (uintptr_t)tcg_ctx->code_gen_buffer
set $cache_end = $cache_start + tcg_ctx->code_gen_buffer_size
python
if int(gdb.parse_and_eval("itb->jmp_target_arg[1]")) != 0:
    raise gdb.GdbError("fixture did not generate an entry link slot")
end
set $st = (KztGuestPcCallbackState *)calloc(1, sizeof(KztGuestPcCallbackState))
'''
    if case == 'interior':
        script += link_target + '''call tb_add_jump($original, 1, $target)
set tcg_ctx->code_gen_ptr = (char *)$cache_start + (400 << 20)
'''
    script += install
    if case == 'unlink_relink':
        script += link_target + '''call tb_add_jump($hook, 1, $target)
call unlink_direct_jmp($hook)
call set_tb_relink_flag($hook, 1)
call tb_add_jump($hook, 1, $target)
''' + install
    if case == 'variants':
        script += '''set $flags = $cp->tcg_cflags
set $cp->tcg_cflags = $flags ^ 1
''' + install + '''set $cp->tcg_cflags = $flags
''' + install + '''# Isolate variant ownership from the separate link-slot regression.
set $hook->canlink[1] = 0
set $hook->jmp_reset_offset[1] = 65535
'''
    script += f'''set itb = $hook
set $hits = 0
disable {breakpoint}
break qemu_log_flush
commands
silent
if (uintptr_t)$ra >= $cache_start && (uintptr_t)$ra < $cache_end
set $hits = $hits + 1
end
continue
end
continue
printf "CALLBACK_HITS=%d EXPECTED=5\\n", $hits
quit
'''
    return script


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('emulator', type=Path)
    parser.add_argument('source', type=Path)
    parser.add_argument('--case', choices=('linked', 'unlink_relink',
                                           'variants', 'interior'))
    args = parser.parse_args()
    if platform.machine() != 'loongarch64' or not shutil.which('gdb'):
        print('SKIP: LoongArch host and GDB with Python support are required')
        return 77
    with tempfile.TemporaryDirectory(prefix='latx-callback-link-') as tmp:
        tmp = Path(tmp)
        supplied = os.environ.get('LATX_KZT_CALLBACK_GUEST')
        guest = Path(supplied).resolve() if supplied else tmp / 'guest'
        if supplied:
            if not guest.is_file():
                raise RuntimeError('supplied callback guest does not exist')
        else:
            cc = shutil.which('x86_64-linux-gnu-gcc')
            if not cc:
                print('SKIP: set LATX_KZT_CALLBACK_GUEST or install an x86-64 cross compiler')
                return 77
            subprocess.run([cc, '-nostdlib', '-static', '-no-pie',
                            '-Wl,--build-id=none', str(args.source),
                            '-o', str(guest)], check=True)
        cases = [args.case] if args.case else (
            'linked', 'unlink_relink', 'variants', 'interior')
        failures = 0
        for case in cases:
            script = tmp / (case + '.gdb')
            script.write_text(commands(case).replace('GUEST', '"' + str(guest) + '"'))
            result = subprocess.run(
                ['gdb', '-q', '-batch', '-x', str(script), str(args.emulator.resolve())],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                timeout=30)
            hits = re.findall(r'CALLBACK_HITS=(\d+) EXPECTED=5', result.stdout)
            passed = result.returncode == 0 and hits == ['5'] and \
                'exited normally' in result.stdout
            print(f'{case}: {"PASS" if passed else "FAIL"}, hits={hits}')
            if not passed:
                print(result.stdout)
                failures += 1
        return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
