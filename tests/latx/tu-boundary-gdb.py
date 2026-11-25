"""GDB runtime gate; use with layout JSON built from this binary's headers."""
import gdb
import json
import os
import re

with open(os.environ['TU_LAYOUT']) as f:
    layout = json.load(f)
stats = dict(tus=0, multipage=0, internal_b=0, external_patch=0,
             external_unlink=0,
             patches=0, internal_patch=0, exact_loads=0, page_loads=0,
             same_page=0, nonanchor=0, rejected_keys=0, max_span_pages=0,
             max_dependency_pages=0, semantic_link_calls=0,
             pending_link_enqueues=0, pending_link_resolves=0,
             unconditional_jump_links=0,
             unconditional_jump_patches=0, host_pc_lookups=0)
errors = []
request = None
loaded_pages = set()
strict = os.environ.get('LATX_AOT') == '3'
guest_mode = os.environ['TU_GUEST_MODE']
jump_tb_pc = int(os.environ.get('TU_JMP_PC', '0'), 0)

def word(addr, size=8):
    data = gdb.selected_inferior().read_memory(addr, size)
    return int.from_bytes(data, 'little')

def arg(n):
    return int(gdb.parse_and_eval('$r%d' % (4 + n)))

def is_tu(tb):
    return bool(word(tb + layout['flags'], 2) & layout['tu_bit'])

def root(tb):
    if not strict:
        return tb
    start = tb
    members = set()
    while tb and tb not in members:
        members.add(tb)
        tb = word(tb + layout['next'])
    if tb != start:
        errors.append('broken runtime TU member ring')
    return min(members)

external_links = {}
pending_host_pc = set()


def check_host_pc_lookup():
    # Members are queued before registration. Check only at a later syscall,
    # after the real publisher has inserted the complete TU into its index.
    pending = sorted(pending_host_pc)
    pending_host_pc.clear()
    for tb in pending:
        if word(tb + layout['cflags'], 4) & layout['invalid_bit']:
            continue
        start = word(tb + layout['ptr'])
        size = word(tb + layout['size'], 4)
        probes = set(range(start, start + size - 1, 32))
        probes.update((start, start + size - 4))
        for pc in sorted(probes):
            found = int(gdb.parse_and_eval(
                '(TranslationBlock *)tcg_tb_lookup(%d)' % pc))
            stats['host_pc_lookups'] += 1
            if found != tb:
                errors.append('host-PC lookup mismatch at %#x: %#x != %#x' %
                              (pc, found, tb))

class Members(gdb.Breakpoint):
    def stop(self):
        if not strict:
            return False
        members = [word(arg(0) + i * 8) for i in range(arg(1))]
        pending_host_pc.update(members)
        pcs = [word(t + layout['pc'], layout['pc_size']) for t in members]
        if len(pcs) != len(set(pcs)):
            errors.append('duplicate member PC in one TU')
        if strict:
            if request is None or request['pc'] not in pcs:
                errors.append('TU does not contain the requested entry')
            else:
                request['count'] += 1
                page = request['pc'] >> 12
                stats['same_page'] += page in loaded_pages
                loaded_pages.add(page)
                stats['nonanchor'] += request['pc'] != min(pcs)
        stats['tus'] += 1
        pages = set()
        for tb, pc in zip(members, pcs):
            size = word(tb + layout['guest_size'], 2)
            pages.update(range(pc >> 12, ((pc + size - 1) >> 12) + 1))
        page_span = max(pages) - min(pages) + 1
        stats['multipage'] += len(pages) > 1
        stats['max_span_pages'] = max(stats['max_span_pages'], page_span)
        stats['max_dependency_pages'] = max(
            stats['max_dependency_pages'], len(pages))
        ranges = [(word(t + layout['ptr']), word(t + layout['size'], 4))
                  for t in members]
        for tb in members:
            if not word(tb + layout['flags'], 2) & layout['jmp_bit']:
                continue
            for n in range(2):
                off = word(tb + layout['jump'] + n * 2, 2)
                if off == 65535:
                    continue
                addr = word(tb + layout['ptr']) + off
                insn = word(addr, 4)
                if insn >> 26 == 0x14:  # LoongArch b, not data/search words
                    imm = ((insn & 1023) << 16) | ((insn >> 10) & 65535)
                    if imm & (1 << 25):
                        imm -= 1 << 26
                    target = addr + (imm << 2)
                    if not any(p <= target < p + s for p, s in ranges):
                        errors.append('internal b escapes TU: %#x' % target)
                    stats['internal_b'] += 1
        print('MEMBERS ' + ' '.join(hex(p) for p in pcs))
        return False

class Loaded:
    def stop(self):
        global request
        drain_checks()
        if request['count'] > 1:
            errors.append('one request loaded multiple TUs')
        request = None
        return False

class LoadTU(gdb.Breakpoint):
    def stop(self):
        global request
        if strict and not stats['rejected_keys']:
            pc, base, flags, cflags = [arg(i) for i in range(4)]
            # Entry is reached under mmap_lock; probe incompatible keys before
            # any publication.  Disable only this breakpoint during the calls.
            self.enabled = False
            try:
                for bad_base, bad_flags, bad_cflags in (
                        (1, flags, cflags),
                        (base, flags ^ 1, cflags),
                        (base, flags, cflags ^ 0x8000)):
                    result = int(gdb.parse_and_eval(
                        '(int)load_aot_tu(%d, %d, %d, %d)' %
                        (pc, bad_base, bad_flags, bad_cflags)))
                    if result:
                        errors.append('incompatible TB key was accepted')
                    stats['rejected_keys'] += 1
            finally:
                self.enabled = True
        stats['exact_loads'] += 1
        if not strict:
            errors.append('exact loader reached with mode disabled')
        request = dict(pc=arg(0), count=0)
        return_checks['load_aot_tu'].append(Loaded())
        return False

class SemanticLink(gdb.Breakpoint):
    def stop(self):
        stats['semantic_link_calls'] += 1
        if (jump_tb_pc and word(arg(1) + layout['pc'],
                                layout['pc_size']) == jump_tb_pc):
            if arg(5) != 1:
                errors.append('unconditional JMP used exit slot %d' % arg(5))
            stats['unconditional_jump_links'] += 1
        return False

class PendingLink(gdb.Breakpoint):
    def stop(self):
        queue = gdb.parse_and_eval('queue')
        if not int(queue):
            return False
        stats['pending_link_enqueues'] += 1
        return False

class PendingResolve(gdb.Breakpoint):
    def stop(self):
        stats['pending_link_resolves'] += 1
        return False

class LoadPage(gdb.Breakpoint):
    def stop(self):
        stats['page_loads'] += 1
        if strict:
            errors.append('page loader reached in exact TU mode')
        return False

class Patch(gdb.Breakpoint):
    def stop(self):
        drain_checks()
        a, b = arg(0), arg(2)
        stats['patches'] += 1
        if word(b + layout['cflags'], 4) & layout['invalid_bit']:
            errors.append('runtime patch targets an invalid TB')
        external = ((is_tu(a) or is_tu(b)) and
                    (not is_tu(a) or not is_tu(b) or root(a) != root(b)))
        if external:
            stats['external_patch'] += 1
        elif is_tu(a) and is_tu(b):
            stats['internal_patch'] += 1
        completed_checks.append(Patched(a, arg(1), b, external))
        return False

class Invalidated:
    def __init__(self, target, links):
        self.target, self.links = target, links

    def stop(self):
        for source, n in self.links:
            dest = word(source + layout['dest'] + n * 8)
            if dest == self.target:
                errors.append('invalidation retained an external incoming link')
            else:
                stats['external_unlink'] += 1
        return False

class Invalidate(gdb.Breakpoint):
    def stop(self):
        drain_checks()
        target = arg(0)
        links = {(source, n) for source, n in external_links.pop(target, set())
                 if word(source + layout['dest'] + n * 8) == target}
        if links:
            completed_checks.append(Invalidated(target, links))
        return False

class Patched:
    def __init__(self, a, n, b, external):
        self.addr = word(a + layout['ptr']) + word(a + layout['patch'] + n * 8)
        self.target = word(b + layout['ptr'])
        self.source, self.n, self.dest, self.external = a, n, b, external

    def stop(self):
        if (word(self.source + layout['dest'] + self.n * 8) & ~1) != self.dest:
            errors.append('runtime patch missing incoming-link registration')
        if self.external:
            external_links.setdefault(self.dest, set()).add(
                (self.source, self.n))
        if (jump_tb_pc and self.n == 1 and
                word(self.source + layout['pc'],
                     layout['pc_size']) == jump_tb_pc):
            stats['unconditional_jump_patches'] += 1
        insn = word(self.addr, 4)
        if insn >> 26 != 0x14:
            # Lazy/far-link sequences need not use a single near b.
            return False
        imm = ((insn & 1023) << 16) | ((insn >> 10) & 65535)
        if imm & (1 << 25):
            imm -= 1 << 26
        if self.addr + (imm << 2) != self.target:
            errors.append('internal b has wrong destination')
        stats['internal_b'] += 1
        return False

# GDB's LoongArch FinishBreakpoint can lose frame identity with optimized
# callers. Observe each real return instruction after the complete operation.
return_checks = {'load_aot_tu': []}
completed_checks = []

def drain_checks():
    pending = completed_checks[:]
    completed_checks.clear()
    for check in pending:
        check.stop()

class Completed(gdb.Breakpoint):
    def stop(self):
        drain_checks()
        check_host_pc_lookup()
        return False

class FunctionReturn(gdb.Breakpoint):
    def __init__(self, address, name):
        super().__init__('*' + address, internal=True)
        self.name = name

    def stop(self):
        pending = return_checks[self.name][:]
        return_checks[self.name].clear()
        for check in pending:
            check.stop()
        return False

for name in return_checks:
    text = gdb.execute('disassemble ' + name, to_string=True)
    addresses = re.findall(r'^\s*(0x[0-9a-f]+).*jirl\s+\$r0,\$r1,0$',
                           text, re.M)
    if not addresses:
        raise RuntimeError('missing return instruction: ' + name)
    for address in addresses:
        FunctionReturn(address, name)

gdb.execute('set pagination off')
gdb.execute('set confirm off')
gdb.execute('handle SIGSEGV nostop noprint pass')
gdb.execute('handle SIGBUS nostop noprint pass')
Members('*tu_link_members', internal=True)
# Release builds retain this static function with a compiler clone suffix.
functions = gdb.execute('info functions do_tb_phys_invalidate', to_string=True)
invalidate_symbols = re.findall(
    r'^(0x[0-9a-f]+)\s+do_tb_phys_invalidate', functions, re.M)
if invalidate_symbols:
    for symbol in invalidate_symbols:
        Invalidate('*' + symbol, internal=True)
else:
    # With debug info GDB prints a source declaration rather than an address.
    Invalidate('*do_tb_phys_invalidate', internal=True)
Patch('*latx_tb_set_jmp_target', internal=True)
LoadTU('*load_aot_tu', internal=True)
# Stable loader inlines load_page; load_aot is its reachable outer entry.
LoadPage('*load_aot', internal=True)
SemanticLink('*aot_link_direct_target', internal=True)
source = os.path.join(os.path.dirname(__file__),
                      '../../target/i386/latx/sbt/aot_link_seg.c')
with open(source) as stream:
    enqueue_line = next(i for i, line in enumerate(stream, 1)
                        if 'g_queue_push_tail(queue, link)' in line)
PendingLink('aot_link_seg.c:%d' % enqueue_line, internal=True)
PendingResolve('*aot_resolve_pending_links', internal=True)
Completed('*do_syscall', internal=True)
gdb.execute('run')
if int(gdb.parse_and_eval('$_exitcode')) != 0:
    errors.append('guest returned failure')
if os.environ.get('LATX_AOT') in ('1', '2', '3'):
    required = ['patches'] if not strict else ['internal_patch']
    if strict:
        required += ['multipage', 'external_patch', 'exact_loads', 'same_page',
                     'rejected_keys', 'semantic_link_calls',
                     'pending_link_enqueues', 'pending_link_resolves',
                     'unconditional_jump_links',
                     'unconditional_jump_patches', 'host_pc_lookups']
    else:
        required += ['page_loads']
    for key in required:
        if not stats[key]:
            errors.append('missing coverage: ' + key)
    if (strict and guest_mode in ('1', '2') and
            not stats['external_unlink']):
        errors.append('external incoming-link invalidation was not exercised')
    expected_span, expected_pages = 64, 16
    if strict and stats['max_span_pages'] != expected_span:
        errors.append('expected TU span %d, got %d' %
                      (expected_span, stats['max_span_pages']))
    if strict and stats['max_dependency_pages'] != expected_pages:
        errors.append('expected dependency pages %d, got %d' %
                      (expected_pages, stats['max_dependency_pages']))
    if (strict and guest_mode == '4' and
            not stats['nonanchor']):
        errors.append('non-anchor entry did not recover its TU')
elif not stats['patches']:
    errors.append('ordinary JIT chaining not exercised')
print('BOUNDARY', stats, errors)
gdb.execute('quit %d' % bool(errors))
