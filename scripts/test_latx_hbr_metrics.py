#!/usr/bin/env python3
"""Regression tests for SHBR static observability metrics."""

import tempfile
import unittest
from pathlib import Path

import latx_hbr_metrics


REPO_ROOT = Path(__file__).resolve().parents[1]


class ShbrMetricsTests(unittest.TestCase):
    def test_current_metrics(self):
        result = latx_hbr_metrics.collect(REPO_ROOT)
        self.assertEqual(result["coverage"]["precise"], 774)
        self.assertEqual(result["coverage"]["conservative"], 0)
        self.assertEqual(result["coverage"]["relevant"], 774)
        self.assertEqual(result["generation_site_counts"]["SHBR_ON_32"], 7)
        self.assertEqual(result["generation_site_counts"]["SHBR_ON_64"], 8)
        self.assertEqual(result["minimum_removed_per_full_hit"]["lasx"], 15)
        self.assertEqual(result["minimum_removed_per_full_hit"]["lsx"], 16)

    def test_missing_gate_is_rejected(self):
        original = latx_hbr_metrics.GENERATION_SITES
        try:
            latx_hbr_metrics.GENERATION_SITES = (
                ("tr-simd.c", "translate_addss", "SHBR_ON_64", 1, 1),
            )
            with self.assertRaisesRegex(
                latx_hbr_metrics.MetricsError, "no longer"
            ):
                latx_hbr_metrics.collect(REPO_ROOT)
        finally:
            latx_hbr_metrics.GENERATION_SITES = original


if __name__ == "__main__":
    unittest.main()
