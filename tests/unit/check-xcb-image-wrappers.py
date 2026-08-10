#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys


EXPECTED = {
    "xcb_create_pixmap_from_bitmap_data": "uFbupuuuuup",
    "xcb_image_create": "pFWWuCCCCuupup",
    "xcb_image_create_native": "pFbWWuCpup",
    "xcb_image_get": "pFbuwwWWuu",
    "xcb_image_native": "pFbpi",
    "xcb_image_put": "UFbuupwwC",
    "xcb_image_shm_put": "pFbuupppwwwwWWC",
}


def main() -> int:
    private = pathlib.Path(sys.argv[1]).read_text()
    wrapper_h = pathlib.Path(sys.argv[2]).read_text()
    wrapper_c = pathlib.Path(sys.argv[3]).read_text()

    for symbol, signature in EXPECTED.items():
        mapping = f"GO({symbol}, {signature})"
        assert mapping in private, f"missing corrected mapping: {mapping}"
        assert f"void {signature}(uintptr_t fcn);" in wrapper_h, (
            f"missing wrapper declaration for {signature}"
        )
        assert f"void {signature}(uintptr_t fcn)" in wrapper_c, (
            f"missing wrapper implementation for {signature}"
        )

    for signature in EXPECTED.values():
        if "b" not in signature:
            continue
        marker = f"void {signature}(uintptr_t fcn)"
        start = wrapper_c.index(marker)
        end = wrapper_c.index("\n", start)
        body = wrapper_c[start:end]
        assert "align_xcb_connection" in body, (
            f"{signature} does not align its xcb_connection_t argument"
        )

    print("xcb-image mappings match the public C prototypes and "
          "XCB alignment ABI")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
