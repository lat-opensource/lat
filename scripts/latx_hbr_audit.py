#!/usr/bin/env python3
"""Audit LATX SHBR instruction coverage against translator mappings."""

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

import generate_latx_hbr_semantics


TRANSLATOR_RE = re.compile(
    r"TRANS_FUNC_GEN\(\s*([A-Z][A-Z0-9_]*)\s*,"
)
TRANSLATOR_ENTRY_RE = re.compile(
    r"TRANS_FUNC_GEN\(\s*([A-Z][A-Z0-9_]*)\s*,\s*"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*\)"
)
SPECIAL_DISPATCH_RE = re.compile(
    r"ir1_opcode\(ir1\)\s*==\s*dt_X86_INS_([A-Z][A-Z0-9_]*)"
)
HBR_OPCODE_RE = re.compile(r"WRAP\(([A-Z][A-Z0-9_]*)\)")
EXPLICIT_TABLE_RE = re.compile(
    r"static\s+const\s+ShbrExplicitSemantic\s+"
    r"shbr_explicit_semantics\s*\[[^\]]+\]\s*=\s*\{"
)
EXPLICIT_ENTRY_RE = re.compile(
    r"\[\s*WRAP\(([A-Z][A-Z0-9_]*)\)\s*\]\s*=\s*\{\s*"
    r"SHBR_RULE_([A-Z0-9_]+)\s*,\s*SHBR_RULE_([A-Z0-9_]+)\s*\}"
)
MODEL_DOMAINS = ("high32", "high64")
DUAL_DOMAIN_FUNCTIONS = (
    "deal_xmm_common",
    "apply_implicit_semantic",
)
SINGLE_DOMAIN_FUNCTIONS = {
    "high32": (
        "deal_dest_not_xmm_32",
        "deal_src_not_xmm_32",
        "xmm_analyse_32",
    ),
    "high64": (
        "deal_dest_not_xmm_64",
        "deal_src_not_xmm_64",
        "xmm_analyse_64",
    ),
}


class AuditError(Exception):
    pass


def strip_c_comments(source):
    def keep_newlines(match):
        return "\n" * match.group(0).count("\n")

    source = re.sub(r"/\*.*?\*/", keep_newlines, source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def parse_translator_mnemonics(source):
    names = set(TRANSLATOR_RE.findall(strip_c_comments(source)))
    names.update(SPECIAL_DISPATCH_RE.findall(strip_c_comments(source)))
    names.discard("INVALID")
    return names


def parse_translator_trailing_x_aliases(source):
    """Find trailing-X opcodes mapped to the same translator as their base."""
    entries = dict(TRANSLATOR_ENTRY_RE.findall(strip_c_comments(source)))
    return {
        mnemonic
        for mnemonic, handler in entries.items()
        if mnemonic.endswith("X")
        and entries.get(mnemonic[:-1]) == handler
    }


def parse_hbr_models(source):
    source = strip_c_comments(source)
    marker = "static void init_xmm_state"
    if marker not in source:
        raise AuditError(f"hbr.c is missing marker: {marker}")
    source = source[:source.index(marker)]
    models = {}
    for match in HBR_OPCODE_RE.finditer(source):
        line = source.count("\n", 0, match.start()) + 1
        models.setdefault(match.group(1), []).append(line)
    return models


def _braced_region(source, opening_brace):
    depth = 0
    for offset in range(opening_brace, len(source)):
        char = source[offset]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return opening_brace + 1, offset
    raise AuditError("unterminated braced region in hbr.c")


def _function_body_region(source, function_name):
    match = re.search(
        rf"static\s+bool\s+{re.escape(function_name)}\s*\(", source
    )
    if not match:
        return None
    opening_brace = source.find("{", match.end())
    if opening_brace < 0:
        raise AuditError(f"hbr.c function has no body: {function_name}")
    return _braced_region(source, opening_brace)


def _record_domain_models(models, source, start, end, domains):
    for match in HBR_OPCODE_RE.finditer(source, start, end):
        line = source.count("\n", 0, match.start()) + 1
        opcode = match.group(1)
        entry = models.setdefault(
            opcode, {domain: [] for domain in MODEL_DOMAINS}
        )
        for domain in domains:
            entry[domain].append(line)


def parse_hbr_model_domains(source):
    """Return opcode model locations split by SHBR high-bit domain."""
    source = strip_c_comments(source)
    models = {}

    table = EXPLICIT_TABLE_RE.search(source)
    if not table:
        raise AuditError("hbr.c is missing shbr_explicit_semantics")
    table_start, table_end = _braced_region(source, table.end() - 1)
    for match in EXPLICIT_ENTRY_RE.finditer(source, table_start, table_end):
        opcode, high32_rule, high64_rule = match.groups()
        line = source.count("\n", 0, match.start()) + 1
        entry = models.setdefault(
            opcode, {domain: [] for domain in MODEL_DOMAINS}
        )
        if high32_rule != "NONE":
            entry["high32"].append(line)
        if high64_rule != "NONE":
            entry["high64"].append(line)

    for function_name in DUAL_DOMAIN_FUNCTIONS:
        region = _function_body_region(source, function_name)
        if region:
            _record_domain_models(
                models, source, *region, domains=MODEL_DOMAINS
            )

    for domain, function_names in SINGLE_DOMAIN_FUNCTIONS.items():
        for function_name in function_names:
            region = _function_body_region(source, function_name)
            if region:
                _record_domain_models(
                    models, source, *region, domains=(domain,)
                )

    for entry in models.values():
        for domain in MODEL_DOMAINS:
            entry[domain] = sorted(set(entry[domain]))
    return models


def require_complete_hbr_model_domains(models, required=()):
    """Reject opcode models that omit either SHBR high-bit domain."""
    incomplete = []
    for opcode in sorted(set(models) | set(required)):
        entry = models.get(opcode, {})
        missing = [
            domain for domain in MODEL_DOMAINS if not entry.get(domain)
        ]
        if missing:
            incomplete.append(f"{opcode} missing {','.join(missing)}")
    if incomplete:
        raise AuditError(
            "opcode domain coverage incomplete: " + "; ".join(incomplete)
        )


def load_inventory(path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise AuditError(f"cannot load {path}: {exc}") from exc


def _require_semantics(policy_name, policies):
    if policy_name not in policies:
        raise AuditError(f"unknown semantic policy: {policy_name}")
    policy = policies[policy_name]
    for domain in ("high32", "high64"):
        semantics = policy.get(domain, {})
        missing = {
            "reads", "definition", "dependencies"
        } - set(semantics)
        if missing:
            raise AuditError(
                f"policy {policy_name} {domain} misses {sorted(missing)}"
            )
    return policy


def _unmodeled_rows(data, policies):
    seen = set()
    rows = []
    for group in data.get("unmodeled_groups", []):
        names = group.get("mnemonics", [])
        expected = group.get("expected_count")
        if len(names) != expected:
            raise AuditError(
                f"group {group.get('name')} has {len(names)} entries, "
                f"expected {expected}"
            )
        for mnemonic in names:
            if mnemonic in seen:
                raise AuditError(f"duplicate inventory mnemonic: {mnemonic}")
            seen.add(mnemonic)
            override = data.get("policy_overrides", {}).get(mnemonic, {})
            policy_name = override.get("policy", group["policy"])
            policy = _require_semantics(policy_name, policies)
            if override:
                current_handling = "missing-implicit-semantics"
            elif group["name"] == "ymm_alias_only":
                current_handling = "skipped-ymm-alias"
            elif group["name"] in {
                "avx_xmm_unmodeled", "fma_unmodeled", "vcmp_unmodeled"
            }:
                current_handling = "incomplete-after-operand-1"
            else:
                current_handling = "conservative-operand-0-1"
            rows.append({
                "mnemonic": mnemonic,
                "coverage": "conservative-unmodeled",
                "group": group["name"],
                "form_scope": group["form_scope"],
                "policy": policy_name,
                "high32": policy["high32"],
                "high64": policy["high64"],
                "implicit": override.get("implicit", []),
                "current_handling": current_handling,
            })
    return rows


def audit(repo_root, inventory_path=None):
    repo_root = Path(repo_root)
    if inventory_path is None:
        inventory_path = repo_root / (
            "target/i386/latx/optimization/hbr-xmm-semantics.json"
        )
    else:
        inventory_path = Path(inventory_path)

    data = load_inventory(inventory_path)
    expected = data["expected_counts"]
    policies = data["semantic_policies"]
    translator_path = repo_root / data["generated_from"]["translator_map"]
    hbr_path = repo_root / data["generated_from"]["hbr_model"]
    translator_source = translator_path.read_text(encoding="utf-8")
    translator = parse_translator_mnemonics(translator_source)
    translator_digest = hashlib.sha256(
        ("\n".join(sorted(translator)) + "\n").encode("ascii")
    ).hexdigest()
    expected_digest = data.get("translator_mnemonic_sha256")
    if translator_digest != expected_digest:
        raise AuditError(
            "translator mnemonic set drift: "
            f"expected {expected_digest}, actual {translator_digest}"
        )

    baseline_rows = _unmodeled_rows(data, policies)
    baseline_unmodeled = {row["mnemonic"] for row in baseline_rows}
    trailing_x_aliases = parse_translator_trailing_x_aliases(translator_source)
    relevant_x_aliases = {
        alias for alias in trailing_x_aliases
        if alias[:-1] in baseline_unmodeled
    }
    missing_x_aliases = relevant_x_aliases - baseline_unmodeled
    if missing_x_aliases:
        raise AuditError(
            "translator trailing-X aliases missing from inventory: "
            f"{sorted(missing_x_aliases)}"
        )

    hbr_source = hbr_path.read_text(encoding="utf-8")
    generated_path = repo_root / data["generated_rule_table"]
    generated_source = generated_path.read_text(encoding="utf-8")
    generated_include = f'#include "{generated_path.name}"'
    if generated_include not in hbr_source:
        raise AuditError(
            f"hbr model does not include generated table: {generated_include}"
        )
    try:
        expected_generated = generate_latx_hbr_semantics.render(data)
    except generate_latx_hbr_semantics.GenerationError as exc:
        raise AuditError(str(exc)) from exc
    if generated_source != expected_generated:
        raise AuditError(f"generated semantic table is stale: {generated_path}")

    expanded_hbr_source = hbr_source.replace(
        generated_include, generated_source, 1
    )
    all_model_domains = parse_hbr_model_domains(expanded_hbr_source)
    require_complete_hbr_model_domains(
        all_model_domains, required=baseline_unmodeled
    )
    excluded = set(data.get("exclusions", {}))
    model_domains = {
        opcode: entry for opcode, entry in all_model_domains.items()
        if opcode not in excluded
    }
    models = {
        opcode: sorted(set(
            entry["high32"] + entry["high64"]
        ))
        for opcode, entry in model_domains.items()
    }
    modeled = set(models)
    if data.get("promote_all_inventory"):
        expected_promoted = baseline_unmodeled
    else:
        expected_promoted = set(data.get("promoted_opcode_specific", []))
    if not expected_promoted <= baseline_unmodeled:
        raise AuditError(
            "promoted mnemonics are not baseline inventory entries: "
            f"{sorted(expected_promoted - baseline_unmodeled)}"
        )
    actual_promoted = modeled & baseline_unmodeled
    if actual_promoted != expected_promoted:
        raise AuditError(
            "opcode-specific promotion drift: "
            f"missing={sorted(expected_promoted - actual_promoted)}, "
            f"unexpected={sorted(actual_promoted - expected_promoted)}"
        )
    rows = [
        row for row in baseline_rows
        if row["mnemonic"] not in actual_promoted
    ]
    unmodeled = baseline_unmodeled - actual_promoted

    overrides = set(data.get("policy_overrides", {}))
    if not overrides <= baseline_unmodeled:
        raise AuditError(
            f"policy overrides are not inventory entries: "
            f"{sorted(overrides - baseline_unmodeled)}"
        )
    unsupported = (modeled | baseline_unmodeled) - translator
    if unsupported:
        raise AuditError(
            f"inventory contains unsupported translator mnemonics: "
            f"{sorted(unsupported)}"
        )

    _require_semantics("opcode-specific", policies)
    generated_rules = generate_latx_hbr_semantics.exact_rules(data)
    baseline_by_mnemonic = {
        row["mnemonic"]: row for row in baseline_rows
    }
    for mnemonic in modeled:
        baseline = baseline_by_mnemonic.get(mnemonic)
        override = data.get("policy_overrides", {}).get(mnemonic)
        policy_name = (
            baseline["policy"] if baseline and override
            else "opcode-specific"
        )
        policy = _require_semantics(policy_name, policies)
        row = {
            "mnemonic": mnemonic,
            "coverage": "opcode-specific",
            "group": "current-hbr-model",
            "form_scope": "decoded-ir1-form",
            "policy": policy_name,
            "high32": policy["high32"],
            "high64": policy["high64"],
            "implicit": baseline["implicit"] if baseline and override else [],
            "current_handling": "opcode-specific",
            "model_lines": models[mnemonic],
            "model_lines_by_domain": model_domains[mnemonic],
        }
        if mnemonic in generated_rules:
            row["semantic_rules"] = generated_rules[mnemonic]
        rows.append(row)

    ymm_only = next(
        group for group in data["unmodeled_groups"]
        if group["name"] == "ymm_alias_only"
    )["mnemonics"]
    counts = {
        "translator_supported": len(translator),
        "opcode_specific": len(modeled),
        "conservative_unmodeled": len(unmodeled),
        "strict_xmm_scope": len(modeled | unmodeled) - len(ymm_only),
        "ymm_alias_only": len(ymm_only),
        "shbr_relevant": len(modeled | unmodeled),
    }
    drift = {
        name: {"expected": expected[name], "actual": actual}
        for name, actual in counts.items()
        if actual != expected[name]
    }
    if drift:
        raise AuditError(f"inventory count drift: {json.dumps(drift)}")

    if excluded & (modeled | unmodeled):
        raise AuditError(
            f"excluded mnemonics are in the inventory: "
            f"{sorted(excluded & (modeled | unmodeled))}"
        )

    return {
        "schema_version": data["schema_version"],
        "intel_manual": data["intel_manual"],
        "counts": counts,
        "translator_mnemonic_sha256": translator_digest,
        "groups": {
            group["name"]: len(set(group["mnemonics"]) - actual_promoted)
            for group in data["unmodeled_groups"]
        },
        "baseline_groups": {
            group["name"]: len(group["mnemonics"])
            for group in data["unmodeled_groups"]
        },
        "promoted_opcode_specific": sorted(actual_promoted),
        "generated_opcode_specific": len(generated_rules),
        "rows": sorted(rows, key=lambda row: row["mnemonic"]),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Check SHBR XMM/YMM instruction semantic coverage"
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    parser.add_argument("--inventory", type=Path)
    parser.add_argument(
        "--json", action="store_true", help="print the expanded inventory"
    )
    args = parser.parse_args(argv)
    try:
        result = audit(args.repo_root, args.inventory)
    except (AuditError, OSError) as exc:
        print(f"SHBR inventory audit failed: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        counts = result["counts"]
        print("SHBR inventory audit passed")
        for name in (
            "translator_supported", "shbr_relevant", "opcode_specific",
            "conservative_unmodeled", "strict_xmm_scope", "ymm_alias_only",
        ):
            print(f"{name}={counts[name]}")
        for name, count in result["groups"].items():
            print(f"group.{name}={count}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
