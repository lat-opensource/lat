#!/bin/sh
set -eu

[ "$#" -eq 3 ] || {
    echo "usage: $0 PYTHON TRANSLATOR GUEST_ABI" >&2
    exit 2
}

python=$1
translator=$2
guest_abi=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

assert_info()
{
    expected_root=$1
    expected_source=$2
    actual=$3

    "$python" -c '
import json
import sys

actual = json.loads(sys.argv[1])
expected = {
    "schema_version": 1,
    "guest_abi": sys.argv[2],
    "runtime_root": sys.argv[3],
    "runtime_source": sys.argv[4],
}
if actual != expected:
    raise SystemExit(f"unexpected runtime info: {actual!r} != {expected!r}")
' "$actual" "$guest_abi" "$expected_root" "$expected_source" ||
        fail "unexpected $expected_source runtime information"
}

case "$guest_abi" in
    x86_64) config_name=latx-x86_64.conf ;;
    i386) config_name=latx-i386.conf ;;
    *) fail "unsupported test guest ABI: $guest_abi" ;;
esac

test_home=$workdir/home
mkdir -p "$test_home/.config"
cat > "$test_home/.config/$config_name" <<EOF
UNKNOWN_RUNTIME_TEST_OPTION=ignored
LAT_LD_PREFIX=$workdir/global-root
[app]
LAT_LD_PREFIX=$workdir/app-root
EOF

output=$(HOME=$test_home "$translator" --runtime-info \
    2> "$workdir/user.stderr") || fail 'user runtime info failed'
assert_info "$workdir/global-root" user_config "$output"

output=$(HOME=$test_home "$translator" --runtime-info /usr/bin/app \
    2> "$workdir/program.stderr") || fail 'program runtime info failed'
assert_info "$workdir/app-root" user_config "$output"

environment_root="$workdir/environment
root"
output=$(HOME=$test_home LAT_LD_PREFIX=$environment_root \
    "$translator" --runtime-info 2> "$workdir/environment.stderr") ||
    fail 'environment runtime info failed'
assert_info "$environment_root" environment "$output"

output=$(HOME=$test_home LAT_LD_PREFIX=$workdir/environment-root \
    "$translator" --runtime-info -L "$workdir/command-line-root" \
    2> "$workdir/command-line.stderr") ||
    fail 'command-line runtime info failed'
assert_info "$workdir/command-line-root" command_line "$output"

output=$(HOME=$test_home LAT_VERSION=1 LATX_AVX_CPUID=1 \
    "$translator" --runtime-info 2> "$workdir/noisy-option.stderr") ||
    fail 'runtime info with a diagnostic option failed'
assert_info "$workdir/global-root" user_config "$output"

echo "PASS: $guest_abi runtime selection information"
