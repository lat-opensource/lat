#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "normalize", Path(__file__).with_name("normalize-invalidation-stats.py"))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class InvalidationStatsTest(unittest.TestCase):
    def test_operation_is_not_subtracted(self):
        text = ("LATC_INVALIDATION_LOAD_BEGIN\n"
                "latx: AOT v2 deactivated range=0-1 reason=map-fixed\n"
                "LATC_INVALIDATION_LOAD_END\n"
                "latx: AOT v2 deactivated range=0-1 reason=map-fixed\n"
                "latx: AOT v2 runtime stats invalidated_instances=2 "
                "invalidated_exec_ranges=2 invalidation_map_fixed=2\n")
        result = module.normalize(text)
        self.assertIn("invalidated_instances=1 ", result)
        self.assertIn("invalidated_exec_ranges=1 ", result)
        self.assertIn("invalidation_map_fixed=1\n", result)
        self.assertEqual(result.count("deactivated"), 2)

    def test_incomplete_output_is_rejected(self):
        for text in ("", "LATC_INVALIDATION_LOAD_BEGIN\n",
                     "LATC_INVALIDATION_LOAD_END\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                module.normalize(text)

    def test_unexpected_loader_event_is_rejected(self):
        with self.assertRaises(ValueError):
            module.normalize("LATC_INVALIDATION_LOAD_BEGIN\n"
                             "latx: AOT v2 deactivated reason=unmap\n")


if __name__ == "__main__":
    unittest.main()
