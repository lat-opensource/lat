#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 LAT Project Authors
# SPDX-License-Identifier: GPL-2.0-only
"""Run self-checking AVX guests in JIT and isolated cold/hot AOT paths."""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time


def run_case(emulator, source, work, prebuilt, guest_bits=64, jit_only=False):
    guest = work / source.stem
    if prebuilt:
        shutil.copy2(prebuilt / source.stem, guest)
    elif source.suffix == ".asm":
        obj = work / (source.stem + ".o")
        subprocess.run(["nasm", "-f", "elf" + str(guest_bits),
                        str(source), "-o", str(obj)],
                       check=True)
        machine = "elf_x86_64" if guest_bits == 64 else "elf_i386"
        subprocess.run(["ld.lld", "-m", machine, "-static", "--build-id=none",
                        str(obj), "-o", str(guest)], check=True)
    else:
        if platform.machine() == "x86_64" and shutil.which("gcc"):
            compiler = ["gcc", "-m" + str(guest_bits)]
        else:
            clang = shutil.which("clang-19") or shutil.which("clang")
            target = "x86_64" if guest_bits == 64 else "i386"
            compiler = [clang, "--target=" + target + "-linux-gnu", "-fuse-ld=lld"]
        flags = []
        if source.suffix == ".c":
            flags = ["-O2", "-mavx2", "-ffreestanding", "-fno-builtin",
                     "-fno-stack-protector"]
        subprocess.run(compiler + flags + ["-nostdlib", "-static", "-Wl,--build-id=none",
                                  str(source), "-o", str(guest)], check=True)
    if emulator == "native":
        subprocess.run([str(guest)], check=True, timeout=30)
        return

    # The executable's unique absolute path is part of the AOT cache key.
    # Never reuse another test's cache or change the user's HOME/configuration.
    cache = Path.home() / ".cache" / "latx"
    key = hashlib.sha256(str(guest).encode()).hexdigest()
    pattern = "v2-*-" + key + ".aot2"
    env = os.environ.copy()
    marker = work / (source.stem + ".aot-loaded")
    env.pop("LATX_TEST_AOT_LOAD_MARKER", None)
    try:
        for mode in (("jit",) if jit_only else ("jit", "cold", "hot")):
            env["LATX_AOT"] = "0" if mode == "jit" else "1"
            if mode == "hot":
                env["LATX_TEST_AOT_LOAD_MARKER"] = str(marker)
            subprocess.run([emulator, str(guest)], env=env,
                           check=True, timeout=30)
            if mode == "hot" and (
                    not marker.exists() or
                    str(guest) not in marker.read_text().splitlines()):
                raise RuntimeError("AOT cache was not loaded for " +
                                   source.name)
            if mode == "cold":
                deadline = time.monotonic() + 10
                while time.monotonic() < deadline:
                    if any(p.stat().st_size for p in cache.glob(pattern)):
                        break
                    time.sleep(0.1)
                else:
                    raise RuntimeError("no non-empty AOT cache for " + source.name)
    finally:
        # Only remove cache artifacts keyed by this temporary executable.
        for artifact in cache.glob(pattern + "*"):
            try:
                artifact.unlink()
            except FileNotFoundError:
                pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prebuilt", type=Path,
                        default=os.environ.get("LATX_AVX_PREBUILT"),
                        help="guests compiled on x86 (or LATX_AVX_PREBUILT)")
    parser.add_argument("emulator", help="LAT executable, or native")
    parser.add_argument("--guest-bits", type=int, choices=(32, 64), default=64,
                        help="guest ELF width (default: 64)")
    parser.add_argument("--jit-only", action="store_true",
                        help="run configurations that disable AOT in JIT only")
    parser.add_argument("sources", type=Path, nargs="+")
    args = parser.parse_args()
    nasm_only = all(source.suffix == ".asm" for source in args.sources)
    if not args.prebuilt and not (nasm_only and shutil.which("nasm") and
                                  shutil.which("ld.lld")) and not (
            (platform.machine() == "x86_64" and shutil.which("gcc")) or
            shutil.which("clang-19") or shutil.which("clang")):
        print("SKIP: an x86 compiler or --prebuilt guests are required")
        return 77
    emulator = args.emulator
    if emulator != "native":
        emulator = str(Path(emulator).resolve())
    failures = 0
    with tempfile.TemporaryDirectory(prefix="latx-avx-regression-") as directory:
        for source in args.sources:
            try:
                run_case(emulator, source.resolve(), Path(directory),
                         args.prebuilt, args.guest_bits, args.jit_only)
                print("PASS:", source.name, flush=True)
            except (subprocess.SubprocessError, RuntimeError) as error:
                failures += 1
                print("FAIL:", source.name, error, flush=True)
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
