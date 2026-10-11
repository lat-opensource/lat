#!/usr/bin/env python3
"""Regression test for generated AOT v2 runner source synchronization."""

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def run(script: Path, repository: Path, latc_root: Path, *arguments: str):
    return subprocess.run(
        [sys.executable, str(script), "--repository-root", str(repository),
         "--latc-root", str(latc_root), *arguments],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} SYNC_SCRIPT", file=sys.stderr)
        return 2
    script = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="latc-aot-v2-source-") as temp:
        repository = Path(temp) / "repository"
        latc_root = repository / "tools" / "latc"
        pairs = [
            ("linux-user/runner.c", "tools/latc/lat/linux-user/runner.c"),
            ("tools/latc/aot-v2/runtime/registry.h", "include/registry.h"),
            ("tools/latc/native/include/native.h", "include/native.h"),
        ]
        for index, (source_name, target_name) in enumerate(pairs):
            source = repository / source_name
            target = repository / target_name
            source.parent.mkdir(parents=True, exist_ok=True)
            target.parent.mkdir(parents=True, exist_ok=True)
            source.write_text(f"canonical {index}\n")
            target.write_bytes(source.read_bytes())
        source_map = {
            "generated": [
                {"source": source, "target": target}
                for source, target in pairs
            ],
        }
        source_map_path = latc_root / "aot-v2-source-map.json"
        source_map_path.write_text(json.dumps(source_map))
        (latc_root / "lat-local.json").write_text(json.dumps({
            "added": [{
                "path": "lat/linux-user/runner.c",
                "generated": True,
                "reason": "test fixture",
            }],
            "modified": [],
        }))

        checked = run(script, repository, latc_root, "--check")
        assert checked.returncode == 0, checked.stdout
        assert "source sync OK generated=3" in checked.stdout, checked.stdout

        for source_name, target_name in pairs:
            target = repository / target_name
            target.write_text("manual drift\n")
            drift = run(script, repository, latc_root, "--check")
            assert drift.returncode == 1, drift.stdout
            assert "AOT v2 source sync failed" in drift.stdout, drift.stdout
            assert target_name in drift.stdout, drift.stdout
            assert f"generated from {source_name}" in drift.stdout, drift.stdout
            synchronized = run(script, repository, latc_root)
            assert synchronized.returncode == 0, synchronized.stdout
            assert target.read_bytes() == (repository / source_name).read_bytes()

        missing_target = repository / pairs[0][1]
        missing_target.unlink()
        missing = run(script, repository, latc_root, "--check")
        assert missing.returncode == 1, missing.stdout
        assert "missing generated copy" in missing.stdout, missing.stdout
        assert run(script, repository, latc_root).returncode == 0

        missing_source = repository / pairs[1][0]
        saved_source = missing_source.read_bytes()
        missing_source.unlink()
        missing = run(script, repository, latc_root, "--check")
        assert missing.returncode == 1, missing.stdout
        assert "missing source" in missing.stdout, missing.stdout
        assert run(script, repository, latc_root).returncode == 1
        missing_source.write_bytes(saved_source)

        duplicate_map = json.loads(json.dumps(source_map))
        duplicate_map["generated"].append({
            "source": pairs[1][0],
            "target": pairs[0][1],
        })
        source_map_path.write_text(json.dumps(duplicate_map))
        duplicate = run(script, repository, latc_root, "--check")
        assert duplicate.returncode == 1, duplicate.stdout
        assert "duplicate generated target" in duplicate.stdout, duplicate.stdout

        escaped_map = json.loads(json.dumps(source_map))
        escaped_map["generated"][0]["source"] = "../outside.c"
        source_map_path.write_text(json.dumps(escaped_map))
        escaped = run(script, repository, latc_root, "--check")
        assert escaped.returncode == 1, escaped.stdout
        assert "path escapes" in escaped.stdout, escaped.stdout

        source_map_path.write_text(json.dumps(source_map))
        (latc_root / "lat-local.json").write_text(json.dumps({
            "added": [{
                "path": "lat/linux-user/runner.c",
                "generated": False,
                "reason": "test fixture",
            }],
            "modified": [],
        }))
        undeclared = run(script, repository, latc_root, "--check")
        assert undeclared.returncode == 1, undeclared.stdout
        assert "missing_flag" in undeclared.stdout, undeclared.stdout
        (latc_root / "lat-local.json").write_text(json.dumps({
            "added": [{
                "path": "lat/linux-user/runner.c",
                "generated": True,
                "reason": "test fixture",
            }],
            "modified": [],
        }))
        checked_again = run(script, repository, latc_root, "--check")
        assert checked_again.returncode == 0, checked_again.stdout

    print("test-aot-v2-source-sync: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
