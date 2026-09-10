#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
tmp_root=${TMPDIR:-${HOME}/tmp}
mkdir -p "$tmp_root"
workdir=$(mktemp -d "$tmp_root/rip-imm-cache.XXXXXX")
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

common_flags="--target=x86_64-linux-gnu -fuse-ld=lld -nostdlib"
linker_flags="-Wl,--build-id=none -Wl,--no-relax"
"$clang" $common_flags -static $linker_flags \
    "$source_file" -o "$workdir/rip-static"
"$clang" $common_flags -static-pie $linker_flags \
    "$source_file" -o "$workdir/rip-pie"
"$clang" $common_flags -shared -Wl,-e,_start $linker_flags \
    "$source_file" -o "$workdir/rip-shared"

run_guest()
{
    guest=$1
    shift
    timeout 10s env LATX_AOT=0 LATX_KZT=0 LATX_VPAES=0 \
        "$@" "$emulator" "$guest"
}

for guest in "$workdir/rip-static" "$workdir/rip-pie" \
             "$workdir/rip-shared"; do
    "$guest"
    run_guest "$guest" LATX_IMM_RIP=0
    run_guest "$guest" LATX_IMM_RIP=1 LATX_IMM_RIP_STATS=1 \
        2>>"$workdir/enabled.err"
done

if run_guest "$workdir/rip-static" LATX_IMM_RIP=invalid >/dev/null 2>&1; then
    echo "FAIL: invalid LATX_IMM_RIP was accepted" >&2
    exit 1
fi
if run_guest "$workdir/rip-static" LATX_IMM_RIP_STATS=2 \
        >/dev/null 2>&1; then
    echo "FAIL: invalid LATX_IMM_RIP_STATS was accepted" >&2
    exit 1
fi

run_guest "$workdir/rip-static" LATX_IMM_RIP=0 LATX_IMM_RIP_STATS=1 \
    2>"$workdir/disabled.err"
if grep -q '^\[LATX\]\[imm-rip\]' "$workdir/disabled.err"; then
    echo "FAIL: disabled RIP cache produced statistics" >&2
    exit 1
fi

grep -Eq '^\[LATX\]\[imm-rip\].*calls=[1-9][0-9]*.*hits=[1-9][0-9]*' \
    "$workdir/enabled.err"
grep -Eq '^\[LATX\]\[imm-rip\].*misses=[1-9][0-9]*' \
    "$workdir/enabled.err"

LATX_IMM_RIP=1 LATX_IMM_RIP_STATS=1 \
    timeout 10s "$emulator" -latx-imm-rip 0 \
    "$workdir/rip-static" 2>"$workdir/override.err"
if grep -q '^\[LATX\]\[imm-rip\]' "$workdir/override.err"; then
    echo "FAIL: command-line disable did not override the environment" >&2
    exit 1
fi

echo "PASS: RIP cache ELF, GOT, page, helper, signal, fork/exec, and SMC matrix"
