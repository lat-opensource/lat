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
    guest="$workdir/fpu-flags-$case_id"

    "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
        -Wl,--build-id=none -DTEST_CASE="$case_id" \
        "$source_file" -o "$guest"

    report="$workdir/report-$case_id.bin"

    set +e
    timeout -s KILL 10 env LATX_AOT=0 LATX_KZT=0 LATX_SOFTFPU=0 \
        LATX_ROUNDING_OPT=0 \
        "$emulator" "$guest" >"$report"
    ret=$?
    set -e

    case $ret in
    0)
        ;;
    124)
        echo "FAIL: $case_name timed out" >&2
        failures=$((failures + 1))
        return 0
        ;;
    *)
        echo "FAIL: $case_name returned $ret" >&2
        failures=$((failures + 1))
        return 0
        ;;
    esac

    report_size=$(wc -c <"$report" | tr -d '[:space:]')
    if [ "$report_size" -ne 8 ]; then
        echo "FAIL: $case_name wrote $report_size report bytes, expected 8" >&2
        failures=$((failures + 1))
        return 0
    fi

    actual_report=$(od -An -v -tx1 "$report" | tr -d '[:space:]')
    if [ "$actual_report" != "$expected_report" ]; then
        echo "FAIL: $case_name produced the wrong raw report" >&2
        echo "  expected: $expected_report" >&2
        echo "  actual:   $actual_report" >&2
        failures=$((failures + 1))
        return 0
    fi

    echo "PASS: $case_name"
}

# Reports contain a little-endian uint16_t x87 status field followed by a
# little-endian uint32_t MXCSR status field.  Both are masked to exception
# flags only by the guest, so the expected values are stable across runs.
run_case 1 x87-invalid-isolated \
    0100000000000000
run_case 2 sse-invalid-isolated \
    0000000001000000
run_case 3 x87-invalid-then-sse-exact \
    0100000000000000
run_case 4 sse-invalid-then-x87-exact \
    0000000001000000
run_case 5 x87-invalid-then-x87-exact-sticky \
    0100000000000000
run_case 6 sse-invalid-then-sse-exact-sticky \
    0000000001000000
run_case 7 x87-inexact-isolated \
    2000000000000000
run_case 8 sse-invalid-then-x87-inexact \
    2000000001000000
run_case 9 x87-inexact-then-sse-invalid \
    2000000001000000
run_case 10 sse-divsd-invalid \
    0000000001000000
run_case 11 sse-divps-invalid \
    0000000001000000
run_case 12 sse-divpd-invalid \
    0000000001000000
run_case 13 sse-addss-invalid \
    0000000001000000
run_case 14 sse-addsd-invalid \
    0000000001000000
run_case 15 sse-addps-invalid \
    0000000001000000
run_case 16 sse-addpd-invalid \
    0000000001000000
run_case 17 sse-subss-invalid \
    0000000001000000
run_case 18 sse-subsd-invalid \
    0000000001000000
run_case 19 sse-subps-invalid \
    0000000001000000
run_case 20 sse-subpd-invalid \
    0000000001000000
run_case 21 sse-mulss-invalid \
    0000000001000000
run_case 22 sse-mulsd-invalid \
    0000000001000000
run_case 23 sse-mulps-invalid \
    0000000001000000
run_case 24 sse-mulpd-invalid \
    0000000001000000
run_case 25 sse-sqrtss-invalid \
    0000000001000000
run_case 26 sse-sqrtsd-invalid \
    0000000001000000
run_case 27 sse-sqrtps-invalid \
    0000000001000000
run_case 28 sse-sqrtpd-invalid \
    0000000001000000
run_case 29 x87-fadd-invalid \
    0100000000000000
run_case 30 x87-fsub-invalid \
    0100000000000000
run_case 31 x87-fmul-invalid \
    0100000000000000
run_case 32 x87-fsqrt-invalid \
    0100000000000000
run_case 33 x87-faddp-invalid \
    0100000000000000
run_case 34 x87-fsubp-invalid \
    0100000000000000
run_case 35 x87-fmulp-invalid \
    0100000000000000
run_case 36 x87-fdivp-inexact \
    2000000000000000
run_case 37 x87-fsubr-invalid \
    0100000000000000
run_case 38 x87-fsubrp-invalid \
    0100000000000000
run_case 39 sse-invalid-then-x87-fdivr-inexact \
    2000000001000000
run_case 40 sse-invalid-then-x87-fdivrp-inexact \
    2000000001000000
run_case 41 x87-fiadd-inexact \
    2000000000000000
run_case 42 x87-fisub-inexact \
    2000000000000000
run_case 43 x87-fisubr-inexact \
    2000000000000000
run_case 44 x87-fimul-invalid \
    0100000000000000
run_case 45 x87-fidiv-inexact \
    2000000000000000
run_case 46 x87-fidivr-inexact \
    2000000000000000
run_case 47 x87-fdiv-zero-fnstsw \
    0400000000000000
run_case 48 x87-fdiv-inexact-fxsave \
    2000000000000000
run_case 49 sse-invalid-fxsave \
    0000000001000000
run_case 50 x87-fdiv-zero-fxsave \
    0400000000000000
run_case 51 x87-fdiv-invalid-fxsave \
    0100000000000000
run_case 56 x87-dirty-then-clean-fxrstor \
    0000000000000000
run_case 57 sse-dirty-then-clean-fxrstor \
    0000000000000000
run_case 58 fxrstor-restored-flags-visible \
    2000000001000000
run_case 59 sse-invalid-then-fnclex \
    0000000001000000
run_case 60 x87-invalid-then-fnclex \
    0000000000000000
run_case 61 sse-invalid-then-fninit \
    0000000001000000

if [ "${LATX_TEST_XSAVE:-0}" = 1 ]; then
    run_case 52 x87-fdiv-inexact-xsave \
        2000000000000000
    run_case 53 sse-invalid-xsave \
        0000000001000000
    run_case 54 x87-fdiv-inexact-xsaveopt \
        2000000000000000
    run_case 55 sse-invalid-xsaveopt \
        0000000001000000
else
    echo "SKIP: XSAVE flag cases require a CONFIG_LATX_AVX_OPT build"
fi

if [ "${LATX_TEST_AVX:-0}" = 1 ]; then
    run_case 62 avx-vdivss-zero \
        0000000004000000
    run_case 63 avx-vsqrtss-invalid \
        0000000001000000
    run_case 64 avx-vaddps-inexact \
        0000000020000000
else
    echo "SKIP: AVX flag cases require a CONFIG_LATX_AVX_OPT build"
fi

if [ "$failures" -ne 0 ]; then
    echo "FAIL: $failures FPU flag test cases failed" >&2
    exit 1
fi
