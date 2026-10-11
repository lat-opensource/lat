#!/usr/bin/env python3
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()

def main() -> int:
    source_check = subprocess.run(
        [sys.executable, str(ROOT / "scripts/sync-aot-v2-sources.py"),
         "--check"],
        check=False,
    )
    if source_check.returncode:
        return source_check.returncode
    manifest = json.loads((ROOT / "cfg-import.json").read_text())
    missing = []
    for kind in ("modified", "added"):
        for rel in manifest[kind]:
            if not (ROOT / "cfg" / rel).is_file():
                missing.append(f"{kind}: cfg/{rel}")
    if missing:
        print("import check failed")
        for item in missing:
            print(f"  missing {item}")
        return 1
    local_manifest = ROOT / "lat-local.json"
    local = json.loads(local_manifest.read_text()) if local_manifest.exists() else {}
    local_paths = {
        item["path"]
        for kind in ("added", "modified")
        for item in local.get(kind, [])
    }
    lat_manifest = ROOT / "lat-import.json"
    if lat_manifest.exists():
        lat = json.loads(lat_manifest.read_text())
        changed = []
        for item in lat.get("files", []):
            path = ROOT / item["local_path"]
            if not path.is_file():
                missing.append(item["local_path"])
            elif item["local_path"] not in local_paths:
                actual = sha256(path)
                if actual != item["sha256"]:
                    changed.append(item["local_path"])
        if missing:
            print("LAT import check failed")
            for item in missing:
                print(f"  missing {item}")
            return 1
        if changed:
            print("LAT import check failed")
            for path in changed:
                print(f"  changed without lat-local.json entry: {path}")
            return 1
        verified = len(lat.get("files", [])) - sum(
            item["local_path"] in local_paths for item in lat.get("files", [])
        )
        print(f"lat import OK commit={lat['source_commit']} "
              f"files={lat['file_count']} verified={verified}")
    if local_manifest.exists():
        for item in local.get("added", []) + local.get("modified", []):
            if not (ROOT / item["path"]).is_file():
                missing.append(item["path"])
        if missing:
            print("LAT local change check failed")
            for item in missing:
                print(f"  missing {item}")
            return 1
        print(f"lat local changes OK added={len(local.get('added', []))} "
              f"modified={len(local.get('modified', []))}")
    print(f"cfg import OK commit={manifest['commit']} usage={manifest['usage']}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
