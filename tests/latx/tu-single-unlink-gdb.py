"""Validate linked BL, reset B/cold stub, and signal relink on a real TU."""
import gdb

source = None
destination = None
checked = False


def arg(index):
    return int(gdb.parse_and_eval('$r%d' % (4 + index)))


def load(address):
    return int.from_bytes(gdb.selected_inferior().read_memory(address, 4),
                          'little')


def target(address):
    insn = load(address)
    immediate = ((insn & 1023) << 16) | ((insn >> 10) & 65535)
    if immediate & (1 << 25):
        immediate -= 1 << 26
    return address + (immediate << 2)


def block(pointer):
    return gdb.parse_and_eval('(TranslationBlock *)%d' % pointer).dereference()


class Link(gdb.Breakpoint):
    def stop(self):
        global source, destination
        tb = block(arg(0))
        marker = int(tb['lazypc'][1]) & 0xffffffff00000000
        if source is None and marker == 0x4852415400000000:
            source, destination = arg(0), arg(1)
        return False


class Check(gdb.Breakpoint):
    def stop(self):
        global checked
        if checked or source is None:
            return False
        tb, callee = block(source), block(destination)
        slot = int(tb['tc']['ptr']) + int(tb['jmp_target_arg'][0])
        cold = int(tb['tc']['ptr']) + int(tb['jmp_target_arg'][1])
        continuation = int(tb['tc']['ptr']) + int(tb['tc']['size'])
        linked = load(slot)
        assert linked >> 26 == 0x15, 'not a linked BL'
        assert target(slot) == int(callee['tc']['ptr']), 'BL target mismatch'
        assert slot + 4 == continuation, 'continuation not adjacent'
        following = load(continuation)
        unlink = 'call (void)unlink_direct_jmp((TranslationBlock *)%d)'
        gdb.execute(unlink % source, to_string=True)
        assert load(slot) >> 26 == 0x14, 'unlink did not install B'
        assert target(slot) == cold, 'unlink did not target cold exit'
        assert load(continuation) == following, 'unlink overwrote continuation'
        pcaddi = load(cold)
        assert (pcaddi & 0xfe00001f) == 0x18000004, 'cold exit lost a0'
        immediate = (pcaddi >> 5) & 0xfffff
        if immediate & (1 << 19):
            immediate -= 1 << 20
        assert cold + (immediate << 2) == slot + 4, 'cold source identity wrong'
        assert int(block(source)['jmp_dest'][0]) == destination
        gdb.execute('call (void)set_tb_relink_flag((TranslationBlock *)%d, 0)' %
                    source, to_string=True)
        # Use the out-of-line ABI entry; optimized tb_add_jump has clones.
        gdb.execute('call (void)tb_add_jump_aot((TranslationBlock *)%d, 0, '
                    '(TranslationBlock *)%d)' % (source, destination),
                    to_string=True)
        assert load(slot) == linked, 'relink did not restore BL'
        assert load(continuation) == following, 'relink overwrote continuation'
        checked = True
        print('single-slot BL -> B cold-exit -> BL signal relink PASS')
        return False


gdb.execute('set pagination off')
gdb.execute('set confirm off')
gdb.execute('handle SIGSEGV nostop noprint pass')
Link('*hras_link_call', internal=True)
Check('*do_syscall', internal=True)
gdb.execute('run')
if not checked or int(gdb.parse_and_eval('$_exitcode')) != 0:
    gdb.execute('quit 1')
gdb.execute('quit 0')
