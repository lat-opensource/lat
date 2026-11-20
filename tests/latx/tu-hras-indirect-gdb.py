"""Check the indirect-call RA slot, cold miss redirect, and signal relink."""
import gdb

checked = False


def word(address):
    return int.from_bytes(gdb.selected_inferior().read_memory(address, 4),
                          'little')


def signed16(value):
    return value - (1 << 16) if value & (1 << 15) else value


def signed28(value):
    return value - (1 << 28) if value & (1 << 27) else value


def branch_target(address, insn):
    offset = ((insn >> 10) & 0xffff) << 2 | (insn & 0x3ff) << 18
    return address + signed28(offset)


def simulate_signal(env, tb_address, tc, bne_off, jirl_off, miss_off, pc):
    jirl = word(tc + jirl_off)
    context = int(gdb.parse_and_eval(
        '(void *)calloc(1, sizeof(ucontext_t))'))
    assert context, 'cannot allocate signal context'
    pc_field = '((ucontext_t *)%d)->uc_mcontext.__pc' % context
    gdb.execute('set %s = %d' % (pc_field, pc), to_string=True)
    gdb.execute('call (void)unlink_indirect_jmp((CPUArchState *)%d, '
                '(TranslationBlock *)%d, (ucontext_t *)%d)' %
                (env, tb_address, context), to_string=True)
    assert int(gdb.parse_and_eval(pc_field)) == pc
    branch = word(tc + jirl_off)
    assert branch >> 26 == 0x14
    assert branch_target(tc + jirl_off, branch) == tc + miss_off
    saved = ('((CPUArchState *)%d)->insn_save[1]' % env)
    assert int(gdb.parse_and_eval(saved)) == jirl
    # Another vCPU may observe the same unlinked slot before relink.
    gdb.execute('call (void)unlink_indirect_jmp((CPUArchState *)%d, '
                '(TranslationBlock *)%d, (ucontext_t *)%d)' %
                (env, tb_address, context), to_string=True)
    assert int(gdb.parse_and_eval(saved)) == jirl
    assert word(tc + jirl_off) == branch
    gdb.execute('call (void)link_indirect_jmp((CPUArchState *)%d)' % env,
                to_string=True)
    assert word(tc + jirl_off) == jirl
    gdb.execute('call (void)free((void *)%d)' % context, to_string=True)


def verify_layout(tb_address, expected_next, next_tb_address):
    tb = gdb.parse_and_eval(
        '(TranslationBlock *)%d' % tb_address).dereference()
    tc = int(tb['tc']['ptr'])
    size = int(tb['tc']['size'])
    jirl_off = int(tb['jmp_target_arg'][1])
    miss_off = int(tb['jmp_target_arg'][0])
    bne_off = int(tb['jmp_reset_offset'][1])
    jirl = word(tc + jirl_off)
    bne = word(tc + bne_off)
    bne_target = tc + bne_off + (signed16((bne >> 10) & 0xffff) << 2)

    assert int(tb['lazypc'][0]) == 0x4852434900000001
    assert int(tb['jmp_reset_offset'][0]) == 0xffff
    assert jirl_off + 4 == size
    assert jirl >> 26 == 0x13 and (jirl & 0x1f) == 1
    assert (jirl & 0x03fffc00) == 0
    assert bne_target == tc + miss_off
    assert miss_off >= size and (miss_off & 3) == 0

    assert expected_next == tc + size, (
        'non-adjacent continuation', int(tb['pc']), int(tb['tc']['size']),
        tc, expected_next, jirl_off, int(tb['jmp_target_arg'][0]))
    next_tb = gdb.parse_and_eval(
        '(TranslationBlock *)%d' % next_tb_address).dereference()
    assert int(next_tb['tc']['ptr']) == tc + size
    continuation = int(tb['pc']) + (int(tb['lazypc'][1]) & 0xffffffff)
    assert int(next_tb['pc']) == continuation, (
        'wrong adjacent continuation', int(next_tb['pc']), continuation,
        int(tb['pc']), int(tb['lazypc'][1]))

    env = int(gdb.parse_and_eval('(CPUArchState *)lsenv->cpu_state'))
    # While unlinked, every executor must branch to the cold dispatcher.
    for pc in (tc + bne_off, tc + bne_off + 4, tc + jirl_off):
        simulate_signal(env, tb_address, tc, bne_off, jirl_off, miss_off, pc)
    # Earlier guest PCs are preserved; their eventual CALL branches cold.
    simulate_signal(env, tb_address, tc, bne_off, jirl_off, miss_off, tc)


class FinishCheck(gdb.FinishBreakpoint):
    def __init__(self, tb_address, next_address, next_tb_address):
        super().__init__(internal=True)
        self.tb_address = tb_address
        self.next_address = next_address
        self.next_tb_address = next_tb_address

    def stop(self):
        global checked
        verify_layout(self.tb_address, self.next_address,
                      self.next_tb_address)
        checked = True
        print('indirect HRAS layout and parallel-safe signal PASS')
        return False


class Start(gdb.Breakpoint):
    def stop(self):
        if checked:
            return False
        tb = gdb.parse_and_eval('tb')
        s_data = tb.dereference()['s_data'].dereference()
        callin = int(gdb.parse_and_eval('IR1_TYPE_CALLIN'))
        if int(s_data['last_ir1_type']) == callin:
            FinishCheck(int(tb),
                        int(gdb.parse_and_eval('(uintptr_t)next->tc.ptr')),
                        int(gdb.parse_and_eval('(uintptr_t)next')))
        return False


Start('hras_callin_activate_layout', internal=True)
gdb.execute('set pagination off')
gdb.execute('set python print-stack full')
gdb.execute('set confirm off')
gdb.execute('set follow-fork-mode child')
gdb.execute('set detach-on-fork on')
gdb.execute('handle SIGILL nostop noprint pass')
gdb.execute('run')
if not checked or int(gdb.parse_and_eval('$_exitcode')) != 0:
    gdb.execute('quit 1')
gdb.execute('quit 0')
