#!/usr/bin/env python3
"""Compile SPECint2000 x86 executables to LoongArch ELF and run train."""

import argparse
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

from specint import replace_run_link, run_spec, selected_programs, sha256


def command_output(command):
    return subprocess.check_output(command, text=True).strip()


def save_manifest(path, manifest):
    path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--latc", required=True, type=Path)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--spec-root", required=True, type=Path)
    parser.add_argument("--workdir", required=True, type=Path)
    parser.add_argument("--guest-dir", default="specbin/x64_gcc12_2_0")
    parser.add_argument("--benchmark", action="append", default=[])
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="seconds allowed for each benchmark harness run")
    args = parser.parse_args()

    args.latc = args.latc.resolve()
    args.runner = args.runner.resolve()
    args.spec_root = args.spec_root.resolve()
    args.workdir = args.workdir.resolve()
    guest_dir = (args.spec_root / args.guest_dir).resolve()
    programs = selected_programs(args.benchmark)
    compile_native = Path(__file__).resolve().parents[1] / "scripts" / \
        "compile-native-elf.sh"
    for path in (args.latc, args.runner, compile_native):
        if not path.is_file():
            raise SystemExit("missing required file: %s" % path)
    if platform.machine() != "loongarch64":
        raise SystemExit("native SPEC execution requires a LoongArch64 host")

    native_bin = args.workdir / "specbin-native"
    logs = args.workdir / "logs"
    raws = args.workdir / "raw"
    homes = args.workdir / "homes"
    for directory in (native_bin, logs, raws, homes):
        directory.mkdir(parents=True, exist_ok=True)

    run_link = args.spec_root / "specbin" / "run"
    if not run_link.is_symlink():
        raise SystemExit("SPEC specbin/run must be a symlink")
    old_target = Path(os.readlink(str(run_link)))
    if not old_target.is_absolute():
        old_target = (run_link.parent / old_target).resolve()

    manifest = {
        "suite": "SPECint2000",
        "input": "train",
        "execution_model": "direct LoongArch ELF with embedded static TBs",
        "host": platform.node(),
        "kernel": platform.release(),
        "machine": platform.machine(),
        "latc": str(args.latc),
        "latc_sha256": sha256(args.latc),
        "runner": str(args.runner),
        "runner_sha256": sha256(args.runner),
        "programs": [],
    }
    manifest_path = args.workdir / "manifest.json"
    env = os.environ.copy()

    entries = {}
    try:
        for benchmark, filename, _strict_mode in programs:
            print("compile %s" % benchmark, flush=True)
            guest = guest_dir / filename
            native = native_bin / filename
            if not guest.is_file():
                raise RuntimeError("missing x86 guest: %s" % guest)
            subprocess.run([str(compile_native), str(args.latc),
                            str(args.runner), str(guest), str(native)],
                           check=True, env=env)

            elf_header = command_output(["readelf", "-h", str(native)])
            if "LoongArch" not in elf_header:
                raise RuntimeError("not a LoongArch ELF: %s" % native)
            dynamic = command_output(["readelf", "-d", str(native)])
            if "latx-x86_64" in dynamic:
                raise RuntimeError("native ELF depends on LAT runner: %s" % native)
            inspect = command_output([str(native), "--latc-inspect"])
            if "execution_model=lat-native-pie-shell" not in inspect:
                raise RuntimeError("native ELF inspection failed: %s" % native)

            entry = {
                "benchmark": benchmark,
                "filename": filename,
                "guest": str(guest),
                "guest_sha256": sha256(guest),
                "native_elf": str(native),
                "native_elf_sha256": sha256(native),
                "native_elf_size": native.stat().st_size,
                "inspect": inspect.splitlines(),
                "generated": True,
                "valid": False,
            }
            entries[benchmark] = entry
            manifest["programs"].append(entry)
            save_manifest(manifest_path, manifest)
            print("GENERATED %s" % benchmark, flush=True)

        replace_run_link(args.spec_root, native_bin)
        for benchmark, _filename, _strict_mode in programs:
            print("run %s train" % benchmark, flush=True)
            home = homes / benchmark
            home.mkdir(exist_ok=True)
            run_env = env.copy()
            run_env["HOME"] = str(home)
            log = logs / (benchmark + ".log")
            reported_time, raw, valid = run_spec(
                args.spec_root, "train", benchmark, run_env, log,
                timeout=args.timeout)
            saved_raw = raws / (benchmark + ".raw")
            shutil.copy2(raw, saved_raw)
            entry = entries[benchmark]
            entry.update({
                "reported_time": reported_time, "valid": valid,
                "log": str(log), "raw": str(saved_raw),
            })
            save_manifest(manifest_path, manifest)
            print("PASS %s %.6f" % (benchmark, reported_time), flush=True)
    finally:
        replace_run_link(args.spec_root, old_target)

    print(json.dumps({"passed": len(manifest["programs"]),
                      "manifest": str(manifest_path)}))


if __name__ == "__main__":
    main()
