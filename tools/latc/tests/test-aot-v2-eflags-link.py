#!/usr/bin/env python3
"""Compare live/dead flag successors with an x86-native reference output."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys


def main():
    if len(sys.argv) != 8:
        raise SystemExit('usage: test-aot-v2-eflags-link.py LATC RUNNER '
                         'RUNTIME ROOTFS GUEST EXPECTED WORK')
    latc, runner, runtime, rootfs, guest, expected, work = sys.argv[1:]
    work = Path(work).resolve()
    work.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('LATX_', 'LATC_', 'LAT_LD_'))}
    env.update(LD_LIBRARY_PATH=runtime, LAT_LD_PREFIX=rootfs,
               LATX_AOT='0', LATC_DISABLE_PRETRANSLATE='1')
    rows = []

    def run(name, command, additions=None):
        selected = dict(env, **(additions or {}))
        if name == 'compile':
            selected.pop('LATX_AOT')
            selected.pop('LATC_DISABLE_PRETRANSLATE')
        with (work / (name + '.out')).open('wb') as out:
            with (work / (name + '.err')).open('wb') as err:
                result = subprocess.run(command, env=selected, stdout=out,
                                        stderr=err, timeout=180)
        rows.append(dict(name=name, command=command, rc=result.returncode,
                         env={k: v for k, v in selected.items()
                              if k.startswith(('LATX_', 'LATC_', 'LAT_LD_'))
                              or k == 'LD_LIBRARY_PATH'}))
        (work / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        assert result.returncode == 0, name

    command = [runner, '-L', rootfs, guest]
    run('jit', command)
    native = work / 'image.native'
    module = work / 'module.so'
    run('compile', [latc, 'compile-module', guest, '-o', str(module),
                    '--runner', runner, '--runtime-dir', runtime],
        {'LATC_NATIVE_OUTPUT': str(native)})
    data = native.read_bytes()
    assert struct.unpack_from('<I', data, 8)[0] == 6
    offset, count = struct.unpack_from('<QQ', data, 72)
    records = list(struct.iter_unpack('<QQIIIHHIHHII',
                                     data[offset:offset + count * 48]))
    assert any(tb[5] or tb[6] for tb in records), 'missing NOP sites'
    assert any(tb[8] or tb[9] for tb in records), 'missing stub bypass sites'
    symbols = subprocess.check_output(['nm', '-n', str(module)], text=True)
    text_begin = next(int(line.split()[0], 16) for line in symbols.splitlines()
                      if line.endswith(' lat_aot_generated_text_begin'))
    functions = []
    guest_symbols = subprocess.check_output(['nm', '-S', guest], text=True)
    for line in guest_symbols.splitlines():
        fields = line.split()
        if (len(fields) == 4 and
                fields[3].split('.')[0].endswith(('_live', '_dead'))):
            functions.append((int(fields[0], 16), int(fields[1], 16),
                              fields[3].split('.')[0]))
    linked = module.read_bytes()
    native_code = struct.unpack_from('<Q', data, 56)[0]
    conditional_proof = []
    indirect_proof = []
    proof = []
    for tb in records:
        for start, size, name in functions:
            if not start <= tb[0] < start + size:
                continue
            if tb[11]:
                site = tb[11] - 1
                assert site % 4 == 0 and site + 152 < tb[2], name
                original = struct.unpack_from(
                    '<I', data, native_code + tb[1] + site)[0]
                final = struct.unpack_from(
                    '<I', linked, text_begin + tb[1] + site)[0]
                assert original == 0x004542ab, name
                assert final != original, name
                indirect_proof.append(dict(function=name, guest_pc=hex(tb[0]),
                                            offset=site, linked=hex(final)))
            if tb[10]:
                site = tb[10] - 1
                assert site % 4 == 0 and site + 4 <= tb[2], name
                original = struct.unpack_from(
                    '<I', data, native_code + tb[1] + site)[0]
                assert 0x16 <= original >> 26 <= 0x1b, name
                displacement = (original >> 10) & 0xffff
                if displacement & 0x8000:
                    displacement -= 0x10000
                target = site + displacement * 4
                assert 0 <= target <= tb[2] - 4, name
                final = struct.unpack_from(
                    '<I', linked, text_begin + tb[1] + site)[0]
                assert original & 0xfc0003ff == final & 0xfc0003ff, name
                conditional_proof.append(dict(
                    function=name, guest_pc=hex(tb[0]), offset=site,
                    original=hex(original), linked=hex(final)))
            for edge in range(2):
                sites = [('nop', tb[5 + edge]), ('stub', tb[8 + edge])]
                for kind, encoded in sites:
                    if encoded:
                        site = text_begin + tb[1] + encoded - 1
                        word = struct.unpack_from('<I', linked, site)[0]
                        changed = (word == 0x03400000 if kind == 'nop'
                                   else word & 0xfc000000 == 0x50000000)
                        proof.append(dict(function=name, edge=edge, kind=kind,
                                          changed=changed,
                                          instruction=hex(word)))
    (work / 'patch-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    (work / 'conditional-proof.json').write_text(
        json.dumps(conditional_proof, indent=2) + '\n')
    (work / 'indirect-proof.json').write_text(
        json.dumps(indirect_proof, indent=2) + '\n')
    assert {'cmp32_live', 'cmp32_dead'} <= {
        p['function'] for p in indirect_proof}, 'missing indirect producer sites'
    # Require producer coverage in fixture code, independent of libc coverage.
    exported = {p['function'] for p in conditional_proof}
    for family in ['cmp8', 'cmp16', 'cmp32', 'cmp64',
                   'test8', 'test16', 'test32', 'test64']:
        for suffix in ['live', 'dead']:
            assert family + '_' + suffix in exported, (family, suffix)
    required = {
        'nop': ['cmp8', 'cmp16', 'cmp32', 'cmp64',
                'test8', 'test16', 'test32', 'test64', 'sub64',
                'bt64', 'btxx', 'cmpxx', 'testxx', 'andjne', 'shrjne'],
        'stub': ['comisd', 'ucomisd', 'comiss', 'ucomiss'],
    }
    for kind, families in required.items():
        for family in families:
            for suffix, changed in [('live', False), ('dead', True)]:
                name = family + '_' + suffix
                sites = [p for p in proof
                         if p['function'] == name and p['kind'] == kind]
                assert {p['edge'] for p in sites} == {0, 1}, name
                assert all(p['changed'] == changed for p in sites), name
    cache = work / 'cache'
    cache.mkdir()
    digest = hashlib.sha256(Path(guest).read_bytes()).hexdigest()
    cached = cache / (digest + '.so')
    cached.write_bytes(module.read_bytes())
    cached.chmod(0o444)
    manifest = cache / (digest + '.current')
    manifest.write_text(
        json.dumps(dict(version=2, module=cached.name), separators=(',', ':')))
    manifest.chmod(0o444)
    additions = dict(LATX_AOT_V2_CACHE_DIR=str(cache), LATX_AOT_V2_STRICT='1',
                     LATC_STRICT_FILE_AOT='1', LATX_AOT_V2_REPORT='1')
    for name in ['cold-load', 'warm-1', 'warm-2']:
        run(name, command, additions)
    reference = Path(expected).read_bytes()
    for name in ['jit', 'cold-load', 'warm-1', 'warm-2']:
        assert (work / (name + '.out')).read_bytes() == reference, name
    print('test-aot-v2-eflags-link: PASS')


if __name__ == '__main__':
    main()
