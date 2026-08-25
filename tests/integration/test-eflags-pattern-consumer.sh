#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

run_case()
{
    entry=$1
    guest="$workdir/$entry"

    "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
        -Wl,--build-id=none -Wl,-e,"$entry" "$source_file" -o "$guest"

    set +e
    HOME="$workdir/home-$entry" LATX_AOT=0 LATX_KZT=0 \
        "$emulator" "$guest"
    ret=$?
    set -e
    if [ "$ret" -ne 0 ]; then
        echo "FAIL: $entry expected=0 actual=$ret" >&2
        exit 1
    fi
}

run_case cmp_setcc_elide
run_case test_cmov_elide
run_case cmp_setcc_keep_cf
run_case test_cmov_keep_flags
run_case cmp_sbb_elide
run_case cmp_sbb_keep_output_zf

echo "PASS: native pattern consumers preserve required EFLAGS semantics"
