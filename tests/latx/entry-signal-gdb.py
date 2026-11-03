# SPDX-License-Identifier: GPL-2.0-only
"""Inject a signal immediately before or after the final TB exit check."""
import gdb
import os
from pathlib import Path

wanted = int(os.environ['ENTRY_SIGNAL_PC'], 0)
stage = os.environ.get('ENTRY_SIGNAL_STAGE', 'prologue')
checked = False
installed = False


def is_target_tb(tb):
    return (int(tb) and
            wanted <= int(tb.dereference()['pc']) < wanted + 32)


class Precheck(gdb.Breakpoint):
    """Stop after publishing the anchor and before reading exit_request."""

    def stop(self):
        global checked
        if checked:
            return False
        anchor = gdb.parse_and_eval('latx_entry_tb')
        if not is_target_tb(anchor):
            return False
        checked = True
        print('inject SIGUSR1 before final TB exit check')
        return True


class Entry(gdb.Breakpoint):
    def stop(self):
        global checked
        if checked:
            return False
        address = int(gdb.parse_and_eval('$r4'))
        tb = gdb.parse_and_eval(
            '(TranslationBlock *)tcg_tb_lookup(%d)' % (address + 1))
        if is_target_tb(tb):
            checked = True
            print('inject SIGUSR1 at translated entry')
            return True
        return False


class Setup(gdb.Breakpoint):
    def stop(self):
        global installed
        if not installed:
            address = int(gdb.parse_and_eval('context_switch_bt_to_native'))
            if address:
                Entry('*%d' % address, internal=True)
                installed = True
        return False


gdb.execute('set pagination off')
gdb.execute('set confirm off')
gdb.execute('handle SIGSEGV nostop noprint pass')
gdb.execute('handle SIGUSR1 nostop noprint pass')

if stage == 'precheck':
    repo = Path(__file__).resolve().parents[2]
    source = repo / 'accel/tcg/cpu-exec.c'
    lines = source.read_text().splitlines()
    line = next(number for number, text in enumerate(lines, 1)
                if 'if (unlikely(qatomic_read(&cpu->exit_request)))' in text)
    Precheck('%s:%d' % (source, line), internal=True)
elif stage == 'prologue':
    Setup('*do_syscall', internal=True)
else:
    raise RuntimeError('unknown ENTRY_SIGNAL_STAGE: ' + stage)

gdb.execute('run')
if checked:
    gdb.execute('signal SIGUSR1')
if not checked or int(gdb.parse_and_eval('$_exitcode')) != 0:
    gdb.execute('quit 1')
gdb.execute('quit 0')
