#!/bin/sh
set -eu

[ "$#" -eq 4 ] || {
    echo "usage: $0 PYTHON MANAGER LATX_X86_64 LATX_I386" >&2
    exit 2
}

python=$1
manager=$("$python" -c 'import os, sys; print(os.path.realpath(sys.argv[1]))' \
    "$2")
latx_x86_64=$("$python" -c \
    'import os, sys; print(os.path.realpath(sys.argv[1]))' "$3")
latx_i386=$("$python" -c \
    'import os, sys; print(os.path.realpath(sys.argv[1]))' "$4")
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

fail()
{
    echo "FAIL: $*" >&2
    exit 1
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
    "$python" - "$loader_abi" "$loader_path" <<'PY'
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

bin=$workdir/bin
home=$workdir/home
runtime_x86_64=$workdir/runtime-x86_64
runtime_i386=$workdir/runtime-i386
app_x86_64=$workdir/app-x86_64
app_i386=$workdir/app-i386
mkdir -p "$bin" "$home/.config"
cp "$manager" "$bin/latu-runtime-manager"
ln -s "$latx_x86_64" "$bin/latx-x86_64"
ln -s "$latx_i386" "$bin/latx-i386"
make_loader "$runtime_x86_64" x86_64
make_loader "$runtime_i386" i386

cat > "$home/.config/latx-x86_64.conf" <<EOF
LAT_LD_PREFIX=$runtime_x86_64
[app]
LAT_LD_PREFIX=$app_x86_64
EOF
cat > "$home/.config/latx-i386.conf" <<EOF
LAT_LD_PREFIX=$runtime_i386
[app]
LAT_LD_PREFIX=$app_i386
EOF

(
    unset LAT_LD_PREFIX XDG_CONFIG_HOME
    HOME=$home
    export HOME
    "$bin/latu-runtime-manager" current --abi x86_64
) > "$workdir/current-x86_64.json" ||
    fail 'real x86-64 current query failed'
"$python" - "$workdir/current-x86_64.json" "$runtime_x86_64" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as source:
    record = json.load(source)
assert record["guest_abi"] == "x86_64", record
assert record["query_status"] == "selected", record
assert record["runtime_root"] == sys.argv[2], record
assert record["runtime_source"] == "user_config", record
PY

(
    unset LAT_LD_PREFIX XDG_CONFIG_HOME
    HOME=$home
    export HOME
    "$bin/latu-runtime-manager" current --abi i386 --program /usr/bin/app
) > "$workdir/current-i386-app.json" ||
    fail 'real i386 program query failed'
"$python" - "$workdir/current-i386-app.json" "$app_i386" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as source:
    record = json.load(source)
assert record["guest_abi"] == "i386", record
assert record["query_status"] == "selected", record
assert record["runtime_root"] == sys.argv[2], record
assert record["runtime_source"] == "user_config", record
PY

(
    unset LAT_LD_PREFIX XDG_CONFIG_HOME
    HOME=$home
    export HOME
    "$bin/latu-runtime-manager" list
) > "$workdir/list.jsonl" || fail 'real dual-ABI list failed'
"$python" - "$workdir/list.jsonl" \
    "$runtime_x86_64" "$runtime_i386" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as source:
    records = [json.loads(line) for line in source]
assert [item["guest_abi"] for item in records] == ["x86_64", "i386"]
assert [item["runtime_root"] for item in records] == sys.argv[2:]
assert all(item["query_status"] == "selected" for item in records)
PY

(
    unset LAT_LD_PREFIX XDG_CONFIG_HOME
    HOME=$home
    export HOME
    "$bin/latu-runtime-manager" doctor
) > "$workdir/doctor.jsonl" || fail 'real dual-ABI doctor failed'
"$python" - "$workdir/doctor.jsonl" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as source:
    records = [json.loads(line) for line in source]
assert [item["guest_abi"] for item in records] == ["x86_64", "i386"]
assert all(item["query_status"] == "selected" for item in records)
assert all(item["inspection_status"] == "ready" for item in records)
assert all(item["readiness"] == "ready" for item in records)
PY

echo 'PASS: real dual-ABI translator runtime diagnostics'
