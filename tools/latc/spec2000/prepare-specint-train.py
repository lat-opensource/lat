#!/usr/bin/env python3
"""Build and validate latc AOT bundles for SPECint2000 train inputs."""

import argparse
import json
import os
import subprocess
from pathlib import Path

from specint import (aggregate_stats, merge_profile, replace_run_link,
                     run_spec, selected_programs, sha256)


def remove_matches(directory, pattern):
    for path in Path(directory).glob(pattern):
        path.unlink()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--latc", required=True, type=Path)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--spec-root", required=True, type=Path)
    parser.add_argument("--workdir", required=True, type=Path)
    parser.add_argument("--guest-dir", default="specbin/x64_gcc12_2_0")
    parser.add_argument("--max-profile-rounds", type=int, default=20)
    parser.add_argument("--benchmark", action="append", default=[])
    args = parser.parse_args()

    args.latc = args.latc.resolve()
    args.runner = args.runner.resolve()
    args.spec_root = args.spec_root.resolve()
    args.workdir = args.workdir.resolve()
    guest_dir = (args.spec_root / args.guest_dir).resolve()
    compile_aot = Path(__file__).resolve().parents[1] / "scripts" / "compile-aot.sh"
    programs = selected_programs(args.benchmark)
    for path in (args.latc, args.runner, compile_aot):
        if not path.is_file():
            raise SystemExit("missing required file: %s" % path)

    directories = {
        name: args.workdir / name for name in (
            "profile-stage", "profiles", "profile-stats", "profile-logs",
            "bundles", "strict-stats", "strict-logs", "homes", "tmp",
        )
    }
    for directory in directories.values():
        directory.mkdir(parents=True, exist_ok=True)
    profile_bin = args.workdir / "specbin-profile"
    final_bin = args.workdir / "specbin-aot"
    profile_bin.mkdir(exist_ok=True)
    final_bin.mkdir(exist_ok=True)

    run_link = args.spec_root / "specbin" / "run"
    old_target = os.readlink(str(run_link)) if run_link.is_symlink() else None
    if old_target is None:
        raise SystemExit("SPEC specbin/run must be a symlink")
    old_target = Path(old_target)
    if not old_target.is_absolute():
        old_target = (run_link.parent / old_target).resolve()

    manifest = {
        "suite": "SPECint2000", "input": "train",
        "runner": str(args.runner), "runner_sha256": sha256(args.runner),
        "programs": [],
    }
    base_env = os.environ.copy()
    base_env["TMPDIR"] = str(directories["tmp"])
    try:
        for benchmark, filename, strict_mode in programs:
            print("prepare %s" % benchmark, flush=True)
            guest = guest_dir / filename
            if not guest.is_file():
                raise RuntimeError("missing x86 guest: %s" % guest)
            stage = directories["profile-stage"] / filename
            raw_profile = directories["profiles"] / (benchmark + ".raw")
            profile = directories["profiles"] / (benchmark + ".profile")
            raw_profile.write_text("")
            subprocess.run([str(args.latc), "compile", str(guest), "-o", str(stage),
                            "--runner", str(args.runner)], check=True)
            profile_target = profile_bin / filename
            if profile_target.exists() or profile_target.is_symlink():
                profile_target.unlink()
            profile_target.symlink_to(stage)
            replace_run_link(args.spec_root, profile_bin)
            remove_matches(directories["profile-stats"], benchmark + "-*.json")
            env = base_env.copy()
            env.update({
                "HOME": str(directories["homes"] / (benchmark + "-profile")),
                "LATX_AOT": "0",
                "LATC_PROFILE_OUT": str(raw_profile),
                "LATC_STATS_OUT": str(directories["profile-stats"] /
                                      (benchmark + "-%p.json")),
            })
            Path(env["HOME"]).mkdir(parents=True, exist_ok=True)
            profile_time, profile_spec_raw, _profile_valid = run_spec(
                args.spec_root, "train", benchmark, env,
                directories["profile-logs"] / (benchmark + ".log"))
            profile_entries = merge_profile(raw_profile, profile)

            bundle = directories["bundles"] / filename
            final_target = final_bin / filename
            seen_supplements = set()
            for strict_round in range(1, args.max_profile_rounds + 1):
                subprocess.run([str(compile_aot), str(args.latc), str(args.runner),
                                str(guest), str(bundle), str(profile)],
                               check=True, env=base_env)
                if final_target.exists() or final_target.is_symlink():
                    final_target.unlink()
                final_target.symlink_to(bundle)
                replace_run_link(args.spec_root, final_bin)
                remove_matches(directories["strict-stats"], benchmark + "-*.json")
                env = base_env.copy()
                env.update({
                    "HOME": str(directories["homes"] / (benchmark + "-strict")),
                    "LATC_STATS_OUT": str(directories["strict-stats"] /
                                          (benchmark + "-%p.json")),
                })
                env["LATC_STRICT_AOT" if strict_mode == "full" else
                    "LATC_STRICT_PROGRAM_AOT"] = "1"
                Path(env["HOME"]).mkdir(parents=True, exist_ok=True)
                strict_time, strict_raw, strict_valid = run_spec(
                    args.spec_root, "train", benchmark, env,
                    directories["strict-logs"] /
                    (benchmark + "-%02d.log" % strict_round),
                    require_valid=False)
                stats_paths = list(directories["strict-stats"].glob(
                    benchmark + "-*.json"))
                if not stats_paths:
                    raise RuntimeError("no strict stats for %s" % benchmark)
                stats = aggregate_stats(stats_paths)
                if stats["runtime_program_tb_gen_attempts"]:
                    supplements = set(stats["runtime_program_first_pcs"])
                    new_supplements = supplements - seen_supplements
                    if not new_supplements:
                        raise RuntimeError("strict profile made no progress for %s: %s" %
                                           (benchmark, stats))
                    seen_supplements.update(new_supplements)
                    with raw_profile.open("a") as stream:
                        for pc in sorted(new_supplements):
                            stream.write("0x%x 1\n" % pc)
                    profile_entries = merge_profile(raw_profile, profile)
                    print("%s strict round %d added %s" %
                          (benchmark, strict_round,
                           ",".join("0x%x" % pc for pc in sorted(new_supplements))),
                          flush=True)
                    continue
                if strict_mode == "full" and stats["runtime_tb_gen_attempts"]:
                    raise RuntimeError("system translator entry remained for %s: %s" %
                                       (benchmark, stats))
                if not strict_valid:
                    raise RuntimeError("SPEC validation failed for %s: %s" %
                                       (benchmark, strict_raw))
                break
            else:
                raise RuntimeError("strict profile rounds exhausted for %s" % benchmark)
            inspect = subprocess.check_output(
                [str(args.latc), "inspect", "--json", str(bundle)], text=True)
            manifest["programs"].append({
                "benchmark": benchmark, "filename": filename,
                "guest_sha256": sha256(guest), "bundle_sha256": sha256(bundle),
                "profile_entries": profile_entries,
                "profile_reported_time": profile_time,
                "strict_reported_time": strict_time,
                "strict_rounds": strict_round,
                "strict_mode": strict_mode, "stats": stats,
                "inspect": json.loads(inspect),
                "profile_events": str(raw_profile),
                "profile_file": str(profile),
                "profile_stage": str(stage),
                "bundle": str(bundle),
                "profile_spec_raw": str(profile_spec_raw),
                "strict_spec_raw": str(strict_raw),
                "profile_log": str(directories["profile-logs"] /
                                   (benchmark + ".log")),
                "strict_log": str(directories["strict-logs"] /
                                  (benchmark + "-%02d.log" % strict_round)),
            })
            (args.workdir / "manifest.json").write_text(
                json.dumps(manifest, indent=2) + "\n")
    finally:
        replace_run_link(args.spec_root, old_target)

    print(json.dumps({"prepared": len(manifest["programs"]),
                      "manifest": str(args.workdir / "manifest.json")}))


if __name__ == "__main__":
    main()
