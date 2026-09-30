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
        "EGL wrapper must validate the host ANGLE capabilities it needs"
    )
    assert "kzt_groups_log_wrapper_rejection" in wrappedlibegl, (
        "EGL wrapper rejection must be reported through the generic policy"
    )
    assert "return -1;" in wrappedlibegl, (
        "an incompatible EGL wrapper must ask the generic loader to fall back"
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
