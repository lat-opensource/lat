#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys


def main() -> int:
    source = pathlib.Path(sys.argv[1]).read_text()

    callback = """static int my_XSynchronizeProc_##A(void *dpy)"""
    dispatch = """RunFunctionFmt(my_XSynchronizeProc_fct_##A, "p", dpy)"""
    reverse = """AddBridge(lib->priv.w.bridge, iFp, fct, 0, NULL)"""

    assert callback in source, (
        "XSynchronizeProc must accept its Display argument"
    )
    assert dispatch in source, (
        "XSynchronizeProc must forward its Display argument"
    )
    assert reverse in source, "native XSynchronizeProc must use the iFp bridge"

    assert "bridge_X11_mutex_functions(lib);" in source, (
        "libX11 must bridge its externally referenced mutex callbacks"
    )
    assert '_XLockMutex_fn' in source and '_XUnlockMutex_fn' in source, (
        "Xlib lock and unlock mutex callbacks must be bridged"
    )
    for unnecessary in (
        "_XCreateMutex_fn", "_XFreeMutex_fn", "_Xthread_self_fn",
    ):
        assert unnecessary not in source, (
            f"unreferenced Xlib internal callback {unnecessary} must not be bridged"
        )

    print("X11 callback ABI and required mutex bridges are present")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
