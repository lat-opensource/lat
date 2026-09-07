#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
failures=0
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

if ! command -v od >/dev/null 2>&1; then
    echo "SKIP: od is required to inspect the guest report"
    exit 77
fi

run_case()
{
    case_id=$1
    case_name=$2
    expected_report=$3
    guest="$workdir/mxcsr-cvt-$case_id"

    "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
        -Wl,--build-id=none -DTEST_CASE="$case_id" \
        "$source_file" -o "$guest"

    for cvt_opt in 0 1; do
        report="$workdir/report-$case_id-$cvt_opt.bin"

        set +e
        timeout -s KILL 10 env LATX_AOT=0 LATX_KZT=0 \
            LATX_SOFTFPU=0 LATX_ROUNDING_OPT=0 LATX_CVT_OPT="$cvt_opt" \
            "$emulator" "$guest" >"$report"
        ret=$?
        set -e

        case $ret in
        0)
            ;;
        124)
            echo "FAIL: $case_name timed out with LATX_CVT_OPT=$cvt_opt" >&2
            failures=$((failures + 1))
            continue
            ;;
        *)
            echo "FAIL: $case_name returned $ret with LATX_CVT_OPT=$cvt_opt" >&2
            failures=$((failures + 1))
            continue
            ;;
        esac

        report_size=$(wc -c <"$report" | tr -d '[:space:]')
        if [ "$report_size" -ne 20 ]; then
            echo "FAIL: $case_name wrote $report_size report bytes, expected 20, with LATX_CVT_OPT=$cvt_opt" >&2
            failures=$((failures + 1))
            continue
        fi

        actual_report=$(od -An -v -tx1 "$report" | tr -d '[:space:]')
        if [ "$actual_report" != "$expected_report" ]; then
            echo "FAIL: $case_name produced the wrong raw report with LATX_CVT_OPT=$cvt_opt" >&2
            echo "  expected: $expected_report" >&2
            echo "  actual:   $actual_report" >&2
            failures=$((failures + 1))
            continue
        fi

        echo "PASS: $case_name with LATX_CVT_OPT=$cvt_opt"
    done
}

# Each expected report is five little-endian uint32_t values encoded as bytes:
# MXCSR status bits followed by up to four conversion results.
run_case 1 exact-normal \
    0000000007000000000000000000000000000000
run_case 2 scalar-cvtt-invalid \
    0100000000000080000000000000000000000000
run_case 3 scalar-cvt-invalid \
    0100000000000080000000000000000000000000
run_case 4 packed-lane0-invalid \
    0100000000000080010000000200000003000000
run_case 5 invalid-then-exact-sticky \
    0100000000000080070000000000000000000000
run_case 6 scalar-inexact \
    2000000001000000000000000000000000000000

if [ "$failures" -ne 0 ]; then
    echo "FAIL: $failures MXCSR/CVT test variants failed" >&2
    exit 1
fi
