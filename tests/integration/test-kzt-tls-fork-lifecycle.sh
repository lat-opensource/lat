#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
emulator=$1
mode=$2
guest_source=$3
probe_source=$4
plugin_source=$5
dummy_source=$6
guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
guest_cc=${LATX_X86_64_CC:-x86_64-linux-gnu-gcc}
native_cc=${LATX_NATIVE_CC:-cc}
x11_include=${LATX_X11_INCLUDE:-/usr/include}
artifacts=${LATX_KZT_FORK_GUEST_ARTIFACT_DIR:-}
if [ "$(uname -m)" != loongarch64 ] || [ ! -d "$guest_root" ]; then
    echo 'SKIP: requires LoongArch and an x86-64 Guest runtime'
    exit 77
fi
if [ "$mode" = flush ] && ! command -v gdb >/dev/null 2>&1; then
    echo 'SKIP: gdb is required to observe hook execution after full TB flush'
    exit 77
fi
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir "$work/guest-lib"
"$native_cc" -shared -fPIC -O2 -Wall -Wextra -Werror -I"$x11_include" \
    "$probe_source" -pthread -ldl -Wl,-soname,libX11.so.6 -o "$work/libX11.so.6"
if [ -n "$artifacts" ]; then
    for source in "$guest_source" "$plugin_source" "$dummy_source"; do
        name=$(basename "$source")
        actual=$(sha256sum "$source" | awk '{print $1}')
        expected=$(awk -v name="$name" '$2 == name {print $1}' "$artifacts/sources.sha256")
        [ "$actual" = "$expected" ] || { echo "FAIL: source mismatch $name"; exit 1; }
    done
    (cd "$artifacts" && sha256sum -c artifacts.sha256)
    cp "$artifacts/guest" "$work/guest"
    cp "$artifacts/guest-lib/"*.so* "$work/guest-lib/"
else
    if ! command -v "$guest_cc" >/dev/null 2>&1; then
        echo 'SKIP: x86-64 compiler unavailable; provide verified Guest artifacts'
        exit 77
    fi
    for name in a b; do
        "$guest_cc" --sysroot="$guest_root" -shared -fPIC -O2 "$plugin_source" \
            -o "$work/guest-lib/libtls-$name.so"
    done
    "$guest_cc" --sysroot="$guest_root" -shared -fPIC -O2 -I"$x11_include" \
        "$dummy_source" -Wl,-soname,libX11.so.6 -o "$work/guest-lib/libX11.so.6"
    "$guest_cc" --sysroot="$guest_root" -fPIE -pie -O2 -I"$x11_include" \
        "$guest_source" -L"$work/guest-lib" -Wl,--no-as-needed -l:libX11.so.6 \
        -ldl -pthread -o "$work/guest"
fi
export LATX_KZT=1 LATX_KZT_GUEST_TLS=1 LATX_KZT_LIBS=x11 LATX_AOT=0
export BOX64_LD_LIBRARY_PATH="$work/guest-lib"
set -- "$emulator" -U LD_PRELOAD -E "LD_LIBRARY_PATH=$work/guest-lib" \
    -L "$guest_root" "$work/guest" "$mode" \
    "$work/guest-lib/libtls-a.so" "$work/guest-lib/libtls-b.so"
if [ "$mode" = flush ]; then
    cat > "$work/hooks.gdb" <<'GDB'
set pagination off
set confirm off
set breakpoint pending on
set $hooks_after_flush = 0
set $flushes = 0
break do_tb_flush
commands
silent
set $flushes = $flushes + 1
continue
end
break kzt_guest_tls_fork_prepare_early
commands
silent
if $flushes > 0
set $hooks_after_flush = $hooks_after_flush + 1
end
continue
end
run
printf "POST_FLUSH_HOOKS=%d FLUSHES=%d\n", $hooks_after_flush, $flushes
quit
GDB
    LD_PRELOAD="$work/libX11.so.6" gdb -q -batch -x "$work/hooks.gdb" \
        --args "$@" > "$work/output.log" 2>&1
    grep -Eq '^POST_FLUSH_HOOKS=[2-9][0-9]* FLUSHES=[1-9][0-9]*$' "$work/output.log" || {
        cat "$work/output.log"; exit 1;
    }
else
    LD_PRELOAD="$work/libX11.so.6" "$@" > "$work/output.log" 2>&1 || {
        cat "$work/output.log"; exit 1;
    }
fi
cat "$work/output.log"
grep -q "^PASS: review lifecycle $mode$" "$work/output.log"
