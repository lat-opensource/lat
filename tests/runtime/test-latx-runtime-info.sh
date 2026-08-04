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

assert_stderr()
{
    expected=$1
    stderr_file=$2
    context=$3

    grep -F -- "$expected" "$stderr_file" > /dev/null ||
        fail "$context: missing '$expected'"
}

make_elf()
{
    elf_kind=$1
    elf_path=$2
    elf_interp=$3
    elf_base=$4

    "$python" - "$guest_abi" "$elf_kind" "$elf_path" \
        "$elf_interp" "$elf_base" <<'PY'
import struct
import sys

abi, kind, output, interpreter, base_arg = sys.argv[1:]
base = int(base_arg, 0)
elf_type = 3 if kind == "loader" else 2

if abi == "x86_64":
    elf_class = 2
    machine = 62
    ehsize = 64
    phentsize = 56
    code = bytes.fromhex("b83c00000031ff0f05")
    pack_ehdr = lambda ident, entry, phnum: struct.pack(
        "<16sHHIQQQIHHHHHH", ident, elf_type, machine, 1, entry, ehsize, 0,
        0, ehsize, phentsize, phnum, 0, 0, 0)
    pack_load = lambda size: struct.pack(
        "<IIQQQQQQ", 1, 5, 0, base, base, size, size, 0x1000)
    pack_interp = lambda offset, size: struct.pack(
        "<IIQQQQQQ", 3, 4, offset, 0, 0, size, size, 1)
elif abi == "i386":
    elf_class = 1
    machine = 3
    ehsize = 52
    phentsize = 32
    code = bytes.fromhex("b80100000031dbcd80")
    pack_ehdr = lambda ident, entry, phnum: struct.pack(
        "<16sHHIIIIIHHHHHH", ident, elf_type, machine, 1, entry, ehsize, 0,
        0, ehsize, phentsize, phnum, 0, 0, 0)
    pack_load = lambda size: struct.pack(
        "<IIIIIIII", 1, 0, base, base, size, size, 5, 0x1000)
    pack_interp = lambda offset, size: struct.pack(
        "<IIIIIIII", 3, offset, 0, 0, size, size, 4, 1)
else:
    raise SystemExit(f"unsupported guest ABI: {abi}")

ident = b"\x7fELF" + bytes([elf_class, 1, 1, 0]) + bytes(8)
interp_data = interpreter.encode() + b"\0" if kind == "dynamic" else b""
phnum = 2 if interp_data else 1
interp_offset = ehsize + phentsize * phnum
code_offset = interp_offset + len(interp_data)
total_size = code_offset + len(code)

ehdr = pack_ehdr(ident, base + code_offset, phnum)
phdrs = pack_load(total_size)
if interp_data:
    phdrs += pack_interp(interp_offset, len(interp_data))

with open(output, "wb") as f:
    if kind == "truncated":
        f.write(ehdr)
    else:
        f.write(ehdr + phdrs + interp_data + code)
PY
    chmod +x "$elf_path"
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

case "$guest_abi" in
    x86_64)
        main_base=0x400000
        ;;
    i386)
        main_base=0x08048000
        ;;
esac

empty_home=$workdir/empty-home
mkdir -p "$empty_home"
guest_loader=/latu-test/actual-$guest_abi-loader.so
dynamic_guest=$workdir/dynamic-$guest_abi
static_guest=$workdir/static-$guest_abi
make_elf dynamic "$dynamic_guest" "$guest_loader" "$main_base"
make_elf static "$static_guest" '' "$main_base"

missing_root=$workdir/missing-root
mkdir -p "$missing_root"
set +e
HOME=$empty_home "$translator" -L "$missing_root" "$dynamic_guest" \
    > "$workdir/missing.stdout" 2> "$workdir/missing.stderr"
missing_status=$?
set -e
[ "$missing_status" -ne 0 ] || fail 'missing loader unexpectedly succeeded'
assert_stderr 'LATU: guest runtime loader failure' \
    "$workdir/missing.stderr" 'missing loader'
assert_stderr "guest ABI: $guest_abi" \
    "$workdir/missing.stderr" 'missing loader'
assert_stderr "PT_INTERP: $guest_loader" \
    "$workdir/missing.stderr" 'missing loader'
assert_stderr "runtime root: $missing_root" \
    "$workdir/missing.stderr" 'missing loader'
assert_stderr 'runtime source: command_line' \
    "$workdir/missing.stderr" 'missing loader'
assert_stderr "configured loader: $missing_root$guest_loader" \
    "$workdir/missing.stderr" 'missing loader'
assert_stderr "attempted loader: $guest_loader" \
    "$workdir/missing.stderr" 'missing loader'
assert_stderr 'failure reason: not_found' \
    "$workdir/missing.stderr" 'missing loader'
assert_stderr "next step: run 'latu-runtime-manager status'" \
    "$workdir/missing.stderr" 'missing loader'

relative_loader=latu-test/missing-$guest_abi-loader.so
relative_guest=$workdir/relative-$guest_abi
make_elf dynamic "$relative_guest" "$relative_loader" "$main_base"
set +e
(
    cd "$workdir"
    HOME=$empty_home "$translator" -L "$missing_root" "$relative_guest"
) > "$workdir/relative.stdout" 2> "$workdir/relative.stderr"
relative_status=$?
set -e
[ "$relative_status" -ne 0 ] || fail 'relative loader unexpectedly succeeded'
assert_stderr "PT_INTERP: $relative_loader" \
    "$workdir/relative.stderr" 'relative loader'
assert_stderr "configured loader: $relative_loader" \
    "$workdir/relative.stderr" 'relative loader'
assert_stderr "attempted loader: $relative_loader" \
    "$workdir/relative.stderr" 'relative loader'
assert_stderr 'failure reason: not_found' \
    "$workdir/relative.stderr" 'relative loader'

corrupt_root=$workdir/corrupt-root
mkdir -p "$corrupt_root$(dirname "$guest_loader")"
printf 'not an ELF loader\n' > "$corrupt_root$guest_loader"
set +e
HOME=$empty_home "$translator" -L "$corrupt_root" "$dynamic_guest" \
    > "$workdir/corrupt.stdout" 2> "$workdir/corrupt.stderr"
corrupt_status=$?
set -e
[ "$corrupt_status" -ne 0 ] || fail 'corrupt loader unexpectedly succeeded'
assert_stderr "PT_INTERP: $guest_loader" \
    "$workdir/corrupt.stderr" 'corrupt loader'
assert_stderr "configured loader: $corrupt_root$guest_loader" \
    "$workdir/corrupt.stderr" 'corrupt loader'
assert_stderr "attempted loader: $corrupt_root$guest_loader" \
    "$workdir/corrupt.stderr" 'corrupt loader'
assert_stderr 'failure reason: invalid_elf' \
    "$workdir/corrupt.stderr" 'corrupt loader'

load_error_root=$workdir/load-error-root
mkdir -p "$load_error_root$(dirname "$guest_loader")"
make_elf truncated "$load_error_root$guest_loader" '' 0
set +e
HOME=$empty_home "$translator" -L "$load_error_root" "$dynamic_guest" \
    > "$workdir/load-error.stdout" 2> "$workdir/load-error.stderr"
load_error_status=$?
set -e
[ "$load_error_status" -ne 0 ] || fail 'truncated loader unexpectedly succeeded'
assert_stderr "attempted loader: $load_error_root$guest_loader" \
    "$workdir/load-error.stderr" 'truncated loader'
assert_stderr 'failure reason: load_error' \
    "$workdir/load-error.stderr" 'truncated loader'

permission_root=$workdir/permission-root
mkdir -p "$permission_root$(dirname "$guest_loader")"
: > "$permission_root$guest_loader"
chmod 000 "$permission_root$guest_loader"
if [ "$(id -u)" -ne 0 ]; then
    set +e
    HOME=$empty_home "$translator" -L "$permission_root" "$dynamic_guest" \
        > "$workdir/permission.stdout" 2> "$workdir/permission.stderr"
    permission_status=$?
    set -e
    [ "$permission_status" -ne 0 ] ||
        fail 'unreadable loader unexpectedly succeeded'
    assert_stderr 'failure reason: permission_denied' \
        "$workdir/permission.stderr" 'unreadable loader'
fi

if [ "${LATX_RUNTIME_TEST_SKIP_GUEST_EXECUTION:-0}" = 1 ]; then
    echo "SKIP: $guest_abi translated guest execution"
else
    success_root=$workdir/success-root
    mkdir -p "$success_root$(dirname "$guest_loader")"
    make_elf loader "$success_root$guest_loader" '' 0
    HOME=$empty_home "$translator" -L "$success_root" "$dynamic_guest" \
        > "$workdir/success.stdout" 2> "$workdir/success.stderr" ||
        fail 'valid runtime loader failed'
    if grep -F 'LATU: guest runtime loader failure' \
            "$workdir/success.stderr" > /dev/null; then
        fail 'valid runtime loader emitted failure guidance'
    fi

    HOME=$empty_home "$translator" -L "$missing_root" "$static_guest" \
        > "$workdir/static.stdout" 2> "$workdir/static.stderr" ||
        fail 'static guest failed without a runtime'
    if grep -F 'LATU: guest runtime loader failure' \
            "$workdir/static.stderr" > /dev/null; then
        fail 'static guest emitted runtime failure guidance'
    fi
fi

echo "PASS: $guest_abi runtime selection and loader guidance"
