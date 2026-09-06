#!/usr/bin/env python3
"""Compile the actual NaN IR2 emitter and interpret its emitted instructions.

Run with Python 3 and CC (default cc); --revision HEAD tests the old emitter.
This is a bit-exact host model, not a replacement for LoongArch runtime tests.
Vector mask, shuffle, pick and select semantics follow the LoongArch ISA.
"""
import argparse
import json
import os
from pathlib import Path
import random
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = "target/i386/latx/translator/tr-avx.c"
MASK64 = (1 << 64) - 1


def extract(source, name):
    m = re.search(r"(?:static )?void " + name + r"\([^;]*?\)\s*\{", source)
    if not m:
        return ""
    end, depth = m.end(), 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[m.start():end]


def programs(source):
    names = ["lasx_fp_fix_nan_from_sources_lane",
             "lasx_fp_fix_packed_nan_from_sources",
             "lasx_fp_fix_vector_nan_from_sources",
             "lasx_fp_fix_nan_from_sources"]
    body = "\n".join(extract(source, name) for name in names)
    vector = ("lasx_fp_fix_vector_nan_from_sources" if extract(source, names[2])
              else "lasx_fp_fix_packed_nan_from_sources")
    header = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
typedef int IR2_OPND;
typedef int IR2_INST;
static int nf, ni, nl, live, peak;
enum { zero_ir2_opnd = 99, fcc0_ir2_opnd = 90 };
static int ra_alloc_ftemp(void) { ++live; if (live > peak) peak = live; return nf++; }
static int ra_alloc_itemp(void) { return ni++; }
static int ra_alloc_label(void) { return nl++; }
static void ra_free_temp(int r) { if (r >= 16 && r < 32) --live; }
static void ra_free_temp_auto(int r) { if (r >= 0) ra_free_temp(r); }
static int ir2_opnd_new_none(void) { return -1; }
static bool ir2_opnd_is_none(const int *r) { return *r == -1; }
static void li_d(int r, uint64_t v) { printf("li_d %d %llu\n", r, (unsigned long long)v); }
'''
    arity = {"label": 1, "b": 1, "bnez": 2, "bcnez": 2,
             "beq": 3, "bne": 3, "and": 3, "or": 3, "movfr2gr_d": 2}
    for prefix in ("v", "xv"):
        arity.update({prefix + name: n for name, n in {
            "fcmp_cond_s": 4, "fcmp_cond_d": 4,
            "mskltz_w": 2, "mskltz_d": 2,
            "fclass_s": 2, "fclass_d": 2,
            "slei_wu": 3, "slei_du": 3, "seteqz_v": 2,
            "slli_w": 3, "slli_d": 3, "bitseti_w": 3, "bitseti_d": 3,
            "and_v": 3, "ori_b": 3, "bitsel_v": 4,
            "shuf4i_w": 3, "shuf4i_d": 3,
        }.items()})
    arity.update({"xv" + n: 3 for n in ("pickve_w", "pickve_d",
                  "pickve2gr_w", "pickve2gr_d", "pickve2gr_du",
                  "insgr2vr_w", "insgr2vr_d")})
    templates = json.loads((ROOT / "target/i386/latx/inst_template.json").read_text())
    signatures = {name.replace(".", "_"): len(spec["opnd"])
                  for name, spec in templates.items() if "opnd" in spec}
    for name in sorted(set(re.findall(r"\bla_(\w+)\b", body))):
        n = arity[name]
        if name.startswith(("v", "xv")):
            assert signatures[name] == n, (name, signatures.get(name), n)
        args = ", ".join(f"int a{i}" for i in range(n))
        fmt = " %d" * n
        values = ", ".join(f"a{i}" for i in range(n))
        header += (f'static IR2_INST *la_{name}({args}) '
                   f'{{ printf("{name}{fmt}\\n", {values}); return NULL; }}\n')
    main = r'''
int main(void) {
    for (int dp = 0; dp < 2; ++dp) {
        for (int form = 0; form < 3; ++form) {
            int lanes = form == 0 ? 1 : (dp ? 2 : 4) * form;
            for (int count = 1; count <= 3; ++count) {
                for (int aliases = 0; aliases < 2; ++aliases) {
                    int sources[] = {1, aliases ? 1 : 2, aliases ? 1 : 3};
                    nf = 16; ni = 32; nl = 1000; live = peak = 0;
                    printf("CASE %d %d %d %d 0\n", dp, lanes, count, aliases);
                    lasx_fp_fix_nan_from_sources(0, sources, count, dp, lanes);
                    printf("END %d\n", peak);
                }
            }
        }
        for (int wide = 0; wide < (dp ? 1 : 2); ++wide) {
            for (int masked = 0; masked < 2; ++masked) {
                int lanes = (dp ? 2 : 4) * (wide + 1);
                int sources[] = {1, 1, 1, 1};
                const int pd[] = {1, 1};
                const int ps[] = {0xb1, -1, 0x1b, 0x4e};
                nf = 16; ni = 32; nl = 1000; live = peak = 0;
                printf("CASE %d %d %d 1 %d\n", dp, lanes, dp ? 2 : 4, masked + 1);
                VECTOR(0, sources, dp ? 2 : 4, dp, lanes, dp ? pd : ps,
                       dp, masked, !dp && masked ? 4 : -1);
                printf("END %d\n", peak);
            }
        }
    }
}
'''.replace("VECTOR", vector)
    with tempfile.TemporaryDirectory(prefix="latx-nan-model-") as directory:
        path = Path(directory)
        (path / "emit.c").write_text(header + body + main)
        subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                       ["-std=c11", "-O2", "-Wall", "-Wextra",
                        "-Wno-unused-function", str(path / "emit.c"),
                        "-o", str(path / "emit")], check=True)
        output = subprocess.check_output([str(path / "emit")], text=True)
    result = []
    for line in output.splitlines():
        op, *args = line.split()
        args = list(map(int, args))
        if op == "CASE":
            case, code = args, []
        elif op == "END":
            result.append((case, code, args[0]))
        else:
            code.append((op, args))
    return result


def nan(x, bits):
    frac = 52 if bits == 64 else 23
    return (x & ((1 << (bits - 1)) - 1)) > (((1 << (bits-frac-1))-1) << frac)


def classify(x, bits):
    frac = 52 if bits == 64 else 23
    sign = x >> (bits-1)
    if nan(x, bits):
        return 2 if x & (1 << (frac-1)) else 1
    exp = (x >> frac) & ((1 << (bits-frac-1))-1)
    if exp == (1 << (bits-frac-1))-1:
        return 1 << (2 if sign else 6)
    if exp:
        return 1 << (3 if sign else 7)
    return 1 << ((4 if sign else 8) if x & ((1 << frac)-1) else (5 if sign else 9))


def lane(v, i, bits):
    return (v >> (bits*i)) & ((1 << bits)-1)


def pack(values, bits):
    return sum(v << (i*bits) for i, v in enumerate(values))


def execute(code, initial, fcsr):
    r = dict(initial)
    r[99] = 0
    labels = {a[0]: i for i, (op, a) in enumerate(code) if op == "label"}
    pc, steps = 0, 0
    while pc < len(code):
        op, a = code[pc]
        pc += 1
        if op == "label":
            continue
        steps += 1
        d = a[0]
        get = lambda reg: r.get(reg, 0)
        if op == "b":
            pc = labels[d]
        elif op in ("bnez", "bcnez"):
            if get(d): pc = labels[a[1]]
        elif op in ("beq", "bne"):
            if (get(d) == get(a[1])) == (op == "beq"): pc = labels[a[2]]
        elif op == "li_d": r[d] = a[1]
        elif op == "and": r[d] = get(a[1]) & get(a[2])
        elif op == "or": r[d] = get(a[1]) | get(a[2])
        elif op == "movfr2gr_d": r[d] = get(a[1]) & MASK64
        elif "pickve2gr" in op:
            bits = 32 if op.endswith("_w") else 64
            v = lane(get(a[1]), a[2], bits)
            r[d] = v | (MASK64 ^ ((1 << bits)-1)) if bits == 32 and v >> 31 else v
        elif "insgr2vr" in op:
            bits = 32 if op.endswith("_w") else 64
            mask = ((1 << bits)-1) << (a[2]*bits)
            r[d] = (get(d) & ~mask) | ((get(a[1]) << (a[2]*bits)) & mask)
        elif op in ("xvpickve_w", "xvpickve_d"):
            r[d] = lane(get(a[1]), a[2], 32 if op.endswith("_w") else 64)
        else:
            width = 256 if op.startswith("xv") else 128
            insn = op[2:] if width == 256 else op[1:]
            bits = 64 if insn.endswith(("_d", "_du")) else 32
            full, mask = (1 << width)-1, (1 << bits)-1
            x = get(a[1])
            if insn == "seteqz_v":
                r[d] = int((x & full) == 0)
                continue
            if insn == "bitsel_v": v = (x & ~get(a[3])) | (get(a[2]) & get(a[3]))
            elif insn == "and_v": v = x & get(a[2])
            elif insn == "ori_b":
                assert a[2] == 0
                v = x
            elif insn.startswith("mskltz"):
                v = sum((lane(x, i, bits) >> (bits-1)) <<
                        ((i*bits//128)*128 + i % (128//bits)) for i in range(width//bits))
            elif insn.startswith("shuf4i"):
                values = []
                for i in range(width//bits):
                    base = i // (128//bits) * (128//bits)
                    idx = (a[2] >> (2*(i % (128//bits)))) & 3
                    if bits == 64:
                        values.append(lane(get(d) if idx < 2 else x, base + idx % 2, bits))
                    else: values.append(lane(x, base+idx, bits))
                v = pack(values, bits)
            else:
                values = []
                if insn.startswith("fcmp"):
                    fcsr &= ~(31 << 24)
                for i in range(width//bits):
                    item = lane(x, i, bits)
                    if insn.startswith("fclass"): value = classify(item, bits)
                    elif insn.startswith("fcmp"):
                        assert a[3] == 8 and a[1] == a[2]
                        value = mask if nan(item, bits) else 0
                        if classify(item, bits) == 1: fcsr |= (1 << 20) | (1 << 28)
                    elif insn.startswith("slei"): value = mask if item <= a[2] else 0
                    elif insn.startswith("slli"): value = (item << a[2]) & mask
                    elif insn.startswith("bitseti"): value = item | (1 << a[2])
                    else: raise ValueError(op)
                    values.append(value)
                v = pack(values, bits)
            # Model LSX's low 128 bits; the caller clears VEX upper halves.
            r[d] = (get(d) & ~full) | (v & full)
    return r, fcsr, steps


def check(all_programs):
    rng = random.Random(438)
    failures, cases, counts = {}, 0, []
    for (dp, lanes, count, aliases, mode), code, peak in all_programs:
        bits = 64 if dp else 32
        width = max(128, lanes*bits)
        quiet = 1 << (51 if dp else 22)
        infinity = 0x7ff0000000000000 if dp else 0x7f800000
        indefinite = (1 << (bits-1)) | infinity | quiet
        samples = [0, 1, infinity-1, infinity, infinity|1,
                   infinity|quiet|0x123, infinity|quiet|0x456,
                   (1 << (bits-1)) | infinity | 0x789]
        for trial in range(300):
            sources = {reg: [rng.choice(samples) if trial < 240 else rng.getrandbits(bits)
                             for _ in range(width//bits)] for reg in (1, 2, 3)}
            original = [rng.choice(samples) for _ in range(width//bits)]
            if not mode and trial < width//bits:
                # Precisely one NaN, including only in the upper LASX half
                # or in an inactive scalar lane. No other lane can hide it.
                sources = {reg: [0] * (width//bits) for reg in (1, 2, 3)}
                sources[1][trial] = infinity | 1
                original = [0] * (width//bits)
                original[trial] = indefinite
            # Ordinary arithmetic propagates any input NaN to its result.
            for i in range(lanes):
                if mode:
                    enabled = trial & (1 << (i % (128//bits)))
                    original[i] = 0 if mode == 2 and not enabled else indefinite
                elif any(nan(sources[1 if aliases else j+1][i], bits) for j in range(count)):
                    original[i] = indefinite
            expected = original[:]
            for i in range(lanes):
                if not nan(original[i], bits): continue
                candidates = []
                for j in range(count):
                    if mode:
                        if dp: idx = i if j == 0 else i ^ 1
                        else:
                            shuffle = [0xb1, 0xe4, 0x1b, 0x4e][j]
                            idx = (i//4)*4 + ((shuffle >> ((i%4)*2)) & 3)
                        candidates.append(sources[1][idx])
                    else: candidates.append(sources[1 if aliases else j+1][i])
                expected[i] = next((x | quiet for x in candidates if nan(x, bits)), indefinite)
            initial = {0: pack(original, bits), **{k: pack(v, bits) for k, v in sources.items()}}
            # Include upper-lane-only NaNs, inactive scalar SNaNs, and nonzero Cause.
            fcsr = [0, 0x08080000, 0x1010011f][trial % 3]
            out, status, steps = execute(code, initial, fcsr)
            checks = {"value": (out[0] & ((1 << width)-1)) == pack(expected, bits),
                      "fcsr": status == fcsr,
                      "sources": all(out[k] == initial[k] for k in (1, 2, 3))}
            for key, ok in checks.items():
                if not ok:
                    failures[key] = failures.get(key, 0)+1
            cases += 1
        if not aliases and not mode:
            finite = {0: 0, 1: 0, 2: 0, 3: 0}
            _, _, fast = execute(code, finite, 0)
            counts.append({"bits": bits, "lanes": lanes, "sources": count,
                           "emitter_ops": sum(op != "label" for op, _ in code),
                           "finite_ops": fast, "helper_ftemps_peak": peak})
    print(json.dumps({"cases": cases, "failures": failures, "counts": counts}, indent=2))
    return bool(failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision")
    args = parser.parse_args()
    source = (subprocess.check_output(["git", "show", f"{args.revision}:{SOURCE}"],
              cwd=ROOT, text=True) if args.revision else (ROOT / SOURCE).read_text())
    return check(programs(source))


if __name__ == "__main__":
    raise SystemExit(main())
