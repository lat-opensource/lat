#!/usr/bin/env python3
"""Regression test for per-application AOT coverage accounting."""

import json
import pathlib
import subprocess
import sys
import tempfile


checker = pathlib.Path(sys.argv[1]).resolve()
source = "a" * 64
def runtime(file_misses=0, submissions=0, failures=0):
    return (
        "latx: AOT v2 runtime stats direct_targets=0 "
        f"file_dispatch_misses={file_misses} nonfile_dispatch_misses=0 "
        f"compiler_submissions={submissions} "
        f"compiler_submission_failures={failures}\n")

with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    (root / "python.stderr").write_text(
        runtime(file_misses=1) +
        "latx: AOT v2 module stats source=%s range=0x1-0x2 "
        "module=registered aot_lookups=999 jit_fallbacks=1 "
        "registration_ns=1\n" % source)
    (root / "sqlite.stderr").write_text(
        runtime(file_misses=10) +
        "latx: AOT v2 module stats source=%s range=0x1-0x2 "
        "module=missing aot_lookups=0 jit_fallbacks=10 "
        "registration_ns=0\n" % source)
    (root / "redis-first.stderr").write_text(
        runtime() + "latx: AOT v2 fork child switched to JIT\n" +
        "latx: AOT v2 module stats source=%s range=0x1-0x2 "
        "module=registered aot_lookups=10 jit_fallbacks=0 "
        "registration_ns=1\n" % source)

    success = subprocess.run(
        [sys.executable, checker, root, "99.9", "python"], check=False)
    assert success.returncode == 0
    python_result = json.loads(
        (root / "python-aot-coverage.json").read_text())
    assert python_result["aot_percent"] == 99.9
    assert python_result["sources"][source]["jit_fallbacks"] == 1
    assert python_result["runtime_file_dispatch_misses"] == 1
    assert python_result["unattributed_file_dispatch_misses"] == 0

    failure = subprocess.run(
        [sys.executable, checker, "--require-no-fork-jit", root, "99.9",
         "sqlite", "redis"], check=False)
    assert failure.returncode == 1
    sqlite_result = json.loads(
        (root / "sqlite-aot-coverage.json").read_text())
    redis_result = json.loads(
        (root / "redis-aot-coverage.json").read_text())
    assert "coverage below requirement" in sqlite_result["failure_reasons"]
    assert "fork child switched to JIT" in redis_result["failure_reasons"]

    (root / "git.stderr").write_text(
        runtime(file_misses=2) +
        "latx: AOT v2 module stats source=%s range=0x1-0x2 "
        "module=registered aot_lookups=100 jit_fallbacks=0 "
        "registration_ns=1\n" % source)
    unattributed = subprocess.run(
        [sys.executable, checker, root, "100", "git"], check=False)
    assert unattributed.returncode == 1
    git_result = json.loads((root / "git-aot-coverage.json").read_text())
    assert git_result["unattributed_file_dispatch_misses"] == 2
    assert "coverage below requirement" in git_result["failure_reasons"]

    (root / "missing-runtime.stderr").write_text(
        "latx: AOT v2 module stats source=%s range=0x1-0x2 "
        "module=registered aot_lookups=10 jit_fallbacks=0 "
        "registration_ns=1\n" % source)
    missing_runtime = subprocess.run(
        [sys.executable, checker, root, "100", "missing-runtime"],
        check=False)
    assert missing_runtime.returncode == 1
    missing_result = json.loads(
        (root / "missing-runtime-aot-coverage.json").read_text())
    assert "no runtime statistics" in missing_result["failure_reasons"]

print("test-check-aot-v2-coverage: PASS")
