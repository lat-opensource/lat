#!/bin/sh
set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
glib_src=$srcdir/third_party/glib
glib_build=$srcdir/build-glib
pc_dir=$glib_build/pkgconfig

if [ -f "$glib_build/build.ninja" ]; then
    meson setup --reconfigure "$glib_build" "$glib_src" \
        -Ddefault_library=static -Dtests=false -Dman=false \
        -Dselinux=disabled -Dlibmount=disabled -Dnls=disabled
else
    meson setup "$glib_build" "$glib_src" \
        -Ddefault_library=static -Dtests=false -Dman=false \
        -Dselinux=disabled -Dlibmount=disabled -Dnls=disabled
fi
ninja -C "$glib_build" glib/libglib-2.0.a
# A unique library name prevents an earlier system -L path from winning.
ln -sf libglib-2.0.a "$glib_build/glib/liblatx-glib.a"

mkdir -p "$glib_build/gmodule" "$pc_dir"
python3 "$glib_src/tools/gen-visibility-macros.py" 2.76.6 \
    visibility-macros GMODULE "$glib_build/gmodule/gmodule-visibility.h"

cat > "$pc_dir/glib-2.0.pc" <<EOF
Name: GLib
Description: GLib subset bundled with LATX
Version: 2.76.6
Cflags: -I$glib_src -I$glib_src/glib -I$glib_src/gmodule -I$glib_build -I$glib_build/glib
Libs: -Wl,--exclude-libs,liblatx-glib.a -L$glib_build/glib -l:liblatx-glib.a -pthread -lm
EOF
cat > "$pc_dir/gthread-2.0.pc" <<EOF
Name: GThread
Description: GThread bundled with LATX
Version: 2.76.6
Requires: glib-2.0
Cflags: -pthread
Libs: -pthread
EOF
