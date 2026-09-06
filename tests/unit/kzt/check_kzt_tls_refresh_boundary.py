#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


callback = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
wrappedlibdl = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8")
myalign = pathlib.Path(sys.argv[3]).read_text(encoding="utf-8")

if callback.count("kzt_guest_tls_refresh_if_needed(") != 1:
    fail("callback entries must share the generation-gated scope")
if callback.count("callback_scope_enter(&scope,") != 4:
    fail("all callback entries, including float results, must enter the scope")
if "kzt_guest_tls_refresh(" in callback:
    fail("callback hot paths must not invoke unconditional TLS refresh")
if wrappedlibdl.count("kzt_guest_tls_refresh(cpu)") < 3:
    fail("Guest loader mutation paths must retain unconditional refresh")

loader_callback = myalign[myalign.index(
    "static void kzt_dynamic_library_change_callback") :]
begin = loader_callback.index("kzt_guest_tls_loader_event_begin()")
observe = loader_callback.index("kzt_public_loader_observer_refresh(")
if begin > observe:
    fail("loader transition must become dirty before observer refresh")

print("kzt Guest TLS refresh boundary checks: PASS")
