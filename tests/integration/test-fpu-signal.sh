#!/bin/sh
set -eu

emulator=$1
source_file=$2
softfpu=${LATX_SOFTFPU:-1}

workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
failures=0

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
    expected=$3
    guest="$workdir/fpu-signal-$case_id"
    report="$workdir/report-$case_id.bin"

    "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
        -Wl,--build-id=none -DTEST_CASE="$case_id" \
        "$source_file" -o "$guest"

    set +e
    timeout -s KILL 10 env LATX_AOT=0 LATX_KZT=0 \
        LATX_SOFTFPU="$softfpu" \
        LATX_ROUNDING_OPT=0 "$emulator" "$guest" >"$report"
    ret=$?
    set -e
    if [ "$ret" -ne 0 ]; then
        echo "FAIL: $case_name returned $ret (124/137 may indicate timeout)" >&2
        failures=$((failures + 1))
        return 0
    fi

    report_size=$(wc -c <"$report" | tr -d '[:space:]')
    if [ "$report_size" -ne 8 ]; then
        echo "FAIL: $case_name wrote $report_size bytes, expected 8" >&2
        failures=$((failures + 1))
        return 0
    fi
    actual=$(od -An -v -tx1 "$report" | tr -d '[:space:]')
    if [ "$actual" != "$expected" ]; then
        echo "FAIL: $case_name produced the wrong raw report" >&2
        echo "  expected: $expected" >&2
        echo "  actual:   $actual" >&2
        failures=$((failures + 1))
        return 0
    fi
    echo "PASS: $case_name"
}

# First word proves handler execution; second is frame flags, restored flags,
# or the SSE result after sigreturn. Cases run in separate guest processes.
run_case 1 signal-frame-sees-sse-invalid 0100000001000000
run_case 2 sigreturn-restores-edited-mxcsr-flags 0100000004000000
run_case 3 sigreturn-restores-edited-mxcsr-round-up 010000000100803f

if [ "$failures" -ne 0 ]; then
    echo "FAIL: $failures signal FPU cases failed" >&2
    exit 1
fi
