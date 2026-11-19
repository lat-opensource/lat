#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Exercise exact AVX memory accesses, mask suppression and guest permissions."""
import argparse
import hashlib
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile
import time
from unittest.mock import patch


def check_aot_mask_modes(emulator, guest, env):
    cache = Path.home() / ".cache" / "latx"
    key = hashlib.sha256(str(guest).encode()).hexdigest()
    pattern = "v2-*-" + key + ".aot2"
    marker = guest.with_suffix(".mask-aot-loaded")
    env = dict(env, LATX_AOT="1", LATX_AOT_GENERATE="1", LATX_MT="0")
    try:
        for masked in (0, 1):
            env["LATX_MASKED_MEMORY"] = str(masked)
            subprocess.run([emulator, str(guest)], env=env, check=True,
                           capture_output=True, timeout=30)
            deadline = time.monotonic() + 10
            while not any(("-mask1-" in path.name) == bool(masked) and
                          path.stat().st_size for path in cache.glob(pattern)):
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"no AOT cache for MASK={masked}")
                time.sleep(0.1)
        for masked in (0, 1, 0, 1):
            env["LATX_MASKED_MEMORY"] = str(masked)
            env["LATX_TEST_AOT_LOAD_MARKER"] = str(marker)
            if marker.exists():
                marker.unlink()
            result = subprocess.run([emulator, str(guest), "c", "16384"],
                                    env=env, capture_output=True, text=True,
                                    timeout=30)
            expected = 0 if masked else 42
            if (result.returncode != expected or not marker.exists() or
                    str(guest) not in marker.read_text().splitlines()):
                raise RuntimeError(f"AOT mask switch MASK={masked} "
                                   f"expected={expected} got={result.returncode}\n"
                                   + result.stdout + result.stderr)
    finally:
        for artifact in cache.glob(pattern + "*"):
            try:
                artifact.unlink()
            except FileNotFoundError:
                pass


def fixtures(guest_bits):
    for op, element in (("vmaskmovps", 4), ("vmaskmovpd", 8),
                        ("vpmaskmovd", 4), ("vpmaskmovq", 8)):
        for wide in (0, 1):
            yield f"{op}-{wide}", dict(KIND=0, OP=op, ELEMENT=element,
                                       WIDE=wide), "vztrRwW"
    for op in ("maskmovdqu", "vmaskmovdqu"):
        yield op, dict(KIND=1, OP=op, ELEMENT=1), "vztwW"
    yield "maskmovq", dict(KIND=7, OP="maskmovq", ELEMENT=1), "vztwW"
    for op, element, index64 in (
            ("vpgatherdd", 4, 0), ("vpgatherdq", 8, 0),
            ("vpgatherqd", 4, 1), ("vpgatherqq", 8, 1),
            ("vgatherdps", 4, 0), ("vgatherdpd", 8, 0),
            ("vgatherqps", 4, 1), ("vgatherqpd", 8, 1)):
        for wide in (0, 1):
            for scale in (1, 4):
                yield f"{op}-{wide}-{scale}", dict(
                    KIND=2, OP=op, ELEMENT=element, INDEX64=index64,
                    WIDE=wide, SCALE=scale), "vztrRph"
    for op in ("vcvtps2pd", "vcvtdq2pd"):
        defines = dict(KIND=3, OP=op)
        if op == "vcvtdq2pd":
            defines["INTEGER_INPUT"] = 1
        yield op, defines, "vt"
    yield "vmovss", dict(KIND=4), "vw"
    for wide in (0, 1):
        yield f"vmovdqu-{wide}", dict(KIND=6, WIDE=wide), "vtrw"
    state_ops = ["xsave", "xsaveopt", "xrstor"]
    if guest_bits == 64:
        state_ops += ["xsave64", "xsaveopt64", "xrstor64"]
    for op in state_ops:
        yield op, dict(KIND=5, OP=op), "vuri" if op.startswith("xrstor") else "vuw"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("emulator", type=Path)
    parser.add_argument("source", type=Path)
    parser.add_argument("--guest-bits", type=int, choices=(32, 64), required=True)
    parser.add_argument("--lsx", action="store_true")
    parser.add_argument("--aot", action="store_true")
    parser.add_argument("--filter", default="")
    parser.add_argument("--group", choices=("all", "permissions", "xsave", "mask",
                                           "gather"), default="all")
    parser.add_argument("--masked-memory", type=int, choices=(0, 1), default=1)
    args = parser.parse_args()
    required = ["nasm", "ld.lld"] + (["gdb"] if args.lsx else [])
    if not all(shutil.which(tool) for tool in required):
        print("SKIP: required tools:", ", ".join(required))
        return 77
    if args.lsx and args.aot:
        parser.error("forced LSX uses JIT, not an AOT cache")
    emulator = str(args.emulator.resolve())
    env = os.environ.copy()
    env.update(LATX_AOT="0", LATX_KZT="0", LATX_SOFTFPU="0",
               LATX_MASKED_MEMORY=str(args.masked_memory))
    env.pop("LATX_TEST_AOT_LOAD_MARKER", None)
    failures = 0
    passed = 0
    with tempfile.TemporaryDirectory(prefix="latx-avx-memory-") as directory:
        work = Path(directory)
        groups = {"permissions": (4, 6), "xsave": (5,),
                  "mask": (0, 1, 7), "gather": (2,)}
        selected = 0
        for name, defines, cases in fixtures(args.guest_bits):
            if args.group != "all" and defines["KIND"] not in groups[args.group]:
                continue
            if args.filter and args.filter not in name:
                continue
            selected += 1
            guest = work / name
            obj = guest.with_suffix(".o")
            subprocess.run(["nasm", "-f", f"elf{args.guest_bits}",
                            *[f"-D{key}={value}" for key, value in defines.items()],
                            str(args.source), "-o", str(obj)], check=True)
            machine = "elf_i386" if args.guest_bits == 32 else "elf_x86_64"
            subprocess.run(["ld.lld", "-m", machine, "-static", "--build-id=none",
                            str(obj), "-o", str(guest)], check=True)
            if not args.lsx and not args.aot and defines["KIND"] in (0, 1, 7):
                # Exit 42 proves a fault at the masked instruction and address;
                # exit 0 proves suppression. Do not accept an arbitrary crash.
                probes = [(env.copy(), [], 0 if args.masked_memory else 42)]
                if name == "vmaskmovps-0":
                    default_env = env.copy()
                    default_env.pop("LATX_MASKED_MEMORY")
                    probes += [(default_env, [], 42),
                               (env.copy(), ["-latx-masked-memory",
                               str(1 - args.masked_memory)],
                                42 if args.masked_memory else 0)]
                    for invalid in ("", "-1", "2", "garbage", "1junk"):
                        invalid_env = env.copy()
                        invalid_env["LATX_MASKED_MEMORY"] = invalid
                        result = subprocess.run([emulator, str(guest)],
                                                env=invalid_env,
                                                capture_output=True, text=True,
                                                timeout=30)
                        if (result.returncode != 1 or
                                "LATX_MASKED_MEMORY must be 0 or 1" not in
                                result.stderr):
                            failures += 1
                            print(f"FAIL: accepted invalid masked-memory value "
                                  f"{invalid!r}: {result.returncode}\n" +
                                  result.stdout + result.stderr, flush=True)
                        else:
                            passed += 1
                for probe_env, options, expected in probes:
                    probe_env["LATX_MT"] = "0"
                    result = subprocess.run([emulator, *options, str(guest),
                                             "c", "16384"], env=probe_env,
                                            capture_output=True, text=True,
                                            timeout=30)
                    if result.returncode != expected:
                        failures += 1
                        print(f"FAIL: {name} switch probe expected={expected} "
                              f"status={result.returncode}\n" +
                              result.stdout + result.stderr, flush=True)
                    else:
                        passed += 1
            if args.aot:
                # Reuse the existing runner's cold/hot cache-load sentinel.
                runner = runpy.run_path(str(Path(__file__).with_name(
                    "test-avx-regressions.py")))
                with tempfile.TemporaryDirectory(prefix="latx-memory-aot-") as aot:
                    with patch.dict(os.environ, env):
                        runner["run_case"](emulator, guest.with_suffix(".asm"),
                                           Path(aot), work, args.guest_bits)
                    passed += 3
                if name == "vmaskmovps-0":
                    check_aot_mask_modes(emulator, guest, env)
                    passed += 4
                continue
            for mt, boundary in ((0, 16384), (1, 4096), (2, 4096)):
                if mt and os.sysconf("SC_PAGE_SIZE") != 16384:
                    continue
                env["LATX_MT"] = str(mt)
                # The bulk path does not promise masked-off fault suppression.
                selected_cases = "v" if not args.masked_memory and defines[
                    "KIND"] in (0, 1, 2, 7) else cases
                for case in selected_cases:
                    command = [emulator, str(guest), case, str(boundary)]
                    if args.lsx:
                        command = ["gdb", "-q", "-batch", "--return-child-result",
                                   "-ex", "handle SIGSEGV nostop noprint pass",
                                   "-ex", "handle SIGBUS nostop noprint pass",
                                   "-ex", "break tr_ir2_generate", "-ex", "run",
                                   "-ex", "set {int}&option_enable_lasx = 0",
                                   "-ex", 'printf "LATX_TEST_LSX_SELECTED\\n"',
                                   "-ex", "disable breakpoints", "-ex", "continue",
                                   "--args", *command]
                    result = subprocess.run(command, env=env, capture_output=True,
                                            text=True, timeout=30)
                    selected = not args.lsx or "LATX_TEST_LSX_SELECTED" in result.stdout
                    if result.returncode or not selected:
                        failures += 1
                        print(f"FAIL: {name} MASK={args.masked_memory} MT={mt} {case} "
                              f"status={result.returncode} selected={selected}",
                              flush=True)
                        print(result.stdout + result.stderr, flush=True)
                    else:
                        passed += 1
    if not selected:
        parser.error("filter selected no fixtures")
    print(f"AVX memory: {passed} passed, {failures} failed", flush=True)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
