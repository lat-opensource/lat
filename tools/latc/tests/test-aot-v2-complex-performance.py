#!/usr/bin/env python3
"""Compare identical complex-application JIT and stable warm-AOT phases."""

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess


parser = argparse.ArgumentParser()
parser.add_argument("latcd")
parser.add_argument("latc")
parser.add_argument("runner")
parser.add_argument("runtime_dir")
parser.add_argument("rootfs")
parser.add_argument("cache")
parser.add_argument("workdir")
parser.add_argument("--rounds", type=int, default=5)
parser.add_argument("--min-aot-percent", type=float, default=100.0)
parser.add_argument("--applications", nargs="+",
                    choices=("python", "git", "sqlite", "redis"),
                    default=("python", "git", "sqlite", "redis"))
args = parser.parse_args()
if args.rounds < 5:
    parser.error("at least five alternating rounds are required")
if not 0 < args.min_aot_percent <= 100:
    parser.error("AOT coverage requirement must be in (0, 100]")

script = pathlib.Path(__file__).with_name("test-aot-v2-complex-apps.sh")
work = pathlib.Path(args.workdir)
work.mkdir(mode=0o700, parents=True, exist_ok=False)


def cache_manifest():
    result = {}
    root = pathlib.Path(args.cache)
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        tracked = path.suffix in (
            ".so", ".native", ".current", ".tbset", ".sha256")
        if path.is_file() and tracked:
            result[str(relative)] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


initial_manifest = cache_manifest()
if not initial_manifest:
    raise SystemExit("stable cache contains no AOT module")
application_binaries = {
    "python": ("python3",),
    "git": ("git",),
    "sqlite": ("sqlite3",),
    "redis": ("redis-server", "redis-cli"),
}
registered_sources = {}
for name in (name for application in args.applications
             for name in application_binaries[application]):
    source = pathlib.Path(args.rootfs) / "usr/bin" / name
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    current = pathlib.Path(args.cache) / f"{digest}.current"
    if not current.is_file():
        raise SystemExit(f"stable cache has no registered module for {name}")
    registered_sources[name] = digest


def run_coverage_validation(label):
    phase_work = work / label
    environment = os.environ.copy()
    environment.update({
        "LATC_COMPLEX_PHASES": "warm",
        "LATC_COMPLEX_APPLICATIONS": " ".join(args.applications),
        "LATC_COMPLEX_CACHE_SOURCE": args.cache,
        "LATC_COMPLEX_WARM_SOCKET": "0",
        "LATC_COMPLEX_WARM_REPORT": "1",
        "LATC_COMPLEX_REQUIRE_REGISTERED": "1",
        "LATC_COMPLEX_MIN_AOT_PERCENT": str(args.min_aot_percent),
        "LATC_COMPLEX_REQUIRE_NO_FORK_JIT": "1",
    })
    command = ["sh", str(script), args.latcd, args.latc, args.runner,
               args.runtime_dir, args.rootfs, str(phase_work)]
    completed = subprocess.run(command, env=environment,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
    (work / f"{label}.stdout").write_text(completed.stdout)
    (work / f"{label}.stderr").write_text(completed.stderr)
    if completed.returncode:
        raise SystemExit(f"coverage validation {label} failed: "
                         f"{completed.returncode}")
    coverage = {}
    for application in args.applications:
        path = phase_work / "warm" / f"{application}-aot-coverage.json"
        coverage[application] = json.loads(path.read_text())
    shutil.rmtree(phase_work / "cache")
    return coverage


coverage_before = run_coverage_validation("coverage-before")
results = []
for round_number in range(1, args.rounds + 1):
    order = ("jit", "warm") if round_number % 2 else ("warm", "jit")
    pair = {"round": round_number, "order": list(order)}
    for mode in order:
        phase_work = work / f"round-{round_number}-{mode}"
        environment = os.environ.copy()
        environment.update({
            "LATC_COMPLEX_PHASES": mode,
            "LATC_COMPLEX_APPLICATIONS": " ".join(args.applications),
            "LATC_COMPLEX_WARM_SOCKET": "0",
            "LATC_COMPLEX_WARM_REPORT": "0",
            "LATC_COMPLEX_REQUIRE_REGISTERED": "0",
        })
        if mode == "warm":
            environment["LATC_COMPLEX_CACHE_SOURCE"] = args.cache
        command = ["sh", str(script), args.latcd, args.latc, args.runner,
                   args.runtime_dir, args.rootfs, str(phase_work)]
        completed = subprocess.run(command, env=environment,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True)
        (work / f"round-{round_number}-{mode}.stdout").write_text(
            completed.stdout)
        (work / f"round-{round_number}-{mode}.stderr").write_text(
            completed.stderr)
        if completed.returncode:
            raise SystemExit(
                f"{mode} round {round_number} failed: {completed.returncode}")
        phase = json.loads((phase_work / mode / "result.json").read_text())
        shutil.rmtree(phase_work / "cache")
        pair[mode] = phase
    pair["warm_to_jit"] = (pair["warm"]["application_total_ns"] /
                           pair["jit"]["application_total_ns"])
    results.append(pair)

coverage_after = run_coverage_validation("coverage-after")

final_manifest = cache_manifest()
if final_manifest != initial_manifest:
    raise SystemExit("stable source cache changed during performance runs")
failed = [pair["round"] for pair in results if
          pair["warm"]["application_total_ns"] >=
          pair["jit"]["application_total_ns"]]
coverage = {"before": coverage_before, "after": coverage_after}
compiler_requests = sum(
    result["compiler_submissions"] for phase in coverage.values()
    for result in phase.values())
compiler_failures = sum(
    result["compiler_submission_failures"] for phase in coverage.values()
    for result in phase.values())
output = {
    "cache_files": initial_manifest,
    "clock": "CLOCK_MONOTONIC",
    "coverage_required_percent": args.min_aot_percent,
    "compiler_failures": compiler_failures,
    "compiler_requests": compiler_requests,
    "coverage": coverage,
    "failed_rounds": failed,
    "registered_sources": registered_sources,
    "rounds": results,
}
(work / "performance.json").write_text(
    json.dumps(output, indent=2, sort_keys=True) + "\n")
print(json.dumps({
    "failed_rounds": failed,
    "ratios": [round(pair["warm_to_jit"], 6) for pair in results],
    "rounds": args.rounds,
}, sort_keys=True))
if failed:
    raise SystemExit(1)
