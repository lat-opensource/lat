#!/usr/bin/env python3
"""Shared SPECint2000 metadata and result helpers."""

import hashlib
import json
import math
import os
import re
import shlex
import signal
import statistics
import subprocess
import time
from pathlib import Path


PROGRAMS = (
    ("164.gzip", "gzip_base.Of.gcc830.dyn", "full"),
    ("175.vpr", "vpr_base.Of.gcc830.dyn", "full"),
    ("176.gcc", "cc1_base.Of.gcc830.dyn", "full"),
    ("181.mcf", "mcf_base.Of.gcc830.dyn", "full"),
    ("186.crafty", "crafty_base.Of.gcc830.dyn", "full"),
    ("197.parser", "parser_base.Of.gcc830.dyn", "full"),
    ("252.eon", "eon_base.Of.gcc830.dyn", "full"),
    ("253.perlbmk", "perlbmk_base.Of.gcc830.dyn", "program"),
    ("254.gap", "gap_base.Of.gcc830.dyn", "full"),
    ("255.vortex", "vortex_base.Of.gcc830.dyn", "program"),
    ("256.bzip2", "bzip2_base.Of.gcc830.dyn", "full"),
    ("300.twolf", "twolf_base.Of.gcc830.dyn", "full"),
)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def strict_aot_v2_env_lines(runtime_dir, module, guest):
    """Return shell exports that require execution through an AOT v2 module."""
    quote = shlex.quote
    return [
        "export LD_LIBRARY_PATH=%s${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" %
        quote(str(runtime_dir)),
        "export LATX_AOT_V2_MODULE=%s" % quote(str(module)),
        "export LATX_AOT_V2_SOURCE=%s" % quote(str(guest)),
        "export LATX_AOT_V2_STRICT=1",
        "export LATC_DISABLE_PRETRANSLATE=1",
        "export LATC_STRICT_AOT=1",
    ]


def selected_programs(names):
    if not names:
        return PROGRAMS
    wanted = set(names)
    selected = tuple(item for item in PROGRAMS if item[0] in wanted)
    unknown = wanted - {item[0] for item in selected}
    if unknown:
        raise SystemExit("unknown benchmark(s): " + ", ".join(sorted(unknown)))
    return selected


def replace_run_link(spec_root, target):
    run_link = Path(spec_root) / "specbin" / "run"
    if run_link.exists() and not run_link.is_symlink():
        raise RuntimeError("SPEC specbin/run is not a symlink")
    temporary = run_link.with_name(".run.latc.%d" % os.getpid())
    if temporary.exists() or temporary.is_symlink():
        temporary.unlink()
    temporary.symlink_to(Path(target).resolve())
    os.replace(str(temporary), str(run_link))


def newest_raw(spec_root, before):
    result_dir = Path(spec_root) / "result"
    candidates = set(result_dir.glob("CINT2000.*.raw")) - before
    if not candidates:
        raise RuntimeError("runspec did not create a CINT2000 raw result")
    return max(candidates, key=lambda path: path.stat().st_mtime_ns)


def process_session_members(session_id):
    members = []
    for stat_path in Path("/proc").glob("[0-9]*/stat"):
        try:
            stat = stat_path.read_text()
            fields = stat[stat.rfind(")") + 2:].split()
            if (len(fields) >= 4 and fields[0] != "Z" and
                    int(fields[3]) == session_id):
                members.append(int(stat_path.parent.name))
        except (FileNotFoundError, ProcessLookupError, ValueError):
            continue
    return members


def signal_process_session(session_id, sig):
    for pid in process_session_members(session_id):
        try:
            os.kill(pid, sig)
        except ProcessLookupError:
            pass


def run_spec(spec_root, size, benchmark, env, log_path, require_valid=True,
             timeout=None):
    result_dir = Path(spec_root) / "result"
    before = set(result_dir.glob("CINT2000.*.raw"))
    command = [str(Path(spec_root) / "myrun1.sh"), size, benchmark]
    with Path(log_path).open("w") as log:
        process = subprocess.Popen(command, cwd=str(spec_root), env=env,
                                   stdout=log, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        try:
            process.wait(timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            signal_process_session(process.pid, signal.SIGTERM)
            deadline = time.monotonic() + 5
            while process_session_members(process.pid) and \
                    time.monotonic() < deadline:
                time.sleep(0.05)
            signal_process_session(process.pid, signal.SIGKILL)
            process.wait()
            raise RuntimeError("runspec timed out after %s seconds for %s; "
                               "see %s" %
                               (timeout, benchmark, log_path)) from exc
    if process.returncode:
        raise RuntimeError("runspec failed for %s; see %s" %
                           (benchmark, log_path))
    raw = newest_raw(spec_root, before)
    text = raw.read_text(errors="replace")
    match = re.search(r"\.reported_time:\s*([0-9.]+)", text)
    result_name = re.escape(benchmark.replace(".", "_"))
    valid = re.search(r"results\.%s\.base\.\d+\.valid:\s*1\s*$" %
                      result_name, text, re.MULTILINE) is not None
    if not match or (require_valid and not valid):
        raise RuntimeError("invalid SPEC result for %s: %s" %
                           (benchmark, raw))
    return float(match.group(1)), raw, valid


def merge_profile(source, destination):
    counts = {}
    with Path(source).open() as stream:
        for line_no, line in enumerate(stream, 1):
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            fields = line.split()
            if len(fields) != 2:
                raise RuntimeError("%s:%d: invalid profile line" %
                                   (source, line_no))
            pc, count = int(fields[0], 0), int(fields[1], 0)
            counts[pc] = counts.get(pc, 0) + count
    with Path(destination).open("w") as stream:
        for pc in sorted(counts):
            stream.write("0x%x %d\n" % (pc, counts[pc]))
    return len(counts)


def aggregate_stats(paths):
    additive = (
        "cfg_tbs", "profiled_tbs", "pretranslated", "continuation_tbs",
        "edge_target_tbs", "failed",
        "same_extent", "shorter_than_cfg", "longer_than_cfg",
        "runtime_tb_gen_calls", "runtime_program_tb_gen_calls",
        "runtime_system_tb_gen_calls", "runtime_tb_gen_attempts",
        "runtime_program_tb_gen_attempts", "runtime_system_tb_gen_attempts",
        "bundle_verify_ns", "guest_extract_ns", "aot_prepare_ns",
    )
    aggregate = {key: 0 for key in additive}
    aggregate["processes"] = 0
    aggregate["runtime_first_pcs"] = []
    aggregate["runtime_first_cflags"] = []
    aggregate["runtime_program_first_pcs"] = []
    aggregate["runtime_system_first_pcs"] = []
    aggregate["aot_cache_hits"] = 0
    aggregate["aot_cache_misses"] = 0
    for path in sorted(paths):
        data = json.loads(Path(path).read_text())
        aggregate["processes"] += 1
        for key in additive:
            aggregate[key] += int(data.get(key, 0))
        if data.get("aot_cache_hit"):
            aggregate["aot_cache_hits"] += 1
        else:
            aggregate["aot_cache_misses"] += 1
        if data.get("runtime_first_pc"):
            aggregate["runtime_first_pcs"].append(data["runtime_first_pc"])
            aggregate["runtime_first_cflags"].append(
                data.get("runtime_first_cflags", 0))
            if data.get("runtime_program_tb_gen_attempts"):
                aggregate["runtime_program_first_pcs"].append(
                    data["runtime_first_pc"])
            else:
                aggregate["runtime_system_first_pcs"].append(
                    data["runtime_first_pc"])
    return aggregate


def sample_summary(values):
    mean = statistics.mean(values)
    deviation = statistics.pstdev(values) if len(values) > 1 else 0.0
    return {
        "samples": values,
        "median": statistics.median(values),
        "mean": mean,
        "min": min(values),
        "max": max(values),
        "cv": deviation / mean if mean else 0.0,
    }


def geometric_mean(values):
    return math.exp(sum(math.log(value) for value in values) / len(values))


def command_output(command):
    return subprocess.check_output(command, text=True).strip()
