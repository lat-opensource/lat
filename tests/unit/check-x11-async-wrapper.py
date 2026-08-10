#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys


def function_body(source: str, marker: str, occurrence: int = 1) -> str:
    definition = -1
    for _ in range(occurrence):
        definition = source.index(marker, definition + 1)
    body_start = source.index("{", definition)
    depth = 0

    for offset, char in enumerate(source[body_start:], body_start):
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[body_start:offset + 1]

    raise AssertionError(f"unterminated function after {marker}")


def check_wrapper(source: str, private: str, generated: str) -> None:
    function = "_XGetRequest"
    signature = "pFpCL"
    native_signature = "pFpCL_t"
    route = f"GOM({function}, {signature})"
    assert route in private, f"{function} must use the custom wrapper: {route}"
    native_target = f"GO({function}, {native_signature})"
    assert native_target in generated, (
        f"{function} is missing from the native wrapper targets: "
        f"{native_target}"
    )

    body = function_body(source, "EXPORT void *my__XGetRequest(", 2)
    bridge = "bridge_XInternalAsyncHandlers(dpy, bridge_XInternalAsyncHandler);"
    native_call = f"my->{function}("
    assert bridge in body, f"my_{function} does not bridge async handlers"
    assert native_call in body, f"my_{function} does not call native {function}"
    assert body.index(bridge) < body.index(native_call), (
        f"my_{function} calls native Xlib before bridging async handlers"
    )

    adapter = function_body(source,
                            "static void *bridge_XInternalAsyncHandler(", 2)
    assert "findXInternalAsyncHandlerFct(handler)" in adapter
    assert "AddAutomaticBridge(my_lib->priv.w.bridge, iFpppip," in adapter


def check_dispatch_wrapper(source: str, private: str, generated: str,
                           function: str, signature: str,
                           native_signature: str, marker: str,
                           occurrence: int = 2) -> None:
    route = f"GOM({function}, {signature})"
    assert route in private, f"{function} must use the custom wrapper: {route}"
    native_target = f"GO({function}, {native_signature})"
    assert native_target in generated, (
        f"{function} is missing from the native wrapper targets: "
        f"{native_target}"
    )

    body = function_body(source, marker, occurrence)
    bridge = "bridge_XInternalAsyncHandlers(dpy, bridge_XInternalAsyncHandler);"
    native_call = f"my->{function}("
    assert bridge in body, f"my_{function} does not bridge async handlers"
    assert native_call in body, f"my_{function} does not call native Xlib"
    assert body.index(bridge) < body.index(native_call), (
        f"my_{function} calls native Xlib before bridging async handlers"
    )


def check_bridge_failure_is_fail_stop(source: str) -> None:
    helper = function_body(
        source, "static void bridge_XInternalAsyncHandlers(", 2)
    assert "&x11_async_bridge_state" in helper, (
        "all Display async-list conversions must share one lock"
    )
    assert "LATX_X11_ASYNC_BRIDGE_OK" in helper, (
        "the wrapper must check whether every guest handler was bridged"
    )
    assert "abort();" in helper, (
        "bridge failure must stop before native Xlib can dispatch a guest "
        "address"
    )


def main() -> int:
    source = pathlib.Path(sys.argv[1]).read_text()
    private = pathlib.Path(sys.argv[2]).read_text()
    generated = pathlib.Path(sys.argv[3]).read_text()

    check_bridge_failure_is_fail_stop(source)
    check_wrapper(source, private, generated)
    check_dispatch_wrapper(source, private, generated,
                           "XEventsQueued", "iFpi", "iFpi_t",
                           "EXPORT int32_t my_XEventsQueued(")
    check_dispatch_wrapper(source, private, generated,
                           "XFlush", "iFp", "iFp_t",
                           "EXPORT int32_t my_XFlush(")
    check_dispatch_wrapper(source, private, generated,
                           "_XFlush", "vFp", "vFp_t",
                           "EXPORT void my__XFlush(")
    check_dispatch_wrapper(source, private, generated,
                           "_XReadEvents", "vFp", "vFp_t",
                           "EXPORT void my__XReadEvents(")
    check_dispatch_wrapper(source, private, generated,
                           "_XReply", "iFppii", "iFppii_t",
                           "EXPORT int32_t my__XReply(")
    check_dispatch_wrapper(source, private, generated,
                           "XNextEvent", "iFpp", "iFpp_t",
                           "EXPORT int32_t my_XNextEvent(")
    check_dispatch_wrapper(source, private, generated,
                           "_XError", "iFpp", "iFpp_t",
                           "EXPORT int32_t my__XError(")
    check_dispatch_wrapper(source, private, generated,
                           "XIfEvent", "iFpppp", "iFpppp_t",
                           "EXPORT int32_t my_XIfEvent(", 1)
    check_dispatch_wrapper(source, private, generated,
                           "XCheckIfEvent", "iFpppp", "iFpppp_t",
                           "EXPORT int32_t my_XCheckIfEvent(")
    check_dispatch_wrapper(source, private, generated,
                           "XPeekIfEvent", "iFpppp", "iFpppp_t",
                           "EXPORT int32_t my_XPeekIfEvent(")
    check_dispatch_wrapper(source, private, generated,
                           "XMaskEvent", "iFplp", "iFplp_t",
                           "EXPORT int32_t my_XMaskEvent(")
    check_dispatch_wrapper(source, private, generated,
                           "XPeekEvent", "iFpp", "iFpp_t",
                           "EXPORT int32_t my_XPeekEvent(")
    check_dispatch_wrapper(source, private, generated,
                           "XWindowEvent", "iFpplp", "iFpplp_t",
                           "EXPORT int32_t my_XWindowEvent(")
    check_dispatch_wrapper(source, private, generated,
                           "XCheckMaskEvent", "iFplp", "iFplp_t",
                           "EXPORT int32_t my_XCheckMaskEvent(")
    check_dispatch_wrapper(source, private, generated,
                           "XCheckTypedEvent", "iFpip", "iFpip_t",
                           "EXPORT int32_t my_XCheckTypedEvent(")
    check_dispatch_wrapper(source, private, generated,
                           "XCheckTypedWindowEvent", "iFppip", "iFppip_t",
                           "EXPORT int32_t my_XCheckTypedWindowEvent(")
    check_dispatch_wrapper(source, private, generated,
                           "XCheckWindowEvent", "iFpplp", "iFpplp_t",
                           "EXPORT int32_t my_XCheckWindowEvent(")
    print("X11 reply, error, flush, and event paths bridge before native Xlib")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
