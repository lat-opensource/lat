"""Validated source ownership for main, imported and staging LAT trees."""

import json
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class GeneratedSource:
    source: Path
    target: Path


def _resolve_below(root: Path, relative: str) -> Path:
    if Path(relative).is_absolute():
        raise ValueError(f"path must be relative: {relative}")
    root = root.resolve()
    path = (root / relative).resolve()
    try:
        path.relative_to(root)
    except ValueError as error:
        raise ValueError(f"path escapes {root}: {relative}") from error
    return path


def _read_manifest(latc_root: Path) -> dict:
    return json.loads((latc_root / "aot-v2-source-map.json").read_text())


def load_generated_sources(repository_root: Path,
                           latc_root: Path) -> list[GeneratedSource]:
    repository_root = repository_root.resolve()
    latc_root = latc_root.resolve()
    manifest = _read_manifest(latc_root)
    result = []
    targets = set()
    for item in manifest.get("generated", []):
        source = _resolve_below(repository_root, item["source"])
        target = _resolve_below(repository_root, item["target"])
        if target in targets:
            raise ValueError(f"duplicate generated target: {item['target']}")
        targets.add(target)
        result.append(GeneratedSource(source=source, target=target))

    imported_root = (latc_root / "lat").resolve()
    local = json.loads((latc_root / "lat-local.json").read_text())
    mapped_imports = {
        str(entry.target.relative_to(latc_root))
        for entry in result
        if entry.target.is_relative_to(imported_root)
    }
    declared_imports = {
        item["path"]
        for kind in ("added", "modified")
        for item in local.get(kind, [])
        if item.get("generated")
    }
    if declared_imports != mapped_imports:
        missing_map = sorted(declared_imports - mapped_imports)
        missing_flag = sorted(mapped_imports - declared_imports)
        raise ValueError(
            "lat-local generated entries disagree with source map: "
            f"missing_map={missing_map} missing_flag={missing_flag}"
        )
    return result


def load_runner_overlays(repository_root: Path, latc_root: Path,
                         without_aot_v2: bool) -> dict[str, Path]:
    repository_root = repository_root.resolve()
    latc_root = latc_root.resolve()
    imported_root = (latc_root / "lat").resolve()
    generated = load_generated_sources(repository_root, latc_root)
    overlays = {}
    for entry in generated:
        try:
            target = str(entry.target.relative_to(imported_root))
        except ValueError:
            target = str(entry.target.relative_to(repository_root))
        if target in overlays:
            raise ValueError(f"duplicate runner overlay target: {target}")
        overlays[target] = entry.source

    local = json.loads((latc_root / "lat-local.json").read_text())
    for kind in ("added", "modified"):
        for item in local.get(kind, []):
            imported_name = item["path"]
            if not item.get("generated"):
                imported = _resolve_below(latc_root, imported_name)
                relative = str(Path(imported_name).relative_to("lat"))
                overlays.setdefault(relative, imported)

    if without_aot_v2:
        variant = _read_manifest(latc_root).get(
            "runner_variants", {}).get("without-aot-v2")
        if not variant:
            raise ValueError("missing without-aot-v2 runner variant")
        overlays[variant["target"]] = _resolve_below(
            repository_root, variant["source"]
        )
    return overlays
