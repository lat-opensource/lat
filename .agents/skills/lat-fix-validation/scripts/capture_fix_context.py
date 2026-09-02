#!/usr/bin/env python3
"""Capture a read-only LAT fix context manifest as JSON."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def run_git(repo: Path, *args: str) -> dict[str, Any]:
    command = ["git", "-C", str(repo), *args]
    try:
        result = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=15,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return {"ok": False, "error": str(exc)}

    output: dict[str, Any] = {
        "ok": result.returncode == 0,
        "returncode": result.returncode,
    }
    if result.stdout:
        output["stdout"] = result.stdout.rstrip("\n")
    if result.stderr:
        output["stderr"] = result.stderr.rstrip("\n")
    return output


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def artifact_record(raw_path: str, redact_paths: bool) -> dict[str, Any]:
    path = Path(raw_path).expanduser().resolve()
    record: dict[str, Any] = {
        "path": path.name if redact_paths else str(path),
        "exists": path.exists(),
    }
    if path.is_file():
        record.update({"size": path.stat().st_size, "sha256": sha256_file(path)})
    elif path.exists():
        record["kind"] = "directory"
    return record


def parse_switch(value: str) -> tuple[str, str]:
    if "=" not in value:
        raise argparse.ArgumentTypeError("switch must use NAME=VALUE")
    name, switch_value = value.split("=", 1)
    if not name:
        raise argparse.ArgumentTypeError("switch name must not be empty")
    return name, switch_value


def redact_text(value: str | None, git_root: Path) -> str | None:
    if value is None:
        return None
    redacted = value.replace(str(git_root), "<repo>")
    redacted = redacted.replace(str(Path.home()), "~")
    return redacted


def redact_worktree_result(result: dict[str, Any]) -> dict[str, Any]:
    if not result.get("stdout"):
        return result
    redacted = dict(result)
    lines = []
    for line in str(result["stdout"]).splitlines():
        if line.startswith("worktree "):
            name = Path(line.removeprefix("worktree ")).name or "root"
            lines.append(f"worktree <redacted>/{name}")
        else:
            lines.append(line)
    redacted["stdout"] = "\n".join(lines)
    return redacted


def build_manifest(args: argparse.Namespace) -> dict[str, Any]:
    repo = Path(args.repo).expanduser().resolve()
    root_result = run_git(repo, "rev-parse", "--show-toplevel")
    git_root = repo
    if root_result.get("ok") and root_result.get("stdout"):
        git_root = Path(str(root_result["stdout"])).resolve()

    try:
        page_size = os.sysconf("SC_PAGE_SIZE")
    except (AttributeError, OSError, ValueError):
        page_size = None

    switches = dict(args.switch or [])
    worktrees = run_git(git_root, "worktree", "list", "--porcelain")
    if args.redact_paths:
        worktrees = redact_worktree_result(worktrees)
    manifest: dict[str, Any] = {
        "schema": "lat-fix-context-v1",
        "captured_at_utc": datetime.now(timezone.utc).isoformat(),
        "privacy_notice": (
            "This manifest may contain local paths, branch names, commands, and "
            "worktree details. Review and redact it before external publication."
        ),
        "repository": {
            "path": git_root.name if args.redact_paths else str(git_root),
            "head": run_git(git_root, "rev-parse", "HEAD"),
            "branch": run_git(git_root, "branch", "--show-current"),
            "status": run_git(git_root, "status", "--short", "--branch"),
            "worktrees": worktrees,
            "submodules": run_git(git_root, "submodule", "status"),
        },
        "host": {
            "system": platform.system(),
            "machine": platform.machine(),
            "release": platform.release(),
            "page_size": page_size,
        },
        "task": {
            "expected_behavior": redact_text(args.expected_behavior, git_root)
            if args.redact_paths
            else args.expected_behavior,
            "observed_failure": redact_text(args.observed_failure, git_root)
            if args.redact_paths
            else args.observed_failure,
            "reproduction_command": redact_text(args.command, git_root)
            if args.redact_paths
            else args.command,
            "target": redact_text(args.target, git_root)
            if args.redact_paths
            else args.target,
            "guest_root": redact_text(args.guest_root, git_root)
            if args.redact_paths
            else args.guest_root,
            "workload": redact_text(args.workload, git_root)
            if args.redact_paths
            else args.workload,
            "switches": switches,
        },
        "binaries": [
            artifact_record(path, args.redact_paths) for path in args.binary
        ],
        "caches": [
            artifact_record(path, args.redact_paths) for path in args.cache
        ],
        "artifacts": [
            artifact_record(path, args.redact_paths) for path in args.artifact
        ],
    }
    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Capture repository, host, artifact, runtime, switch, and reproduction "
            "identity for a LAT fix. The script is read-only except for an optional "
            "output file."
        )
    )
    parser.add_argument("--repo", default=".", help="LAT repository path")
    parser.add_argument("--binary", action="append", default=[], help="binary path")
    parser.add_argument("--cache", action="append", default=[], help="cache path")
    parser.add_argument("--artifact", action="append", default=[], help="evidence file")
    parser.add_argument("--switch", action="append", type=parse_switch, help="NAME=VALUE")
    parser.add_argument("--guest-root")
    parser.add_argument("--target")
    parser.add_argument("--workload")
    parser.add_argument("--command", help="exact reproduction command; review for secrets")
    parser.add_argument("--expected-behavior")
    parser.add_argument("--observed-failure")
    parser.add_argument("--redact-paths", action="store_true")
    parser.add_argument("--output", help="write JSON here instead of stdout")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = build_manifest(args)
    rendered = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    if args.output:
        output = Path(args.output).expanduser().resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(rendered, encoding="utf-8")
    else:
        sys.stdout.write(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
