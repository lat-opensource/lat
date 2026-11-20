#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
if ! command -v unshare >/dev/null || ! unshare -Ur true 2>/dev/null; then
    echo 'SKIP: unprivileged user namespaces are unavailable'
    exit 77
fi
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
guest=${LATX_PAGEFLAGS_TEST_GUEST:-}
if [ -z "$guest" ]; then
    if ! command -v clang >/dev/null || ! command -v ld.lld >/dev/null; then
        echo 'SKIP: x86-capable clang and ld.lld are required'
        exit 77
    fi
    guest=$work/guest
    clang --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
        -Wl,--build-id=none "$2" -o "$guest"
fi
file "$guest" | grep -q 'ELF 64-bit.*x86-64'
for mode in 0 1 3; do
    env -u SBX_USER_NS LATX_AOT="$mode" LATX_KZT=0 "$1" "$guest"
done
echo 'PASS: page metadata reclamation preserves user namespaces'
