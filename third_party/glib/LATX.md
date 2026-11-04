# GLib subset for LATX

This directory contains the GLib core from GLib 2.76.6, obtained from
https://download.gnome.org/sources/glib/2.76/glib-2.76.6.tar.xz.
The release archive SHA-256 is
`1136ae6987dcbb64e0be3197a80190520f7acab81e2bfb937dc85c11c8aa9f04`.
The original license notices and license texts are retained. GLib is primarily
licensed under LGPL-2.1-or-later; individual files may use other licenses.

Only GLib core is built. The upstream build definition was trimmed to remove
unrelated subprojects and components, and the regular expression implementation
was omitted because LATX does not use it. The static linker selects the objects
needed by LATX. `latxbuild/build-bundled-glib.sh` builds the core and generates
private pkg-config metadata for the LATX release build.
