"""Execute an AOT-restored cold miss, its helper, and the signal exit."""
import os
import gdb

low = int(os.environ['TU_COLD_BEGIN'], 0)
high = int(os.environ['TU_COLD_END'], 0)
IR1_TYPE_RET = 4
checked = False
armed = False
helper_checked = False
restored_rets = set()


def word(address):
    return int.from_bytes(gdb.selected_inferior().read_memory(address, 4),
                          'little')


def branch_target(address):
    insn = word(address)
    assert insn >> 26 == 0x14, 'local miss slot must remain one B'
    off = ((insn >> 10) & 0xffff) << 2 | (insn & 0x3ff) << 18
    if off & (1 << 27):
        off -= 1 << 28
    return address + off


class Helper(gdb.Breakpoint):
    def __init__(self, low, high):
        self.low, self.high = low, high
        super().__init__('kzt_get_alternate_pc', internal=True, temporary=True)

    def stop(self):
        global helper_checked
        self.enabled = False
        caller = int(gdb.parse_and_eval('$r1')) - 4
        assert self.low <= caller < self.high, (
            'helper called outside moved tail')
        assert word(caller) >> 26 == 0x13, 'helper caller is not the moved JIRL'
        helper_checked = True
        print('AOT moved KZT helper relocation executed PASS')
        return False


class Cold(gdb.Breakpoint):
    def __init__(self, address, env, jirl, original):
        self.env, self.jirl, self.original = env, jirl, original
        super().__init__('*%d' % address, internal=True, temporary=True)

    def stop(self):
        global checked
        self.enabled = False
        assert word(self.jirl) == 0x03400000, 'unlink did not replace JIRL'
        gdb.execute('call (void)link_indirect_jmp((CPUArchState *)%d)' %
                    self.env, to_string=True)
        assert word(self.jirl) == self.original, 'relink did not restore JIRL'
        checked = True
        cold = int(gdb.parse_and_eval('$pc'))
        guard = next((address for address in range(cold, cold + 32, 4)
                      if word(address) >> 26 == 0x1a), None)
        assert guard is not None, 'missing KZT address guard'
        # This guest's low target would normally skip the helper. Bypass only
        # its guard to execute the relocated helper load and call; the helper
        # itself returns low targets unchanged with KZT either off or on.
        Helper(guard + 4, guard + 128)
        gdb.execute('set $pc = %d' % (guard + 4), to_string=True)
        print('AOT restored cold tail executed after signal unlink/relink PASS')
        return False


class Hit(gdb.Breakpoint):
    def __init__(self, address, tb, cold):
        self.tb, self.cold, self.jirl = tb, cold, address
        # Break on the preceding target load, so GDB does not shadow JIRL.
        super().__init__('*%d' % (address - 4), internal=True, temporary=True)

    def stop(self):
        self.enabled = False
        env = int(gdb.parse_and_eval('(CPUArchState *)lsenv->cpu_state'))
        context = int(gdb.parse_and_eval('calloc(1, sizeof(ucontext_t))'))
        assert context, 'cannot allocate signal context'
        jirl = self.jirl
        original = word(jirl)
        assert original >> 26 == 0x13, 'recorded lookup slot is not JIRL'
        pc_field = '((ucontext_t *)%d)->uc_mcontext.__pc' % context
        gdb.execute('set %s = %d' % (pc_field, jirl), to_string=True)
        gdb.execute('call (void)unlink_indirect_jmp((CPUArchState *)%d, '
                    '(TranslationBlock *)%d, (ucontext_t *)%d)' %
                    (env, self.tb, context), to_string=True)
        assert int(gdb.parse_and_eval(pc_field)) == jirl
        assert word(jirl) == 0x03400000
        gdb.execute('call (void)free((void *)%d)' % context, to_string=True)
        Cold(self.cold, env, jirl, original)
        return False


class Relocate(gdb.Breakpoint):
    def stop(self):
        if armed:
            return False
        # At function entry, LoongArch a0/a1 hold the TB and its AOT record.
        tb_address = int(gdb.parse_and_eval('$r4'))
        pc = int(gdb.parse_and_eval('((TranslationBlock *)$r4)->pc'))
        if low <= pc < high:
            kind = int(gdb.parse_and_eval(
                '((struct aot_tb *)$r5)->last_ir1_type'))
            # Address reuse must not retain the classification of an old TB.
            restored_rets.discard(tb_address)
            if kind == IR1_TYPE_RET:
                restored_rets.add(tb_address)
        return False


class Register(gdb.Breakpoint):
    def stop(self):
        global armed
        if armed:
            return False
        try:
            # aot_tb_register() always receives the block in LoongArch a0 ($r4).
            # Read the argument instead of an optimized-out caller local.
            tb_address = int(gdb.parse_and_eval('$r4'))
            tb = gdb.parse_and_eval(
                '(TranslationBlock *)%d' % tb_address).dereference()
            pc = int(tb['pc'])
        except gdb.error:
            return False
        if not low <= pc < high:
            return False
        assert int(tb['bool_flags']) & 8, 'must be a restored AOT TB'
        if not int(tb['bool_flags']) & 0x80:
            return False
        if tb_address not in restored_rets:
            return False
        tc, size = int(tb['tc']['ptr']), int(tb['tc']['size'])
        slot = tc + size - 4
        cold = branch_target(slot)
        if cold <= tc + size or cold - tc > 262144:
            return False
        for index in range(2):
            assert int(tb['jmp_reset_offset'][index]) == 0xffff, \
                'cold layout must not add reset slots'
        lookup = tc + int(tb['jmp_indirect'])
        # Use actual code rather than importing a fixed hash/JIRL offset.
        jirl = next((address for address in range(lookup, slot, 4)
                     if word(address) >> 26 == 0x13), None)
        assert jirl is not None and jirl + 4 == slot
        assert branch_target(slot) == cold
        Hit(jirl, tb_address, cold)
        armed = True
        print('restored indirect miss layout pc=%#x size=%d cold=%#x' %
              (int(tb['pc']), size, cold))
        return False


gdb.execute('set pagination off')
gdb.execute('set confirm off')
gdb.execute('set python print-stack full')
gdb.execute('handle SIGILL nostop noprint pass')
gdb.execute('handle SIGSEGV nostop noprint pass')
Relocate('*aot_do_tb_reloc', internal=True)
Register('*aot_tb_register', internal=True)
gdb.execute('run')
code = gdb.parse_and_eval('$_exitcode')
assert code.type.code != gdb.TYPE_CODE_VOID and int(code) == 0
assert armed and checked, 'intended restored cold exit was not executed'
assert helper_checked, 'moved helper relocation was not executed'
gdb.execute('quit 0')
