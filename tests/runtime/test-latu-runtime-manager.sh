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
    assert_expected=$1
    assert_actual=$2
    assert_context=$3
    [ "$assert_actual" -eq "$assert_expected" ] ||
        fail "$assert_context: expected status $assert_expected," \
            "got $assert_actual"
}

assert_output()
{
    output_expected=$1
    output_actual=$2
    output_context=$3
    [ "$output_actual" = "$output_expected" ] || {
        echo "FAIL: $output_context: unexpected output" >&2
        echo 'expected:' >&2
        printf '%s\n' "$output_expected" >&2
        echo 'actual:' >&2
        printf '%s\n' "$output_actual" >&2
        exit 1
    }
}

assert_contains()
{
    contains_expected=$1
    contains_actual=$2
    contains_context=$3
    case "$contains_actual" in
        *"$contains_expected"*) ;;
        *) fail "$contains_context: missing '$contains_expected'" ;;
    esac
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

install_query_translator()
{
    query_bin=$1
    query_name=$2
    query_json=$3
    cat > "$query_bin/$query_name" <<'EOF'
#!/bin/sh
set -eu

name=${0##*/}
abi=${name#latx-}
[ "$#" -ge 1 ] && [ "$1" = --runtime-info ] || exit 2
if [ -n "${LATU_TEST_ARGV_LOG-}" ]; then
    : > "$LATU_TEST_ARGV_LOG.$abi"
    for argument do
        printf '%s\n' "$argument" >> "$LATU_TEST_ARGV_LOG.$abi"
    done
fi
if [ -e "$0.exit" ]; then
    exit "$(cat "$0.exit")"
fi
cat "$0.json"
EOF
    chmod +x "$query_bin/$query_name"
    printf '%s\n' "$query_json" > "$query_bin/$query_name.json"
}

runtime_info_json()
{
    info_abi=$1
    info_root=$2
    info_source=$3
    printf '{"schema_version":1,"guest_abi":"%s",' "$info_abi"
    printf '"runtime_root":"%s","runtime_source":"%s"}' \
        "$info_root" "$info_source"
}

runtime_query_json()
{
    query_abi=$1
    query_translator=$2
    query_status=$3
    query_root=$4
    query_source=$5
    printf '{"schema_version":1,"guest_abi":"%s",' "$query_abi"
    printf '"translator_status":"%s","query_status":"%s",' \
        "$query_translator" "$query_status"
    printf '"runtime_root":%s,"runtime_source":%s}' \
        "$query_root" "$query_source"
}

make_loader()
{
    loader_root=$1
    loader_abi=$2
    case "$loader_abi" in
        x86_64) loader_path=$loader_root/lib64/ld-linux-x86-64.so.2 ;;
        i386) loader_path=$loader_root/lib/ld-linux.so.2 ;;
        *) fail "unsupported loader ABI: $loader_abi" ;;
    esac
    mkdir -p "${loader_path%/*}"
    python3 - "$loader_abi" "$loader_path" <<'PY'
import struct
import sys

abi, path = sys.argv[1:]
if abi == "x86_64":
    ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + bytes(8)
    ehdr = struct.pack(
        "<16sHHIQQQIHHHHHH", ident, 3, 62, 1, 0, 64, 0, 0,
        64, 56, 1, 0, 0, 0)
    phdr = struct.pack("<IIQQQQQQ", 1, 5, 0, 0, 0, 120, 120, 4096)
else:
    ident = b"\x7fELF" + bytes([1, 1, 1, 0]) + bytes(8)
    ehdr = struct.pack(
        "<16sHHIIIIIHHHHHH", ident, 3, 3, 1, 0, 52, 0, 0,
        52, 32, 1, 0, 0, 0)
    phdr = struct.pack("<IIIIIIII", 1, 0, 0, 0, 84, 84, 5, 4096)
with open(path, "wb") as output:
    output.write(ehdr + phdr)
PY
    chmod +x "$loader_path"
}

make_program()
{
    program_path=$1
    program_abi=$2
    program_interp=${3-}
    mkdir -p "${program_path%/*}"
    python3 - "$program_path" "$program_abi" "$program_interp" <<'PY'
import struct
import sys

path, abi, interp = sys.argv[1:]
dynamic = bool(interp)
if abi == "x86_64":
    ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + bytes(8)
    phnum = 2 if dynamic else 1
    ehdr = struct.pack(
        "<16sHHIQQQIHHHHHH", ident, 3, 62, 1, 0, 64, 0, 0,
        64, 56, phnum, 0, 0, 0)
    payload_offset = 64 + 56 * phnum
    phdrs = [
        struct.pack("<IIQQQQQQ", 1, 5, 0, 0, 0,
                    payload_offset, payload_offset, 4096),
    ]
    if dynamic:
        encoded = interp.encode() + b"\0"
        phdrs.append(struct.pack("<IIQQQQQQ", 3, 4, payload_offset,
                                 0, 0, len(encoded), len(encoded), 1))
else:
    ident = b"\x7fELF" + bytes([1, 1, 1, 0]) + bytes(8)
    phnum = 2 if dynamic else 1
    ehdr = struct.pack(
        "<16sHHIIIIIHHHHHH", ident, 3, 3, 1, 0, 52, 0, 0,
        52, 32, phnum, 0, 0, 0)
    payload_offset = 52 + 32 * phnum
    phdrs = [
        struct.pack("<IIIIIIII", 1, 0, 0, 0,
                    payload_offset, payload_offset, 5, 4096),
    ]
    if dynamic:
        encoded = interp.encode() + b"\0"
        phdrs.append(struct.pack("<IIIIIIII", 3, payload_offset, 0, 0,
                                 len(encoded), len(encoded), 4, 1))
with open(path, "wb") as output:
    output.write(ehdr)
    output.write(b"".join(phdrs))
    if dynamic:
        output.write(encoded)
PY
    chmod +x "$program_path"
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

root=$workdir/translator-dangling-symlink
mkdir -p "$root/usr/libexec/latu" "$root/usr/bin"
ln -s ../libexec/latu/latx-x86_64 "$root/usr/bin/latx-x86_64"
run_status "$root"
assert_status 0 "$status_code" 'dangling translator symlink inside root'
assert_output "$neither" "$status_output" \
    'dangling translator symlink inside root'

root=$workdir/translator-dangling-symlink-escape
escape_parent=$workdir/translator-dangling-outside
mkdir -p "$root/usr/libexec" "$root/usr/bin" "$escape_parent"
ln -s ../libexec/alias "$root/usr/bin/latx-x86_64"
ln -s "$escape_parent/missing" "$root/usr/libexec/alias"
run_status "$root"
assert_status 0 "$status_code" 'multi-hop dangling translator escape'
assert_output "$x86_64_unknown" "$status_output" \
    'multi-hop dangling translator escape'

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
"$manager" install > "$workdir/usage.stdout" 2> "$workdir/usage.stderr"
usage_status=$?
set -e
assert_status 2 "$usage_status" 'unsupported subcommand'
grep -F 'usage: latu-runtime-manager status [--root ROOT]' \
    "$workdir/usage.stderr" > /dev/null || fail 'usage diagnostic missing'

query_bin=$workdir/query-bin
mkdir -p "$query_bin"
cp "$manager" "$query_bin/latu-runtime-manager"
install_query_translator "$query_bin" latx-x86_64 \
    "$(runtime_info_json x86_64 /runtime/64 user_config)"
install_query_translator "$query_bin" latx-i386 \
    "$(runtime_info_json i386 /runtime/32 system_config)"

expected=$(runtime_query_json x86_64 present selected \
    '"/runtime/64"' '"user_config"')
set +e
actual=$("$query_bin/latu-runtime-manager" current --abi x86_64 \
    2> "$workdir/current.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'current x86-64 selection'
assert_output "$expected" "$actual" 'current x86-64 selection'

i386_expected=$(runtime_query_json i386 present selected \
    '"/runtime/32"' '"system_config"')
expected_list="$expected
$i386_expected"
set +e
actual=$("$query_bin/latu-runtime-manager" current \
    2> "$workdir/current-all.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'current selection without explicit ABI'
assert_output "$expected_list" "$actual" \
    'current selection without explicit ABI'

set +e
actual=$("$query_bin/latu-runtime-manager" list \
    2> "$workdir/list.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'list selected runtimes'
assert_output "$expected_list" "$actual" 'list selected runtimes'

LATU_TEST_ARGV_LOG=$workdir/argv \
    "$query_bin/latu-runtime-manager" current --abi i386 \
    --program /guest/app > "$workdir/program.stdout" ||
    fail 'program-specific current query failed'
assert_output '--runtime-info
--
/guest/app' "$(cat "$workdir/argv.i386")" \
    'program-specific translator arguments'

mv "$query_bin/latx-i386" "$query_bin/latx-i386.absent"
expected_missing=$(runtime_query_json i386 missing translator_missing \
    null null)
set +e
actual=$("$query_bin/latu-runtime-manager" current --abi i386 \
    2> "$workdir/missing-query.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'missing translator query'
assert_output "$expected_missing" "$actual" 'missing translator query'

set +e
actual=$("$query_bin/latu-runtime-manager" current \
    2> "$workdir/missing-peer-query.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'one selected default query'
assert_output "$expected
$expected_missing" "$actual" 'one selected default query'
mv "$query_bin/latx-i386.absent" "$query_bin/latx-i386"

invalid_expected=$(runtime_query_json x86_64 present invalid_runtime_info \
    null null)
for invalid_case in \
    '{' \
    '{"schema_version":2,"guest_abi":"x86_64",'\
'"runtime_root":"/runtime/64","runtime_source":"default"}' \
    '{"schema_version":1,"guest_abi":"i386",'\
'"runtime_root":"/runtime/64","runtime_source":"default"}' \
    '{"schema_version":1,"guest_abi":"x86_64",'\
'"runtime_root":"/runtime/64","runtime_source":"provider"}' \
    '{"schema_version":1,"guest_abi":"x86_64",'\
'"runtime_root":"/runtime/64","runtime_source":"default",}' \
    '{"schema_version":1,"guest_abi":"x86_64",'\
'"runtime_root":"/runtime/64","runtime_source":"default"} trailing'
do
    printf '%s\n' "$invalid_case" > "$query_bin/latx-x86_64.json"
    set +e
    actual=$("$query_bin/latu-runtime-manager" current --abi x86_64 \
        2> "$workdir/invalid.stderr")
    actual_status=$?
    set -e
    assert_status 2 "$actual_status" 'invalid runtime information'
    assert_output "$invalid_expected" "$actual" \
        'invalid runtime information'
done

printf '%s' \
    '{"schema_version":1,"guest_abi":"x86_64","runtime_root":"' \
    > "$query_bin/latx-x86_64.json"
printf '\377' >> "$query_bin/latx-x86_64.json"
printf '%s\n' '","runtime_source":"default"}' \
    >> "$query_bin/latx-x86_64.json"
set +e
actual=$("$query_bin/latu-runtime-manager" current --abi x86_64 \
    2> "$workdir/invalid-utf8.stderr")
actual_status=$?
set -e
assert_status 2 "$actual_status" 'invalid UTF-8 runtime information'
assert_output "$invalid_expected" "$actual" \
    'invalid UTF-8 runtime information'

printf '%s' \
    "$(runtime_info_json x86_64 /runtime/64 default)" \
    > "$query_bin/latx-x86_64.json"
printf '\000trailing' >> "$query_bin/latx-x86_64.json"
set +e
actual=$("$query_bin/latu-runtime-manager" current --abi x86_64 \
    2> "$workdir/embedded-nul.stderr")
actual_status=$?
set -e
assert_status 2 "$actual_status" 'embedded NUL runtime information'
assert_output "$invalid_expected" "$actual" \
    'embedded NUL runtime information'

marker=$workdir/must-not-exist
special_input='line\nquote\"slash\\\u4e2d$(touch '$marker')'
printf '%s\n' \
    "$(runtime_info_json x86_64 "$special_input" environment)" \
    > "$query_bin/latx-x86_64.json"
special_root='"line\nquote\"slash\\中$(touch '$marker')"'
special_expected=$(runtime_query_json x86_64 present selected \
    "$special_root" '"environment"')
set +e
actual=$("$query_bin/latu-runtime-manager" current --abi x86_64 \
    2> "$workdir/special.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'escaped runtime root'
assert_output "$special_expected" "$actual" 'escaped runtime root'
[ ! -e "$marker" ] || fail 'runtime information executed shell content'

printf '%s\n' \
    "$(runtime_info_json x86_64 /runtime/64 default)" \
    > "$query_bin/latx-x86_64.json"
printf '3\n' > "$query_bin/latx-x86_64.exit"
failed_expected=$(runtime_query_json x86_64 present \
    translator_query_failed null null)
set +e
actual=$("$query_bin/latu-runtime-manager" current --abi x86_64 \
    2> "$workdir/failed.stderr")
actual_status=$?
set -e
assert_status 2 "$actual_status" 'translator query failure'
assert_output "$failed_expected" "$actual" 'translator query failure'
rm "$query_bin/latx-x86_64.exit"

cat > "$query_bin/latx-x86_64" <<'EOF'
#!/bin/sh
trap '' PIPE
while :; do
    dd if=/dev/zero bs=65536 count=32 2> /dev/null || :
done
EOF
chmod +x "$query_bin/latx-x86_64"
set +e
actual=$("$query_bin/latu-runtime-manager" current --abi x86_64 \
    2> "$workdir/oversized.stderr")
actual_status=$?
set -e
assert_status 2 "$actual_status" 'oversized translator response'
assert_output "$failed_expected" "$actual" 'oversized translator response'
install_query_translator "$query_bin" latx-x86_64 \
    "$(runtime_info_json x86_64 /runtime/64 default)"

set +e
"$query_bin/latu-runtime-manager" current --abi amd64 \
    > "$workdir/abi.stdout" 2> "$workdir/abi.stderr"
abi_status=$?
set -e
assert_status 2 "$abi_status" 'unknown guest ABI'

runtime_root=$workdir/runtime-both
make_loader "$runtime_root" x86_64
make_loader "$runtime_root" i386
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root "$runtime_root" \
    2> "$workdir/inspect.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'dual-ABI runtime root'
assert_contains '"guest_abi":"x86_64"' "$actual" \
    'dual-ABI x86-64 inspection'
assert_contains '"guest_abi":"i386"' "$actual" \
    'dual-ABI i386 inspection'
ready_count=$(printf '%s\n' "$actual" | \
    grep -c '"inspection_status":"ready","reason":"ready"')
assert_status 2 "$ready_count" 'dual-ABI ready records'

set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi x86_64 \
    "$runtime_root" 2> "$workdir/inspect-one.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'single-ABI runtime root'
assert_contains '"guest_abi":"x86_64"' "$actual" \
    'single-ABI x86-64 inspection'
case "$actual" in
    *'"guest_abi":"i386"'*) fail 'single-ABI inspection reported i386' ;;
esac

missing_root=$workdir/runtime-missing
mkdir -p "$missing_root"
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi i386 \
    "$missing_root" 2> "$workdir/inspect-missing.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'missing runtime loader'
assert_contains '"inspection_status":"missing","reason":"loader_not_found"' \
    "$actual" 'missing runtime loader'

invalid_root=$workdir/runtime-invalid
make_loader "$invalid_root" i386
mkdir -p "$invalid_root/lib64"
cp "$invalid_root/lib/ld-linux.so.2" \
    "$invalid_root/lib64/ld-linux-x86-64.so.2"
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi x86_64 \
    "$invalid_root" 2> "$workdir/inspect-invalid.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'wrong loader ABI'
assert_contains '"inspection_status":"invalid","reason":"loader_invalid_elf"' \
    "$actual" 'wrong loader ABI'

truncated_root=$workdir/runtime-truncated
mkdir -p "$truncated_root/lib64"
printf '\177ELF\002\001\001' > \
    "$truncated_root/lib64/ld-linux-x86-64.so.2"
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi x86_64 \
    "$truncated_root" 2> "$workdir/inspect-truncated.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'truncated loader'
assert_contains '"inspection_status":"invalid","reason":"loader_invalid_elf"' \
    "$actual" 'truncated loader'

bad_ehsize_root=$workdir/runtime-bad-ehsize
make_loader "$bad_ehsize_root" x86_64
python3 - "$bad_ehsize_root/lib64/ld-linux-x86-64.so.2" <<'PY'
import sys

with open(sys.argv[1], "r+b") as loader:
    loader.seek(52)
    loader.write(b"\0\0")
PY
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi x86_64 \
    "$bad_ehsize_root" 2> "$workdir/inspect-ehsize.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'invalid ELF header size'
assert_contains '"inspection_status":"invalid","reason":"loader_invalid_elf"' \
    "$actual" 'invalid ELF header size'

bad_phentsize_root=$workdir/runtime-bad-phentsize
make_loader "$bad_phentsize_root" x86_64
python3 - "$bad_phentsize_root/lib64/ld-linux-x86-64.so.2" <<'PY'
import sys

with open(sys.argv[1], "r+b") as loader:
    loader.seek(54)
    loader.write(b"\x40\0")
PY
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi x86_64 \
    "$bad_phentsize_root" 2> "$workdir/inspect-phentsize.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'invalid ELF program header size'
assert_contains '"inspection_status":"invalid","reason":"loader_invalid_elf"' \
    "$actual" 'invalid ELF program header size'

em486_root=$workdir/runtime-em486
make_loader "$em486_root" i386
python3 - "$em486_root/lib/ld-linux.so.2" <<'PY'
import sys

with open(sys.argv[1], "r+b") as loader:
    loader.seek(18)
    loader.write(b"\x06\0")
PY
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi i386 \
    "$em486_root" 2> "$workdir/inspect-em486.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'EM_486 loader'
assert_contains '"inspection_status":"ready","reason":"ready"' \
    "$actual" 'EM_486 loader'

symlink_root=$workdir/runtime-symlink
mkdir -p "$symlink_root/usr/lib/runtime" "$symlink_root/lib64"
make_loader "$workdir/symlink-loader" x86_64
cp "$workdir/symlink-loader/lib64/ld-linux-x86-64.so.2" \
    "$symlink_root/usr/lib/runtime/loader.so"
ln -s ../usr/lib/runtime/loader.so \
    "$symlink_root/lib64/ld-linux-x86-64.so.2"
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi x86_64 \
    "$symlink_root" 2> "$workdir/inspect-symlink.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'root-confined loader symlink'
assert_contains '"inspection_status":"ready","reason":"ready"' \
    "$actual" 'root-confined loader symlink'

escape_root=$workdir/runtime-escape
outside_loader=$workdir/outside-loader
make_loader "$outside_loader" x86_64
mkdir -p "$escape_root/lib64"
ln -s "$outside_loader/lib64/ld-linux-x86-64.so.2" \
    "$escape_root/lib64/ld-linux-x86-64.so.2"
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi x86_64 \
    "$escape_root" 2> "$workdir/inspect-escape.stderr")
actual_status=$?
set -e
assert_status 2 "$actual_status" 'escaping loader symlink'
assert_contains '"inspection_status":"unknown","reason":"loader_escapes_root"' \
    "$actual" 'escaping loader symlink'

broken_root=$workdir/runtime-broken-link
mkdir -p "$broken_root/lib"
ln -s nowhere "$broken_root/lib/ld-linux.so.2"
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi i386 \
    "$broken_root" 2> "$workdir/inspect-broken.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'broken loader symlink'
assert_contains \
    '"inspection_status":"invalid",'\
'"reason":"loader_symlink_broken"' \
    "$actual" 'broken loader symlink'

directory_root=$workdir/runtime-directory-loader
mkdir -p "$directory_root/lib/ld-linux.so.2"
set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi i386 \
    "$directory_root" 2> "$workdir/inspect-directory.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'directory loader'
assert_contains '"inspection_status":"invalid","reason":"loader_not_regular"' \
    "$actual" 'directory loader'

set +e
actual=$("$query_bin/latu-runtime-manager" inspect-root --abi x86_64 \
    "$workdir/no-such-runtime" 2> "$workdir/inspect-no-root.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'missing runtime root'
assert_contains '"inspection_status":"missing","reason":"root_not_found"' \
    "$actual" 'missing runtime root'

dash_root=$workdir/-runtime
make_loader "$dash_root" x86_64
(
    cd "$workdir"
    "$query_bin/latu-runtime-manager" inspect-root --abi x86_64 -- -runtime
) > "$workdir/inspect-dash.stdout" || fail 'dash-prefixed root failed'
assert_contains '"inspection_status":"ready","reason":"ready"' \
    "$(cat "$workdir/inspect-dash.stdout")" 'dash-prefixed root'

printf '%s\n' \
    "$(runtime_info_json x86_64 "$runtime_root" user_config)" \
    > "$query_bin/latx-x86_64.json"
printf '%s\n' \
    "$(runtime_info_json i386 "$runtime_root" system_config)" \
    > "$query_bin/latx-i386.json"
set +e
actual=$("$query_bin/latu-runtime-manager" doctor \
    2> "$workdir/doctor.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'healthy dual-ABI doctor'
doctor_ready_count=$(printf '%s\n' "$actual" | \
    grep -c '"inspection_status":"ready","readiness":"ready","reason":"ready"')
assert_status 2 "$doctor_ready_count" 'healthy dual-ABI doctor records'

mv "$query_bin/latx-i386" "$query_bin/latx-i386.absent"
set +e
actual=$("$query_bin/latu-runtime-manager" doctor \
    2> "$workdir/doctor-partial.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'one ready ABI doctor'
assert_contains '"guest_abi":"x86_64"' "$actual" \
    'one ready ABI doctor'
assert_contains \
    '"guest_abi":"i386","translator_status":"missing",'\
'"query_status":"translator_missing"' \
    "$actual" 'unavailable i386 doctor'
assert_contains \
    '"inspection_status":"not_checked","readiness":"unavailable",'\
'"reason":"translator_missing"' \
    "$actual" 'unavailable i386 doctor'
mv "$query_bin/latx-i386.absent" "$query_bin/latx-i386"

printf '%s\n' \
    "$(runtime_info_json x86_64 "$invalid_root" environment)" \
    > "$query_bin/latx-x86_64.json"
set +e
actual=$("$query_bin/latu-runtime-manager" doctor \
    2> "$workdir/doctor-broken.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'broken runtime doctor'
assert_contains '"guest_abi":"x86_64"' "$actual" \
    'broken runtime doctor'
assert_contains \
    '"inspection_status":"invalid","readiness":"broken",'\
'"reason":"loader_invalid_elf"' \
    "$actual" 'broken runtime doctor'
assert_contains '"guest_abi":"i386"' "$actual" \
    'healthy peer runtime doctor'

printf '%s\n' '{"schema_version":1,"guest_abi":"x86_64"}' \
    > "$query_bin/latx-x86_64.json"
set +e
actual=$("$query_bin/latu-runtime-manager" doctor \
    2> "$workdir/doctor-unknown.stderr")
actual_status=$?
set -e
assert_status 2 "$actual_status" 'invalid runtime-info doctor'
assert_contains '"query_status":"invalid_runtime_info"' "$actual" \
    'invalid runtime-info doctor'
assert_contains \
    '"inspection_status":"not_checked","readiness":"unknown",'\
'"reason":"invalid_runtime_info"' \
    "$actual" 'invalid runtime-info doctor'

mv "$query_bin/latx-x86_64" "$query_bin/latx-x86_64.absent"
mv "$query_bin/latx-i386" "$query_bin/latx-i386.absent"
set +e
actual=$("$query_bin/latu-runtime-manager" doctor \
    2> "$workdir/doctor-unavailable.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'no translator doctor'
unavailable_count=$(printf '%s\n' "$actual" | \
    grep -c '"readiness":"unavailable","reason":"translator_missing"')
assert_status 2 "$unavailable_count" 'no translator doctor records'
mv "$query_bin/latx-x86_64.absent" "$query_bin/latx-x86_64"
mv "$query_bin/latx-i386.absent" "$query_bin/latx-i386"

printf '%s\n' \
    "$(runtime_info_json x86_64 "$runtime_root" default)" \
    > "$query_bin/latx-x86_64.json"
set +e
actual=$("$query_bin/latu-runtime-manager" doctor --abi x86_64 \
    2> "$workdir/doctor-one.stderr")
actual_status=$?
set -e
assert_status 0 "$actual_status" 'single-ABI doctor'
assert_contains '"guest_abi":"x86_64"' "$actual" 'single-ABI doctor'
case "$actual" in
    *'"guest_abi":"i386"'*) fail 'single-ABI doctor reported i386' ;;
esac

program_dir=$workdir/programs
x86_64_program=$program_dir/x86_64-dynamic
make_program "$x86_64_program" x86_64 /lib64/ld-linux-x86-64.so.2
LATU_TEST_ARGV_LOG=$workdir/doctor-program-argv \
    "$query_bin/latu-runtime-manager" doctor --program "$x86_64_program" \
    > "$workdir/doctor-program.stdout" ||
    fail 'program doctor did not infer x86-64 ABI'
actual=$(cat "$workdir/doctor-program.stdout")
assert_contains '"guest_abi":"x86_64"' "$actual" \
    'program doctor inferred x86-64 ABI'
assert_contains '"loader_path":"/lib64/ld-linux-x86-64.so.2"' "$actual" \
    'program doctor used PT_INTERP'
case "$actual" in
    *'"guest_abi":"i386"'*) fail 'x86-64 program doctor queried i386' ;;
esac
[ ! -e "$workdir/doctor-program-argv.i386" ] ||
    fail 'x86-64 program doctor executed i386 translator'

i386_program=$program_dir/i386-dynamic
make_program "$i386_program" i386 /lib/ld-linux.so.2
LATU_TEST_ARGV_LOG=$workdir/doctor-i386-argv \
    "$query_bin/latu-runtime-manager" doctor --program "$i386_program" \
    > "$workdir/doctor-i386.stdout" ||
    fail 'program doctor did not infer i386 ABI'
actual=$(cat "$workdir/doctor-i386.stdout")
assert_contains '"guest_abi":"i386"' "$actual" \
    'program doctor inferred i386 ABI'
assert_contains '"loader_path":"/lib/ld-linux.so.2"' "$actual" \
    'i386 program doctor used PT_INTERP'
case "$actual" in
    *'"guest_abi":"x86_64"'*) fail 'i386 program doctor queried x86-64' ;;
esac
[ ! -e "$workdir/doctor-i386-argv.x86_64" ] ||
    fail 'i386 program doctor executed x86-64 translator'

set +e
"$query_bin/latu-runtime-manager" doctor --abi x86_64 \
    --program "$i386_program" > "$workdir/doctor-mismatch.stdout" \
    2> "$workdir/doctor-mismatch.stderr"
actual_status=$?
set -e
assert_status 2 "$actual_status" 'program and requested ABI mismatch'
grep -F 'does not match requested ABI' "$workdir/doctor-mismatch.stderr" \
    > /dev/null || fail 'program ABI mismatch diagnostic missing'

custom_root=$workdir/runtime-custom-loader
custom_interp=/opt/latu/lib/ld-custom-x86-64.so.1
make_loader "$workdir/custom-loader-source" x86_64
mkdir -p "$custom_root${custom_interp%/*}"
cp "$workdir/custom-loader-source/lib64/ld-linux-x86-64.so.2" \
    "$custom_root$custom_interp"
custom_program=$program_dir/x86_64-custom-interp
make_program "$custom_program" x86_64 "$custom_interp"
printf '%s\n' \
    "$(runtime_info_json x86_64 "$custom_root" user_config)" \
    > "$query_bin/latx-x86_64.json"
actual=$("$query_bin/latu-runtime-manager" doctor \
    --program "$custom_program") || fail 'custom PT_INTERP doctor failed'
assert_contains '"loader_path":"/opt/latu/lib/ld-custom-x86-64.so.1"' \
    "$actual" 'custom PT_INTERP path'
assert_contains '"inspection_status":"ready","readiness":"ready"' \
    "$actual" 'custom PT_INTERP readiness'

host_loader_root=$workdir/host-loader-source
make_loader "$host_loader_root" x86_64
host_loader=$host_loader_root/lib64/ld-linux-x86-64.so.2
fallback_program=$program_dir/x86_64-host-fallback
make_program "$fallback_program" x86_64 "$host_loader"
fallback_root=$workdir/runtime-without-configured-loader
mkdir -p "$fallback_root"
printf '%s\n' \
    "$(runtime_info_json x86_64 "$fallback_root" environment)" \
    > "$query_bin/latx-x86_64.json"
actual=$("$query_bin/latu-runtime-manager" doctor \
    --program "$fallback_program") || fail 'host loader fallback failed'
assert_contains '"inspection_status":"missing"' "$actual" \
    'host fallback root status'
assert_contains '"readiness":"ready_with_host_fallback"' "$actual" \
    'host fallback readiness'
assert_contains '"effective_loader_status":"ready"' "$actual" \
    'host fallback effective loader status'
assert_contains '"loader_source":"host_fallback"' "$actual" \
    'host fallback source'

configured_fallback=$fallback_root${host_loader}
mkdir -p "${configured_fallback%/*}"
cp "$workdir/custom-loader-source/lib64/ld-linux-x86-64.so.2" \
    "$configured_fallback"
python3 - "$configured_fallback" <<'PY'
import sys

with open(sys.argv[1], "r+b") as loader:
    loader.seek(18)
    loader.write(b"\x03\0")
PY
set +e
actual=$("$query_bin/latu-runtime-manager" doctor \
    --program "$fallback_program" 2> "$workdir/no-fallback.stderr")
actual_status=$?
set -e
assert_status 1 "$actual_status" 'existing invalid root loader'
assert_contains '"effective_loader_status":"invalid"' "$actual" \
    'invalid configured loader blocks fallback'
assert_contains '"loader_source":"runtime_root"' "$actual" \
    'invalid configured loader source'

static_program=$program_dir/x86_64-static
make_program "$static_program" x86_64
printf '%s\n' \
    "$(runtime_info_json x86_64 "$workdir/no-such-static-runtime" default)" \
    > "$query_bin/latx-x86_64.json"
actual=$("$query_bin/latu-runtime-manager" doctor \
    --program "$static_program") || fail 'static program doctor failed'
assert_contains '"loader_path":null' "$actual" \
    'static program has no loader'
assert_contains '"inspection_status":"not_required",'\
'"readiness":"ready","reason":"static_program"' "$actual" \
    'static program readiness'

printf 'not an ELF\n' > "$program_dir/invalid"
set +e
"$query_bin/latu-runtime-manager" doctor --program "$program_dir/invalid" \
    > "$workdir/doctor-invalid.stdout" \
    2> "$workdir/doctor-invalid.stderr"
actual_status=$?
set -e
assert_status 2 "$actual_status" 'invalid program ELF'
grep -F 'program_invalid_elf' "$workdir/doctor-invalid.stderr" \
    > /dev/null || fail 'invalid program diagnostic missing'
actual=$(cat "$workdir/doctor-invalid.stdout")
assert_contains '"guest_abi":null' "$actual" \
    'invalid program ABI is unknown'
assert_contains '"program_status":"invalid",'\
'"program_reason":"program_invalid_elf"' "$actual" \
    'invalid program structured diagnosis'

set +e
"$query_bin/latu-runtime-manager" doctor \
    --program "$program_dir/does-not-exist" \
    > "$workdir/doctor-missing-program.stdout" \
    2> "$workdir/doctor-missing-program.stderr"
actual_status=$?
set -e
assert_status 2 "$actual_status" 'missing program ELF'
actual=$(cat "$workdir/doctor-missing-program.stdout")
assert_contains '"program_status":"invalid",'\
'"program_reason":"program_not_found"' "$actual" \
    'missing program structured diagnosis'

echo 'PASS: distribution-neutral runtime queries and diagnosis'
