# SPDX-License-Identifier: GPL-2.0-only
"""Check signal resume PCs while unlinking a real two-word HRAS CALL."""
import gdb

source = None
destination = None
checked = False


def arg(index):
    return int(gdb.parse_and_eval('$r%d' % (4 + index)))


def block(pointer):
    return gdb.parse_and_eval('(TranslationBlock *)%d' % pointer).dereference()


def word(address):
    return int.from_bytes(gdb.selected_inferior().read_memory(address, 4),
                          'little')


class Link(gdb.Breakpoint):
    def stop(self):
        global source, destination
        tb = block(arg(0))
        if (source is None and not int(tb['bool_flags']) & 0x40 and
                int(tb['lazypc'][1]) & 0xffffffff00000000 ==
                0x4852415300000000):
            source, destination = arg(0), arg(1)
        return False


class Check(gdb.Breakpoint):
    def stop(self):
        global checked
        if checked or source is None:
            return False
        tb = block(source)
        slot = int(tb['tc']['ptr']) + int(tb['jmp_target_arg'][0])
        if word(slot) != 0x03400000 or word(slot + 4) >> 26 != 0x15:
            return False
        env = arg(0)
        context = int(gdb.parse_and_eval(
            '(void *)calloc(1, sizeof(ucontext_t))'))
        assert context, 'cannot allocate signal context'
        pc = '((ucontext_t *)%d)->uc_mcontext.__pc' % context
        regs = '((ucontext_t *)%d)->uc_mcontext.__gregs' % context
        metadata = int(tb['lazypc'][1])
        linked = (word(slot), word(slot + 4))
        original = int(tb['lazylinkinst'][0])
        cases = [(0, True, False), (4, True, False),
                 (4, True, True), (4, False, False)]
        for offset, tagged, reset in cases:
            gdb.execute('set %s = %d' % (pc, slot + offset), to_string=True)
            gdb.execute('set %s[12] = 0' % regs, to_string=True)
            gdb.execute('set %s[3] = 0x12345000' % regs, to_string=True)
            if not tagged:
                gdb.execute('set ((TranslationBlock *)%d)->lazypc[1] = 0' %
                            source, to_string=True)
            if reset:
                # Another thread may have restored the pair already.
                gdb.execute('call (void)unlink_direct_jmp('
                            '(TranslationBlock *)%d)' % source, to_string=True)
            gdb.execute('call (void)tb_exit_to_qemu((CPUX86State *)%d, '
                        '(ucontext_t *)%d)' % (env, context), to_string=True)
            expected = slot if tagged else slot + offset
            assert int(gdb.parse_and_eval(pc)) == expected, (
                'signal resumed inside restored pair: '
                'offset=%d tagged=%s reset=%s' %
                (offset, tagged, reset))
            assert int(gdb.parse_and_eval(regs + '[12]')) == 0
            assert int(gdb.parse_and_eval(regs + '[3]')) == 0x12345000
            assert word(slot) == original & 0xffffffff
            assert word(slot + 4) == original >> 32
            assert int(block(source)['jmp_dest'][0]) == destination
            gdb.execute('set ((TranslationBlock *)%d)->lazypc[1] = %d' %
                        (source, metadata), to_string=True)
            gdb.execute('call (void)set_tb_relink_flag((TranslationBlock *)%d, '
                        '0)' % source, to_string=True)
            gdb.execute('call (void)tb_add_jump((TranslationBlock *)%d, 0, '
                        '(TranslationBlock *)%d)' % (source, destination),
                        to_string=True)
            assert (word(slot), word(slot + 4)) == linked
        gdb.execute('call (void)free((void *)%d)' % context, to_string=True)
        checked = True
        print('two-word HRAS signal resume and ordinary-pair control PASS')
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
