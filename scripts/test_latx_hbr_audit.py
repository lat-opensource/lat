#!/usr/bin/env python3

import copy
import json
import tempfile
import unittest
from pathlib import Path

import latx_hbr_audit


REPO_ROOT = Path(__file__).resolve().parents[1]
INVENTORY = REPO_ROOT / (
    "target/i386/latx/optimization/hbr-xmm-semantics.json"
)


class HbrAuditTest(unittest.TestCase):
    def test_repository_inventory(self):
        result = latx_hbr_audit.audit(REPO_ROOT)
        self.assertEqual(result["counts"]["translator_supported"], 1098)
        self.assertEqual(result["counts"]["shbr_relevant"], 771)
        self.assertEqual(result["counts"]["opcode_specific"], 255)
        self.assertEqual(result["counts"]["conservative_unmodeled"], 516)
        self.assertEqual(len(result["rows"]), 771)
        self.assertEqual(len(result["promoted_opcode_specific"]), 99)
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

    def test_opcode_promotion_drift_fails(self):
        data = json.loads(INVENTORY.read_text(encoding="utf-8"))
        data = copy.deepcopy(data)
        data["promoted_opcode_specific"].pop()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(
                latx_hbr_audit.AuditError, "opcode-specific promotion drift"
            ):
                latx_hbr_audit.audit(REPO_ROOT, path)


if __name__ == "__main__":
    unittest.main()
