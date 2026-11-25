"""Check late PC-thunk inlining and its recovered continuation exit."""
import gdb
import json
import os

caller = int(os.environ['TU_THUNK_CALLER'], 0)
continuation = int(os.environ['TU_THUNK_CONTINUATION'], 0)
thunk = int(os.environ['TU_THUNK_TARGET'], 0)
phase = os.environ.get('TU_THUNK_PHASE', 'generate')
external = os.environ.get('TU_THUNK_EXTERNAL', '1') == '1'
stats = dict(cleared=0, discovered=0, held_predecessor=0, terminal=0,
             external=0, known_at_codegen=0, inlined=0,
             ordinary_call=0, continuation_exit=0, recovered=0,
             linked=0, executed=0, jit=0)
errors = []
prior_continuation = None


def current_caller():
    tb = gdb.parse_and_eval(
        '(TranslationBlock *)((TRANSLATION_DATA *)lsenv->tr_data)->curr_tb')
    return tb if int(tb) and int(tb['pc']) == caller else None


class Clear(gdb.Breakpoint):
    def stop(self):
        gdb.execute('call (void)g_hash_table_remove_all(ht_pc_thunk)',
                    to_string=True)
        stats['cleared'] += 1
        self.enabled = False
        return False


class Discover(gdb.Breakpoint):
    def stop(self):
        if (int(gdb.parse_and_eval('in_pre_translate')) and
                int(gdb.parse_and_eval('thunk_addr')) == thunk):
            stats['discovered'] += 1
        return False


class HoldPredecessor(gdb.Breakpoint):
    def stop(self):
        # Model an admission boundary without changing CALL decoding or exit
        # generation. Otherwise predecessor completion pulls this CALL into
        # the continuation's first TU and hides the external-continuation case.
        tb = gdb.parse_and_eval('tb')
        if (int(gdb.parse_and_eval('in_pre_translate')) and
                int(tb['pc']) == continuation):
            gdb.execute('return', to_string=True)
            stats['held_predecessor'] += 1
        return False


class Unit(gdb.Breakpoint):
    def stop(self):
        global prior_continuation
        if not int(gdb.parse_and_eval('in_pre_translate')):
            return False
        count = int(gdb.parse_and_eval('tb_num_in_tu'))
        blocks = gdb.parse_and_eval('tb_list')
        for i in range(count):
            tb = blocks[i]
            if int(tb['pc']) == caller:
                stats['terminal'] += (int(tb['icount']) == 1 and
                    int(tb['s_data']['last_ir1_type']) ==
                    int(gdb.parse_and_eval('IR1_TYPE_CALL')))
                stats['external'] += not int(tb['s_data']['next_tb'][0])
                stats['known_at_codegen'] += bool(stats['discovered'])
                if external:
                    if any(int(blocks[j]['pc']) == continuation
                           for j in range(count)):
                        errors.append('continuation is still in the caller TU')
                    if (prior_continuation is None or
                            not int(prior_continuation['tc']['ptr'])):
                        errors.append('continuation was not already translated')
        for i in range(count):
            if int(blocks[i]['pc']) == continuation:
                prior_continuation = blocks[i]
        return False


class Instruction(gdb.Breakpoint):
    def __init__(self, symbol, counter):
        super().__init__(symbol, internal=True)
        self.counter = counter

    def stop(self):
        if (int(gdb.parse_and_eval('in_pre_translate')) and
                current_caller() is not None):
            stats[self.counter] += 1
        return False


class Exit(gdb.Breakpoint):
    def stop(self):
        if not int(gdb.parse_and_eval('in_pre_translate')):
            return False
        tb = current_caller()
        if tb is not None:
            if (int(tb['lazypc'][0]) == continuation - caller and
                    int(tb['jmp_reset_offset'][0]) != 65535):
                stats['continuation_exit'] += 1
        return False


class Execute(gdb.Breakpoint):
    def stop(self):
        stats['executed'] += 1
        self.enabled = False
        return False


class Recover(gdb.Breakpoint):
    def stop(self):
        tb = gdb.parse_and_eval('tb')
        if int(tb['pc']) != caller:
            return False
        stats['recovered'] += 1
        if external and (int(tb['lazypc'][0]) != continuation - caller or
                         int(tb['jmp_reset_offset'][0]) == 65535):
            errors.append('recovered exit does not target the continuation')
        Execute('*%#x' % int(tb['tc']['ptr']), internal=True)
        return False


class Link(gdb.Breakpoint):
    def stop(self):
        tb = gdb.parse_and_eval('tb')
        if not external or int(tb['pc']) != caller:
            return False
        target = gdb.parse_and_eval('next_tb')
        if (int(gdb.parse_and_eval('n')) != 0 or
                int(target['pc']) != continuation):
            errors.append('thunk continuation linked to the wrong target')
        else:
            stats['linked'] += 1
        return False


class Jit(gdb.Breakpoint):
    def stop(self):
        if int(gdb.parse_and_eval('pc')) == caller:
            stats['jit'] += 1
        return False


gdb.execute('set pagination off')
gdb.execute('set confirm off')
gdb.execute('handle SIGSEGV nostop noprint pass')
if phase == 'generate':
    gdb.execute('set follow-fork-mode child')
    Clear('translate_lib', internal=True)
    Discover('ht_pc_thunk_insert', internal=True)
    if external:
        HoldPredecessor('get_ts_fallthrough_predecessor', internal=True)
    Unit('translate_tu', internal=True)
    Instruction('translate_callthunk', 'inlined')
    Instruction('translate_call', 'ordinary_call')
    Exit('la_jump_epilogue', internal=True)
else:
    Recover('aot_tb_register', internal=True)
    Link('latx_tb_set_jmp_target', internal=True)
    Jit('tb_gen_code', internal=True)
gdb.execute('run')
if phase == 'generate':
    passed = all(stats[key] for key in
                 ('cleared', 'discovered', 'terminal',
                  'known_at_codegen', 'inlined')) and not stats['ordinary_call']
    if external:
        passed = passed and all(stats[key] for key in
                                ('held_predecessor', 'external',
                                 'continuation_exit'))
    else:
        passed = passed and not stats['external']
else:
    passed = all(stats[key] for key in
                 ('recovered', 'executed')) and not stats['jit']
    if external:
        passed = passed and stats['linked']
print('late-thunk', phase, json.dumps(stats, sort_keys=True), errors)
if not passed or errors or int(gdb.parse_and_eval('$_exitcode')) != 0:
    gdb.execute('quit 1')
print('late-thunk continuation ' + phase + ' PASS')
gdb.execute('quit 0')
