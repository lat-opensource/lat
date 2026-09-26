#!/usr/bin/env python3
"""Audit LATX GHBR consumer and mov32 generation-site inventory."""

import argparse
import json
import re
import sys
from pathlib import Path


CATEGORIES = {
    "architectural-writeback",
    "conditional-writeback",
    "input-normalization",
    "address-normalization",
    "temporary-conversion",
    "central-writeback-review",
    "candidate-review",
    "non-64-mode",
}

TRANSLATOR_FILES = (
    "tr-arith.c",
    "tr-logic.c",
    "tr-misc.c",
    "tr-mov.c",
    "tr-opnd-process.c",
    "tr-pattern.c",
)


class AuditError(Exception):
    pass


def function_headers(source):
    pattern = re.compile(
        r"^(?:static\s+)?(?:inline\s+)?"
        r"(?:bool|void|int|IR2_OPND)\s+"
        r"([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*?\)\s*\{",
        re.MULTILINE,
    )
    headers = []
    for match in pattern.finditer(source):
        headers.append((match.start(), match.group(1)))
    return headers


def site_key(file_name, function):
    return f"{file_name}:{function}"


def collect_sites(repo_root):
    translator = repo_root / "target/i386/latx/translator"
    consumer_sites = {}
    mov32_sites = {}

    for file_name in TRANSLATOR_FILES:
        source = (translator / file_name).read_text(encoding="utf-8")
        headers = function_headers(source)
        for token, result in (
            ("GHBR_ON", consumer_sites),
            ("la_mov32_zx", mov32_sites),
        ):
            for match in re.finditer(r"\b" + token + r"\b", source):
                owner = None
                for start, function in headers:
                    if start < match.start():
                        owner = function
                    else:
                        break
                line = source.count("\n", 0, match.start()) + 1
                if owner is None:
                    raise AuditError(
                        f"{file_name}:{line}: {token} outside a known function"
                    )
                key = site_key(file_name, owner)
                result.setdefault(key, []).append(line)

    return consumer_sites, mov32_sites


def load_inventory(path):
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise AuditError(f"cannot load inventory {path}: {exc}") from exc
    if data.get("schema_version") != 1:
        raise AuditError("unsupported GHBR inventory schema version")
    if data.get("tracked_state") != "gpr_high32":
        raise AuditError("GHBR inventory must track gpr_high32")
    return data


def inventory_map(entries, field):
    result = {}
    for entry in entries:
        key = site_key(entry["file"], entry["function"])
        if key in result:
            raise AuditError(f"duplicate inventory entry: {key}")
        category = entry["category"]
        if category not in CATEGORIES:
            raise AuditError(f"{key}: unknown category {category}")
        if field in entry and not isinstance(entry[field], bool):
            raise AuditError(f"{key}: {field} must be boolean")
        result[key] = entry
    return result


def compare_sites(name, actual, expected):
    actual_keys = set(actual)
    expected_keys = set(expected)
    missing = sorted(actual_keys - expected_keys)
    stale = sorted(expected_keys - actual_keys)
    if missing:
        raise AuditError(f"{name}: missing inventory entries: {missing}")
    if stale:
        raise AuditError(f"{name}: stale inventory entries: {stale}")


def audit_may_def_model(repo_root):
    source = (
        repo_root / "target/i386/latx/optimization/hbr.c"
    ).read_text(encoding="utf-8")
    required = (
        "gpr_may_def",
        "set_may_def_reg",
        "case WRAP(CMOVA)",
    )
    missing = [token for token in required if token not in source]
    if missing:
        raise AuditError(f"missing conditional-write model: {missing}")


def audit(repo_root, inventory_path=None):
    repo_root = Path(repo_root)
    if inventory_path is None:
        inventory_path = repo_root / (
            "target/i386/latx/optimization/hbr-gpr-semantics.json"
        )
    inventory = load_inventory(Path(inventory_path))
    consumers = inventory_map(inventory["consumer_sites"], "gated")
    mov32 = inventory_map(inventory["mov32_sites"], "gated")
    actual_consumers, actual_mov32 = collect_sites(repo_root)
    compare_sites("consumer sites", actual_consumers, consumers)
    compare_sites("mov32 sites", actual_mov32, mov32)

    gated_functions = {
        key for key, entry in mov32.items() if entry["gated"]
    }
    consumer_functions = set(consumers)
    ungated_consumers = gated_functions - consumer_functions
    if ungated_consumers:
        raise AuditError(
            "gated mov32 sites without GHBR_ON consumer: "
            f"{sorted(ungated_consumers)}"
        )

    audit_may_def_model(repo_root)

    category_counts = {}
    for entry in mov32.values():
        category = entry["category"]
        category_counts[category] = category_counts.get(category, 0) + 1
    return {
        "consumer_functions": len(consumers),
        "mov32_functions": len(mov32),
        "categories": category_counts,
        "may_def_model": True,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = audit(args.repo_root)
    except (AuditError, OSError) as exc:
        print(f"GHBR semantic audit failed: {exc}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print("GHBR semantic audit: PASS")
        print(f"GHBR consumer functions: {result['consumer_functions']}")
        print(f"la_mov32_zx functions: {result['mov32_functions']}")
        for category in sorted(result["categories"]):
            print(f"{category}: {result['categories'][category]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
