#!/usr/bin/env python3

import copy
import json
import tempfile
import unittest
from pathlib import Path

import audit_latx_ghbr_semantics as audit


REPO_ROOT = Path(__file__).resolve().parents[1]
INVENTORY = REPO_ROOT / (
    "target/i386/latx/optimization/hbr-gpr-semantics.json"
)


class GhbrAuditTest(unittest.TestCase):
    def load_inventory(self):
        return json.loads(INVENTORY.read_text(encoding="utf-8"))

    def audit_with(self, data):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            return audit.audit(REPO_ROOT, path)

    def test_repository_inventory(self):
        result = self.audit_with(self.load_inventory())
        self.assertEqual(result["consumer_functions"], 29)
        self.assertEqual(result["mov32_functions"], 38)
        self.assertEqual(result["categories"].get("candidate-review", 0), 0)
        self.assertTrue(result["may_def_model"])

    def test_missing_mov32_site_fails(self):
        data = self.load_inventory()
        data["mov32_sites"].pop()
        with self.assertRaisesRegex(audit.AuditError, "missing inventory"):
            self.audit_with(data)

    def test_stale_mov32_site_fails(self):
        data = self.load_inventory()
        data["mov32_sites"].append({
            "file": "tr-arith.c",
            "function": "not_a_real_function",
            "gated": False,
            "category": "temporary-conversion",
        })
        with self.assertRaisesRegex(audit.AuditError, "stale inventory"):
            self.audit_with(data)

    def test_unknown_category_fails(self):
        data = copy.deepcopy(self.load_inventory())
        data["mov32_sites"][0]["category"] = "unknown"
        with self.assertRaisesRegex(audit.AuditError, "unknown category"):
            self.audit_with(data)

    def test_gated_consumer_mismatch_fails(self):
        data = self.load_inventory()
        site = next(
            site for site in data["mov32_sites"]
            if site["function"] == "translate_mul"
        )
        site["gated"] = True
        with self.assertRaisesRegex(audit.AuditError, "gated mov32 sites"):
            self.audit_with(data)


if __name__ == "__main__":
    unittest.main()
