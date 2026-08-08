#!/bin/sh
set -eu

[ "$#" -eq 1 ] || {
    echo "usage: $0 LATU_RUNTIME_MANAGER" >&2
    exit 2
}

manager=$1
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

assert_status()
{
    expected=$1
    actual=$2
    context=$3
    [ "$actual" -eq "$expected" ] ||
        fail "$context: expected status $expected, got $actual"
}

assert_output()
{
    expected=$1
    actual=$2
    context=$3
    [ "$actual" = "$expected" ] || {
        echo "FAIL: $context: unexpected output" >&2
        echo 'expected:' >&2
        printf '%s\n' "$expected" >&2
        echo 'actual:' >&2
        printf '%s\n' "$actual" >&2
        exit 1
    }
}

install_translator()
{
    install_root=$1
    install_name=$2
    mkdir -p "$install_root/usr/bin"
    : > "$install_root/usr/bin/$install_name"
    chmod +x "$install_root/usr/bin/$install_name"
}

run_status()
{
    status_root=$1
    set +e
    status_output=$("$manager" status --root "$status_root" \
        2> "$workdir/stderr")
    status_code=$?
    set -e
}

both='translator_x86_64=present
translator_i386=present'
x86_64_only='translator_x86_64=present
translator_i386=missing'
i386_only='translator_x86_64=missing
translator_i386=present'
neither='translator_x86_64=missing
translator_i386=missing'
unknown='translator_x86_64=unknown
translator_i386=unknown'
x86_64_unknown='translator_x86_64=unknown
translator_i386=missing'

root=$workdir/both
install_translator "$root" latx-x86_64
install_translator "$root" latx-i386
run_status "$root"
assert_status 0 "$status_code" 'both translators installed'
assert_output "$both" "$status_output" 'both translators installed'

root=$workdir/x86-64-only
install_translator "$root" latx-x86_64
run_status "$root"
assert_status 0 "$status_code" 'only x86-64 translator installed'
assert_output "$x86_64_only" "$status_output" \
    'only x86-64 translator installed'

root=$workdir/i386-only
install_translator "$root" latx-i386
run_status "$root"
assert_status 0 "$status_code" 'only i386 translator installed'
assert_output "$i386_only" "$status_output" \
    'only i386 translator installed'

root=$workdir/neither
mkdir -p "$root/usr/bin"
run_status "$root"
assert_status 0 "$status_code" 'neither translator installed'
assert_output "$neither" "$status_output" 'neither translator installed'

root=$workdir/not-executable
mkdir -p "$root/usr/bin"
: > "$root/usr/bin/latx-x86_64"
: > "$root/usr/bin/latx-i386"
run_status "$root"
assert_status 0 "$status_code" 'non-executable files'
assert_output "$neither" "$status_output" 'non-executable files'

root=$workdir/translator-symlink
mkdir -p "$root/usr/libexec/latu" "$root/usr/bin"
: > "$root/usr/libexec/latu/latx-x86_64"
chmod +x "$root/usr/libexec/latu/latx-x86_64"
ln -s ../libexec/latu/latx-x86_64 "$root/usr/bin/latx-x86_64"
run_status "$root"
assert_status 0 "$status_code" 'translator symlink inside root'
assert_output "$x86_64_only" "$status_output" \
    'translator symlink inside root'

outside=$workdir/translator-outside
root=$workdir/translator-symlink-escape
install_translator "$outside" latx-x86_64
mkdir -p "$root/usr/bin"
ln -s "$outside/usr/bin/latx-x86_64" "$root/usr/bin/latx-x86_64"
run_status "$root"
assert_status 0 "$status_code" 'translator symlink escapes root'
assert_output "$x86_64_unknown" "$status_output" \
    'translator symlink escapes root'

outside=$workdir/outside
root=$workdir/symlink-escape
install_translator "$outside" latx-x86_64
mkdir -p "$root"
ln -s "$outside/usr" "$root/usr"
run_status "$root"
assert_status 0 "$status_code" 'root symlink escape'
assert_output "$unknown" "$status_output" 'root symlink escape'

root=$workdir/invalid-layout
mkdir -p "$root/usr"
: > "$root/usr/bin"
run_status "$root"
assert_status 0 "$status_code" 'uninspectable translator directory'
assert_output "$unknown" "$status_output" \
    'uninspectable translator directory'

invalid_root=$workdir/not-a-directory
: > "$invalid_root"
run_status "$invalid_root"
assert_status 2 "$status_code" 'invalid root'
assert_output '' "$status_output" 'invalid root'
grep -F "invalid root directory: $invalid_root" "$workdir/stderr" \
    > /dev/null || fail 'invalid root diagnostic missing'

set +e
default_output=$("$manager" status 2> "$workdir/default.stderr")
default_status=$?
set -e
assert_status 0 "$default_status" 'default root'
case "$default_output" in
    'translator_x86_64='*'
translator_i386='*) ;;
    *) fail 'default status output does not contain both translators' ;;
esac

real_bin=$workdir/real-bin
alias_bin=$workdir/alias-bin
mkdir -p "$real_bin" "$alias_bin"
cp "$manager" "$real_bin/latu-runtime-manager"
: > "$real_bin/latx-x86_64"
: > "$real_bin/latx-i386"
chmod +x "$real_bin/latx-x86_64" "$real_bin/latx-i386"
ln -s "$real_bin/latu-runtime-manager" "$alias_bin/latu-runtime-manager"
set +e
symlink_output=$("$alias_bin/latu-runtime-manager" status \
    2> "$workdir/symlink.stderr")
symlink_status=$?
set -e
assert_status 0 "$symlink_status" 'manager invoked through symlink'
assert_output "$both" "$symlink_output" 'manager invoked through symlink'

alternatives_bin=$workdir/alternatives-bin
mkdir -p "$alternatives_bin"
mv "$real_bin/latx-x86_64" "$alternatives_bin/latx-x86_64"
ln -s "$alternatives_bin/latx-x86_64" "$real_bin/latx-x86_64"
set +e
alternatives_output=$("$real_bin/latu-runtime-manager" status \
    2> "$workdir/alternatives.stderr")
alternatives_status=$?
set -e
assert_status 0 "$alternatives_status" 'translator installed via symlink'
assert_output "$both" "$alternatives_output" \
    'translator installed via symlink'

set +e
"$manager" doctor > "$workdir/usage.stdout" 2> "$workdir/usage.stderr"
usage_status=$?
set -e
assert_status 2 "$usage_status" 'unsupported subcommand'
grep -F 'usage: latu-runtime-manager status [--root ROOT]' \
    "$workdir/usage.stderr" > /dev/null || fail 'usage diagnostic missing'

echo 'PASS: distribution-neutral dual-translator status'
