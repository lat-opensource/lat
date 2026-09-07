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
    guest="$workdir/x87-pop-$case_id"

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
    if [ "$report_size" -ne 16 ]; then
        echo "FAIL: $case_name wrote $report_size report bytes, expected 16" >&2
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

# Reports contain the arithmetic result followed by the surviving stack
# sentinel, both as little-endian IEEE-754 binary64 values.
run_case 1 x87-faddp-result-and-pop \
    00000000000018400000000000002240
run_case 2 x87-fmulp-result-and-pop \
    00000000000020400000000000002240
run_case 3 x87-fsubp-result-direction-and-pop \
    00000000000000c00000000000002240
run_case 4 x87-fdivp-result-direction-and-pop \
    000000000000e03f0000000000002240
run_case 5 x87-fsubrp-result-direction-and-pop \
    00000000000000400000000000002240
run_case 6 x87-fdivrp-result-direction-and-pop \
    00000000000000400000000000002240

if [ "$failures" -ne 0 ]; then
    echo "FAIL: $failures x87 pop test cases failed" >&2
    exit 1
fi
