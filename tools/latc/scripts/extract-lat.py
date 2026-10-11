#!/usr/bin/env python3
"""Extract the source files used by one successful LAT Ninja target build."""

import argparse
import hashlib
import json
import shutil
from pathlib import Path, PurePosixPath


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def source_relative(raw: str, recorded_source: str,
                    recorded_build: str) -> str | None:
    raw = raw.strip()
    if not raw:
        return None
    if raw.startswith(recorded_source + "/"):
        return raw[len(recorded_source) + 1:]
    if raw.startswith(recorded_build + "/"):
        return None
    p = PurePosixPath(raw)
    if p.is_absolute():
        return None
    parts = p.parts
    if parts and parts[0] == "..":
        while parts and parts[0] == "..":
            parts = parts[1:]
        return str(PurePosixPath(*parts)) if parts else None
    return None


def dep_headers(path: Path, wanted_outputs: set[str]) -> set[str]:
    result: set[str] = set()
    current_wanted = False
    for line in path.read_text(errors="replace").splitlines():
        if line and not line[0].isspace():
            output = line.split(":", 1)[0]
            current_wanted = output in wanted_outputs
        elif current_wanted and line.startswith("    "):
            result.add(line.strip())
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--recorded-source-root", required=True)
    parser.add_argument("--recorded-build-root", required=True)
    parser.add_argument("--compdb", type=Path, required=True)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--deps", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    compdb = json.loads(args.compdb.read_text())
    outputs = {entry["output"] for entry in compdb
               if entry.get("output", "").endswith((".o", ".obj"))}
    candidates = {entry["file"] for entry in compdb if entry.get("file")}
    candidates.update(args.inputs.read_text().splitlines())
    candidates.update(dep_headers(args.deps, outputs))

    files: list[dict[str, object]] = []
    for raw in sorted(candidates):
        rel = source_relative(raw, args.recorded_source_root,
                              args.recorded_build_root)
        if not rel or rel.startswith("tools/latc/"):
            continue
        src = args.source_root / rel
        if not src.is_file():
            continue
        files.append({
            "upstream_path": rel,
            "local_path": str(Path("lat") / rel),
            "sha256": sha256(src),
            "locally_modified": False,
        })
        if args.apply:
            dst = args.destination / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)

    manifest = {
        "source_repository": "https://github.com/lat-opensource/lat",
        "source_commit": args.commit,
        "target": "latx-x86_64",
        "method": "ninja inputs + target compdb + compiler deps",
        "file_count": len(files),
        "files": files,
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(manifest, indent=2) + "\n")
    verb = "copied" if args.apply else "selected"
    print(f"{verb} {len(files)} LAT source files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
