"""Require a large TU on the actual AOT generation and recovery paths."""
import gdb
import json
import os

phase = os.environ['TU_MEMBER_PHASE']
limit = int(os.environ['TU_MEMBER_LIMIT'])
hras = os.environ['LATX_HRAS'] == '1'
require_cold = os.environ.get('TU_MEMBER_REQUIRE_COLD', '1') == '1'
stats = dict(max_members=0, tus=0, callin_activations=0, loaded_callins=0,
             cold_ranges=0, cold_owners=0)
errors = []
seen_pcs = set()
tb_pointer = gdb.lookup_type('TranslationBlock').pointer()


def argument(index):
    return int(gdb.parse_and_eval('$r%d' % (4 + index)))


class Members(gdb.Breakpoint):
    def stop(self):
        generating = bool(gdb.parse_and_eval('in_pre_translate'))
        if generating != (phase == 'generate'):
            return False
        count = argument(0 if generating else 1)
        pointer = argument(1 if generating else 0)
        members = gdb.Value(pointer).cast(tb_pointer.pointer())
        stats['max_members'] = max(stats['max_members'], count)
        stats['tus'] += 1
        if count > limit:
            errors.append('TU exceeds member limit: %d' % count)
        if not generating:
            for index in range(count):
                tb = members[index].dereference()
                pc = int(tb['pc'])
                seen_pcs.add(pc)
                stats['loaded_callins'] += (
                    int(tb['lazypc'][0]) == 0x4852434900000001)
        return False


class Activate(gdb.Breakpoint):
    def stop(self):
        if not bool(gdb.parse_and_eval('in_pre_translate')):
            return False
        tb = gdb.Value(argument(0)).cast(tb_pointer).dereference()
        kind = int(tb['s_data'].dereference()['last_ir1_type'])
        if kind != int(gdb.parse_and_eval('IR1_TYPE_CALLIN')):
            errors.append('CALLIN layout applied to another TB type')
        stats['callin_activations'] += 1
        return False


class Inserted(gdb.FinishBreakpoint):
    def __init__(self, tb):
        self.tb = tb
        super().__init__(internal=True)

    def stop(self):
        tb = self.tb.dereference()
        base = int(tb['tc']['ptr'])
        cold = base + int(tb['tu_cold_offset'])
        owner = int(gdb.parse_and_eval(
            '(TranslationBlock *)tcg_tb_lookup((uintptr_t)%d)' % cold))
        # The private fast helper may be inlined. Exercise the public query;
        # exact misses and TBMini fallback are covered by the index unit test.

        stats['cold_ranges'] += 1
        stats['cold_owners'] += owner == int(self.tb)
        if owner != int(self.tb):
            errors.append('cold tail lookup returned the wrong TB')
        return False


class Insert(gdb.Breakpoint):
    def stop(self):
        tb = gdb.Value(argument(0)).cast(tb_pointer)
        if int(tb['tu_cold_size']):
            Inserted(tb)
        return False


for command in ['set pagination off', 'set confirm off',
                'set python print-stack full',
                'handle SIGILL nostop noprint pass',
                'handle SIGSEGV nostop noprint pass']:
    gdb.execute(command)
if phase == 'generate':
    gdb.execute('set follow-fork-mode child')
    Members('translate_tu', internal=True)
    Activate('hras_callin_activate_layout', internal=True)
else:
    Members('tu_link_members', internal=True)
    Insert('tcg_tb_insert', internal=True)
gdb.execute('run')
exitcode = gdb.parse_and_eval('$_exitcode')
if exitcode.type.code == gdb.TYPE_CODE_VOID or int(exitcode) != 0:
    errors.append('inferior did not exit normally with status zero')
if stats['max_members'] != limit:
    errors.append('required %d-member TU was not observed' % limit)
if phase == 'generate' and hras and not stats['callin_activations']:
    errors.append('indirect CALL cold layout was not exercised')
if phase == 'load':
    stats['unique_loaded_members'] = len(seen_pcs)
    if len(seen_pcs) < 1100 or stats['tus'] < 2:
        errors.append('members beyond the first full TU were not loaded')
    if hras and not stats['loaded_callins']:
        errors.append('indirect CALL cold layout was not recovered')
    if require_cold and not stats['cold_ranges']:
        errors.append('outlined cold tail was not recovered')
    if stats['cold_owners'] != stats['cold_ranges']:
        errors.append('not every cold tail resolved to its owning TB')
print('TU_MEMBER_RESULT ' + json.dumps(dict(phase=phase, hras=hras,
                                           stats=stats, errors=errors)))
gdb.execute('quit %d' % bool(errors))
