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
run_case cmp_je_elide
run_case cmp_jne_elide
run_case cmp_jb_elide
run_case cmp_jae_elide
run_case test_je_elide
run_case test_jne_elide
run_case cmp_je_keep_zf
run_case test_jne_keep_zf
run_case cmp_jbe_elide
run_case cmp_ja_elide
run_case cmp_jl_elide
run_case cmp_jge_elide
run_case cmp_jle_elide
run_case cmp_jg_elide
run_case test_js_elide
run_case test_jns_elide
run_case test_jle_elide
run_case test_jg_elide
run_case test_jno_elide
run_case test_jo_elide
run_case test_jb_elide
run_case test_jbe_elide
run_case test_ja_elide
run_case test_jae_elide
run_case bt_jb_elide
run_case bt_jae_elide
run_case sub_jne_elide
run_case sub_jl_elide
run_case lock_sub_jne_keep_flags
run_case shr_jne_elide
run_case shr_zero_jne_keep_flags
run_case and_jne_elide
run_case lock_and_jne_keep_flags
run_case comisd_je_elide
run_case comiss_jne_elide
run_case ucomisd_jb_nan_elide
run_case ucomiss_jne_elide
run_case ucomisd_je_keep_zf
run_case cmp_xx_jne_elide
run_case test_xx_je_elide
run_case bt_xx_jb_elide
run_case ucomisd_xx_je_elide
run_case cmp_xx_je_keep_zf
run_case neg_cmovs_elide
run_case lock_neg_cmovs_keep_flags
run_case neg_cmovs_keep_sf
run_case ucomisd_seta_elide
run_case ucomisd_seta_keep_zf

echo "PASS: native pattern consumers preserve required EFLAGS semantics"
