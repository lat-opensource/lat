#!/usr/bin/env python3
"""Compare M4, AOT v2, old LAT AOT/TU, and native SPEC train."""

import argparse
import json
import os
import platform
import shlex
import shutil
from pathlib import Path

from specint import (geometric_mean, replace_run_link, run_spec,
                     sample_summary, selected_programs, sha256,
                     strict_aot_v2_env_lines)


def quote(value):
    return shlex.quote(str(value))


def wrapper(path, lines):
    path.write_text("#!/bin/sh\nset -eu\n" + "\n".join(lines) + "\n")
    path.chmod(0o755)


def save(report, workdir):
    (workdir / "report.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n")


def save_markdown(report, workdir):
    lines = [
        "# SPECint2000 train performance baseline",
        "",
        "Runtime is the median of %d measured rounds. Efficiency is "
        "native runtime/runtime; higher is faster." % report["rounds"],
        "",
        "Host: `%s`; kernel: `%s`; CPU: `%s`; LAT cache: `%s`; "
        "ASLR disabled: `%s`." %
        (report["host"], report["kernel"], report["cpu"],
         report["lat_cache"], str(report["aslr_disabled"]).lower()),
        "",
        "| Benchmark | M4 runtime (s) | M4 efficiency | "
        "AOT v2 runtime (s) | AOT v2 efficiency | "
        "Old AOT runtime (s) | Old AOT efficiency | "
        "LA native runtime (s) | LA native efficiency |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for benchmark, result in report["results"].items():
        lines.append(
            "| %s | %.6f | %.2f%% | %.6f | %.2f%% | "
            "%.6f | %.2f%% | %.6f | %.2f%% |" %
            (benchmark, result["m4"]["median"],
             result["translation_efficiency_percent"]["m4"],
             result["aot_v2"]["median"],
             result["translation_efficiency_percent"]["aot_v2"],
             result["old_aot"]["median"],
             result["translation_efficiency_percent"]["old_aot"],
             result["native"]["median"],
             result["translation_efficiency_percent"]["native"]))
    geomean = report["geomean_translation_efficiency_percent"]
    lines.extend([
        "| **Geometric mean** | - | **%.2f%%** | - | **%.2f%%** | "
        "- | **%.2f%%** | - | **%.2f%%** |" %
        (geomean["m4"], geomean["aot_v2"], geomean["old_aot"],
         geomean["native"]),
        "",
        "AOT v2 geometric-mean speedup: %.4fx vs M4, %.4fx vs old AOT, "
        "and %.4fx vs native." %
        (report["geomean_aot_v2_speedup"]["m4"],
         report["geomean_aot_v2_speedup"]["old_aot"],
         report["geomean_aot_v2_speedup"]["native"]),
        "",
    ])
    (workdir / "baseline.md").write_text("\n".join(lines))


def aot_cache_files(home):
    cache = home / ".cache" / "latx"
    return sorted(path for path in cache.glob("*.aot2")
                  if path.stat().st_size > 0)


def cpu_frequency_settings(cpu):
    root = Path("/sys/devices/system/cpu/cpu%d/cpufreq" % cpu)
    result = {}
    for name in ("scaling_driver", "scaling_governor", "scaling_min_freq",
                 "scaling_max_freq"):
        path = root / name
        if path.is_file():
            result[name] = path.read_text().strip()
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--latc-dir", required=True, type=Path)
    parser.add_argument("--aot-v2-dir", required=True, type=Path)
    parser.add_argument("--aot-v2-module-dir", required=True, type=Path)
    parser.add_argument("--aot-v2-runtime-dir", required=True, type=Path)
    parser.add_argument("--native-dir", required=True, type=Path)
    parser.add_argument("--spec-root", required=True, type=Path)
    parser.add_argument("--workdir", required=True, type=Path)
    parser.add_argument("--guest-dir", default="specbin/x64_gcc12_2_0")
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--cpu", type=int, default=2)
    parser.add_argument("--timeout", type=float, default=90.0)
    parser.add_argument("--lat-cache", choices=("warm", "cold"),
                        default="warm")
    parser.add_argument("--disable-aslr", action="store_true")
    parser.add_argument("--benchmark", action="append", default=[])
    args = parser.parse_args()

    args.runner = args.runner.resolve()
    args.latc_dir = args.latc_dir.resolve()
    args.aot_v2_dir = args.aot_v2_dir.resolve()
    args.aot_v2_module_dir = args.aot_v2_module_dir.resolve()
    args.aot_v2_runtime_dir = args.aot_v2_runtime_dir.resolve()
    args.native_dir = args.native_dir.resolve()
    args.spec_root = args.spec_root.resolve()
    args.workdir = args.workdir.resolve()
    if not args.aot_v2_runtime_dir.is_dir():
        raise SystemExit("missing AOT v2 runtime directory: %s" %
                         args.aot_v2_runtime_dir)
    guest_dir = (args.spec_root / args.guest_dir).resolve()
    programs = selected_programs(args.benchmark)
    modes = ("m4", "aot_v2", "old_aot", "native")
    args.workdir.mkdir(parents=True, exist_ok=True)
    wrappers = args.workdir / "wrappers"
    logs = args.workdir / "logs"
    raws = args.workdir / "raws"
    homes = args.workdir / "homes"
    for directory in (wrappers, logs, raws, homes):
        directory.mkdir(exist_ok=True)
    mode_dirs = {mode: wrappers / mode for mode in modes}
    for directory in mode_dirs.values():
        directory.mkdir(exist_ok=True)

    command_prefix = "taskset -c %d" % args.cpu
    if args.disable_aslr:
        command_prefix += " setarch %s -R" % quote(platform.machine())

    for benchmark, filename, _strict in programs:
        guest = guest_dir / filename
        m4 = args.latc_dir / filename
        aot_v2 = args.aot_v2_dir / filename
        aot_v2_module = args.aot_v2_module_dir / (filename + ".so")
        native = args.native_dir / filename
        for path in (guest, m4, aot_v2, aot_v2_module, native):
            if not path.is_file():
                raise SystemExit("missing %s input: %s" % (benchmark, path))
        home = homes / benchmark
        home.mkdir(exist_ok=True)
        wrapper(mode_dirs["m4"] / filename, [
            "exec %s %s \"$@\"" % (command_prefix, quote(m4)),
        ])
        wrapper(mode_dirs["aot_v2"] / filename,
                strict_aot_v2_env_lines(
                    args.aot_v2_runtime_dir, aot_v2_module, guest) + [
            "exec %s %s \"$@\"" % (command_prefix, quote(aot_v2)),
        ])
        wrapper(mode_dirs["old_aot"] / filename, [
            "export HOME=%s" % quote(home),
            "export LD_LIBRARY_PATH=%s${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" %
            quote(args.runner.parent),
            "export LATX_AOT=1",
            "export LATX_TU=1",
            "exec %s %s %s \"$@\"" %
            (command_prefix, quote(args.runner), quote(guest)),
        ])
        wrapper(mode_dirs["native"] / filename, [
            "exec %s %s \"$@\"" % (command_prefix, quote(native)),
        ])

    report = {
        "suite": "SPECint2000", "input": "train",
        "host": platform.node(), "kernel": platform.release(),
        "machine": platform.machine(), "cpu": args.cpu,
        "aslr_disabled": args.disable_aslr,
        "rounds": args.rounds, "lat_cache": args.lat_cache,
        "cpu_frequency_settings": cpu_frequency_settings(args.cpu),
        "runner": str(args.runner), "runner_sha256": sha256(args.runner),
        "m4_dir": str(args.latc_dir),
        "aot_v2_dir": str(args.aot_v2_dir),
        "aot_v2_module_dir": str(args.aot_v2_module_dir),
        "aot_v2_runtime_dir": str(args.aot_v2_runtime_dir),
        "native_dir": str(args.native_dir),
        "input_sha256": {
            benchmark: {
                "guest": sha256(guest_dir / filename),
                "m4": sha256(args.latc_dir / filename),
                "aot_v2": sha256(args.aot_v2_dir / filename),
                "aot_v2_module": sha256(
                    args.aot_v2_module_dir / (filename + ".so")),
                "native": sha256(args.native_dir / filename),
            }
            for benchmark, filename, _strict in programs
        },
        "aot_cache": {},
        "samples": {benchmark: {mode: [] for mode in modes}
                    for benchmark, _filename, _strict in programs},
        "results": {},
    }
    run_link = args.spec_root / "specbin" / "run"
    old_target = Path(os.readlink(run_link))
    if not old_target.is_absolute():
        old_target = (run_link.parent / old_target).resolve()
    env = os.environ.copy()

    def measure(benchmark, mode, index, warmup=False):
        replace_run_link(args.spec_root, mode_dirs[mode])
        suffix = "warmup" if warmup else "%02d" % index
        log = logs / (benchmark + "-" + mode + "-" + suffix + ".log")
        run_env = env.copy()
        if mode == "old_aot" and args.lat_cache == "cold":
            shutil.rmtree(homes / benchmark / ".cache" / "latx",
                          ignore_errors=True)
        value, raw, valid = run_spec(args.spec_root, "train", benchmark,
                                     run_env, log, timeout=args.timeout)
        if not valid:
            raise RuntimeError("invalid SPEC result: %s %s" %
                               (benchmark, mode))
        (raws / (benchmark + "-" + mode + "-" + suffix + ".raw")).write_bytes(
            raw.read_bytes())
        if not warmup:
            report["samples"][benchmark][mode].append(value)
            save(report, args.workdir)
        print("%s %s %s %.6f" %
              (benchmark, mode, suffix, value), flush=True)
        return value

    try:
        for benchmark, _filename, _strict in programs:
            measure(benchmark, "m4", 0, warmup=True)
            measure(benchmark, "aot_v2", 0, warmup=True)
            measure(benchmark, "native", 0, warmup=True)
            if args.lat_cache == "warm":
                home = homes / benchmark
                files = []
                generation_runs = 0
                while generation_runs < 3 and not files:
                    generation_runs += 1
                    measure(benchmark, "old_aot", generation_runs,
                            warmup=True)
                    files = aot_cache_files(home)
                if not files:
                    raise RuntimeError(
                        "%s did not create a non-empty AOT cache in %s" %
                        (benchmark, home / ".cache" / "latx"))
                measure(benchmark, "old_aot", generation_runs + 1,
                        warmup=True)
                files = aot_cache_files(home)
                report["aot_cache"][benchmark] = {
                    "generation_runs": generation_runs,
                    "hot_verification_runs": 1,
                    "files": [{"name": path.name,
                               "size": path.stat().st_size}
                              for path in files],
                }
                save(report, args.workdir)
            else:
                measure(benchmark, "old_aot", 0, warmup=True)
            for index in range(1, args.rounds + 1):
                order = modes[index % len(modes):] + modes[:index % len(modes)]
                for mode in order:
                    measure(benchmark, mode, index)
    finally:
        replace_run_link(args.spec_root, old_target)

    aot_v2_speedup = {mode: [] for mode in modes if mode != "aot_v2"}
    translation_efficiency_percent = {mode: [] for mode in modes}
    for benchmark, _filename, _strict in programs:
        result = {mode: sample_summary(report["samples"][benchmark][mode])
                  for mode in modes}
        result["aot_v2_speedup"] = {
            mode: result[mode]["median"] / result["aot_v2"]["median"]
            for mode in modes if mode != "aot_v2"
        }
        native_runtime = result["native"]["median"]
        result["translation_efficiency_percent"] = {
            mode: native_runtime / result[mode]["median"] * 100.0
            for mode in modes
        }
        report["results"][benchmark] = result
        for mode in aot_v2_speedup:
            aot_v2_speedup[mode].append(result["aot_v2_speedup"][mode])
        for mode in modes:
            translation_efficiency_percent[mode].append(
                result["translation_efficiency_percent"][mode])
    report["geomean_aot_v2_speedup"] = {
        mode: geometric_mean(values)
        for mode, values in aot_v2_speedup.items()
    }
    report["geomean_translation_efficiency_percent"] = {
        mode: geometric_mean(translation_efficiency_percent[mode])
        for mode in modes
    }
    save(report, args.workdir)
    save_markdown(report, args.workdir)
    print(json.dumps({
        "report": str(args.workdir / "report.json"),
        "baseline": str(args.workdir / "baseline.md"),
        "geomean_aot_v2_speedup": report["geomean_aot_v2_speedup"],
        "geomean_translation_efficiency_percent":
            report["geomean_translation_efficiency_percent"],
    }))


if __name__ == "__main__":
    main()
