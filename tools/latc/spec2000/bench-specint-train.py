#!/usr/bin/env python3
"""Compare latc bundles with matching-runner LAT AOT on SPECint train."""

import argparse
import hashlib
import json
import os
import platform
import shlex
import struct
import subprocess
from pathlib import Path

from specint import (aggregate_stats, geometric_mean, replace_run_link, run_spec,
                     sample_summary, selected_programs, sha256)


FOOTER = struct.Struct("<8sII11Q64s64s160s")


def bundle_runner_hash(path):
    with Path(path).open("rb") as stream:
        stream.seek(-FOOTER.size, os.SEEK_END)
        footer = FOOTER.unpack(stream.read(FOOTER.size))
        if footer[0] != b"LATCBND1":
            raise RuntimeError("not a latc bundle: %s" % path)
        runner_size = footer[3]
        stream.seek(0)
        digest = hashlib.sha256()
        left = runner_size
        while left:
            block = stream.read(min(left, 1024 * 1024))
            if not block:
                raise RuntimeError("short runner in bundle: %s" % path)
            digest.update(block)
            left -= len(block)
    return digest.hexdigest(), runner_size


def shell_quote(value):
    return shlex.quote(str(value))


def write_wrapper(path, lines):
    path.write_text("#!/bin/sh\nset -eu\n" + "\n".join(lines) + "\n")
    path.chmod(0o755)


def metadata(command, fallback=""):
    try:
        return subprocess.check_output(command, text=True,
                                       stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return fallback


def save_report(report, workdir):
    output = workdir / "report.json"
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")


def summarize(report, programs, workdir):
    speedups = []
    rows = []
    for benchmark, _filename, _strict in programs:
        latc = sample_summary(report["samples"][benchmark]["latc"])
        lat = sample_summary(report["samples"][benchmark]["lat_aot"])
        speedup = lat["median"] / latc["median"]
        speedups.append(speedup)
        rows.append((benchmark, latc, lat, speedup))
        report["results"][benchmark] = {
            "latc": latc, "lat_aot": lat, "speedup": speedup,
        }
    report["geomean_speedup"] = geometric_mean(speedups)
    save_report(report, workdir)

    with (workdir / "report.tsv").open("w") as stream:
        stream.write("benchmark\tlatc_median_s\tlat_aot_median_s\t"
                     "speedup\tlatc_cv\tlat_aot_cv\n")
        for benchmark, latc, lat, speedup in rows:
            stream.write("%s\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\n" %
                         (benchmark, latc["median"], lat["median"], speedup,
                          latc["cv"], lat["cv"]))
    with (workdir / "report.md").open("w") as stream:
        stream.write("# SPECint2000 train: latc vs LAT AOT\n\n")
        stream.write("| benchmark | latc median (s) | LAT AOT median (s) | speedup |\n")
        stream.write("|---|---:|---:|---:|\n")
        for benchmark, latc, lat, speedup in rows:
            stream.write("| %s | %.6f | %.6f | %.4fx |\n" %
                         (benchmark, latc["median"], lat["median"], speedup))
        stream.write("\nGeometric mean speedup: **%.4fx**\n" %
                     report["geomean_speedup"])
        stream.write("\nThe baseline uses the exact runner embedded in every bundle. "
                     "A speedup above 1.0 means the latc bundle is faster.\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--spec-root", required=True, type=Path)
    parser.add_argument("--bundle-dir", required=True, type=Path)
    parser.add_argument("--workdir", required=True, type=Path)
    parser.add_argument("--guest-dir", default="specbin/x64_gcc12_2_0")
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--extra-rounds", type=int, default=5)
    parser.add_argument("--cv-threshold", type=float, default=0.03)
    parser.add_argument("--cpu", type=int, default=2)
    parser.add_argument("--source-commit", default="")
    parser.add_argument("--benchmark", action="append", default=[])
    args = parser.parse_args()
    if args.rounds < 1 or args.extra_rounds < 0:
        raise SystemExit("round counts must be positive")

    args.runner = args.runner.resolve()
    args.spec_root = args.spec_root.resolve()
    args.bundle_dir = args.bundle_dir.resolve()
    args.workdir = args.workdir.resolve()
    guest_dir = (args.spec_root / args.guest_dir).resolve()
    programs = selected_programs(args.benchmark)
    runner_hash = sha256(args.runner)
    args.workdir.mkdir(parents=True, exist_ok=True)
    logs = args.workdir / "logs"
    raws = args.workdir / "raw"
    wrappers = args.workdir / "wrappers"
    homes = args.workdir / "homes"
    stats = args.workdir / "stats"
    for directory in (logs, raws, wrappers, homes, stats):
        directory.mkdir(parents=True, exist_ok=True)
    latc_bin = wrappers / "latc"
    lat_bin = wrappers / "lat-aot"
    latc_bin.mkdir(exist_ok=True)
    lat_bin.mkdir(exist_ok=True)

    bundles = {}
    for benchmark, filename, strict_mode in programs:
        guest = guest_dir / filename
        bundle = args.bundle_dir / filename
        if not guest.is_file() or not bundle.is_file():
            raise SystemExit("missing input for %s: %s or %s" %
                             (benchmark, guest, bundle))
        embedded_hash, runner_size = bundle_runner_hash(bundle)
        if embedded_hash != runner_hash:
            raise SystemExit("runner mismatch for %s: bundle=%s runner=%s" %
                             (benchmark, embedded_hash, runner_hash))
        bundles[benchmark] = {
            "path": str(bundle), "sha256": sha256(bundle),
            "runner_size": runner_size,
        }
        latc_home = homes / (benchmark + "-latc")
        lat_home = homes / (benchmark + "-lat-aot")
        latc_home.mkdir(exist_ok=True)
        lat_home.mkdir(exist_ok=True)
        strict_var = "LATC_STRICT_AOT" if strict_mode == "full" else \
                     "LATC_STRICT_PROGRAM_AOT"
        write_wrapper(latc_bin / filename, [
            "export HOME=%s" % shell_quote(latc_home),
            "export %s=1" % strict_var,
            "export LATC_STATS_OUT=%s" % shell_quote(
                stats / (benchmark + "-latc-%p.json")),
            "exec taskset -c %d %s \"$@\"" %
            (args.cpu, shell_quote(bundle)),
        ])
        write_wrapper(lat_bin / filename, [
            "export HOME=%s" % shell_quote(lat_home),
            "export LATX_AOT=1",
            "exec taskset -c %d %s %s \"$@\"" %
            (args.cpu, shell_quote(args.runner), shell_quote(guest)),
        ])

    run_link = args.spec_root / "specbin" / "run"
    if not run_link.is_symlink():
        raise SystemExit("SPEC specbin/run must be a symlink")
    old_target = Path(os.readlink(str(run_link)))
    if not old_target.is_absolute():
        old_target = (run_link.parent / old_target).resolve()
    env = os.environ.copy()
    report = {
        "suite": "SPECint2000", "input": "train",
        "latc_mode": "embedded LAT AOT bundle with strict runtime checks",
        "baseline": "same LAT runner with LATX_AOT=1 and its normal cache",
        "host": platform.node(), "kernel": platform.release(),
        "machine": platform.machine(), "cpu": args.cpu,
        "lscpu": metadata(["lscpu"]), "governor": metadata([
            "cat", "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor" %
            args.cpu]),
        "source_commit": args.source_commit,
        "runner": str(args.runner), "runner_sha256": runner_hash,
        "rounds": args.rounds, "extra_rounds": args.extra_rounds,
        "cv_threshold": args.cv_threshold, "bundles": bundles,
        "samples": {item[0]: {"latc": [], "lat_aot": []}
                    for item in programs},
        "sample_metadata": [], "results": {},
    }

    def measure(benchmark, mode, index, warmup=False):
        target = latc_bin if mode == "latc" else lat_bin
        replace_run_link(args.spec_root, target)
        stats_before = set(stats.glob(benchmark + "-latc-*.json"))
        suffix = "warmup" if warmup else "%02d" % index
        log = logs / (benchmark + "-" + mode + "-" + suffix + ".log")
        value, raw, _valid = run_spec(args.spec_root, "train", benchmark, env, log)
        saved_raw = raws / (benchmark + "-" + mode + "-" + suffix + ".raw")
        saved_raw.write_bytes(raw.read_bytes())
        runtime_stats = None
        if mode == "latc":
            stats_after = set(stats.glob(benchmark + "-latc-*.json"))
            new_stats = stats_after - stats_before
            if not new_stats:
                raise RuntimeError("latc produced no runtime stats for %s" % benchmark)
            runtime_stats = aggregate_stats(new_stats)
        if not warmup:
            report["samples"][benchmark][mode].append(value)
            report["sample_metadata"].append({
                "benchmark": benchmark, "mode": mode, "round": index,
                "reported_time": value, "loadavg": Path("/proc/loadavg").read_text().strip(),
                "raw": str(saved_raw), "log": str(log),
                "runtime_stats": runtime_stats,
            })
            save_report(report, args.workdir)
        print("%s %s %s %.6f" %
              (benchmark, mode, "warmup" if warmup else index, value), flush=True)

    try:
        for benchmark, _filename, _strict in programs:
            measure(benchmark, "lat_aot", 0, warmup=True)
            measure(benchmark, "latc", 0, warmup=True)
            for index in range(1, args.rounds + 1):
                modes = ("latc", "lat_aot") if index % 2 else ("lat_aot", "latc")
                for mode in modes:
                    measure(benchmark, mode, index)
            current = [sample_summary(report["samples"][benchmark][mode])["cv"]
                       for mode in ("latc", "lat_aot")]
            if max(current) > args.cv_threshold:
                for index in range(args.rounds + 1,
                                   args.rounds + args.extra_rounds + 1):
                    modes = ("latc", "lat_aot") if index % 2 else \
                            ("lat_aot", "latc")
                    for mode in modes:
                        measure(benchmark, mode, index)
    finally:
        replace_run_link(args.spec_root, old_target)

    summarize(report, programs, args.workdir)
    print(json.dumps({"report": str(args.workdir / "report.json"),
                      "geomean_speedup": report["geomean_speedup"]}))


if __name__ == "__main__":
    main()
