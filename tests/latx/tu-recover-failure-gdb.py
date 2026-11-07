"""Inject a recoverable failure before TU publication, then retry in place."""
import gdb
import json
import os
import re

with open(os.environ['TU_LAYOUT']) as stream:
    layout = json.load(stream)
target = int(os.environ['TU_FAULT_PC'], 0)
fault = os.environ['TU_FAULT']
active = False
injected = False
checked = False
retrying = False
published = 0
allocations = 0
flushes = 0
context = 0
saved = {}
key = ()


def arg(index):
    return int(gdb.parse_and_eval('$r%d' % (index + 4)))


def field(name):
    data = gdb.selected_inferior().read_memory(context + layout[name], 8)
    return int.from_bytes(data, 'little')


def set_field(name, value):
    gdb.selected_inferior().write_memory(context + layout[name],
                                       value.to_bytes(8, 'little'))


class Loaded(gdb.Breakpoint):
    def stop(self):
        global active, checked, retrying
        if retrying:
            assert arg(0) == 1, 'retry failed'
            retrying = False
            checked = True
            return False
        if not active:
            return False
        active = False
        assert injected, 'fault not injected'
        assert arg(0) == 0, 'failed TU reported as loaded'
        assert published == 0, 'partial TU published'
        for name in ('code_ptr', 'tb_ptr', 'data_ptr'):
            assert field(name) == saved[name], 'leaked allocation: ' + name
        assert int(gdb.parse_and_eval('*(size_t *)&aot_global_info_index')) == 0
        for name in ('code_limit', 'tb_limit'):
            set_field(name, saved[name])
        if fault in ('fallback', 'exhaust'):
            checked = True
            return False
        # The function has restored the caller's stack and registers. Retry
        # from its entry with the same key, avoiding debugger inferior calls.
        entry.enabled = False
        retrying = True
        for i, value in enumerate(key):
            gdb.execute('set $r%d = %d' % (i + 4, value))
        gdb.execute('set $pc = (void *)load_aot_tu')
        return False


class Entry(gdb.Breakpoint):
    def stop(self):
        global active, context, saved, key, injected
        # A source-line breakpoint can run after the prologue has reused
        # argument registers; read DWARF variables in this frame instead.
        values = tuple(int(gdb.parse_and_eval(name)) for name in
                       ('pc', 'cs_base', 'flags', 'cflags'))
        if values[0] != target or checked:
            return False
        key = values
        context = int(gdb.parse_and_eval('*(void **)&tcg_ctx'))
        saved = {name: field(name) for name in
                 ('code_ptr', 'tb_ptr', 'data_ptr', 'code_limit', 'tb_limit')}
        active = True
        if fault == 'exhaust':
            # Consume the real remaining code region and let JIT flush it.
            set_field('code_ptr', saved['code_limit'] + 1)
            saved['code_ptr'] = saved['code_limit'] + 1
            injected = True
        if fault in ('code', 'tbs'):
            name = 'code' if fault == 'code' else 'tb'
            set_field(name + '_limit', saved[name + '_ptr'])
            injected = True
        return False


class Allocate(gdb.Breakpoint):
    def stop(self):
        global allocations, injected
        if active:
            allocations += 1
            fail_at = {'first': 1, 'second': 2, 'fallback': 2}.get(fault)
            if allocations == fail_at:
                gdb.execute('return (void *)0', to_string=True)
                injected = True
        return False


class Reserve(gdb.Breakpoint):
    def stop(self):
        if active and fault == 'links':
            # Force the real queue-growth allocation, then fail that call.
            gdb.execute('set *(size_t *)&aot_global_info_total = 0')
        return False


class Reallocate(gdb.Breakpoint):
    def stop(self):
        global injected
        if active and fault == 'links':
            gdb.execute('return (void *)0', to_string=True)
            injected = True
        return False


class Flush(gdb.Breakpoint):
    def stop(self):
        global flushes
        if checked:
            flushes += 1
        return False


class Publish(gdb.Breakpoint):
    def stop(self):
        global published
        if active:
            published += 1
        return False


gdb.execute('set pagination off')
gdb.execute('set confirm off')
gdb.execute('handle SIGSEGV nostop noprint pass')
entry = Entry('load_aot_tu', internal=True)
Allocate('tcg_tb_alloc', internal=True)
Reserve('aot_link_tree_reserve', internal=True)
Reallocate('g_try_realloc_n', internal=True)
Flush('do_tb_flush', internal=True)
Publish('aot_tb_register', internal=True)
# Observe at the instruction returning from the loader; this avoids GDB's
# LoongArch FinishBreakpoint frame tracking during force-return injection.
loader_text = gdb.execute('disassemble load_aot_tu', to_string=True)
for address in re.findall(r'^\s*(0x[0-9a-f]+).*jirl\s+\$r0,\$r1,0$',
                          loader_text, re.M):
    Loaded('*' + address, internal=True)
gdb.execute('run')
if (not checked or int(gdb.parse_and_eval('$_exitcode')) != 0 or
        (fault == 'exhaust' and not flushes)):
    print('TU recovery failure FAIL', fault)
    gdb.execute('quit 1')
print('TU recovery failure PASS', fault, 'allocations', allocations,
      'flushes', flushes)
gdb.execute('quit 0')
