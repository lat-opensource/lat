#!/usr/bin/env python3
"""Build full AOT v2 modules and validate SPECint2000 train."""

import argparse
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

from specint import replace_run_link, run_spec, selected_programs, sha256


def command_json(command):
    return json.loads(subprocess.check_output(command, text=True))


def save_manifest(path, manifest):
    path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--latc", required=True, type=Path)
    parser.add_argument("--exporter", required=True, type=Path)
    parser.add_argument("--aot-runner", required=True, type=Path)
    parser.add_argument("--runtime-dir", required=True, type=Path)
    parser.add_argument("--spec-root", required=True, type=Path)
    parser.add_argument("--workdir", required=True, type=Path)
    parser.add_argument("--guest-dir", default="specbin/x64_gcc12_2_0")
    parser.add_argument("--benchmark", action="append", default=[])
    parser.add_argument("--timeout", type=float, default=60.0)
    args = parser.parse_args()

    for name in ("latc", "exporter", "aot_runner", "runtime_dir",
                 "spec_root", "workdir"):
        setattr(args, name, getattr(args, name).resolve())
    if platform.machine() != "loongarch64":
        raise SystemExit("AOT v2 SPEC execution requires a LoongArch64 host")

    script_dir = Path(__file__).resolve().parents[1] / "scripts"
    compile_native = script_dir / "compile-native-image.sh"
    link_module = script_dir / "link-aot-v2-module.sh"
    for path in (args.latc, args.exporter, args.aot_runner,
                 compile_native, link_module):
        if not path.is_file():
            raise SystemExit("missing required file: %s" % path)

    programs = selected_programs(args.benchmark)
    guest_dir = (args.spec_root / args.guest_dir).resolve()
    wrappers = args.workdir / "specbin-aot-v2"
    images = args.workdir / "native-images"
    modules = args.workdir / "modules"
    logs = args.workdir / "logs"
    raws = args.workdir / "raw"
    homes = args.workdir / "homes"
    stats = args.workdir / "stats"
    for directory in (wrappers, images, modules, logs, raws, homes, stats):
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
        "execution_model": "LAT AOT v2 full static module",
        "host": platform.node(),
        "kernel": platform.release(),
        "latc_sha256": sha256(args.latc),
        "exporter_sha256": sha256(args.exporter),
        "aot_runner_sha256": sha256(args.aot_runner),
        "programs": [],
    }
    manifest_path = args.workdir / "manifest.json"
    entries = {}
    base_env = os.environ.copy()

    try:
        for benchmark, filename, _strict_mode in programs:
            print("build %s" % benchmark, flush=True)
            guest = guest_dir / filename
            image = images / (filename + ".latnative")
            module = modules / (filename + ".so")
            wrapper = wrappers / filename
            if not guest.is_file():
                raise RuntimeError("missing x86 guest: %s" % guest)
            subprocess.run([str(compile_native), str(args.latc),
                            str(args.exporter), str(guest), str(image)],
                           check=True, env=base_env,
                           stdout=subprocess.DEVNULL)
            subprocess.run([str(link_module), str(args.latc), str(image),
                            str(args.runtime_dir), str(module)], check=True,
                           env=base_env, stdout=subprocess.DEVNULL)
            native_info = command_json(
                [str(args.latc), "inspect-native", "--json", str(image)])
            module_info = command_json(
                [str(args.latc), "inspect-module", "--json", str(module)])
            if (module_info["tbs"] != native_info["tbs"] or
                    module_info["pc_maps"] != native_info["pc_maps"]):
                raise RuntimeError("partial AOT v2 module for %s" % benchmark)
            subprocess.run([str(args.latc), "compile", str(guest), "-o",
                            str(wrapper), "--runner", str(args.aot_runner)],
                           check=True, env=base_env,
                           stdout=subprocess.DEVNULL)
            entry = {
                "benchmark": benchmark,
                "filename": filename,
                "guest_sha256": sha256(guest),
                "native_tbs": native_info["tbs"],
                "module_tbs": module_info["tbs"],
                "pc_maps": module_info["pc_maps"],
                "valid": False,
            }
            entries[benchmark] = entry
            manifest["programs"].append(entry)
            save_manifest(manifest_path, manifest)

        replace_run_link(args.spec_root, wrappers)
        for benchmark, filename, _strict_mode in programs:
            print("run %s train" % benchmark, flush=True)
            run_env = base_env.copy()
            run_env.update({
                "HOME": str(homes / benchmark),
                "LD_LIBRARY_PATH": str(args.runtime_dir) +
                    ((":" + run_env["LD_LIBRARY_PATH"])
                     if run_env.get("LD_LIBRARY_PATH") else ""),
                "LATX_AOT_V2_MODULE": str(modules / (filename + ".so")),
                "LATX_AOT_V2_SOURCE": str(guest_dir / filename),
                "LATX_AOT_V2_STRICT": "1",
                "LATC_DISABLE_PRETRANSLATE": "1",
                "LATC_STRICT_AOT": "1",
                "LATC_STATS_OUT": str(stats / (benchmark + ".json")),
            })
            Path(run_env["HOME"]).mkdir(exist_ok=True)
            log = logs / (benchmark + ".log")
            reported_time, raw, valid = run_spec(
                args.spec_root, "train", benchmark, run_env, log,
                timeout=args.timeout)
            runtime_stats = json.loads(
                (stats / (benchmark + ".json")).read_text())
            if runtime_stats["runtime_tb_gen_attempts"] or \
                    runtime_stats["runtime_tb_gen_calls"]:
                raise RuntimeError("runtime translation used by %s" % benchmark)
            saved_raw = raws / (benchmark + ".raw")
            shutil.copy2(raw, saved_raw)
            entries[benchmark].update({
                "reported_time": reported_time,
                "valid": valid,
                "runtime_tb_gen_attempts": 0,
                "runtime_tb_gen_calls": 0,
                "log": str(log),
                "raw": str(saved_raw),
            })
            save_manifest(manifest_path, manifest)
            print("PASS %s %.6f" % (benchmark, reported_time), flush=True)
    finally:
        replace_run_link(args.spec_root, old_target)

    print(json.dumps({"passed": len(manifest["programs"]),
                      "manifest": str(manifest_path)}))


if __name__ == "__main__":
    main()
