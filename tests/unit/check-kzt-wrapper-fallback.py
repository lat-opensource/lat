#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys


def main() -> int:
    wrappedlibdl = pathlib.Path(sys.argv[1]).read_text()
    wrappedlibegl = pathlib.Path(sys.argv[2]).read_text()
    library_list = pathlib.Path(sys.argv[3]).read_text()
    group_list = pathlib.Path(sys.argv[4]).read_text()

    assert "needs_guest_egl" not in wrappedlibdl, (
        "dlfcn must not special-case guest fallback by library name"
    )
    assert "call_guest_dlopen" in wrappedlibdl, (
        "wrapped-library loading must have a guest-loader fallback"
    )
    get_library = wrappedlibdl.index("lib = GetLibInternal(rfilename);")
    fallback = wrappedlibdl.index("call_guest_dlopen(filename, flag, dl);",
                                  get_library)
    assert fallback > get_library, (
        "guest fallback must run after wrapped-library initialization fails"
    )

    assert "host_egl_supports_angle_passthrough" in wrappedlibegl, (
        "the EGL wrapper must validate the ANGLE capabilities it needs"
    )
    assert "query_string(display, 0x3055)" in wrappedlibegl, (
        "the ANGLE extensions are display extensions, so query the display"
    )
    assert "query_string(NULL, 0x3055)" in wrappedlibegl, (
        "implementations that report the extensions as client extensions "
        "must still be accepted"
    )
    assert "kzt_group_was_named(KZT_GROUP_EGL)" in wrappedlibegl, (
        "an explicit LATX_KZT_LIBS request for the egl group must override "
        "the default capability guard"
    )
    assert "kzt_groups_log_wrapper_rejection" in wrappedlibegl, (
        "EGL wrapper rejection must be reported through the generic policy"
    )

    assert 'GO("libEGL.so.1", libegl, KZT_GROUP_EGL)' in library_list, (
        "libEGL must have its own group so guest ANGLE stacks can opt out"
    )
    assert ('X(EGL,    "egl",    9, STABLE,      '
            'KZT_GROUP_CORE | KZT_GROUP_X11)' in group_list), (
        "the stable egl group must stay enabled by default"
    )

    assert ('GO("libvulkan.so.1", vulkan, KZT_GROUP_VULKAN)' in
            library_list), "libvulkan must remain eligible for KZT passthrough"
    assert ('X(VULKAN, "vulkan", 3, STABLE,      '
            'KZT_GROUP_CORE | KZT_GROUP_X11)' in group_list), (
        "the stable Vulkan group must remain enabled by default"
    )

    print("KZT uses generic wrapper fallback and keeps Vulkan passthrough")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
