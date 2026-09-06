#!/usr/bin/env python3
"""Host-independent semantics test of the actual MXCSR IR2 emitter.

Run: python3 tests/latx/test-mxcsr-fcsr.py
Use --revision HEAD^ to check the pre-fix emitter without changing checkout.
Requires a C compiler. This models emitted integer instructions, not LASX
execution, helper-call preservation, or floating-point exception delivery.
"""

import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
SOURCE = "target/i386/latx/translator/tr-fctrl.c"


def function(source, name):
    match = re.search(r"static void " + name + r"\([^)]*\)\s*\{", source)
    if not match:
        raise RuntimeError(f"missing function: {name}")
    depth = 1
    end = match.end()
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


HARNESS = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int IR2_OPND;
static uint64_t regs[32];
static uint32_t hardware_fcsr;
static int next_reg, option_enable_fcsr_exc;
enum { zero_ir2_opnd = 0, fcsr_ir2_opnd = 31 };
static int ra_alloc_itemp(void) { return next_reg++; }
static void ra_free_temp(int r) { (void)r; }
static uint64_t sext32(uint32_t v) { return (int64_t)(int32_t)v; }
static void la_movfcsr2gr(int d, int s)
{ (void)s; regs[d] = sext32(hardware_fcsr); }
static void la_movgr2fcsr(int d, int s)
{ (void)d; hardware_fcsr = regs[s]; }
static void la_bitrev_w(int d, int s)
{
    uint32_t x = regs[s], r = 0;
    for (int i = 0; i < 32; ++i) { r = (r << 1) | (x & 1); x >>= 1; }
    regs[d] = sext32(r);
}
static void la_bstrpick_w(int d, int s, int hi, int lo)
{ regs[d] = sext32(((uint32_t)regs[s] >> lo) & ((1ULL << (hi-lo+1))-1)); }
static void la_bstrpick_d(int d, int s, int hi, int lo)
{ regs[d] = (regs[s] >> lo) & ((1ULL << (hi-lo+1))-1); }
static void la_bstrins_w(int d, int s, int hi, int lo)
{
    uint32_t mask = ((1ULL << (hi-lo+1))-1) << lo;
    regs[d] = sext32(((uint32_t)regs[d] & ~mask) |
                     (((uint32_t)regs[s] << lo) & mask));
}
static void la_srli_w(int d, int s, int n)
{ regs[d] = sext32((uint32_t)regs[s] >> n); }
static void la_srli_d(int d, int s, int n) { regs[d] = regs[s] >> n; }
static void la_slli_d(int d, int s, int n) { regs[d] = regs[s] << n; }
static void la_xori(int d, int s, int imm) { regs[d] = regs[s] ^ imm; }
static void la_andi(int d, int s, int imm) { regs[d] = regs[s] & imm; }
static void la_or(int d, int a, int b) { regs[d] = regs[a] | regs[b]; }
"""

CHECK = r"""
int main(void)
{
    /* Explicit architecture mapping, independent of emitter bit reversal. */
    const int x86_bits[] = {5, 4, 3, 2, 0};
    const uint32_t initial[] = {0, 0x1f1f031f, 0x001f0000, 0x150a020a};
    const uint32_t changed = (31u << 24) | (31u << 16) | 31u;
    unsigned failures[2] = {0, 0};
    unsigned cases = 0;
    for (int option = 0; option < 2; ++option) {
        option_enable_fcsr_exc = option;
        for (uint32_t mxcsr = 0; mxcsr < 65536; ++mxcsr) {
            for (unsigned k = 0; k < sizeof(initial)/sizeof(initial[0]); ++k) {
                uint32_t expected = initial[k] & ~changed;
                for (int i = 0; i < 5; ++i) {
                    expected |= ((mxcsr >> x86_bits[i]) & 1) << (16+i);
                    expected |= (((mxcsr >> (x86_bits[i]+7)) & 1) ^ 1) << i;
                }
                memset(regs, 0, sizeof(regs));
                next_reg = 2;
                regs[1] = mxcsr;
                hardware_fcsr = initial[k];
                LOAD_MXCSR(1);
                if (hardware_fcsr != expected || regs[1] != mxcsr) {
                    if (failures[option]++ == 0) {
                        fprintf(stderr, "option=%d mxcsr=%04x old=%08x "
                                "actual=%08x expected=%08x\n", option,
                                mxcsr, initial[k], hardware_fcsr, expected);
                    }
                }
                /* The next STMXCSR must not bring back cleared flags. */
                next_reg = 2;
                update_mxcsr_flags_by_fcsr(1);
                if (regs[1] != mxcsr) ++failures[option];
                ++cases;
            }
        }
    }
    printf("%u cases; failures with option=0: %u, option=1: %u\n",
           cases, failures[0], failures[1]);
    return !!(failures[0] || failures[1]);
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision")
    args = parser.parse_args()
    if args.revision:
        source = subprocess.check_output(
            ["git", "show", f"{args.revision}:{SOURCE}"], cwd=ROOT, text=True)
    else:
        source = (ROOT / SOURCE).read_text()
    # Follow the helper actually called by translate_ldmxcsr().
    load_body = source[source.index("bool translate_ldmxcsr("):]
    name = re.search(r"(update_fcsr_\w+)\(new_mxcsr\);", load_body).group(1)
    definitions = (ROOT / "target/i386/latx/include/env.h").read_text()
    constants = "\n".join(re.findall(
        r"^#define (?:FCSR_|X87_CR_OFF_)\w+\s+(?:0x[0-9a-fA-F]+|\d+)\s*$",
        definitions, re.MULTILINE))
    emitter = function(source, "update_fcsr_enable") + "\n" + function(source, name)
    emitter += "\n" + function(source, "update_mxcsr_flags_by_fcsr")
    with tempfile.TemporaryDirectory(prefix="latx-mxcsr-test-") as directory:
        path = Path(directory)
        (path / "test.c").write_text(constants + "\n" + HARNESS + emitter +
                                    f"\n#define LOAD_MXCSR {name}\n" + CHECK)
        subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                       ["-std=c11", "-O2", "-Wall", "-Wextra",
                        "-Wno-unused-function", str(path / "test.c"),
                        "-o", str(path / "test")], check=True)
        return subprocess.run([str(path / "test")]).returncode


if __name__ == "__main__":
    raise SystemExit(main())
