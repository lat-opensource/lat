#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys


def main() -> int:
    source = pathlib.Path(sys.argv[1]).read_text()
    private = pathlib.Path(sys.argv[2]).read_text()

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

    assert "GOM(XInitThreads, iFv)" in private, (
        "XInitThreads must route through the wrapper so a late mutex "
        "callback assignment is bridged"
    )
    xinit = """EXPORT uint32_t my_XInitThreads(void)
{
    uint32_t ret = my->XInitThreads();

    bridge_X11_mutex_functions(my_lib);
    return ret;
}"""
    assert xinit in source, (
        "the XInitThreads wrapper must re-bridge the mutex callbacks after "
        "the host call"
    )

    print("X11 callback ABI and required mutex bridges are present")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
