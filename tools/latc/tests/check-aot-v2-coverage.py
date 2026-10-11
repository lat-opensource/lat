#!/usr/bin/env python3
"""Check file-backed AOT coverage for complex-application stderr logs."""

import argparse
import json
import pathlib
import re


parser = argparse.ArgumentParser()
parser.add_argument("phase_root", type=pathlib.Path)
parser.add_argument("required_percent", type=float)
parser.add_argument("applications", nargs="+")
parser.add_argument("--require-no-fork-jit", action="store_true")
args = parser.parse_args()

module_pattern = re.compile(
    r"source=([0-9a-f]{64}).*module=(\w+) "
    r"aot_lookups=(\d+) jit_fallbacks=(\d+)")
runtime_pattern = re.compile(
    r"AOT v2 runtime stats .*?\bfile_dispatch_misses=(\d+) .*"
    r"compiler_submissions=(\d+) compiler_submission_failures=(\d+)")
failed = []

for application in args.applications:
    paths = sorted(args.phase_root.rglob(f"{application}*.stderr"))
    aot = fallback = submissions = submission_failures = 0
    runtime_file_misses = runtime_reports = 0
    states = {}
    sources = {}
    fork_jit = 0
    for path in paths:
        text = path.read_text(errors="replace")
        fork_jit += text.count("AOT v2 fork child switched to JIT")
        for source, state, hits, misses in module_pattern.findall(text):
            aot += int(hits)
            fallback += int(misses)
            states[state] = states.get(state, 0) + 1
            source_result = sources.setdefault(source, {
                "aot_lookups": 0,
                "jit_fallbacks": 0,
                "states": {},
            })
            source_result["aot_lookups"] += int(hits)
            source_result["jit_fallbacks"] += int(misses)
            source_result["states"][state] = \
                source_result["states"].get(state, 0) + 1
        for file_misses, submitted, compiler_failed in \
                runtime_pattern.findall(text):
            runtime_file_misses += int(file_misses)
            submissions += int(submitted)
            submission_failures += int(compiler_failed)
            runtime_reports += 1
    unattributed = max(0, runtime_file_misses - fallback)
    total = aot + fallback + unattributed
    percent = 100.0 * aot / total if total else 0.0
    result = {
        "aot_lookups": aot,
        "aot_percent": percent,
        "application": application,
        "compiler_submission_failures": submission_failures,
        "compiler_submissions": submissions,
        "fork_child_jit": fork_jit,
        "jit_fallbacks": fallback,
        "module_states": states,
        "required_percent": args.required_percent,
        "runtime_file_dispatch_misses": runtime_file_misses,
        "runtime_reports": runtime_reports,
        "sources": sources,
        "stderr_files": [str(path.relative_to(args.phase_root))
                         for path in paths],
        "unattributed_file_dispatch_misses": unattributed,
    }
    output = args.phase_root / f"{application}-aot-coverage.json"
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print("%s AOT coverage: %.6f%% (%d/%d) submissions=%d fork_jit=%d" %
          (application, percent, aot, total, submissions, fork_jit))
    reasons = []
    if not paths or not total:
        reasons.append("no module statistics")
    if not runtime_reports:
        reasons.append("no runtime statistics")
    if fallback > runtime_file_misses:
        reasons.append("file dispatch accounting mismatch")
    if percent + 1e-12 < args.required_percent:
        reasons.append("coverage below requirement")
    if submissions or submission_failures:
        reasons.append("warm run submitted compiler work")
    if args.require_no_fork_jit and fork_jit:
        reasons.append("fork child switched to JIT")
    if reasons:
        result["failure_reasons"] = reasons
        output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
        failed.append(application)

if failed:
    raise SystemExit("AOT coverage failed: " + ", ".join(failed))
