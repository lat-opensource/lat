#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys


def main() -> int:
    source = pathlib.Path(sys.argv[1]).read_text()

    callback = """static int my_XSynchronizeProc_##A(void *dpy)"""
    dispatch = """RunFunctionWithState(my_XSynchronizeProc_fct_##A, 1, dpy)"""
    reverse = """AddBridge(lib->priv.w.bridge, iFp, fct, 0, NULL)"""

    assert callback in source, (
        "XSynchronizeProc must accept its Display argument"
    )
    assert dispatch in source, (
        "XSynchronizeProc must forward its Display argument"
    )
    assert reverse in source, "native XSynchronizeProc must use the iFp bridge"
    print("XSynchronizeProc preserves its one-argument callback ABI")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
