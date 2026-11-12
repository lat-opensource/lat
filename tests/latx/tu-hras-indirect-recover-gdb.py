"""Validate an indirect HRAS CALL restored from AOT and its signal relink."""
import gdb

checked = False
candidates = set()


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


def verify_candidate(tb_address):
    global checked
    if not int(gdb.parse_and_eval(
            'hras_callin_is_active((TranslationBlock *)%d)' % tb_address)):
        return False
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
    continuation = int(tb['pc']) + (int(tb['lazypc'][1]) & 0xffffffff)
    next_tb = int(gdb.parse_and_eval(
        '(TranslationBlock *)tcg_tb_lookup((uintptr_t)%d)' % (tc + size)))

    assert int(tb['lazypc'][0]) == 0x4852434900000001
    assert int(tb['jmp_reset_offset'][0]) == 0xffff
    assert jirl_off + 4 == size
    assert jirl >> 26 == 0x13 and (jirl & 0x1f) == 1
    assert (jirl & 0x03fffc00) == 0
    assert bne_target == tc + miss_off
    assert miss_off >= size and (miss_off & 3) == 0
    assert next_tb, 'recovered continuation TB is not adjacent'
    assert int(gdb.parse_and_eval(
        '((TranslationBlock *)%d)->pc' % next_tb)) == continuation

    env = int(gdb.parse_and_eval('(CPUArchState *)lsenv->cpu_state'))
    for pc in (tc + bne_off, tc + bne_off + 4, tc + jirl_off):
        simulate_signal(env, tb_address, tc, bne_off, jirl_off, miss_off, pc)
    simulate_signal(env, tb_address, tc, bne_off, jirl_off, miss_off, tc)
    checked = True
    print('recovered indirect HRAS layout and parallel-safe signal PASS')
    return True


class AfterLoad(gdb.FinishBreakpoint):
    def __init__(self):
        super().__init__(internal=True)

    def stop(self):
        for address in tuple(candidates):
            verify_candidate(address)
            if checked:
                return False
        return False


class Load(gdb.Breakpoint):
    def stop(self):
        if not checked:
            AfterLoad()
        return False


class Create(gdb.Breakpoint):
    def stop(self):
        record = gdb.parse_and_eval('p_aot_tb').dereference()
        if int(record['lazypc'][0]) == 0x4852434900000001:
            assert int(record['last_ir1_type']) == 6
            assert int(record['bool_flags']) & 0x80
        return False


class Register(gdb.Breakpoint):
    def stop(self):
        tb = int(gdb.parse_and_eval('(uintptr_t)tb'))
        if not checked and int(gdb.parse_and_eval(
                'hras_callin_is_active((TranslationBlock *)%d)' % tb)):
            candidates.add(tb)
        return False


Create('creat_tb', internal=True)
Register('aot_tb_register', internal=True)
Load('load_aot_tu', internal=True)
Load('load_aot', internal=True)
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
