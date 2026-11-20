#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check non-masked AVX memory operand widths against a protected host page."""
import argparse
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile
from unittest.mock import patch


def fixtures(bits):
    for op, conversion in (("vcvtps2pd", 1), ("vcvtdq2pd", 2)):
        for reg, width in (("xmm", 8), ("ymm", 16)):
            yield f"{op}-{reg}", f"{op} {reg}0, [BP]", width, conversion
    for op in ("vcvtpd2ps", "vcvtpd2dq", "vcvttpd2dq"):
        for size, width in (("oword", 16), ("yword", 32)):
            yield f"{op}-{width}", f"{op} xmm0, {size} [BP]", width, 0
    for op in ("vcvtps2dq", "vcvttps2dq", "vcvtdq2ps", "vcvtph2ps"):
        for reg, width in (("xmm", 16), ("ymm", 32)):
            if op == "vcvtph2ps":
                width //= 2
            yield f"{op}-{reg}", f"{op} {reg}0, [BP]", width, 0
    for op, width in (("vbroadcastss", 4), ("vbroadcastsd", 8),
                      ("vbroadcastf128", 16), ("vbroadcasti128", 16),
                      ("vpbroadcastb", 1), ("vpbroadcastw", 2),
                      ("vpbroadcastd", 4), ("vpbroadcastq", 8)):
        for reg in ("xmm", "ymm"):
            if reg == "xmm" and op in ("vbroadcastsd", "vbroadcastf128",
                                       "vbroadcasti128"):
                continue
            yield f"{op}-{reg}", f"{op} {reg}0, [BP]", width, 0
    for sign in ("s", "z"):
        for suffix, width in (("bw", 8), ("bd", 4), ("bq", 2),
                              ("wd", 8), ("wq", 4), ("dq", 8)):
            op = "vpmov" + sign + "x" + suffix
            for reg, scale in (("xmm", 1), ("ymm", 2)):
                yield f"{op}-{reg}", f"{op} {reg}0, [BP]", width * scale, 0
    for op, width in (("vmovss", 4), ("vmovsd", 8), ("vmovd", 4),
                      ("vmovq", 8), ("vmovddup", 8)):
        yield op, f"{op} xmm0, [BP]", width, 0
    for op in ("vmovlps", "vmovhps", "vmovlpd", "vmovhpd"):
        yield op, f"{op} xmm0, xmm1, [BP]", 8, 0
    for op in ("vmovups", "vmovupd", "vmovdqu", "vmovsldup", "vmovshdup",
               "vaddps", "vaddpd", "vpaddb", "vpmaddwd", "vpshufb",
               "vpsllvd", "vpsrlvq", "vpsravd", "vfmadd132ps",
               "vfmadd213pd", "vaesenc", "vaesenclast", "vaesdec"):
        for reg, width in (("xmm", 16), ("ymm", 32)):
            operands = f"{reg}0, {reg}1, [BP]"
            if op.startswith("vmov"):
                operands = f"{reg}0, [BP]"
            yield f"{op}-{reg}", f"{op} {operands}", width, 0
    yield "vmovddup-ymm", "vmovddup ymm0, [BP]", 32, 0
    for op in ("vptest", "vtestps", "vtestpd"):
        for reg, width in (("xmm", 16), ("ymm", 32)):
            yield f"{op}-{reg}", f"{op} {reg}0, [BP]", width, 0
    for op in ("vpsllw", "vpslld", "vpsllq", "vpsrlw", "vpsrld", "vpsrlq",
               "vpsraw", "vpsrad"):
        for reg in ("xmm", "ymm"):
            yield f"{op}-{reg}", f"{op} {reg}0, {reg}1, [BP]", 16, 0
    for op, width in (("vaddss", 4), ("vaddsd", 8), ("vcvtss2sd", 4),
                      ("vcvtsd2ss", 8), ("vfmadd132ss", 4),
                      ("vfmadd213sd", 8), ("vsqrtss", 4), ("vsqrtsd", 8)):
        yield op, f"{op} xmm0, xmm1, [BP]", width, 0
    for op, width in (("vcomiss", 4), ("vcomisd", 8), ("vucomiss", 4),
                      ("vucomisd", 8)):
        yield op, f"{op} xmm0, [BP]", width, 0
    for op, width in (("vcmpss", 4), ("vcmpsd", 8), ("vroundss", 4),
                      ("vroundsd", 8), ("vinsertps", 4)):
        yield op, f"{op} xmm0, xmm1, [BP], 0", width, 0
    for op in ("vinsertf128", "vinserti128"):
        yield op, f"{op} ymm0, ymm1, [BP], 1", 16, 0
    for reg, width in (("xmm", 16), ("ymm", 32)):
        yield f"vpclmulqdq-{reg}", f"vpclmulqdq {reg}0, {reg}1, [BP], 0", width, 0
    for op in ("vcvtsi2ss", "vcvtsi2sd"):
        for size, width in (("dword", 4), ("qword", 8)):
            if bits == 32 and width == 8:
                continue
            yield f"{op}-{width}", f"{op} xmm0, xmm1, {size} [BP]", width, 0
    for reg, width in (("eax", 4), ("rax", 8)):
        if bits == 32 and width == 8:
            continue
        for op in ("vcvttss2si", "vcvttsd2si", "vcvtss2si", "vcvtsd2si"):
            read = 4 if "ss" in op else 8
            yield f"{op}-{width}", f"{op} {reg}, [BP]", read, 0
    for op, width in (("cvtps2pd", 8), ("cvtdq2pd", 8), ("cvtpd2ps", 16),
                      ("cvtpd2dq", 16), ("cvttpd2dq", 16), ("aesenc", 16),
                      ("aesdec", 16), ("sha1msg1", 16), ("sha256msg1", 16)):
        yield op, f"{op} xmm0, [BP]", width, 0
    for dest, src, second, size, width in (("eax", "ecx", "edx", "dword", 4),
                                          ("rax", "rcx", "rdx", "qword", 8)):
        if bits == 32 and width == 8:
            continue
        for op in ("andn", "pdep", "pext"):
            yield f"{op}-{width}", f"{op} {dest}, {src}, [BP]", width, 0
        for op in ("bextr", "bzhi", "shlx", "shrx", "sarx"):
            yield f"{op}-{width}", f"{op} {dest}, [BP], {src}", width, 0
        for op in ("blsi", "blsr", "blsmsk", "popcnt", "lzcnt", "tzcnt",
                   "adcx", "adox"):
            yield f"{op}-{width}", f"{op} {dest}, {size} [BP]", width, 0
        yield f"mulx-{width}", f"mulx {dest}, {second}, [BP]", width, 0
        yield f"rorx-{width}", f"rorx {dest}, [BP], 1", width, 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("emulator", type=Path)
    parser.add_argument("source", type=Path)
    parser.add_argument("--guest-bits", type=int, choices=(32, 64), required=True)
    parser.add_argument("--lsx", action="store_true")
    parser.add_argument("--aot", action="store_true")
    parser.add_argument("--filter", default="")
    args = parser.parse_args()
    required = ["nasm", "ld.lld"] + (["gdb"] if args.lsx else [])
    if not all(shutil.which(tool) for tool in required):
        print("SKIP: required tools:", ", ".join(required))
        return 77
    if args.lsx and args.aot:
        parser.error("forced LSX requires translation through JIT")
    env = dict(os.environ, LATX_AOT="0", LATX_MT="0", LATX_KZT="0",
               LATX_SOFTFPU="0")
    env.pop("LATX_TEST_AOT_LOAD_MARKER", None)
    emulator = str(args.emulator.resolve())
    passed = failed = selected = 0
    runner = runpy.run_path(str(Path(__file__).with_name("test-avx-regressions.py")))
    with tempfile.TemporaryDirectory(prefix="latx-avx-width-") as directory:
        work = Path(directory)
        for name, instruction, width, conversion in fixtures(args.guest_bits):
            if args.filter not in name:
                continue
            selected += 1
            guest = work / name
            obj = guest.with_suffix(".o")
            command = [emulator, str(guest)]
            try:
                subprocess.run(["nasm", "-f", "elf" + str(args.guest_bits),
                                "-DTEST=" + instruction, f"-DREAD_BYTES={width}",
                                f"-DCONVERSION={conversion}",
                                "-DDEST_BYTES=" + ("32" if "ymm0" in instruction else "16"),
                                str(args.source), "-o", str(obj)], check=True)
                machine = "elf_x86_64" if args.guest_bits == 64 else "elf_i386"
                subprocess.run(["ld.lld", "-m", machine, "-static", "--build-id=none",
                                str(obj), "-o", str(guest)], check=True)
                if args.aot:
                    with tempfile.TemporaryDirectory(prefix="aot-", dir=work) as aot:
                        with patch.dict(os.environ, env, clear=True):
                            runner["run_case"](emulator, guest.with_suffix(".asm"),
                                               Path(aot), work, args.guest_bits)
                else:
                    if args.lsx:
                        command = ["gdb", "-q", "-batch", "--return-child-result",
                                   "-ex", "handle SIGSEGV nostop noprint pass",
                                   "-ex", "handle SIGBUS nostop noprint pass",
                                   "-ex", "break tr_ir2_generate", "-ex", "run",
                                   "-ex", "set {int}&option_enable_lasx = 0",
                                   "-ex", "printf \"LATX_TEST_LSX_SELECTED\\n\"",
                                   "-ex", "disable breakpoints", "-ex", "continue",
                                   "--args", *command]
                    result = subprocess.run(command, env=env, capture_output=True,
                                            text=True, timeout=30)
                    if result.returncode or (args.lsx and
                            "LATX_TEST_LSX_SELECTED" not in result.stdout):
                        raise RuntimeError(f"status={result.returncode}\n" +
                                           result.stdout + result.stderr)
                passed += 1
            except (subprocess.SubprocessError, RuntimeError) as error:
                failed += 1
                print(f"FAIL: {name}: {error}", flush=True)
    print(f"AVX load widths: {passed} passed, {failed} failed", flush=True)
    if not selected:
        parser.error("filter selected no fixtures")
    return bool(failed)


if __name__ == "__main__":
    raise SystemExit(main())
