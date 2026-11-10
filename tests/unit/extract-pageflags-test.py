#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Expose production page metadata bodies to the isolated lifetime test."""
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text()
start = source.index("typedef struct PageFlagsNode {")
end = source.index("} PageFlagsNode;", start) + len("} PageFlagsNode;")
parts = [source[start:end], "IntervalTreeRoot pageflags_root;"]
for signature in (
    "static void pageflags_free_rcu(", "static void pageflags_free(",
    "static PageFlagsNode *pageflags_find(",
    "target_ulong get_first_page(", "int page_get_flags(",
    "bool test_flags(", "static void pageflags_create(",
    "static void pageflags_create_merge(", "static bool pageflags_unset(",
    "bool pageflags_set_clear(", "bool page_check_range(",
):
    start = source.find(signature)
    if start < 0 and signature.startswith("static void pageflags_free"):
        continue  # The same fixture must expose immediate reclamation on RED.
    if start < 0:
        raise ValueError("missing production body: " + signature)
    end = source.index("\n}", start) + 2
    # Generic host tests poison target configuration macros.
    parts.append(source[start:end].replace("TARGET_PAGE_SIZE",
                                          "TEST_PAGE_SIZE"))
pathlib.Path(sys.argv[2]).write_text("\n".join(parts) + "\n")
