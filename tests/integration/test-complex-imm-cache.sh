#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
tmp_root=${TMPDIR:-${HOME}/tmp}
mkdir -p "$tmp_root"
workdir=$(mktemp -d "$tmp_root/complex-imm-cache.XXXXXX")
cleanup()
{
    for attempt in 1 2 3 4 5; do
        if rm -rf "$workdir"; then
            return
        fi
        sleep 0.1
    done
    return 1
}
trap cleanup EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none -Wl,--no-relax "$source_file" \
    -o "$workdir/complex-static"

run_guest()
{
    timeout 10s env LATX_AOT=0 LATX_KZT=0 LATX_VPAES=0 \
        "$@" "$emulator" "$workdir/complex-static"
}

run_guest LATX_IMM_COMPLEX=0
run_guest LATX_IMM_REG=1111 LATX_IMM_COMPLEX=7
run_guest LATX_IMM_COMPLEX_STATS=1 2>"$workdir/default.err"

for mask in 1 2 4 7; do
    run_guest LATX_IMM_COMPLEX="$mask" LATX_IMM_COMPLEX_STATS=1 \
        2>"$workdir/mask-$mask.err"
done

mkdir -p "$workdir/aot-home"
for pass in 1 2; do
    timeout 10s env HOME="$workdir/aot-home" LATX_AOT=1 LATX_KZT=0 \
        LATX_VPAES=0 LATX_IMM_COMPLEX=7 \
        "$emulator" "$workdir/complex-static"
done

if run_guest LATX_IMM_COMPLEX=invalid >/dev/null 2>&1; then
    echo "FAIL: invalid LATX_IMM_COMPLEX was accepted" >&2
    exit 1
fi
if run_guest LATX_IMM_COMPLEX=8 >/dev/null 2>&1; then
    echo "FAIL: out-of-range LATX_IMM_COMPLEX was accepted" >&2
    exit 1
fi
if run_guest LATX_IMM_COMPLEX_STATS=2 >/dev/null 2>&1; then
    echo "FAIL: invalid LATX_IMM_COMPLEX_STATS was accepted" >&2
    exit 1
fi

run_guest LATX_IMM_COMPLEX=0 LATX_IMM_COMPLEX_STATS=1 \
    2>"$workdir/disabled.err"
if grep -Eq '^\[LATX\]\[imm-complex\].*calls=[1-9][0-9]*' \
        "$workdir/disabled.err"; then
    echo "FAIL: disabled complex cache attempted a mode" >&2
    exit 1
fi

check_mode()
{
    mask=$1
    mode=$2
    grep -E "^\\[LATX\\]\\[imm-complex\\].*mode=$mode" \
        "$workdir/mask-$mask.err" |
        grep -Eq 'calls=[1-9][0-9]*.*hits=[1-9][0-9]*'
}

check_mode 1 base-disp
check_mode 2 index-disp
check_mode 4 base-index-disp
check_mode 7 base-disp
check_mode 7 index-disp
check_mode 7 base-index-disp

for mode in base-disp index-disp base-index-disp; do
    grep -E "^\[LATX\]\[imm-complex\].*mode=$mode" \
        "$workdir/default.err" |
        grep -Eq 'calls=[1-9][0-9]*.*hits=[1-9][0-9]*'
done

grep -Eq '^\[LATX\]\[imm-complex-skip\].*addr-size=[1-9][0-9]*' \
    "$workdir/mask-7.err"
grep -Eq '^\[LATX\]\[imm-complex-skip\].*segment=[1-9][0-9]*' \
    "$workdir/mask-7.err"
grep -Eq '^\[LATX\]\[imm-complex-skip\].*base-index-invalidations=[1-9][0-9]*' \
    "$workdir/mask-7.err"
grep -E '^\[LATX\]\[imm-complex\].*mode=base-index-disp' \
    "$workdir/mask-7.err" |
    grep -Eq 'replacements=[1-9][0-9]*'
LATX_IMM_COMPLEX=7 LATX_IMM_COMPLEX_STATS=1 \
    timeout 10s "$emulator" -latx-imm-complex 0 \
    "$workdir/complex-static" 2>"$workdir/override.err"
if grep -Eq '^\[LATX\]\[imm-complex\].*calls=[1-9][0-9]*' \
        "$workdir/override.err"; then
    echo "FAIL: command-line disable did not override the environment" >&2
    exit 1
fi

echo "PASS: complex immediate cache modes, JIT/AOT, and safety matrix"
