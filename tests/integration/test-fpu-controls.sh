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

run_case()
{
    case_id=$1
    case_name=$2
    expected=$3
    expected_size=${4:-4}
    guest="$workdir/fpu-controls-$case_id"
    report="$workdir/report-$case_id.bin"

    "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
        -Wl,--build-id=none -DTEST_CASE="$case_id" \
        "$source_file" -o "$guest"

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
    if [ "$report_size" -ne "$expected_size" ]; then
        echo "FAIL: $case_name wrote $report_size report bytes, expected $expected_size" >&2
        failures=$((failures + 1))
        return 0
    fi

    actual=$(od -An -v -tx1 "$report" | tr -d '[:space:]')
    if [ "$actual" != "$expected" ]; then
        echo "FAIL: $case_name produced the wrong result" >&2
        echo "  expected: $expected" >&2
        echo "  actual:   $actual" >&2
        failures=$((failures + 1))
        return 0
    fi

    echo "PASS: $case_name"
}

run_case 1 fninit-preserves-mxcsr-round-up 0100803f
run_case 2 x87-round-up-after-fldcw 0100803f
run_case 3 fldcw-invalidates-sse-domain 0100803f
run_case 4 fldenv-invalidates-sse-domain 0100803f
run_case 5 frstor-restores-x87-round-down 0000803f

run_case 48 haddps-round-up 0100803f
run_case 49 haddpd-round-up 010000000000f03f 8
run_case 50 hsubps-round-up 0100803f
run_case 51 hsubpd-round-up 010000000000f03f 8
run_case 52 dpps-round-up 0100803f
run_case 53 dppd-round-up 010000000000f03f 8
run_case 54 addsubps-round-up 0100803f
run_case 55 addsubpd-round-up 010000000000f03f 8

if [ "${LATX_TEST_XSAVE:-0}" = 1 ]; then
    run_case 6 xrstor-restores-x87-round-down 0000803f
    run_case 7 xrstor-x87-only-restores-round-down 0000803f
    run_case 8 xrstor-sse-only-restores-round-down 0000803f
    run_case 9 xrstor-both-restores-round-down 0000803f0000803f 8
else
    echo "SKIP: XRSTOR control case requires LATX_TEST_XSAVE=1"
fi

if [ "${LATX_TEST_AVX:-0}" = 1 ]; then
    run_case 10 vaddss-round-up 0100803f
    run_case 11 vaddps-round-up 0100803f
    run_case 12 vaddsd-round-up 010000000000f03f 8
    run_case 13 vaddpd-round-up 010000000000f03f 8
    run_case 14 vsubss-round-up 0100803f
    run_case 15 vsubps-round-up 0100803f
    run_case 16 vsubsd-round-up 010000000000f03f 8
    run_case 17 vsubpd-round-up 010000000000f03f 8
    run_case 18 vmulss-round-up 0200a03f
    run_case 19 vmulps-round-up 0200a03f
    run_case 20 vmulsd-round-up 020000000000f43f 8
    run_case 21 vmulpd-round-up 020000000000f43f 8
    run_case 22 vdivss-round-up 0bd7233d
    run_case 23 vdivps-round-up 0bd7233d
    run_case 24 vdivsd-round-up 565555555555d53f 8
    run_case 25 vdivpd-round-up 565555555555d53f 8
    run_case 26 vsqrtss-round-up f404b53f
    run_case 27 vsqrtps-round-up f404b53f
    run_case 28 vsqrtsd-round-up ab4c58e87ab6fb3f 8
    run_case 29 vsqrtpd-round-up ab4c58e87ab6fb3f 8
    run_case 30 vaddsubps-round-up 0100803f
    run_case 31 vaddsubpd-round-up 010000000000f03f 8
    run_case 32 vhaddps-round-up 0100803f
    run_case 33 vhaddpd-round-up 010000000000f03f 8
    run_case 34 vhsubps-round-up 0100803f
    run_case 35 vhsubpd-round-up 010000000000f03f 8
    run_case 36 vfmadd132ss-round-up 0100803f
    run_case 37 vfmadd132ps-round-up 0100803f
    run_case 38 vfmadd132sd-round-up 010000000000f03f 8
    run_case 39 vfmadd132pd-round-up 010000000000f03f 8
    run_case 40 vfmsub132ss-round-up 0100803f
    run_case 41 vfmsub132sd-round-up 010000000000f03f 8
    run_case 42 vfnmadd132ss-round-up 0100803f
    run_case 43 vfnmadd132sd-round-up 010000000000f03f 8
    run_case 44 vfnmsub132ss-round-up 0100803f
    run_case 45 vfnmsub132sd-round-up 010000000000f03f 8
    run_case 46 vdpps-round-up 0100803f
    run_case 47 vdppd-round-up 010000000000f03f 8
else
    echo "SKIP: AVX arithmetic cases require LATX_TEST_AVX=1"
fi

if [ "$failures" -ne 0 ]; then
    echo "FAIL: $failures FPU control test cases failed" >&2
    exit 1
fi
