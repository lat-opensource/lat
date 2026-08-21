#!/usr/bin/env python3

import copy
import json
import re
import tempfile
import unittest
from pathlib import Path

import latx_hbr_audit
import generate_latx_hbr_semantics


REPO_ROOT = Path(__file__).resolve().parents[1]
INVENTORY = REPO_ROOT / (
    "target/i386/latx/optimization/hbr-xmm-semantics.json"
)


class HbrAuditTest(unittest.TestCase):
    def test_repository_inventory(self):
        result = latx_hbr_audit.audit(REPO_ROOT)
        self.assertEqual(result["counts"]["translator_supported"], 1098)
        self.assertEqual(result["counts"]["shbr_relevant"], 774)
        self.assertEqual(result["counts"]["opcode_specific"], 774)
        self.assertEqual(result["counts"]["conservative_unmodeled"], 0)
        self.assertEqual(len(result["rows"]), 774)
        self.assertEqual(len(result["promoted_opcode_specific"]), 618)
        self.assertEqual(result["generated_opcode_specific"], 519)
        gather = next(
            row for row in result["rows"] if row["mnemonic"] == "VGATHERDPD"
        )
        self.assertEqual(
            gather["semantic_rules"],
            {"high32": "GATHER", "high64": "GATHER"},
        )
        self.assertEqual(
            result["translator_mnemonic_sha256"],
            "32bdf9f0e48c0b73ac1f3f0e914d2af621540ffd7c6ccebb86d059c14137250d",
        )
        xrstor = next(
            row for row in result["rows"] if row["mnemonic"] == "XRSTOR"
        )
        self.assertEqual(xrstor["policy"], "implicit-read-write-all")
        self.assertIn("preserve path", xrstor["high32"]["reads"])

    def test_commented_hbr_case_is_ignored(self):
        models = latx_hbr_audit.parse_hbr_models(
            "/* case WRAP(BAD): */\n"
            "case WRAP(GOOD):\n"
            "static void init_xmm_state(void) {}\n"
        )
        self.assertEqual(models, {"GOOD": [2]})

    def test_hbr_model_domains_are_classified_by_handler(self):
        models = latx_hbr_audit.parse_hbr_model_domains(
            "static const ShbrExplicitSemantic "
            "shbr_explicit_semantics[1] = {\n"
            "  [WRAP(TABLE)] = { SHBR_RULE_ACCESS, SHBR_RULE_IGNORE },\n"
            "};\n"
            "static bool deal_xmm_common(void) {\n"
            "  case WRAP(COMMON): return true;\n"
            "}\n"
            "static bool apply_implicit_semantic(void) {\n"
            "  case WRAP(IMPLICIT): return true;\n"
            "}\n"
            "static bool xmm_analyse_32(void) {\n"
            "  case WRAP(ONLY32): return true;\n"
            "}\n"
            "static bool xmm_analyse_64(void) {\n"
            "  case WRAP(ONLY64): return true;\n"
            "}\n"
        )
        self.assertTrue(models["TABLE"]["high32"])
        self.assertTrue(models["TABLE"]["high64"])
        self.assertTrue(models["COMMON"]["high32"])
        self.assertTrue(models["COMMON"]["high64"])
        self.assertTrue(models["IMPLICIT"]["high32"])
        self.assertTrue(models["IMPLICIT"]["high64"])
        self.assertTrue(models["ONLY32"]["high32"])
        self.assertFalse(models["ONLY32"]["high64"])
        self.assertFalse(models["ONLY64"]["high32"])
        self.assertTrue(models["ONLY64"]["high64"])

    def test_single_domain_model_fails(self):
        data = json.loads(INVENTORY.read_text(encoding="utf-8"))
        hbr_source = (
            REPO_ROOT / data["generated_from"]["hbr_model"]
        ).read_text(encoding="utf-8")
        hbr_source = hbr_source.replace(
            "[WRAP(MOVDQ2Q)] = { SHBR_RULE_READ_ACCESS, SHBR_RULE_IGNORE },",
            "[WRAP(MOVDQ2Q)] = { SHBR_RULE_READ_ACCESS, SHBR_RULE_NONE },",
            1,
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative_path, source in (
                (data["generated_from"]["translator_map"], (
                    REPO_ROOT / data["generated_from"]["translator_map"]
                ).read_text(encoding="utf-8")),
                (data["generated_from"]["hbr_model"], hbr_source),
                (data["generated_rule_table"], (
                    REPO_ROOT / data["generated_rule_table"]
                ).read_text(encoding="utf-8")),
            ):
                path = root / relative_path
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(source, encoding="utf-8")
            with self.assertRaisesRegex(
                latx_hbr_audit.AuditError,
                "MOVDQ2Q missing high64",
            ):
                latx_hbr_audit.audit(root, INVENTORY)

    def test_exact_family_rules(self):
        data = json.loads(INVENTORY.read_text(encoding="utf-8"))
        rules = generate_latx_hbr_semantics.exact_rules(data)
        self.assertEqual(len(rules), 519)
        self.assertEqual(
            rules["VFMADD132SS"],
            {"high32": "DEST", "high64": "DEST"},
        )
        self.assertEqual(rules["VFMADD213SD"]["high32"], "ACCESS_READ")
        self.assertEqual(rules["VFMADD213SD"]["high64"], "DEST")
        self.assertEqual(rules["VFMADD132PS"]["high32"], "ACCESS_READ")
        self.assertEqual(rules["CMPEQPS"]["high64"], "ACCESS_READ")
        self.assertEqual(rules["VCMPEQPS"]["high32"], "ACCESS_READ")
        self.assertEqual(rules["VCMPEQSS"]["high32"], "SRC1")
        self.assertEqual(rules["VCMPTRUE_USSD"]["high32"], "ACCESS_READ")
        self.assertEqual(rules["VCMPTRUE_USSD"]["high64"], "SRC1")
        self.assertEqual(rules["VMOVSD"]["high32"], "MOVSD")
        self.assertEqual(rules["VGATHERDPD"]["high64"], "GATHER")
        self.assertEqual(rules["VEXTRACTPS"]["high32"], "EXTRACT")
        self.assertEqual(rules["VPSLLVQ"]["high32"], "SCALAR_SHIFT")
        self.assertEqual(rules["VPSRLVQ"]["high64"], "SCALAR_SHIFT")
        self.assertEqual(rules["VPSIGND"]["high32"], "PSIGN")
        self.assertEqual(rules["VCVTPD2DQX"]["high32"], "ACCESS_READ")
        self.assertEqual(rules["VCVTPD2PSX"]["high64"], "ACCESS_READ")
        self.assertEqual(rules["VCVTTPD2DQX"]["high32"], "ACCESS_READ")
        self.assertEqual(rules["CVTDQ2PS"]["high64"], "ACCESS_READ")
        self.assertEqual(rules["VROUNDPS"]["high32"], "ACCESS_READ")
        self.assertEqual(rules["XRSTOR"]["high64"], "IMPLICIT")

    def test_generated_rules_exist_in_c_enum(self):
        source = (
            REPO_ROOT / "target/i386/latx/optimization/hbr.c"
        ).read_text(encoding="utf-8")
        enum_body = source.split("typedef enum ShbrExplicitRule {", 1)[1]
        enum_body = enum_body.split("} ShbrExplicitRule;", 1)[0]
        c_rules = set(re.findall(r"SHBR_RULE_([A-Z0-9_]+)", enum_body))
        self.assertTrue(generate_latx_hbr_semantics.RULES <= c_rules)

    def test_packed_fp_sources_are_direct_reads(self):
        source = (
            REPO_ROOT / "target/i386/latx/optimization/hbr.c"
        ).read_text(encoding="utf-8")
        for opcode, rule in (
            ("VADDPS", "ALL_SOURCES_READ"),
            ("VMULPD", "ALL_SOURCES_READ"),
            ("VSQRTPS", "SRC1_READ"),
        ):
            self.assertRegex(
                source,
                rf"\[WRAP\({opcode}\)\]\s*=\s*\{{\s*"
                rf"SHBR_RULE_{rule}",
            )

        common = source.split("static bool deal_xmm_common", 1)[1]
        common = common.split("static bool deal_dest_not_xmm_32", 1)[0]
        for start, end in (
            ("case WRAP(ADDPD):", "case WRAP(ADDSUBPS):"),
            ("case WRAP(ADDSUBPS):", "/* 32 ~ 127 Unmodified"),
            ("case WRAP(CMPPD):", "case WRAP(PCMPGTB):"),
        ):
            block = common.split(start, 1)[1].split(end, 1)[0]
            self.assertIn("shbr_record_vector_reads(ir1, xmm);", block)
            self.assertLess(
                block.index("shbr_record_vector_reads(ir1, xmm);"),
                block.index("src_des_update_des(ir1, xmm);"),
            )

    def test_o1_preserves_capstone_operand_access(self):
        configure = (REPO_ROOT / "configure").read_text(encoding="utf-8")
        meson = (
            REPO_ROOT / "target/i386/latx/meson.build"
        ).read_text(encoding="utf-8")
        mapping = (
            REPO_ROOT
            / "target/i386/latx/capstone_git/arch/X86/X86Mapping.c"
        ).read_text(encoding="utf-8")
        printer = (
            REPO_ROOT
            / "target/i386/latx/capstone_git/arch/X86/X86IntelInstPrinter.c"
        ).read_text(encoding="utf-8")
        self.assertIn("CONFIG_LATX_CAPSTONE_OP_ACCESS=y", configure)
        self.assertIn("LATX_CAPSTONE_OP_ACCESS", meson)
        self.assertIn(
            "!defined(CAPSTONE_DIET) || defined(LATX_CAPSTONE_OP_ACCESS)",
            mapping,
        )
        self.assertIn(
            "defined(CAPSTONE_DIET) && defined(LATX_CAPSTONE_OP_ACCESS)",
            printer,
        )

    def test_special_dispatch_is_supported(self):
        names = latx_hbr_audit.parse_translator_mnemonics(
            "TRANS_FUNC_GEN(ADDSS, addss),\n"
            "if (ir1_opcode(ir1) == dt_X86_INS_MOVSD) {}\n"
        )
        self.assertEqual(names, {"ADDSS", "MOVSD"})

    def test_duplicate_inventory_entry_fails(self):
        data = json.loads(INVENTORY.read_text(encoding="utf-8"))
        data = copy.deepcopy(data)
        duplicate = data["unmodeled_groups"][0]["mnemonics"][0]
        data["unmodeled_groups"][1]["mnemonics"].append(duplicate)
        data["unmodeled_groups"][1]["expected_count"] += 1
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(
                latx_hbr_audit.AuditError, "duplicate inventory mnemonic"
            ):
                latx_hbr_audit.audit(REPO_ROOT, path)

    def test_deleted_trailing_x_alias_inventory_entry_fails(self):
        data = json.loads(INVENTORY.read_text(encoding="utf-8"))
        data = copy.deepcopy(data)
        alias = "VCVTPD2DQX"
        group = next(
            group for group in data["unmodeled_groups"]
            if alias in group["mnemonics"]
        )
        group["mnemonics"].remove(alias)
        group["expected_count"] -= 1
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(
                latx_hbr_audit.AuditError,
                "trailing-X aliases missing from inventory.*VCVTPD2DQX",
            ):
                latx_hbr_audit.audit(REPO_ROOT, path)

    def test_generated_rule_drift_fails(self):
        data = json.loads(INVENTORY.read_text(encoding="utf-8"))
        data = copy.deepcopy(data)
        data["exact_rule_overrides"]["AESDEC"] = {
            "high32": "IGNORE",
            "high64": "IGNORE",
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(
                latx_hbr_audit.AuditError, "generated semantic table is stale"
            ):
                latx_hbr_audit.audit(REPO_ROOT, path)

    def test_invalid_exact_rule_fails(self):
        data = json.loads(INVENTORY.read_text(encoding="utf-8"))
        data = copy.deepcopy(data)
        data["exact_rule_overrides"]["AESDEC"] = {"high64": "UNKNOWN"}
        with self.assertRaisesRegex(
            generate_latx_hbr_semantics.GenerationError,
            "AESDEC has invalid high64 rule",
        ):
            generate_latx_hbr_semantics.exact_rules(data)


if __name__ == "__main__":
    unittest.main()
