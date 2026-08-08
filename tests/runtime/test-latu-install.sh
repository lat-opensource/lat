#!/bin/sh
set -eu

[ "$#" -eq 3 ] || {
    echo "usage: $0 NINJA BUILD_DIR PREFIX" >&2
    exit 2
}

ninja=$1
builddir=$2
prefix=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

destdir=$workdir/dest
install_log=$workdir/install.log
DESTDIR="$destdir" "$ninja" -C "$builddir" install \
    > "$install_log" 2>&1 || {
        cat "$install_log" >&2
        fail 'ninja install into DESTDIR failed'
    }

manager=$destdir$prefix/bin/latu-runtime-manager
[ -x "$manager" ] ||
    fail "installed runtime manager is not executable: $manager"
[ ! -e "$destdir$prefix/bin/latu" ] ||
    fail 'runtime manager installation created an overlapping latu command'

x86_64_status=missing
i386_status=missing
[ ! -x "$destdir$prefix/bin/latx-x86_64" ] || x86_64_status=present
[ ! -x "$destdir$prefix/bin/latx-i386" ] || i386_status=present
expected_default="translator_x86_64=$x86_64_status
translator_i386=$i386_status"
actual=$("$manager" status) ||
    fail 'installed runtime manager default status failed'
[ "$actual" = "$expected_default" ] || {
    echo 'FAIL: installed runtime manager did not probe its siblings' >&2
    echo 'expected:' >&2
    printf '%s\n' "$expected_default" >&2
    echo 'actual:' >&2
    printf '%s\n' "$actual" >&2
    exit 1
}

root=$workdir/example-root
mkdir -p "$root/usr/bin"
: > "$root/usr/bin/latx-x86_64"
: > "$root/usr/bin/latx-i386"
chmod +x "$root/usr/bin/latx-x86_64" "$root/usr/bin/latx-i386"

expected='translator_x86_64=present
translator_i386=present'
actual=$("$manager" status --root "$root") ||
    fail 'installed runtime manager status failed'
[ "$actual" = "$expected" ] || {
    echo 'FAIL: installed runtime manager produced unexpected output' >&2
    echo 'expected:' >&2
    printf '%s\n' "$expected" >&2
    echo 'actual:' >&2
    printf '%s\n' "$actual" >&2
    exit 1
}

echo 'PASS: installed independent LATU runtime manager'
